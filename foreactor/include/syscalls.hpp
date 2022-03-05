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
        std::string pathname;
        int flags;
        mode_t mode;

        long SyscallSync(void *output_buf);
        void PrepUring(struct io_uring_sqe *sqe);
        void ReflectResult(void *output_buf);

    public:
        SyscallOpen() = delete;
        // Arguments could be not ready at the point of constructing the
        // SCGraph, in which case those they must be set before calling
        // Issue on this syscall node.
        SyscallOpen(std::string pathname, int flags, mode_t mode,
                    std::vector<bool> arg_ready
                        = std::vector{true, true, true});
        ~SyscallOpen() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallOpen& n);

        // For setting arguments whose value was not ready at construction.
        void SetArgPathname(std::string pathname_);
        void SetArgFlags(int flags_);
        void SetArgMode(mode_t mode_);

        // For validating argument values at the time of syscall hijacking,
        // to verify that the arguments recorded (and probably used in async
        // submissions) have the same value as fed by the invocation.
        // For syscalls that have non-ready arguments until the timepoint of
        // invocation, this function will install their values.
        void CheckArgs(const char *pathname_, int flags_, mode_t mode_);
};


// pread
class SyscallPread final : public SyscallNode {
    private:
        int fd;
        size_t count;
        off_t offset;

        char *internal_buf = nullptr;   // used when issued async

        long SyscallSync(void *output_buf);
        void PrepUring(struct io_uring_sqe *sqe);
        void ReflectResult(void *output_buf);

    public:
        SyscallPread() = delete;
        SyscallPread(int fd, size_t count, off_t offset,
                     std::vector<bool> arg_ready
                        = std::vector{true, true, true});
        ~SyscallPread();

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallPread& n);

        void SetArgFd(int fd_);
        void SetArgCount(size_t count_);
        void SetArgOffset(off_t offset_);

        void CheckArgs(int fd_, size_t count_, off_t offset_);
};


}


#endif
