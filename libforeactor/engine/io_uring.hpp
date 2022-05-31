#include <tuple>
#include <unordered_set>
#include <assert.h>
#include <liburing.h>

#include "debug.hpp"
#include "io_engine.hpp"
#include "scg_nodes.hpp"
#include "value_pool.hpp"


#ifndef __FOREACTOR_IO_URING_H__
#define __FOREACTOR_IO_URING_H__


namespace foreactor {


// Each IOUring instance is a pair of io_uring SQ/CQ queues.
// If using IOUring backend, there should be one IOUring instance per SCGraph
// per thread.
class IOUring : public IOEngine {
    using IOEngine::EntryId;

    private:
        struct io_uring ring;
        int sq_length = 0;

    public:
        IOUring() = delete;
        IOUring(int sq_length);
        ~IOUring();

        void Prepare(SyscallNode *node, int epoch_sum);
        int SubmitAll();
        std::tuple<SyscallNode *, int, long> CompleteOne();

        void CleanUp();
};


}


#endif