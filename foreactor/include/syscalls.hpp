#include <string>
#include <vector>

#include "scg_nodes.hpp"


#ifndef __FOREACTOR_SYSCALLS_H__
#define __FOREACTOR_SYSCALLS_H__


namespace foreactor {


class SyscallOpen : public SyscallNode {
    public:
        std::string filename;
        int flags;
        mode_t mode;

    private:
        std::vector<bool> arg_ready;

        long SyscallSync();
        void PrepUring(struct io_uring_sqe *sqe);
        void ReflectResult();

    public:
        SyscallOpen() = delete;
        // Arguments could be not ready at the point of constructing the
        // SCGraph, in which case those they must be set before calling
        // Issue on this syscall node.
        SyscallOpen(std::string filename, int flags, mode_t mode,
                    std::vector<bool> arg_ready);
        // Syntax sugar for syscall nodes with all arguments ready.
        SyscallOpen(std::string filename, int flags, mode_t mode);
        ~SyscallOpen() {}

        // For setting arguments whose value was not ready at construction.
        void SetArgFilename(std::string filename_);
        void SetArgFlags(int flags_);
        void SetArgMode(mode_t mode_);
};

class SyscallPread : public SyscallNode {
    public:
        int fd;
        char *buf;
        size_t count;
        off_t offset;

    private:
        std::vector<bool> arg_ready;
        char *internal_buf = nullptr;   // used when issued async

        long SyscallSync();
        void PrepUring(struct io_uring_sqe *sqe);
        void ReflectResult();

    public:
        SyscallPread() = delete;
        SyscallPread(int fd, char *buf, size_t count, off_t offset,
                     std::vector<bool> arg_ready);
        SyscallPread(int fd, char *buf, size_t count, off_t offset);
        ~SyscallPread();

        void SetArgFd(int fd_);
        void SetArgBuf(char * buf_);
        void SetArgCount(size_t count_);
        void SetArgOffset(off_t offset_);
};


}


#endif
