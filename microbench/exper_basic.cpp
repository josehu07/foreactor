#include <vector>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "exper_impl.hpp"


void do_reqs_basic(std::vector<Req>& reqs) {
    for (auto& req : reqs) {
        ssize_t ret;

        if (req.write)
            ret = pwrite(req.fd, req.buf, req.count, req.offset);
        else
            ret = pread(req.fd, req.buf, req.count, req.offset);

        if (static_cast<size_t>(ret) != req.count)
            throw std::runtime_error("req rc does not match count");

        req.completed = true;
    }
}


std::vector<double> run_exper_basic(std::vector<Req>& reqs,
                                    size_t timing_rounds,
                                    size_t warmup_rounds) {
    std::vector<double> times_us;
    times_us.reserve(timing_rounds);

    for (size_t i = 0; i < warmup_rounds + timing_rounds; ++i) {
        auto ts_beg = std::chrono::high_resolution_clock::now();

        do_reqs_basic(reqs);

        auto ts_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed_us = ts_end - ts_beg;
        if (i >= warmup_rounds)
            times_us.push_back(elapsed_us.count());

        for (auto& req : reqs)
            req.completed = false;
    }

    return times_us;
}
