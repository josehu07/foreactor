#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <stdexcept>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "exper_impl.hpp"


static std::mutex start_mu, finish_mu;
static std::condition_variable start_cv, finish_cv;
static std::vector<bool> thread_start;
static std::vector<bool> thread_done;


void thread_func(std::vector<Req> *reqs, size_t id, size_t num_threads) {
    while (true) {
        {
            std::unique_lock start_lk(start_mu);
            while (!thread_start[id])
                start_cv.wait(start_lk);

            // reset to false for possible next round
            thread_start[id] = false;
        }

        for (size_t idx = id; idx < reqs->size(); idx += num_threads) {
            Req& req = reqs->at(idx);
            ssize_t ret;

            if (req.write)
                ret = pwrite(req.fd, req.buf, req.count, req.offset);
            else
                ret = pread(req.fd, req.buf, req.count, req.offset);

            if (static_cast<size_t>(ret) != req.count)
                throw std::runtime_error("req rc does not match count");
        }

        std::unique_lock finish_lk(finish_mu);
        thread_done[id] = true;
        if (std::all_of(thread_done.begin(), thread_done.end(),
            [](bool b) { return b; })) {
            finish_cv.notify_one();
        }
    }
}

void do_reqs_thread_pool(std::vector<Req>& reqs, size_t num_threads) {
    {
        std::unique_lock start_lk(start_mu);
        for (size_t id = 0; id < num_threads; ++id)
            thread_start[id] = true;
        start_cv.notify_all();
    }

    {
        std::unique_lock finish_lk(finish_mu);
        finish_cv.wait(finish_lk);
    }

    if (!std::all_of(thread_done.begin(), thread_done.end(),
        [](bool b) { return b; })) {
        throw std::runtime_error("main thread wakes up prematurely");
    }
    for (size_t id = 0; id < num_threads; ++id) {
        // reset to false for possible next round
        thread_done[id] = false;
    }

    if (!std::all_of(thread_start.begin(), thread_start.end(),
        [](bool b) { return !b; })) {
        throw std::runtime_error("some worker did not reset thread_start");
    }
}


std::vector<double> run_exper_thread_pool(std::vector<Req>& reqs,
                                          size_t num_threads,
                                          size_t timing_rounds,
                                          size_t warmup_rounds) {
    std::vector<bool> thread_start(num_threads, false);
    std::vector<bool> thread_done(num_threads, false);

    std::vector<std::thread> workers(num_threads);
    for (size_t id = 0; id < num_threads; ++id)
        workers[id] = std::thread(thread_func, &reqs, id, num_threads);

    // Sleep for a while to ensure all workers ready, listening on cv
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::vector<double> times_us;
    times_us.reserve(timing_rounds);

    for (size_t i = 0; i < warmup_rounds + timing_rounds; ++i) {
        auto ts_beg = std::chrono::high_resolution_clock::now();

        do_reqs_thread_pool(reqs, num_threads);

        auto ts_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed_us = ts_end - ts_beg;
        if (i >= warmup_rounds)
            times_us.push_back(elapsed_us.count());

        for (auto& req : reqs)
            req.completed = false;
    }

    // No joins for simplicity...

    return times_us;
}
