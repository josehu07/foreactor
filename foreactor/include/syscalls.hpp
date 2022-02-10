#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>

#include "depgraph.hpp"
#include "io_uring.hpp"


#ifndef __FOREACTOR_SYSCALLS_H__
#define __FOREACTOR_SYSCALLS_H__


namespace foreactor {


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
    SyscallPread(int fd, char *buf, size_t count, off_t offset, IOUring& ring)
            : fd(fd), buf(buf), count(count), offset(offset), ring(ring) {
        internal_buf = new char[count];
    }

    ~SyscallPread() {
        delete[] internal_buf;
    }
};


}


#endif
