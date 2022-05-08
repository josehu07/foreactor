#include <iostream>
#include <vector>
#include <assert.h>
#include <liburing.h>

#include "ring_buffer.hpp"


#ifndef __FOREACTOR_SCG_NODES_H__
#define __FOREACTOR_SCG_NODES_H__


namespace foreactor {


class SCGraph;  // forward declaration


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

        // Fields that progress across loops.
        int curr_epoch;     // actual execution up to frontier
        int peek_epoch;     // used in the pre-issuing algorithm
        RingBuffer<SyscallStage> stage;
        RingBuffer<long> rc;

        SyscallNode() = delete;
        SyscallNode(std::string name, SyscallType sc_type, bool pure_sc,
                    size_t rb_capacity);
        virtual ~SyscallNode() {}

        // Every child class must implement these methods.
        virtual bool RefreshStage(int epoch) = 0;
        virtual long SyscallSync(int epoch, void *output_buf) = 0;
        virtual void PrepUringSqe(int epoch, struct io_uring_sqe *sqe) = 0;
        virtual void ReflectResult(int epoch, void *output_buf) = 0;

        void PrepAsync(int epoch);
        void CmplAsync(int epoch);

        static bool IsForeactable(EdgeType edge, SyscallNode *next,
                                  int epoch);

    public:
        void PrintCommonInfo(std::ostream& s) const;
        friend std::ostream& operator<<(std::ostream& s, const SyscallNode& n);

        // Set the next node that this node points to. Weak edge means there's
        // early exit logic on that edge and the next node is not guaranteed
        // to be issued.
        void SetNext(SCGraphNode *node, bool weak_edge = false);

        // Invoke this syscall, possibly pre-issuing the next few syscalls
        // in graph. Must be invoked on current frontier node only.
        long Issue(int epoch, void *output_buf = nullptr);
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
        SCGraphNode **children;                         // array of node ptrs
        std::unordered_set<SCGraphNode *> *enclosed;    // array of sets

        // Fields that progress across loops.
        RingBuffer<int> decision;

        // Pick a child node based on decision. If the edge crossed is a
        // back-pointing edge, increments the corresponding epoch number
        // of that node.
        SCGraphNode *PickBranch(int epoch);

    public:
        BranchNode() = delete;
        BranchNode(std::string name, size_t num_children, size_t rb_capacity);
        ~BranchNode();

        friend std::ostream& operator<<(std::ostream& s, const BranchNode& n);

        // Add child node to children array.
        // If the first argument is nullptr, it means an edge pointing to end
        // of graph.
        // If the second argument is given a non-empty set, it means the edge
        // to this child is a looping-back edge, and this edge encloses the
        // nodes given in the set -- whenever this back-pointing edge is
        // traversed, those nodes' epoch numbers will be incremented.
        void SetChild(int child_idx, SCGraphNode *child_node,
                      std::unordered_set<SCGraphNode *> *enclosed = nullptr);
};


}


#endif
