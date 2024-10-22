#include <tuple>
#include <vector>
#include <unordered_set>
#include <thread>
#include <future>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "debug.hpp"
#include "io_engine.hpp"
#include "scg_nodes.hpp"
#include "syscalls.hpp"
#include "value_pool.hpp"
#include "concurrent_queue.hpp"


#pragma once


namespace foreactor {


struct ThreadPoolSQEntry {
    IOEngine::EntryId entry_id;
    SyscallType sc_type;
    // Arguments of syscalls:
    int fd;
    off_t offset;
    uint64_t buf;
    union {
        size_t rw_len;
        mode_t open_mode;
        uint64_t stat_path;
        size_t dirp_count;
    };
    union {
        int rw_flags;
        int open_flags;
        int stat_flags;
    };
};

struct ThreadPoolCQEntry {
    IOEngine::EntryId entry_id;
    long rc;
};


// User-level thread pool backend, mimicking the io_uring interface.
class ThreadPool : public IOEngine {
    using IOEngine::EntryId;
    static constexpr EntryId termination_entry_id = 0xDEAD00000000DEAD;

    using SQEntry = ThreadPoolSQEntry;
    using CQEntry = ThreadPoolCQEntry;

    private:
        int nthreads = 0;
        std::vector<std::thread> workers;

        moodycamel::BlockingConcurrentQueue<SQEntry> submission_queue;
        moodycamel::BlockingConcurrentQueue<CQEntry> completion_queue;

        static void HandleSQEntry(const SQEntry& sqe, CQEntry& cqe);
        void WorkerThreadFunc(int id, std::promise<void> init_barrier);

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
