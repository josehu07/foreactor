#include <tuple>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/file.h>

#include "debug.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"
#include "posix_itf.hpp"
#include "value_pool.hpp"


namespace foreactor::posix {


// Credit to uLayFS code by Shawn Zhong (https://github.com/ShawnZhong).
#define INIT_FN(fn)                                                          \
    const decltype(&::fn) fn = []() noexcept {                               \
        auto res = reinterpret_cast<decltype(&::fn)>(dlsym(RTLD_NEXT, #fn)); \
        assert(res != nullptr);                                              \
        return res;                                                          \
    }()

INIT_FN(open);
INIT_FN(close);
INIT_FN(pread);

#undef INIT_FN


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
        node->CheckArgs(epoch, pathname, flags, mode);
        DEBUG("open<%p>->Issue()\n", node);             // FIXME: print epoch
        return static_cast<int>(node->Issue(epoch));
    }
}

int close(int fd) {
    // FIXME: complete this
    DEBUG("posix::close(%d)\n", fd);
    return posix::close(fd);
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    if (active_scgraph == nullptr) {
        DEBUG("posix::pread(%d, %p, %lu, %ld)\n", fd, buf, count, offset);
        return posix::pread(fd, buf, count, offset);
    } else {
        DEBUG("foreactor::pread(%d, %lu, %ld)\n", fd, count, offset);
        auto [node, epoch] = active_scgraph->GetFrontier<SyscallPread>();
        assert(node != nullptr);
        assert(node->sc_type == SC_PREAD);
        node->CheckArgs(epoch, fd, count, offset);
        DEBUG("pread<%p>->Issue(%p)\n", node, buf);     // FIXME: print epoch
        return static_cast<ssize_t>(node->Issue(epoch, buf));
    }
}


}

}
