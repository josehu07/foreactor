#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>

#include "scg_nodes.hpp"


#ifndef __FOREACTOR_SYSCALLS_H__
#define __FOREACTOR_SYSCALLS_H__


namespace foreactor {


class SyscallOpen : public SyscallNode {
    private:
        const char * const filename;
        const int flags;
        const mode_t mode;

        long SyscallSync() {
            return open(filename, flags, mode);
        }

        void PrepUring(struct io_uring_sqe *sqe) {
            io_uring_prep_openat(sqe, AT_FDCWD, filename, flags, mode);
        }

        void ReflectResult() {
            return;
        }

    public:
        SyscallOpen() = delete;
        SyscallOpen(const char *filename, int flags, mode_t mode)
                : SyscallNode(NODE_SYSCALL_SIDE), filename(filename),
                  flags(flags), mode(mode) {
            assert(filename != nullptr);
        }

        ~SyscallOpen() {}
};

class SyscallPread : public SyscallNode {
    private:
        const int fd;
        char * const buf;
        const size_t count;
        const off_t offset;

        // used when issued async
        char *internal_buf = nullptr;

        long SyscallSync() {
            return pread(fd, buf, count, offset);
        }

        void PrepUring(struct io_uring_sqe *sqe) {
            io_uring_prep_read(sqe, fd, internal_buf, count, offset);
        }

        void ReflectResult() {
            memcpy(buf, internal_buf, count);
        }

    public:
        SyscallPread() = delete;
        SyscallPread(int fd, char *buf, size_t count, off_t offset)
                : SyscallNode(NODE_SYSCALL_PURE), fd(fd), buf(buf),
                  count(count), offset(offset) {
            assert(buf != nullptr);
            internal_buf = new char[count];
        }

        ~SyscallPread() {
            // FIXME: think about how to safely delete internal_buf
            // delete[] internal_buf;
        }
};


}


#endif
