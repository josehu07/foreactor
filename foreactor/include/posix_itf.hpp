#include <fcntl.h>
#include <unistd.h>


#ifndef __FOREACTOR_POSIX_ITF_H__
#define __FOREACTOR_POSIX_ITF_H__


// Do posix::xxx() to invoke the actual POSIX call.
// Function names are POSIX C library function names, which might not be
// the exact syscall names. For example, an `open` call to the POSIX library
// is translated into an `openat64` syscall.
namespace foreactor::posix {


// Some decltype magic to hijack POSIX library calls into our own functions,
// also see posix_itf.cpp.
// Credit to uLayFS code by Shawn Zhong (https://github.com/ShawnZhong).
#define DECL_FN(fn) extern const decltype(&::fn) fn

DECL_FN(open);
DECL_FN(close);
DECL_FN(pread);

#undef DECL_FN


}


#endif
