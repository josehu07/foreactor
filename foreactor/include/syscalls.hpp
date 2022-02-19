#include <iostream>
#include <string>
#include <vector>

#include "scg_nodes.hpp"


#ifndef __FOREACTOR_SYSCALLS_H__
#define __FOREACTOR_SYSCALLS_H__


namespace foreactor {


class SyscallOpen final : public SyscallNode {
    public:
        std::string filename;
        int flags;
        mode_t mode;

    private:
        long SyscallSync(void *output_buf);
        void PrepUring(struct io_uring_sqe *sqe);
        void ReflectResult(void *output_buf);

    public:
        SyscallOpen() = delete;
        // Arguments could be not ready at the point of constructing the
        // SCGraph, in which case those they must be set before calling
        // Issue on this syscall node.
        SyscallOpen(std::string filename, int flags, mode_t mode,
                    std::vector<bool> arg_ready
                        = std::vector{true, true, true});
        ~SyscallOpen() {}

        // For setting arguments whose value was not ready at construction.
        void SetArgFilename(std::string filename_);
        void SetArgFlags(int flags_);
        void SetArgMode(mode_t mode_);

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallOpen& n);
};

class SyscallPread final : public SyscallNode {
    public:
        int fd;
        size_t count;
        off_t offset;

    private:
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

        void SetArgFd(int fd_);
        void SetArgCount(size_t count_);
        void SetArgOffset(off_t offset_);

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallPread& n);
};


}


#endif
