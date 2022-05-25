#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <liburing.h>

#include "scg_nodes.hpp"
#include "value_pool.hpp"


#ifndef __FOREACTOR_SYSCALLS_H__
#define __FOREACTOR_SYSCALLS_H__


namespace foreactor {


// open
class SyscallOpen final : public SyscallNode {
    private:
        ValuePool<std::string> pathname;
        ValuePool<int> flags;
        ValuePool<mode_t> mode;

        long SyscallSync(const EpochList& epoch, void *output_buf);
        void PrepUringSqe(const EpochList& epoch, struct io_uring_sqe *sqe);
        void ReflectResult(const EpochList& epoch, void *output_buf);
        void Reset();

    public:
        SyscallOpen() = delete;
        // Arguments could be not ready at the point of constructing the
        // SCGraph, in which case those they must be set before calling
        // Issue on this syscall node.
        SyscallOpen(unsigned node_id, std::string name, SCGraph *scgraph,
                    const std::unordered_set<int>& assoc_dims);
        ~SyscallOpen() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallOpen& n);

        // For validating argument values at the time of syscall hijacking,
        // to verify that the arguments recorded (and probably used in async
        // submissions) have the same value as fed by the invocation.
        // For syscalls that have non-ready arguments until the timepoint of
        // invocation, this function will install their values.
        void CheckArgs(const EpochList& epoch,
                       const char *pathname_, int flags_, mode_t mode_);
};


// pread
class SyscallPread final : public SyscallNode {
    private:
        ValuePool<int> fd;
        ValuePool<size_t> count;
        ValuePool<off_t> offset;

        // Used when issued async.
        ValuePool<char *> internal_buf;

        long SyscallSync(const EpochList& epoch, void *output_buf);
        void PrepUringSqe(const EpochList& epoch, struct io_uring_sqe *sqe);
        void ReflectResult(const EpochList& epoch, void *output_buf);
        void Reset();

    public:
        SyscallPread() = delete;
        SyscallPread(unsigned node_id, std::string name, SCGraph *scgraph,
                     const std::unordered_set<int>& assoc_dims);
        ~SyscallPread() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallPread& n);

        void CheckArgs(const EpochList& epoch,
                       int fd_, size_t count_, off_t offset_);
};


}


#endif