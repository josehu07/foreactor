#include <vector>
#include <assert.h>
#include <liburing.h>


#ifndef __FOREACTOR_SCG_NODES_H__
#define __FOREACTOR_SCG_NODES_H__


namespace foreactor {


class SCGraph;

typedef enum NodeType {
    NODE_BASE,
    NODE_SC_PURE,   // pure syscall with no state changes, e.g., pread
    NODE_SC_SEFF,   // syscall with side effects that change state, e.g., open
    NODE_BRANCH     // special branching control flow
} NodeType;

typedef enum EdgeType {
    EDGE_BASE,
    EDGE_MUST,      // if predecessor occurs, the successor must occur
    EDGE_WEAK       // early exit could happen on this edge
} EdgeType;

// Parent class of a node in the dependency graph.
class SCGraphNode {
    friend class SCGraph;

    public:
        NodeType node_type;

    protected:
        SCGraph *scgraph = nullptr;     // assigned when inserted into SCGraph

        SCGraphNode() = delete;
        SCGraphNode(NodeType node_type)
                : node_type(node_type), scgraph(nullptr) {}
        virtual ~SCGraphNode() {}
};


typedef enum SyscallStage {
    STAGE_NOTREADY,     // there are missing arguments, not ready for issuing
    STAGE_UNISSUED,     // args are complete, not issued yet
    STAGE_PROGRESS,     // issued async, completion not harvested yet
    STAGE_FINISHED      // issued sync / issued async and completion harvested
} SyscallStage;

// Parent class of a syscall node in the dependency graph.
// Each syscall type is a child class that inherits from this class, and a
// dependency graph instance should be composed of instances of those
// child classes -- no direct instantiation of this parent class.
// See syscalls.hpp.
class SyscallNode : public SCGraphNode {
    friend class SCGraph;

    protected:
        SyscallStage stage = STAGE_NOTREADY;
        SCGraphNode *next_node = nullptr;
        EdgeType edge_type = EDGE_BASE;

    public:
        long rc = -1;

    protected:
        // Every child class must implement the following three functions.
        virtual long SyscallSync() = 0;
        virtual void PrepUring(struct io_uring_sqe *sqe) = 0;
        virtual void ReflectResult() = 0;

        long CallSync();
        void PrepAsync();
        void CmplAsync();

        SyscallNode() = delete;
        SyscallNode(bool pure_sc, bool args_ready);
        virtual ~SyscallNode() {}

    public:
        // Set the next node that this node points to.
        void SetNext(SCGraphNode *node, bool weak_edge);

        // Invoke this syscall, possibly pre-issuing the next few syscalls
        // in graph.
        long Issue();
};


// Special branching node.
// Used only when there is some decision to be made during the execution of
// a syscall graph, where the decision is not known at the time of building
// the graph.
class BranchNode : public SCGraphNode {
    friend class SCGraph;
    friend class SyscallNode;

    protected:
        std::vector<SCGraphNode *> children;
        int decision = -1;      // set by SetDecision

        SCGraphNode *PickBranch();

    public:
        BranchNode();
        ~BranchNode() {}

        // Set the next children nodes. Index of child in this vector
        // should correspond to the decision int.
        void SetChildren(std::vector<SCGraphNode *> children_list);
};


}


#endif
