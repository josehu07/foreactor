#include <string>
#include <syscall.h>
#include <unistd.h>

#include "debug.hpp"


namespace foreactor {


// thread ID
thread_local const pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
thread_local const std::string tid_str = std::to_string(tid);


}
