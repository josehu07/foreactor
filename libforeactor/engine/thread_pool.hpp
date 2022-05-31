#include <tuple>
#include <unordered_set>
#include <assert.h>

#include "debug.hpp"
#include "io_engine.hpp"
#include "scg_nodes.hpp"
#include "value_pool.hpp"


#ifndef __FOREACTOR_THREAD_POOL_H__
#define __FOREACTOR_THREAD_POOL_H__


namespace foreactor {


// User-level thread pool backend, mimicking the io_uring interface.
class ThreadPool : public IOEngine {
    using IOEngine::EntryId;

    private:
        int nthreads = 0;

    public:
        ThreadPool() = delete;
        ThreadPool(int nthreads);
        ~ThreadPool();

        void Prepare(SyscallNode *node, int epoch_sum);
        int SubmitAll();
        std::tuple<SyscallNode *, int, long> CompleteOne();

        void CleanUp();
};


}


#endif
