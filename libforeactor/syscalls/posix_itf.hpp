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
DECL_POSIX_FN(fstat);
DECL_POSIX_FN(fstatat);
DECL_POSIX_FN(statx);
DECL_POSIX_FN(getdents64);

#undef DECL_POSIX_FN


// glibc only provides getdents64. We make a wrapper for the name getdents.
[[maybe_unused]] static ssize_t getdents(int fd, void *dirp, size_t count) {
    return getdents64(fd, dirp, count);
}


}
