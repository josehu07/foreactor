#include <fcntl.h>
#include <unistd.h>


#ifndef __FOREACTOR_POSIX_ITF_H__
#define __FOREACTOR_POSIX_ITF_H__


namespace foreactor::posix {


// Do posix::xxx() to invoke the actual POSIX call.
// Function names are POSIX C library function names, which might not be
// the exact syscall names. For example, an `open` call to the POSIX library
// is translated into an `openat64` syscall.

// Credit to uLayFS code by Shawn Zhong (https://github.com/ShawnZhong).
#define DECL_POSIX_FN(fn) extern const decltype(&::fn) fn

DECL_POSIX_FN(open);
DECL_POSIX_FN(close);
DECL_POSIX_FN(pread);
DECL_POSIX_FN(pwrite);

#undef DECL_POSIX_FN


}


#endif
