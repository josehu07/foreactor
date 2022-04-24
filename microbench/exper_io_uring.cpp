#include <vector>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <new>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <liburing.h>

#include "exper_impl.hpp"


static struct io_uring ring;

static void print_sq_poll_kthread_running(void) {
    if (system("ps --ppid 2 | grep io_uring-sq > /dev/null") == 0)
        std::cout << "kernel thread io_uring-sq is running..." << std::endl;
    else
        std::cout << "kernel thread io_uring-sq NOT running." << std::endl;
}


void do_reqs_io_uring(std::vector<Req>& reqs, bool fixed_file, bool fixed_buf,
                      bool iosqe_async) {
    for (size_t i = 0; i < reqs.size(); ++i) {
        Req& req = reqs[i];

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (sqe == nullptr)
            throw std::runtime_error("io_uring_get_sqe failed");

        if (req.write) {
            if (fixed_buf)
                io_uring_prep_write_fixed(sqe, req.fd, req.buf, req.count, req.offset, i);
            else
                io_uring_prep_write(sqe, req.fd, req.buf, req.count, req.offset);
        } else {
            if (fixed_buf)
                io_uring_prep_read_fixed(sqe, req.fd, req.buf, req.count, req.offset, i);
            else
                io_uring_prep_read(sqe, req.fd, req.buf, req.count, req.offset);
        }

        if (iosqe_async)
            sqe->flags |= IOSQE_ASYNC;
        if (fixed_file)
            sqe->flags |= IOSQE_FIXED_FILE;

        io_uring_sqe_set_data(sqe, (void *) &req);
    }

    int submitted = io_uring_submit(&ring);
    if (submitted < static_cast<int>(reqs.size()))
        throw std::runtime_error("fewer than all prepared entries submitted: " +
                                 std::to_string(submitted));

    for (auto& req : reqs) {
        // this req's completion might have been harvested in previous iteration
        if (req.completed)
            continue;

        // loop and harvest, until the completion for this req processed
        while (true) {
            struct io_uring_cqe *cqe;
            int ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0)
                throw std::runtime_error("io_uring_wait_cqe failed");

            Req *req_ptr = reinterpret_cast<Req *>(io_uring_cqe_get_data(cqe));
            if (static_cast<size_t>(cqe->res) != req_ptr->count)
                throw std::runtime_error("io_uring res does not match count");

            io_uring_cqe_seen(&ring, cqe);

            req_ptr->completed = true;
            if (req_ptr == &req)
                break;
        }
    }
}


std::vector<double> run_exper_io_uring(std::vector<Req>& reqs,
                                       size_t queue_len,
                                       bool fixed_buf,
                                       bool fixed_file,
                                       bool sq_poll,
                                       bool iosqe_async,
                                       size_t timing_rounds,
                                       size_t warmup_rounds,
                                       bool shuffle_offset,
                                       size_t file_size, size_t req_size) {
    // if sq_poll, give correct flag and set idle timeout
    if (sq_poll) {
        if (geteuid() != 0)
            throw std::runtime_error("using io_uring_sq_poll, need root privilege");

        struct io_uring_params params;
        memset(&params, 0, sizeof(params));
        params.flags |= IORING_SETUP_SQPOLL;
        params.sq_thread_idle = 2000;   // 2s

        print_sq_poll_kthread_running();
        int rc = io_uring_queue_init_params(queue_len, &ring, &params);
        if (rc != 0)
            throw std::runtime_error("io_uring_queue_init_params failed");
        print_sq_poll_kthread_running();
    } else {
        int rc = io_uring_queue_init(queue_len, &ring, 0);
        if (rc != 0)
            throw std::runtime_error("io_uring_queue_init failed");
    }

    // if fixed_file, register files array
    int *fds = NULL;
    if (fixed_file) {
        std::vector<int> fds_vec;
        for (auto& req : reqs) {
            auto it = std::find(fds_vec.begin(), fds_vec.end(), req.fd);
            if (it == fds_vec.end()) {   // not added yet
                fds_vec.push_back(req.fd);
                req.fd = fds_vec.size() - 1;    // feed index in fds array instead
            } else {
                req.fd = it - fds_vec.begin();
            }
        }

        fds = new int[fds_vec.size()];
        for (size_t i = 0; i < fds_vec.size(); ++i)
            fds[i] = fds_vec[i];

        int rc = io_uring_register_files(&ring, fds, fds_vec.size());
        if (rc != 0)
            throw std::runtime_error("io_uring_register_files failed");
    }

    // if fixed_buf, register buffers
    struct iovec *iovs = NULL;
    if (fixed_buf) {
        iovs = new struct iovec[reqs.size()];
        for (size_t i = 0; i < reqs.size(); ++i) {
            iovs[i].iov_base = reqs[i].buf;
            iovs[i].iov_len = reqs[i].count;
        }

        int rc = io_uring_register_buffers(&ring, iovs, reqs.size());
        if (rc != 0)
            throw std::runtime_error("io_uring_register_buffers failed");
    }

    std::vector<double> times_us;
    times_us.reserve(timing_rounds);

    for (size_t i = 0; i < warmup_rounds + timing_rounds; ++i) {
        auto ts_beg = std::chrono::high_resolution_clock::now();

        do_reqs_io_uring(reqs, fixed_file, fixed_buf, iosqe_async);

        auto ts_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed_us = ts_end - ts_beg;
        if (i >= warmup_rounds)
            times_us.push_back(elapsed_us.count());

        for (auto& req : reqs)
            req.completed = false;

        if (shuffle_offset)
            shuffle_reqs_offset(reqs, file_size, req_size);
    }

    io_uring_queue_exit(&ring);
    if (fixed_file)
        delete[] fds;
    if (fixed_buf)
        delete[] iovs;

    return times_us;
}
