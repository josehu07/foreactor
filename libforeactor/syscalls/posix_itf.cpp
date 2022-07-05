#include <tuple>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "debug.hpp"
#include "posix_itf.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"
#include "value_pool.hpp"


///////////////////////////////////////////
// Find original POSIX library functions //
///////////////////////////////////////////

namespace foreactor::posix {


// Credit to uLayFS code by Shawn Zhong (https://github.com/ShawnZhong).
#define FIND_POSIX_FN(fn)                                                    \
    const decltype(&::fn) fn = []() noexcept {                               \
        auto res = reinterpret_cast<decltype(&::fn)>(dlsym(RTLD_NEXT, #fn)); \
        assert(res != nullptr);                                              \
        return res;                                                          \
    }()

FIND_POSIX_FN(open);
FIND_POSIX_FN(openat);
FIND_POSIX_FN(close);
FIND_POSIX_FN(pread);
FIND_POSIX_FN(pwrite);
FIND_POSIX_FN(lseek);
FIND_POSIX_FN(__fxstat);
FIND_POSIX_FN(__fxstatat);

#undef FIND_POSIX_FN


}


//////////////////////////////////////////////
// Libc call hijackers implementation below //
//////////////////////////////////////////////

namespace foreactor {

extern "C" {


int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, mode_t);
        va_end(arg);
    }

    if (active_scgraph == nullptr) {
        DEBUG("posix::open(\"%s\", %d, %u)\n", pathname, flags, mode);
        return posix::open(pathname, flags, mode);
    } else {
        DEBUG("foreactor::open(\"%s\", %d, %u)\n", pathname, flags, mode);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallOpen>();
        assert(node != nullptr);
        assert(node->sc_type == SC_OPEN);
        node->CheckArgs(*epoch, pathname, flags, mode);
        DEBUG("open<%p>->Issue(%s)\n",
              node, StreamStr(*epoch).c_str());
        return static_cast<int>(node->Issue(*epoch));
    }
}

int open64(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, mode_t);
        va_end(arg);
    }

    return open(pathname, flags, mode);
}


int openat(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, mode_t);
        va_end(arg);
    }

    if (active_scgraph == nullptr) {
        DEBUG("posix::openat(%d, \"%s\", %d, %u)\n",
              dirfd, pathname, flags, mode);
        return posix::openat(dirfd, pathname, flags, mode);
    } else {
        DEBUG("foreactor::openat(%d, \"%s\", %d, %u)\n",
              dirfd, pathname, flags, mode);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallOpenat>();
        assert(node != nullptr);
        assert(node->sc_type == SC_OPENAT);
        node->CheckArgs(*epoch, dirfd, pathname, flags, mode);
        DEBUG("openat<%p>->Issue(%s)\n",
              node, StreamStr(*epoch).c_str());
        return static_cast<int>(node->Issue(*epoch));
    }
}

int openat64(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, mode_t);
        va_end(arg);
    }

    return openat(dirfd, pathname, flags, mode);
}


int close(int fd) {
    if (active_scgraph == nullptr) {
        DEBUG("posix::close(%d)\n", fd);
        return posix::close(fd);
    } else {
        DEBUG("foreactor::close(%d)\n", fd);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallClose>();
        assert(node != nullptr);
        assert(node->sc_type == SC_CLOSE);
        node->CheckArgs(*epoch, fd);
        DEBUG("close<%p>->Issue(%s)\n",
              node, StreamStr(*epoch).c_str());
        return static_cast<int>(node->Issue(*epoch));
    }
}


ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    if (active_scgraph == nullptr) {
        DEBUG("posix::pread(%d, %p, %lu, %ld)\n", fd, buf, count, offset);
        return posix::pread(fd, buf, count, offset);
    } else {
        DEBUG("foreactor::pread(%d, %p, %lu, %ld)\n", fd, buf, count, offset);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallPread>();
        assert(node != nullptr);
        assert(node->sc_type == SC_PREAD);
        node->CheckArgs(*epoch, fd, buf, count, offset);
        DEBUG("pread<%p>->Issue(%s)\n",
              node, StreamStr(*epoch).c_str());
        return static_cast<ssize_t>(node->Issue(*epoch));
    }
}

ssize_t pread64(int fd, void *buf, size_t count, off_t offset) {
    return pread(fd, buf, count, offset);
}

ssize_t read(int fd, void *buf, size_t count) {
    off_t offset = posix::lseek(fd, 0, SEEK_CUR);
    ssize_t ret = pread(fd, buf, count, offset);
    if (ret > 0)
        offset = posix::lseek(fd, static_cast<size_t>(ret), SEEK_CUR);
    return ret;
}


ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    if (active_scgraph == nullptr) {
        DEBUG("posix::pwrite(%d, %p, %lu, %ld)\n", fd, buf, count, offset);
        return posix::pwrite(fd, buf, count, offset);
    } else {
        DEBUG("foreactor::pwrite(%d, %p, %lu, %ld)\n", fd, buf, count, offset);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallPwrite>();
        assert(node != nullptr);
        assert(node->sc_type == SC_PWRITE);
        node->CheckArgs(*epoch, fd, buf, count, offset);
        DEBUG("pwrite<%p>->Issue(%s)\n",
              node, StreamStr(*epoch).c_str());
        return static_cast<ssize_t>(node->Issue(*epoch));
    }
}

ssize_t pwrite64(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite(fd, buf, count, offset);
}

ssize_t write(int fd, const void *buf, size_t count) {
    off_t offset = posix::lseek(fd, 0, SEEK_CUR);
    ssize_t ret = pwrite(fd, buf, count, offset);
    if (ret > 0)
        offset = posix::lseek(fd, static_cast<size_t>(ret), SEEK_CUR);
    return ret;
}


off_t lseek(int fd, off_t offset, int whence) {
    if (active_scgraph == nullptr) {
        DEBUG("posix::lseek(%d, %ld, %d)\n", fd, offset, whence);
        return posix::lseek(fd, offset, whence);
    } else {
        DEBUG("foreactor::lseek(%d, %ld, %d)\n", fd, offset, whence);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallLseek>();
        assert(node != nullptr);
        assert(node->sc_type == SC_LSEEK);
        node->CheckArgs(*epoch, fd, offset, whence);
        DEBUG("lseek<%p>->Issue(%s)\n",
              node, StreamStr(*epoch).c_str());
        return static_cast<off_t>(node->Issue(*epoch));
    }
}

off64_t lseek64(int fd, off64_t offset, int whence) {
    return lseek(fd, static_cast<off_t>(offset), whence);
}


int __fxstat([[maybe_unused]] int ver, int fd, struct stat *buf) {
    if (active_scgraph == nullptr) {
        DEBUG("posix::fstat(%d, %p)\n", fd, buf);
        return posix::fstat(fd, buf);
    } else {
        DEBUG("foreactor::fstat(%d, %p)\n", fd, buf);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallFstat>();
        assert(node != nullptr);
        assert(node->sc_type == SC_FSTAT);
        node->CheckArgs(*epoch, fd, buf);
        DEBUG("fstat<%p>->Issue(%s)\n",
              node, StreamStr(*epoch).c_str());
        return static_cast<int>(node->Issue(*epoch));
    }
}

int __fxstat64([[maybe_unused]] int ver, int fd, struct stat64 *buf) {
    return __fxstat(ver, fd, reinterpret_cast<struct stat *>(buf));
}

int __fxstatat([[maybe_unused]] int ver, int dirfd, const char *pathname,
               struct stat *buf, int flags) {
    if (active_scgraph == nullptr) {
        DEBUG("posix::fstatat(%d, \"%s\", %p, %d)\n",
              dirfd, pathname, buf, flags);
        return posix::fstatat(dirfd, pathname, buf, flags);
    } else {
        DEBUG("foreactor::fstatat(%d, \"%s\", %p, %d)\n",
              dirfd, pathname, buf, flags);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallFstatat>();
        assert(node != nullptr);
        assert(node->sc_type == SC_FSTATAT);
        node->CheckArgs(*epoch, dirfd, pathname, buf, flags);
        DEBUG("fstatat<%p>->Issue(%s)\n",
              node, StreamStr(*epoch).c_str());
        return static_cast<int>(node->Issue(*epoch));
    }
}

int __fxstatat64([[maybe_unused]] int ver, int dirfd, const char *pathname,
                 struct stat64 *buf, int flags) {
    return __fxstatat(ver, dirfd, pathname,
                      reinterpret_cast<struct stat *>(buf), flags);
}


}

}
