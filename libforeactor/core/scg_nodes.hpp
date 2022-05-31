#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <assert.h>
#include <liburing.h>

#include "value_pool.hpp"
#include "foreactor.h"


#ifndef __FOREACTOR_SCG_NODES_H__
#define __FOREACTOR_SCG_NODES_H__


namespace foreactor {


class SCGraph;  // forward declarations
class IOEngine;
class IOUring;


// Types of nodes in graph.
typedef enum NodeType {
    NODE_BASE,
    NODE_SC_PURE,   // pure syscall with no state changes, e.g., pread
    NODE_SC_SEFF,   // syscall with side effects that change state, e.g., open
    NODE_BRANCH     // special branching control flow
} NodeType;

// Types of edges between nodes.
typedef enum EdgeType {
    EDGE_BASE,
    EDGE_MUST,      // if predecessor occurs, the successor must occur
    EDGE_WEAK       // early exit could happen on this edge
} EdgeType;


// Parent class of a node in the dependency graph.
// Never do direct instantiation of this parent class.
class SCGraphNode {
    friend class SCGraph;

    public:
        const unsigned node_id;
        const std::string name;
        const NodeType node_type;

    protected:
        // Pointer to containing SCGraph.
        SCGraph *scgraph = nullptr;

        // Epoch dimensions associated with this node, which basically mark
        // the back-pointing edges enclosing this node.
        std::unordered_set<int> assoc_dims;

        SCGraphNode() = delete;
        SCGraphNode(unsigned node_id, std::string name, NodeType node_type,
                    SCGraph *scgraph,
                    const std::unordered_set<int>& assoc_dims);
        virtual ~SCGraphNode() {}

        // Take sum of associated dimensions from an EpochList.
        [[nodiscard]] int EpochSum(const EpochList& epoch);

        // Reset all ValuePools of this node.
        virtual void ResetValuePools() = 0;

    public:
        friend std::ostream& operator<<(std::ostream& s, const SCGraphNode& n);
};


// Concrete syscall types of SyscallNode.
typedef enum SyscallType {
    SC_BASE,
    SC_OPEN,    // open
    SC_CLOSE,   // close
    SC_PREAD,   // pread
    SC_PWRITE   // pwrite
} SyscallType;

// Stages of a SyscallNode.
typedef enum SyscallStage {
    STAGE_NOTREADY,     // there are missing arguments, not ready for issuing
    STAGE_ARGREADY,     // args are complete, not issued yet
    STAGE_PREPARED,     // ready to get filled to engine's submission queue,
                        // actual io_uring_prep_xxx() not called yet
    STAGE_ONTHEFLY,     // prepared and has been submitted to engine async,
                        // completion not harvested yet
    STAGE_FINISHED      // issued sync / issued async and completion harvested
} SyscallStage;


// Parent class of a syscall node in the dependency graph.
// Each syscall type is a child class that inherits from this class, and a
// dependency graph instance should be composed of instances of those
// child classes -- no direct instantiation of this parent class.
// See syscalls.hpp.
class SyscallNode : public SCGraphNode {
    friend class SCGraph;
    friend class IOEngine;
    friend class IOUring;

    public:
        const SyscallType sc_type = SC_BASE;

    protected:
        // Fields that stay the same across loops.
        SCGraphNode *next_node = nullptr;
        EdgeType edge_type = EDGE_BASE;

        // Fields that progress across loops.
        ValuePool<SyscallStage> stage;
        ValuePool<long> rc;

        SyscallNode() = delete;
        SyscallNode(unsigned node_id, std::string name, SyscallType sc_type,
                    bool pure_sc, SCGraph *scgraph,
                    const std::unordered_set<int>& assoc_dims);
        virtual ~SyscallNode() {}

        // Every child class must implement these methods.
        virtual long SyscallSync(const EpochList& epoch,
                                 void *output_buf) = 0;
        virtual void PrepUringSqe(int epoch_sum,
                                  struct io_uring_sqe *sqe) = 0;
        virtual void ReflectResult(const EpochList& epoch,
                                   void *output_buf) = 0;
        virtual bool GenerateArgs(const EpochList&) = 0;
        virtual void RemoveOneEpoch(const EpochList&) = 0;
        virtual void ResetValuePools() = 0;

        void PrepAsync(const EpochList& epoch);
        void CmplAsync(const EpochList& epoch);
        void RemoveOneFromCommonPools(const EpochList& epoch);
        void ResetCommonPools();

        static bool IsForeactable(bool weak_state, const SyscallNode *next);

    public:
        void PrintCommonInfo(std::ostream& s) const;
        friend std::ostream& operator<<(std::ostream& s, const SyscallNode& n);

        // Set the next node that this node points to. Weak edge means there's
        // early exit logic on that edge and the next node is not guaranteed
        // to be issued.
        void SetNext(SCGraphNode *node, bool weak_edge = false);

        // Invoke this syscall, possibly pre-issuing the next few syscalls
        // in graph. Must be invoked on current frontier node only. Both epoch
        // numbers are managed (incremented) by this function as well.
        long Issue(const EpochList& epoch, void *output_buf = nullptr);
};


// Special branching node.
// Used when there is branching on some condition that leads to different
// syscall subgraphs.
class BranchNode final : public SCGraphNode {
    friend class SCGraph;
    friend class SyscallNode;

    private:
        // Fields that stay the same across loops.
        size_t num_children = 0;
        std::vector<SCGraphNode *> children;
        std::vector<int> epoch_dims;

        // Fields that progress across loops.
        ValuePool<int> decision;

        // User-provided decision generator function. Returns true if decision
        // for that epoch ends up being ready; returns false otherwise.
        std::function<bool(const int *, int *)> arggen_func;
        bool GenerateDecision(const EpochList&);

        // Pick a child node based on decision value. Returns nullptr if
        // decision cannot be made yet. If traverses through a back-pointing
        // edge, will increment the corresponding epoch dimension.
        SCGraphNode *PickBranch(EpochList& epoch, bool do_remove = false);

        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        BranchNode() = delete;
        BranchNode(unsigned node_id, std::string name, size_t num_children,
                   SCGraph *scgraph,
                   const std::unordered_set<int>& assoc_dims,
                   std::function<bool(const int *, int *)> arggen_func);
        ~BranchNode() {}

        friend std::ostream& operator<<(std::ostream& s, const BranchNode& n);

        // Add child node to children array. The epoch_dim argument is given
        // when the edge to this child is a back-pointing edge, in which case
        // the epoch_dim specifies which dimension in the EpochList this
        // back-pointing edge corresponds to.
        void AppendChild(SCGraphNode *child_node, int epoch_dim = -1);
};


}


#endif
