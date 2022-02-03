#include <vector>
#include <liburing.h>

#include "io_uring.hpp"


#ifndef __FOREACTOR_DEPGRAPH_H__
#define __FOREACTOR_DEPGRAPH_H__


namespace foreactor {


class Intention;
class SyscallNode;


// Any syscall node is in one of the three stages at any time.
typedef enum SyscallStage {
    STAGE_UNISSUED,
    STAGE_ISSUED,
    STAGE_FINISHED
} SyscallStage;


// Parent class of a syscall node in the dependency graph.
// Each syscall type is a child class that inherits from this class, and a
// dependency graph instance should be composed of instances of those
// child classes -- no direct instantiation of this parent class.
class SyscallNode {
  friend Intention;

  protected:
    SyscallNode *pred = nullptr;
    SyscallNode *succ = nullptr;

    struct io_uring *ring;
    int pre_issue_depth;

    SyscallStage stage = STAGE_UNISSUED;
    long rc = -1;

    // Every child class must implement the following three functions.
    virtual long SyscallSync() = 0;
    virtual void PrepUring(struct io_uring_sqe *sqe) = 0;
    virtual void ReflectResult() = 0;

    long CallSync();
    void PrepAsync();

  public:
    // The only public API for applications to invoke a syscall in graph.
    long Issue();
};


// Applications call Enter() as the first step of an intention to register
// a syscall dependency graph. Call Leave() before finishing the intention.
void DepGraphEnter(std::vector<SyscallNode *>& syscalls, int pre_issue_depth,
                   IOUring& ring);
void DepGraphLeave();


}


#endif
