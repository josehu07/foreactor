#include <iostream>
#include <vector>
#include <assert.h>
#include <liburing.h>

#include "value_pool.hpp"


#ifndef __FOREACTOR_SCG_NODES_H__
#define __FOREACTOR_SCG_NODES_H__


namespace foreactor {


class SCGraph;  // forward declaration


// Types of nodes on graph.
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
        const std::string name;
        const NodeType node_type;

    protected:
        SCGraph *scgraph = nullptr;

        SCGraphNode() = delete;
        SCGraphNode(std::string name, NodeType node_type)
                : name(name), node_type(node_type), scgraph(nullptr) {}
        virtual ~SCGraphNode() {}

    public:
        friend std::ostream& operator<<(std::ostream& s, const SCGraphNode& n);
};


// Stages of a SyscallNode.
typedef enum SyscallStage {
    STAGE_NOTREADY,     // there are missing arguments, not ready for issuing
    STAGE_UNISSUED,     // args are complete, not issued yet
    STAGE_PREPARED,     // ready to get filled to io_uring submission queue,
                        // actual io_uring_prep_xxx() not called yet
    STAGE_PROGRESS,     // prepared and has been submitted to io_uring async,
                        // completion not harvested yet
    STAGE_FINISHED      // issued sync / issued async and completion harvested
} SyscallStage;

// Concrete syscall types of SyscallNode.
typedef enum SyscallType {
    SC_BASE,
    SC_OPEN,    // open
    SC_PREAD    // pread
} SyscallType;


// Parent class of a syscall node in the dependency graph.
// Each syscall type is a child class that inherits from this class, and a
// dependency graph instance should be composed of instances of those
// child classes -- no direct instantiation of this parent class.
// See syscalls.hpp.
class SyscallNode : public SCGraphNode {
    friend class SCGraph;
    friend class IOUring;

    public:
        const SyscallType sc_type = SC_BASE;

    protected:
        // Fields that stay the same across loops.
        SCGraphNode *next_node = nullptr;
        EdgeType edge_type = EDGE_BASE;

        // Fields that may vary across loops.
        ValuePoolBase<SyscallStage> *stage;
        ValuePoolBase<long> *rc;

        SyscallNode() = delete;
        SyscallNode(std::string name, SyscallType sc_type, bool pure_sc,
                    ValuePoolBase<SyscallStage> *stage,
                    ValuePoolBase<long> *rc);
        virtual ~SyscallNode() {}

        // Every child class must implement the following three functions.
        virtual bool RefreshStage(EpochListBase *epoch) = 0;
        virtual long SyscallSync(EpochListBase *epoch, void *output_buf) = 0;
        virtual void PrepUring(EpochListBase *epoch, struct io_uring_sqe *sqe) = 0;
        virtual void ReflectResult(EpochListBase *epoch, void *output_buf) = 0;

        void PrepAsync(EpochListBase *epoch);
        void CmplAsync(EpochListBase *epoch);

        static bool IsForeactable(EdgeType edge, SyscallNode *next,
                                  EpochListBase *epoch);

    public:
        void PrintCommonInfo(std::ostream& s) const;
        friend std::ostream& operator<<(std::ostream& s, const SyscallNode& n);

        // Set the next node that this node points to. Weak edge means there's
        // early exit logic on that edge and the next node is not guaranteed
        // to be issued.
        void SetNext(SCGraphNode *node, bool weak_edge = false);

        // Invoke this syscall, possibly pre-issuing the next few syscalls
        // in graph. Must be invoked on current frontier node only.
        long Issue(EpochListBase *epoch, void *output_buf = nullptr);
};


// Special branching node.
// Used only when there is some decision to be made during the execution of
// a syscall graph, where the decision is not known at the time of building
// the graph.
class BranchNode final : public SCGraphNode {
    friend class SCGraph;
    friend class SyscallNode;

    private:
        // Fields that stay the same across loops.
        std::vector<SCGraphNode *> children;
        std::vector<int> epoch_dim_idx;

        // Fields that may vary across loops.
        ValuePoolBase<int> *decision;

        // Pick a child node based on decision. If the edge crossed is a
        // back-pointing edge, increments the corresponding epoch number
        // in epoch.
        SCGraphNode *PickBranch(EpochListBase *epoch);

    public:
        BranchNode() = delete;
        BranchNode(std::string name, ValuePoolBase<int> *decision);
        ~BranchNode() {}

        friend std::ostream& operator<<(std::ostream& s, const BranchNode& n);

        // Append child node to children list. Index of child in this vector
        // should correspond to the decision int. The second argument dim_idx
        // >= 0 means that the edge to this child is a looping-back edge and
        // is associated with the epoch number at this dimension index.
        void AppendChild(SCGraphNode *child, int dim_idx = -1);
};


}


#endif
