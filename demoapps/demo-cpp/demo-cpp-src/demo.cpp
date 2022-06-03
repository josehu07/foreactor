#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>
#include <chrono>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <liburing.h>

#include "hijackees.hpp"
#include "thread_pool.hpp"


static const std::string rand_string(size_t length) {
    static constexpr char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string str;
    str.reserve(length + 1);
    for (size_t i = 0; i < length; ++i)
        str += alphanum[rand() % (sizeof(alphanum) - 1)];
    return str;
}

static void print_usage_exit(const char *self) {
    std::cerr << "Usage: " << self << " EXPER_NAME DBDIR_PATH NUM_ITERS"
              << " [--drop_caches] [--dump_result] [--manual_ring|pool]"
              << std::endl;
    exit(1);
}

static void cmd_drop_caches(void) {
    [[maybe_unused]] int rc =
        system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    assert(rc == 0);
}


void run_iters(ExperFunc exper_func, ExperArgs *exper_args,
               unsigned num_iters, bool drop_caches, bool print_time) {
    std::vector<double> elapsed_us;
    for (unsigned i = 0; i < num_iters; ++i) {
        if (drop_caches)    // drain page cache before each run?
            cmd_drop_caches();
        
        auto t1 = std::chrono::high_resolution_clock::now();
        exper_func(exper_args);
        auto t2 = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::micro> duration = t2 - t1;
        elapsed_us.push_back(duration.count());
    }

    if (print_time) {
        double sum_us = 0.;
        int skip = 3, cnt = 0;
        int show = elapsed_us.size() > 3 ? elapsed_us.size() - 3 : 0;

        std::cerr << "  Time elapsed: [ ";
        if (show > 0)
            std::cerr << "... ";
        
        for (double& us : elapsed_us) {
            if (show <= 0)
                std::cerr << us << " ";
            else
                show--;
            if (skip <= 0) {
                sum_us += us;
                cnt++;
            } else
                skip--;
        }
        std::cerr << "] ";
        
        if (cnt > 0)
            std::cerr << "avg " << sum_us / cnt;
        std::cerr << " us" << std::endl;
    }
}

void run_exper(const char *self, std::string& dbdir, std::string& exper,
               unsigned num_iters, bool drop_caches, bool dump_result,
               bool manual_ring, bool manual_pool) {
    std::filesystem::current_path(dbdir);

    struct io_uring ring;
    ThreadPool pool;
    if (manual_ring) {
        [[maybe_unused]] int ret = io_uring_queue_init(256, &ring, 0);
        assert(ret == 0);
    } else if (manual_pool)
        pool.StartThreads(8);

    if (exper == "simple") {
        ExperSimpleArgs args("simple.dat", rand_string(8192));
        run_iters(exper_simple, &args, num_iters, drop_caches, !dump_result);
        if (dump_result) {
            std::cout << std::string(args.rbuf0, args.rbuf0 + args.rlen)
                      << std::endl
                      << std::string(args.rbuf1, args.rbuf1 + args.rlen)
                      << std::endl;
        }

    } else if (exper == "branching") {
        ExperBranchingArgs args("branching.dat", rand_string(4096),
                                rand_string(4096), rand_string(4096));
        run_iters(exper_branching, &args, num_iters, drop_caches, !dump_result);
        if (dump_result) {
            std::cout << std::string(args.rbuf0, args.rbuf0 + args.rlen)
                      << std::endl
                      << std::string(args.rbuf1, args.rbuf1 + args.rlen)
                      << std::endl;
        }

    } else if (exper == "looping") {
        ExperLoopingArgs args("looping.dat", rand_string(1024), 10, 20, 5);
        run_iters(exper_looping, &args, num_iters, drop_caches, !dump_result);
        if (dump_result) {
            for (auto buf : args.rbufs)
                std::cout << std::string(buf, buf + args.rlen) << std::endl;
        }

    } else if (exper == "read_seq") {
        int fd = open("read_seq.dat", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        unsigned nreads = 128;
        size_t rlen = (1 << 20);
        std::string wcontent = rand_string(rlen);
        for (unsigned i = 0; i < nreads; ++i) {
            [[maybe_unused]] ssize_t ret =
                pwrite(fd, wcontent.c_str(), rlen, i * rlen);
        }
        ExperReadSeqArgs args(fd, rlen, nreads);

        if (manual_ring) {
            args.manual_ring = &ring;
            run_iters(exper_read_seq_manual_ring, &args, num_iters, drop_caches, !dump_result);
        } else if (manual_pool) {
            args.manual_pool = &pool;
            run_iters(exper_read_seq_manual_pool, &args, num_iters, drop_caches, !dump_result);
        } else
            run_iters(exper_read_seq, &args, num_iters, drop_caches, !dump_result);
        
        if (dump_result) {
            for (auto buf : args.rbufs)
                std::cout << std::string(buf, buf + args.rlen) << std::endl;
        }

    } else if (exper == "write_seq") {
        int fd = open("write_seq.dat", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        unsigned nwrites = 128;
        size_t wlen = (1 << 20);
        ExperWriteSeqArgs args(fd, rand_string(wlen), nwrites);
        
        if (manual_ring) {
            args.manual_ring = &ring;
            run_iters(exper_write_seq_manual_ring, &args, num_iters, drop_caches, !dump_result);
        } else if (manual_pool) {
            args.manual_pool = &pool;
            run_iters(exper_write_seq_manual_pool, &args, num_iters, drop_caches, !dump_result);
        } else
            run_iters(exper_write_seq, &args, num_iters, drop_caches, !dump_result);

    } else
        print_usage_exit(self);

    if (manual_ring)
        io_uring_queue_exit(&ring);
    else if (manual_pool)
        pool.JoinThreads();
}


int main(int argc, char *argv[]) {
    srand(time(NULL));

    if ((argc < 4) || (argc > 6))
        print_usage_exit(argv[0]);

    std::string exper(argv[1]);
    std::string dbdir(argv[2]);
    unsigned num_iters = std::stoul(std::string(argv[3]));

    bool drop_caches = false;
    bool dump_result = false;
    bool manual_ring = false;
    bool manual_pool = false;
    for (int i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--drop_caches") == 0) {
            drop_caches = true;
        } else if (strcmp(argv[i], "--dump_result") == 0) {
            dump_result = true;
            srand(1234567);     // use fixed seed for result comparison
        } else if (strcmp(argv[i], "--manual_ring") == 0) {
            manual_ring = true;
        } else if (strcmp(argv[i], "--manual_pool") == 0)
            manual_pool = true;
    }
    if (manual_pool && manual_ring) {
        std::cerr << "Error: --manual_ring and --manual_pool both given"
                  << std::endl;
        print_usage_exit(argv[0]);
    }

    run_exper(argv[0], dbdir, exper, num_iters, drop_caches, dump_result,
              manual_ring, manual_pool);
    return 0;
}
