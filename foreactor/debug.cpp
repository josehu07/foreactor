#include <syscall.h>
#include <unistd.h>

#include "debug.hpp"



thread_local const pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
