#include <assert.h>
#include <liburing.h>


#ifndef __FOREACTOR_IO_URING_H__
#define __FOREACTOR_IO_URING_H__


namespace foreactor {


class IOUring {
  private:
    struct io_uring ring;
    bool initialized = false;

    // How many syscalls we try to issue ahead of time.
    // Must be no larger than the length of SQ of uring.
    int pre_issue_depth = 0;

  public:
    IOUring() = delete;
    IOUring(int sq_length, int pre_issue_depth)
            : pre_issue_depth(pre_issue_depth) {
        assert(pre_issue_depth >= 0);
        assert(pre_issue_depth <= sq_length);

        if (sq_length > 0) {
            int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
            assert(ret == 0);
            initialized = true;
        }
    }

    ~IOUring() {
        if (initialized)
            io_uring_queue_exit(&ring);
    }

    struct io_uring *RingPtr() {
        return &ring;
    }
};


}


#endif
