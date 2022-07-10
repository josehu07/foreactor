#include <tuple>
#include <vector>
#include <unordered_set>
#include <assert.h>
#include <signal.h>
#include <liburing.h>

#include "debug.hpp"
#include "io_engine.hpp"
#include "scg_nodes.hpp"
#include "value_pool.hpp"


#pragma once


namespace foreactor {


// Each IOUring instance is a pair of io_uring SQ/CQ queues.
// If using IOUring backend, there should be one IOUring instance per SCGraph
// per thread.
class IOUring : public IOEngine {
    using IOEngine::EntryId;

    private:
        struct io_uring ring;
        int sq_length = 0;
        bool sqe_async_flag = false;

    public:
        IOUring() = delete;
        IOUring(int sq_length, bool sqe_async_flag);
        ~IOUring();

        void Prepare(SyscallNode *node, int epoch_sum);
        int SubmitAll();
        std::tuple<SyscallNode *, int, long> CompleteOne();

        void CleanUp();
};


}
