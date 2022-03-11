#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

#include "scg_nodes.hpp"
#include "value_pool.hpp"


#ifndef __FOREACTOR_SYSCALLS_H__
#define __FOREACTOR_SYSCALLS_H__


namespace foreactor {


// open
class SyscallOpen final : public SyscallNode {
    private:
        ValuePoolBase<std::string> *pathname;
        ValuePoolBase<int> *flags;
        ValuePoolBase<mode_t> *mode;

        bool RefreshStage(EpochListBase *epoch);
        long SyscallSync(EpochListBase *epoch, void *output_buf);
        void PrepUring(EpochListBase *epoch, struct io_uring_sqe *sqe);
        void ReflectResult(EpochListBase *epoch, void *output_buf);

    public:
        SyscallOpen() = delete;
        // Arguments could be not ready at the point of constructing the
        // SCGraph, in which case those they must be set before calling
        // Issue on this syscall node.
        SyscallOpen(std::string name,
                    ValuePoolBase<SyscallStage> *stage,
                    ValuePoolBase<long> *rc,
                    ValuePoolBase<std::string> *pathname,
                    ValuePoolBase<int> *flags,
                    ValuePoolBase<mode_t> *mode);
        ~SyscallOpen() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallOpen& n);

        // For validating argument values at the time of syscall hijacking,
        // to verify that the arguments recorded (and probably used in async
        // submissions) have the same value as fed by the invocation.
        // For syscalls that have non-ready arguments until the timepoint of
        // invocation, this function will install their values.
        void CheckArgs(EpochListBase *epoch,
                       const char *pathname_, int flags_, mode_t mode_);
};


// pread
class SyscallPread final : public SyscallNode {
    private:
        ValuePoolBase<int> *fd;
        ValuePoolBase<size_t> *count;
        ValuePoolBase<off_t> *offset;

        // Used when issued async.
        ValuePoolBase<char *> *internal_buf;

        bool RefreshStage(EpochListBase *epoch);
        long SyscallSync(EpochListBase *epoch, void *output_buf);
        void PrepUring(EpochListBase *epoch, struct io_uring_sqe *sqe);
        void ReflectResult(EpochListBase *epoch, void *output_buf);

    public:
        SyscallPread() = delete;
        SyscallPread(std::string name,
                     ValuePoolBase<SyscallStage> *stage,
                     ValuePoolBase<long> *rc,
                     ValuePoolBase<int> *fd,
                     ValuePoolBase<size_t> *count,
                     ValuePoolBase<off_t> *offset,
                     ValuePoolBase<char *> *internal_buf);
        ~SyscallPread() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallPread& n);

        void CheckArgs(EpochListBase *epoch,
                       int fd_, size_t count_, off_t offset_);
};


}


#endif
