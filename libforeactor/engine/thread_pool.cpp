#include <tuple>
#include <vector>
#include <thread>
#include <future>
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_engine.hpp"
#include "thread_pool.hpp"
#include "scg_nodes.hpp"
#include "syscalls.hpp"
#include "value_pool.hpp"
#include "concurrent_queue.hpp"


namespace foreactor {


void ThreadPool::HandleSQEntry(const SQEntry& sqe, CQEntry& cqe) {
    cqe.entry_id = sqe.entry_id;

    switch (sqe.sc_type) {
    case SC_OPEN:
        cqe.rc = open(reinterpret_cast<const char *>(sqe.buf), sqe.open_flags,
                      sqe.open_mode);
        break;
    case SC_CLOSE:
        cqe.rc = close(sqe.fd);
        break;
    case SC_PREAD:
        cqe.rc = pread(sqe.fd, reinterpret_cast<char *>(sqe.buf), sqe.rw_len,
                       sqe.offset);
        break;
    case SC_PWRITE:
        cqe.rc = pwrite(sqe.fd, reinterpret_cast<const char *>(sqe.buf),
                        sqe.rw_len, sqe.offset);
        break;
    default:
        DEBUG("unknown syscall type %u in SQEntry\n", sqe.sc_type);
        cqe.rc = -22;
    }
}

void ThreadPool::WorkerThreadFunc(int id, std::promise<void> init_barrier) {
    // signal main thread for initialization complete
    init_barrier.set_value();

    // loop on dequeuing from submission queue indefinitely
    while (true) {
        SQEntry sqe;
        submission_queue.wait_dequeue(sqe);     // SQE will be moved

        // call the corresponding syscall handler based on entry type, fill
        // in CQE struct
        CQEntry cqe;
        HandleSQEntry(sqe, cqe);

        // move CQE to enqueue completion queue
        [[maybe_unused]] bool success =
            completion_queue.enqueue(std::move(cqe));
        assert(success);
    }
}


ThreadPool::ThreadPool(int nthreads)
        : nthreads(nthreads) {
    const unsigned ncores = std::thread::hardware_concurrency();
    assert(nthreads > 0);
    assert(ncores > 0);
    PANIC_IF(nthreads >= static_cast<int>(ncores),
             "too many worker threads %d / %u\n", nthreads, ncores);

    // spawn worker threads
    std::vector<std::future<void>> init_barriers;
    for (int id = 0; id < nthreads; ++id) {
        std::promise<void> init_barrier;
        init_barriers.push_back(init_barrier.get_future());
        workers.push_back(std::thread(&ThreadPool::WorkerThreadFunc, this, id,
                                      std::move(init_barrier)));

        // set CPU core affinity to core with the same index as thread ID
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(id, &cpuset);
        [[maybe_unused]] int ret =
            pthread_setaffinity_np(workers.back().native_handle(),
                                   sizeof(cpu_set_t), &cpuset);
        assert(ret == 0);

        workers.back().detach();
    }

    // wait for everyone finishes initialization
    for (auto&& init_barrier : init_barriers)
        init_barrier.wait();

    DEBUG("inited ThreadPool with %d worker threads\n", nthreads);
}

ThreadPool::~ThreadPool() {
    DEBUG("destroyed ThreadPool\n");
}


void ThreadPool::Prepare(SyscallNode *node, int epoch_sum) {
    EntryId entry_id = EncodeEntryId(node, epoch_sum);

    assert(!prepared.contains(entry_id));
    prepared.insert(entry_id);
}

int ThreadPool::SubmitAll() {
    size_t num_entries = prepared.size();
    std::vector<SQEntry> entries;
    entries.reserve(num_entries);

    // move everything from prepared set to onthefly set
    for (EntryId entry_id : prepared) {
        auto [node, epoch_sum] = DecodeEntryId(entry_id);
        assert(!onthefly.contains(entry_id));
        assert(node->stage.Get(epoch_sum) == STAGE_PREPARED);

        entries.emplace_back(SQEntry{.entry_id = entry_id});

        // do syscall-specific preparation, filling in args etc.
        node->PrepUpoolSqe(epoch_sum, &entries.back());

        onthefly.insert(entry_id);
        node->stage.Set(epoch_sum, STAGE_ONTHEFLY);
    }

    prepared.clear();

    // submit to submission_queue as a bulk
    [[maybe_unused]] bool success =
        submission_queue.enqueue_bulk(std::make_move_iterator(entries.begin()),
                                      num_entries);
    assert(success);
    return num_entries;
}

std::tuple<SyscallNode *, int, long> ThreadPool::CompleteOne() {
    CQEntry cqe;
    completion_queue.wait_dequeue(cqe);     // CQE will be moved

    EntryId entry_id = cqe.entry_id;
    auto [node, epoch_sum] = DecodeEntryId(entry_id);
    assert(node->stage.Get(epoch_sum) == STAGE_ONTHEFLY);

    assert(onthefly.contains(entry_id));
    onthefly.erase(entry_id);

    return std::make_tuple(node, epoch_sum, cqe.rc);
}


void ThreadPool::CleanUp() {
    // clear the prepared set
    prepared.clear();

    // call CmplAsync() on all requests in the onthefly set
    while (onthefly.size() > 0) {
        EntryId entry_id = *(onthefly.begin());
        auto [node, epoch_sum] = DecodeEntryId(entry_id);
        assert(node->stage.Get(epoch_sum) == STAGE_ONTHEFLY);
        node->CmplAsync(epoch_sum);     // entry_id will be removed here
    }
}


}
