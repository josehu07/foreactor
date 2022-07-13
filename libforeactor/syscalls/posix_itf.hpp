#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>


#pragma once


namespace foreactor::posix {


// Do posix::xxx() to invoke the actual POSIX call.
// Function names are POSIX C library function names, which might not be
// the exact syscall names. For example, an `open` call to the POSIX library
// is translated into an `openat64` syscall.

// Credit to uLayFS code by Shawn Zhong (https://github.com/ShawnZhong).
#define DECL_POSIX_FN(fn) extern const decltype(&::fn) fn

DECL_POSIX_FN(open);
DECL_POSIX_FN(openat);
DECL_POSIX_FN(close);
DECL_POSIX_FN(pread);
DECL_POSIX_FN(pwrite);
DECL_POSIX_FN(lseek);
DECL_POSIX_FN(__fxstat);
DECL_POSIX_FN(__fxstatat);
DECL_POSIX_FN(statx);
// FIND_POSIX_FN(opendir);
// FIND_POSIX_FN(fdopendir);

#undef DECL_POSIX_FN


// The user function defined by glibc for the SYSCALL_fstat syscall is named
// `__fxstat()`, and `fstat` is just the name of a macro defined over the
// `__fxstat()` function. Here, we make `fstat` a function name w/o ver arg.
[[maybe_unused]] static int fstat(int fd, struct stat *buf) {
    return __fxstat(_STAT_VER, fd, buf);
}

[[maybe_unused]] static int fstatat(int dirfd, const char *pathname,
                                    struct stat *buf, int flags) {
    return __fxstatat(_STAT_VER, dirfd, pathname, buf, flags);
}


}
