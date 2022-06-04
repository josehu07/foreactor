#include <tuple>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/file.h>

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
FIND_POSIX_FN(close);
FIND_POSIX_FN(pread);
FIND_POSIX_FN(pwrite);

#undef FIND_POSIX_FN


}


/////////////////////////////////////////////
// Libc call hijacker implementation below //
/////////////////////////////////////////////

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


}

}
