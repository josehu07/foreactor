#include <string>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>


#ifndef __FOREACTOR_DEBUG_H__
#define __FOREACTOR_DEBUG_H__


///////////////////
// DEBUG() macro //
///////////////////

// `DEBUG()` and `assert()` are not active if NDEBUG is defined at
// compilation time by the build system.
#ifdef NDEBUG

#define DEBUG(msg, ...)

#else

#define DEBUG(msg, ...)                              \
    do {                                             \
        const char *tmp = strrchr(__FILE__, '/');    \
        const char *file = tmp ? tmp + 1 : __FILE__; \
        fprintf(stderr, "[%15s:%-4d@t:%-6d]  " msg,  \
                file, __LINE__, tid, ##__VA_ARGS__); \
    } while (0)

#endif


//////////////////////
// PANIC_IF() macro //
//////////////////////

// Use `assert()` on conditions that should never occur and that indicate
// a fault of the library/plugin programmer.
//
// Use `PANIC_IF()` on conditions that indicate wrong input from users,
// e.g., wrong env variable input.
#define PANIC_IF(cond, msg, ...)                         \
    do {                                                 \
        if (cond) {                                      \
            const char *tmp = strrchr(__FILE__, '/');    \
            const char *file = tmp ? tmp + 1 : __FILE__; \
            fprintf(stderr, "[%15s:%-4d@t:%-6d]  " msg,  \
                    file, __LINE__, tid, ##__VA_ARGS__); \
            exit(-1);                                    \
        }                                                \
    } while (0)


namespace foreactor {


// thread ID
extern thread_local const pid_t tid;
extern thread_local const std::string tid_str;


}


#endif
