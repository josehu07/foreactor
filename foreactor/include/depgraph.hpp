#include <vector>
#include <liburing.h>

#include "io_uring.hpp"


#ifndef __FOREACTOR_DEPGRAPH_H__
#define __FOREACTOR_DEPGRAPH_H__


namespace foreactor {


// Parent class of a node in the dependency graph.
typedef enum NodeType {
    NODE_BASE,
    NODE_SYSCALL,
    NODE_BRANCH
} NodeType;

class DepGraphNode {
  protected:
    NodeType node_type = NODE_BASE;
};


// Parent class of a syscall node in the dependency graph.
// Each syscall type is a child class that inherits from this class, and a
// dependency graph instance should be composed of instances of those
// child classes -- no direct instantiation of this parent class.
// See syscalls.hpp.
typedef enum SyscallStage {
    STAGE_UNISSUED,
    STAGE_ISSUED,
    STAGE_FINISHED
} SyscallStage;

class SyscallNode : public DepGraphNode {
  protected:
    NodeType node_type = NODE_SYSCALL;
    SyscallNode *next = nullptr;

    IOUring& ring;

    SyscallStage stage = STAGE_UNISSUED;
    long rc = -1;

    // Every child class must implement the following three functions.
    virtual long SyscallSync() = 0;
    virtual void PrepUring(struct io_uring_sqe *sqe) = 0;
    virtual void ReflectResult() = 0;

    long CallSync();
    void PrepAsync();

  public:
    void SetNext(SyscallNode *next) {
        assert(next != nullptr);
        next = next;
    }

    // The only public API for applications to invoke a syscall in graph.
    long Issue();
};


// Special branching node.
class BranchNode : public DepGraphNode {
  protected:
    NodeType node_type = NODE_BRANCH;
    std::vector<SyscallNode *> children;

    // Condition function, must return an index to child.
    int (*condition)(void *) = nullptr;

  public:
    BranchNode() = delete;
    BranchNode(int (*condition)(void *)) : condition(condition) {}

    void SetChildren(std::vector<SyscallNode *>& children) {
        
    }

    SyscallNode *PickBranch(void *arg) const {
        if (condition != nullptr)
            return condition(arg);
        return nullptr;
    }
}


}


#endif
