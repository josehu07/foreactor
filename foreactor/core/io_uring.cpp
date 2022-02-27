#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_uring.hpp"


namespace foreactor {


IOUring::IOUring(int sq_length)
        : sq_length(sq_length) {
    ring_initialized = false;

    TIMER_START("uring-init");
    if (sq_length > 0) {
        int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
        if (ret != 0) {
            DEBUG("initilize IOUring failed %d\n", ret);
            return;
        }

        ring_initialized = true;
        DEBUG("initialized IOUring @ %p sq_length %d\n", &ring, sq_length);
    }
    TIMER_PAUSE("uring-init");
}

IOUring::~IOUring() {
    TIMER_START("uring-exit");
    if (ring_initialized) {
        io_uring_queue_exit(&ring);
        DEBUG("destroyed IOUring @ %p\n", &ring);
    }
    TIMER_PAUSE("uring-exit");
    
    TIMER_PRINT_ALL(TIME_MICRO);
    TIMER_RESET_ALL();
}


}
