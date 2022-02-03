#include <assert.h>
#include <liburing.h>


#ifndef __FOREACTOR_IO_URING_H__
#define __FOREACTOR_IO_URING_H__


namespace foreactor {


class IOUring {
  private:
    struct io_uring ring;

  public:
    IOUring() = delete;
    IOUring(int sq_length) {
        int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
        assert(ret == 0);
    }

    ~IOUring() {
        io_uring_queue_exit(&ring);
    }

    struct io_uring *RingPtr() {
        return &ring;
    }
};


}


#endif
