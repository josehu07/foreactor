#include <iostream>
#include <vector>
#include <assert.h>
#include <liburing.h>


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
        const NodeType node_type;

    protected:
        SCGraph *scgraph = nullptr;

        SCGraphNode() = delete;
        SCGraphNode(NodeType node_type)
                : node_type(node_type), scgraph(nullptr) {}
        virtual ~SCGraphNode() {}

    public:
        friend std::ostream& operator<<(std::ostream& s, const SCGraphNode& n);
};


// Stages of a SyscallNode.
typedef enum SyscallStage {
    STAGE_NOTREADY,     // there are missing arguments, not ready for issuing
    STAGE_UNISSUED,     // args are complete, not issued yet
    STAGE_PROGRESS,     // issued async, completion not harvested yet
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

    public:
        const SyscallType sc_type = SC_BASE;
        SyscallStage stage = STAGE_NOTREADY;

    protected:
        std::vector<bool> arg_ready;
        SCGraphNode *next_node = nullptr;
        EdgeType edge_type = EDGE_BASE;
        long rc = -1;

        // Every child class must implement the following three functions.
        virtual long SyscallSync(void *output_buf) = 0;
        virtual void PrepUring(struct io_uring_sqe *sqe) = 0;
        virtual void ReflectResult(void *output_buf) = 0;

        void PrepAsync();
        void CmplAsync();

        SyscallNode() = delete;
        SyscallNode(SyscallType sc_type, bool pure_sc,
                    std::vector<bool> arg_ready);
        virtual ~SyscallNode() {}

    public:
        void PrintCommonInfo(std::ostream& s) const;
        friend std::ostream& operator<<(std::ostream& s, const SyscallNode& n);

        static inline bool AllArgsReady(std::vector<bool>& arg_ready) {
            return std::all_of(arg_ready.begin(), arg_ready.end(),
                               [](bool a) { return a; });
        }

        // Set the next node that this node points to.
        void SetNext(SCGraphNode *node, bool weak_edge);

        // Set argument installation dependencies to fill not-ready argument
        // values in some later syscall node.
        // TODO: finish this
        // void CalcArg(SCGraphNode *node, CalcArgFunc func);

        // Invoke this syscall, possibly pre-issuing the next few syscalls
        // in graph. Must be invoked on current frontier node only.
        long Issue(void *output_buf = nullptr);
};


// Special branching node.
// Used only when there is some decision to be made during the execution of
// a syscall graph, where the decision is not known at the time of building
// the graph.
class BranchNode final : public SCGraphNode {
    friend class SCGraph;
    friend class SyscallNode;

    private:
        std::vector<SCGraphNode *> children;
        int decision = -1;      // set by SetDecision

        SCGraphNode *PickBranch();

    public:
        BranchNode(int decision = -1);
        ~BranchNode() {}

        friend std::ostream& operator<<(std::ostream& s, const BranchNode& n);

        // Set the next children nodes. Index of child in this vector
        // should correspond to the decision int.
        void SetChildren(std::vector<SCGraphNode *> children_list);

        // Append child node to children list.
        void AppendChild(SCGraphNode *child);

        // Install the decision made.
        void SetDecision(int decision_);
};


}


#endif
