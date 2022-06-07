#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "concurrent_queue.hpp"


#pragma once


typedef enum SyscallType : unsigned {
    SC_BASE,
    SC_OPEN,    // open
    SC_CLOSE,   // close
    SC_PREAD,   // pread
    SC_PWRITE   // pwrite
} SyscallType;


struct ThreadPoolSQEntry {
    uint64_t entry_id;
    SyscallType sc_type;
    // Arguments of syscalls:
    int fd;
    off_t offset;
    uint64_t buf;
    union {
        size_t rw_len;
        mode_t open_mode;
    };
    union {
        int rw_flags;
        int open_flags;
    };
};

struct ThreadPoolCQEntry {
    uint64_t entry_id;
    long rc;
};


class ThreadPool {
    static constexpr uint64_t termination_entry_id = 0xDEAD00000000DEAD;

    using SQEntry = ThreadPoolSQEntry;
    using CQEntry = ThreadPoolCQEntry;

    std::vector<std::vector<uint64_t>> handle_nsecs;

    private:
        int nthreads = 0;
        std::vector<std::thread> workers;

        moodycamel::BlockingConcurrentQueue<SQEntry> submission_queue;
        moodycamel::BlockingConcurrentQueue<CQEntry> completion_queue;

        static void HandleSQEntry(const SQEntry& sqe, CQEntry& cqe) {
            cqe.entry_id = sqe.entry_id;

            switch (sqe.sc_type) {
            case SC_OPEN:
                cqe.rc = open(reinterpret_cast<const char *>(sqe.buf),
                              sqe.open_flags, sqe.open_mode);
                break;
            case SC_CLOSE:
                cqe.rc = close(sqe.fd);
                break;
            case SC_PREAD:
                cqe.rc = pread(sqe.fd, reinterpret_cast<char *>(sqe.buf),
                               sqe.rw_len, sqe.offset);
                break;
            case SC_PWRITE:
                cqe.rc = pwrite(sqe.fd, reinterpret_cast<const char *>(sqe.buf),
                                sqe.rw_len, sqe.offset);
                break;
            default:
                cqe.rc = -22;
            }
        }
        
        void WorkerThreadFunc(int id, std::promise<void> init_barrier) {
            // signal main thread for initialization complete
            init_barrier.set_value();

            // loop on dequeuing from submission queue indefinitely
            while (true) {
                SQEntry sqe;
                submission_queue.wait_dequeue(sqe);     // SQE will be moved

                // if see speical entry_id of termination, exit this thread
                if (sqe.entry_id == termination_entry_id)
                    return;

                // call the corresponding syscall handler based on entry type,
                // fill in CQE struct
                CQEntry cqe;
                struct timespec start_ts;
                struct timespec pause_ts;
                [[maybe_unused]] int ret = clock_gettime(CLOCK_MONOTONIC, &start_ts);
                HandleSQEntry(sqe, cqe);
                ret = clock_gettime(CLOCK_MONOTONIC, &pause_ts);
                uint64_t nsecs = (pause_ts.tv_sec - start_ts.tv_sec) * 1e9
                                 + (pause_ts.tv_nsec - start_ts.tv_nsec);
                handle_nsecs[id].push_back(nsecs);

                // move CQE to enqueue completion queue
                [[maybe_unused]] bool success =
                    completion_queue.enqueue(std::move(cqe));
                assert(success);
            }
        }

    public:
        ThreadPool(int nthreads) : nthreads(nthreads) {
            assert(nthreads > 0);
        }
        ~ThreadPool() {}

        void StartThreads() {
            // spawn worker threads
            std::vector<std::future<void>> init_barriers;
            for (int id = 0; id < nthreads; ++id) {
                std::promise<void> init_barrier;
                init_barriers.push_back(init_barrier.get_future());
                workers.push_back(std::thread(&ThreadPool::WorkerThreadFunc,
                                              this, id, std::move(init_barrier)));

                // set core affinity to core with the same index as thread ID
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(id, &cpuset);
                [[maybe_unused]] int ret =
                    pthread_setaffinity_np(workers.back().native_handle(),
                                           sizeof(cpu_set_t), &cpuset);
                assert(ret == 0);
            }

            // wait for everyone finishes initialization
            for (auto&& init_barrier : init_barriers)
                init_barrier.wait();

            for (int id = 0; id < nthreads; ++id)
                handle_nsecs.emplace_back();
        }

        void JoinThreads() {
            // put nthreads termination entries into submission queue
            for (int i = 0; i < nthreads; ++i)
                submission_queue.enqueue(SQEntry{.entry_id = termination_entry_id});

            // wait and join all worker threads
            for (int id = 0; id < nthreads; ++id)
                workers[id].join();

            double sum = 0.;
            size_t cnt = 0;
            for (auto&& vec : handle_nsecs) {
                for (auto time : vec) {
                    sum += time / 1000.;
                    cnt++;
                }
            }
            double avg = sum / cnt;
            std::cout << "??? sum " << sum << " avg " << avg << std::endl;
        }

        void SubmitBulk(std::vector<SQEntry>& entries) {
            [[maybe_unused]] bool success =
                submission_queue.enqueue_bulk(
                    std::make_move_iterator(entries.begin()), entries.size());
            assert(success);
        }

        void CompleteOne() {
            CQEntry cqe;
            completion_queue.wait_dequeue(cqe);     // CQE will be moved
        }
};
