#include <string>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <string.h>
#include <stdarg.h>


#ifndef __FOREACTOR_DEBUG_H__
#define __FOREACTOR_DEBUG_H__


// thread ID
extern thread_local const pid_t tid;


// DEBUG() and assert() are not active if NDEBUG is defined
#ifdef NDEBUG
#define DEBUG(msg, ...)
#else
#define DEBUG(msg, ...)                                                        \
    do {                                                                       \
        const char *tmp = strrchr(__FILE__, '/');                              \
        const char *file = tmp ? tmp + 1 : __FILE__;                           \
        printf("[%15s:%-4d@t:%-6d]  " msg, file, __LINE__, tid, ##__VA_ARGS__); \
    } while (0)
#endif


namespace foreactor {


template <typename T>
static std::string StreamStr(T *item) {
    std::ostringstream ss;
    ss << *item;
    return ss.str();
}


}


#endif
