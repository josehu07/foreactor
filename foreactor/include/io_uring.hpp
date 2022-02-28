#include <assert.h>
#include <liburing.h>


#ifndef __FOREACTOR_IO_URING_H__
#define __FOREACTOR_IO_URING_H__


namespace foreactor {


class SCGraph;  // forward declaration


// Each IOUring instance is a pair of io_uring SQ/CQ queues.
// There should be one IOUring instance per wrapped function per thread.
class IOUring {
    friend class SCGraph;

    private:
        struct io_uring ring;
        bool ring_initialized = false;
        int sq_length = 0;

        struct io_uring *Ring() {
            return &ring;
        }

    public:
        IOUring();
        IOUring(int sq_length);
        ~IOUring();

        void Initialize(int sq_length);
        bool IsInitialized() const;
};


}


#endif
