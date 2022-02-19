#include <assert.h>
#include <liburing.h>


#ifndef __FOREACTOR_IO_URING_H__
#define __FOREACTOR_IO_URING_H__


namespace foreactor {


class SCGraph;


class IOUring {
    friend class SCGraph;

    private:
        struct io_uring ring;
        bool ring_initialized = false;

    protected:
        int sq_length = 0;

        struct io_uring *Ring() {
            return &ring;
        }

    public:
        IOUring() = delete;
        IOUring(int sq_length);
        ~IOUring();
};


}


#endif
