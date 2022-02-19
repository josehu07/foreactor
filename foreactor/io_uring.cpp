#include <liburing.h>

#include "io_uring.hpp"


namespace foreactor {


IOUring::IOUring(int sq_length)
        : sq_length(sq_length) {
    ring_initialized = false;

    if (sq_length > 0) {
        int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
        assert(ret == 0);
        ring_initialized = true;
    }
}

IOUring::~IOUring() {
    if (ring_initialized)
        io_uring_queue_exit(&ring);
}


}
