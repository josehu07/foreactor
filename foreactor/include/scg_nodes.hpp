#include <vector>
#include <assert.h>
#include <liburing.h>


#ifndef __FOREACTOR_SCG_NODES_H__
#define __FOREACTOR_SCG_NODES_H__


namespace foreactor {


class SCGraph;


typedef enum NodeType {
    NODE_BASE,
    NODE_SYSCALL_PURE,  // pure syscall with no state changes, e.g., pread
    NODE_SYSCALL_SIDE,  // syscall that has side effects
    NODE_BRANCH         // special branching control flow
} NodeType;

typedef enum EdgeType {
    DEP_NONE,
    DEP_OCCURRENCE,
    DEP_ARGUMENT
} EdgeType;

// Parent class of a node in the dependency graph.
class SCGraphNode {
    friend class SCGraph;

    public:
        NodeType node_type = NODE_BASE;

    protected:
        SCGraph *scgraph = nullptr;     // assigned when inserted into SCGraph

        SCGraphNode() = delete;
        SCGraphNode(NodeType node_type)
                : node_type(node_type), scgraph(nullptr) {}
        virtual ~SCGraphNode() {}
};


typedef enum SyscallStage {
    STAGE_UNISSUED,
    STAGE_PROGRESS,
    STAGE_FINISHED
} SyscallStage;

class SyscallNode;
typedef void (*ArgLinkFunc)(SyscallNode *, SyscallNode *);

// Parent class of a syscall node in the dependency graph.
// Each syscall type is a child class that inherits from this class, and a
// dependency graph instance should be composed of instances of those
// child classes -- no direct instantiation of this parent class.
// See syscalls.hpp.
class SyscallNode : public SCGraphNode {
    friend class SCGraph;

    protected:
        SCGraphNode *next = nullptr;
        EdgeType next_dep = DEP_NONE;
        ArgLinkFunc arg_link_func = nullptr;
        SyscallStage stage = STAGE_UNISSUED;

    public:
        long rc = -1;

    protected:
        // Every child class must implement the following three functions.
        virtual long SyscallSync() = 0;
        virtual void PrepUring(struct io_uring_sqe *sqe) = 0;
        virtual void ReflectResult() = 0;

        long CallSync();
        void PrepAsync();
        void CompAsync();

        SyscallNode() = delete;
        SyscallNode(NodeType node_type)
                : SCGraphNode(node_type), next(nullptr),
                  stage(STAGE_UNISSUED), rc(-1) {
            assert(node_type == NODE_SYSCALL_PURE ||
                   node_type == NODE_SYSCALL_SIDE);
        }
        virtual ~SyscallNode() {}

    public:
        void SetNextDepOccurrence(SCGraphNode *node) {
            assert(node != nullptr);
            next = node;
            next_dep = DEP_OCCURRENCE;
        }

        void SetNextDepArgument(SCGraphNode *node, ArgLinkFunc link_func) {
            assert(node != nullptr);
            next = node;
            next_dep = DEP_ARGUMENT;
            arg_link_func = link_func;
        }

        // The public API for applications to invoke a syscall in graph.
        long Issue();
};


typedef int (*ConditionFunc)(void *);

// Special branching node, must be associated with a condition function.
class BranchNode : public SCGraphNode {
    friend class SCGraph;
    friend class SyscallNode;

    protected:
        std::vector<SCGraphNode *> children;
        // Condition function, must return an index to child.
        ConditionFunc condition_func = nullptr;
        void *condition_arg = nullptr;
        int branch_taken = -1;      // set after first exec of PickBranch

        SCGraphNode *PickBranch() {
            if (branch_taken >= 0 && branch_taken < (int) children.size())
                return children[branch_taken];
            int child_idx = condition_func(condition_arg);
            if (child_idx >= 0 && child_idx < (int) children.size()) {
                branch_taken = child_idx;
                return children[child_idx];
            }
            return nullptr;
        }

    public:
        BranchNode() = delete;
        BranchNode(ConditionFunc condition, void *arg)
                : SCGraphNode(NODE_BRANCH), condition_func(condition),
                  condition_arg(arg), branch_taken(-1) {
            assert(condition_func != nullptr);
        }
        ~BranchNode() {}

        void SetChildren(std::vector<SCGraphNode *> children_list) {
            assert(children_list.size() > 0);
            children = children_list;
        }
};


}


#endif
