#include <vector>
#include <chrono>
#include <stdexcept>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <liburing.h>

#include "exper_impl.hpp"


static struct io_uring ring;


void do_reqs_io_uring(std::vector<Req>& reqs) {
    for (auto& req : reqs) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (sqe == nullptr)
            throw std::runtime_error("io_uring_get_sqe failed");

        io_uring_prep_read(sqe, req.fd, req.buf, req.count, req.offset);
        io_uring_sqe_set_data(sqe, (void *) &req);
    }

    int submitted = io_uring_submit(&ring);
    if (submitted != static_cast<int>(reqs.size()))
        throw std::runtime_error("fewer than all prepared entries submitted");

    for (auto& req : reqs) {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
            throw std::runtime_error("io_uring_wait_cqe failed");

        Req *req = reinterpret_cast<Req *>(io_uring_cqe_get_data(cqe));
        if (cqe->res != req->count)
            throw std::runtime_error("io_uring res does not match count");

        io_uring_cqe_seen(&ring, cqe);
    }
}


std::vector<double> run_exper_io_uring(std::vector<Req>& reqs,
                                       size_t queue_len,
                                       size_t timing_rounds,
                                       size_t warmup_rounds) {
    int rc = io_uring_queue_init(queue_len, &ring, 0);
    if (rc != 0)
        throw std::runtime_error("io_uring_queue_init failed");

    std::vector<double> times_us;
    times_us.reserve(timing_rounds);

    for (size_t i = 0; i < warmup_rounds + timing_rounds; ++i) {
        auto ts_beg = std::chrono::high_resolution_clock::now();

        do_reqs_io_uring(reqs);

        auto ts_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed_us = ts_end - ts_beg;
        if (i >= warmup_rounds)
            times_us.push_back(elapsed_us.count());
    }

    io_uring_queue_exit(&ring);

    return times_us;
}
