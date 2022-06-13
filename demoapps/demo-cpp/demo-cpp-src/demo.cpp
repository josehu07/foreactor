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

static void cmd_drop_caches(void) {
    [[maybe_unused]] int rc =
        system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    assert(rc == 0);
}

static void print_usage_exit(const char *self);


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
               bool manual_ring, bool manual_pool, bool same_buffer,
               bool o_direct, bool multi_file) {
    std::filesystem::current_path(dbdir);

    struct io_uring ring;
    ThreadPool pool(8);
    if (manual_ring) {
        [[maybe_unused]] int ret = io_uring_queue_init(256, &ring, 0);
        assert(ret == 0);
    } else if (manual_pool)
        pool.StartThreads();

    if (exper == "simple") {
        ExperSimpleArgs args("simple.dat", rand_string(8192));

        run_iters(exper_simple, &args, num_iters, drop_caches, !dump_result);

        if (dump_result) {
            std::cout << std::string(args.rbuf0, args.rbuf0 + args.rlen) << std::endl
                      << std::string(args.rbuf1, args.rbuf1 + args.rlen) << std::endl;
        }

    } else if (exper == "branching") {
        ExperBranchingArgs args("branching.dat", rand_string(4096),
                                rand_string(4096), rand_string(4096));

        run_iters(exper_branching, &args, num_iters, drop_caches, !dump_result);

        if (dump_result) {
            std::cout << std::string(args.rbuf0, args.rbuf0 + args.rlen) << std::endl
                      << std::string(args.rbuf1, args.rbuf1 + args.rlen) << std::endl;
        }

    } else if (exper == "looping") {
        ExperLoopingArgs args("looping.dat", rand_string(1024), 10, 20, 5);

        run_iters(exper_looping, &args, num_iters, drop_caches, !dump_result);

        if (dump_result) {
            for (auto buf : args.rbufs)
                std::cout << std::string(buf, buf + args.rlen) << std::endl;
        }

    } else if (exper == "weak_edge") {
        int fd = open("weak_edge.dat", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        std::vector<std::string> wcontents;
        for (int i = 0; i < 5; ++i)
            wcontents.push_back(rand_string(512));
        ExperWeakEdgeArgs args(fd, std::move(wcontents));

        run_iters(exper_weak_edge, &args, num_iters, drop_caches, !dump_result);

        if (dump_result) {
            std::cout << std::string(args.rbuf0, args.rbuf0 + args.len) << std::endl
                      << std::string(args.rbuf1, args.rbuf1 + args.len) << std::endl;
        }

        close(fd);

    } else if (exper == "crossing") {
        int fd = open("crossing.dat", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        unsigned nblocks = 16;
        size_t len = 4096;
        std::string wcontent = rand_string(len);
        for (unsigned i = 0; i < nblocks; ++i) {
            [[maybe_unused]] ssize_t ret =
                pwrite(fd, wcontent.c_str(), len, i * len);
        }
        ExperCrossingArgs args(fd, len, nblocks);

        run_iters(exper_crossing, &args, num_iters, drop_caches, !dump_result);

        if (dump_result) {
            char *rbuf = new char[len];
            for (unsigned i = 0; i < nblocks; ++i) {
                [[maybe_unused]] ssize_t ret = pread(fd, rbuf, len, i * len);
                std::cout << std::string(rbuf, rbuf + len) << std::endl;
            }
            delete[] rbuf;
        }

        close(fd);

    } else if (exper == "read_seq") {
        unsigned nreads = 128;
        size_t rlen = 64 * 1024;
        
        int open_flags = O_CREAT | O_RDWR;
        if (o_direct)
            open_flags |= O_DIRECT;

        std::vector<int> fds;
        fds.push_back(open("read_seq.dat", open_flags, S_IRUSR | S_IWUSR));
        if (multi_file) {
            for (unsigned i = 1; i < nreads; ++i) {
                std::string filename = "read_seq_" + std::to_string(i) + ".dat";
                fds.push_back(open(filename.c_str(), open_flags, S_IRUSR | S_IWUSR));
            }
        }
        
        std::string wcontent = rand_string(rlen);
        char *wbuf = new (std::align_val_t(512)) char[rlen];
        memcpy(wbuf, wcontent.c_str(), wcontent.length());
        for (unsigned i = 0; i < nreads; ++i) {
            int fd = multi_file ? fds[i] : fds[0];
            off_t offset = multi_file ? 0 : i * rlen;
            [[maybe_unused]] ssize_t ret = pwrite(fd, wbuf, rlen, offset);
        }
        delete[] wbuf;

        ExperReadSeqArgs args(fds, rlen, nreads, same_buffer, multi_file);

        if (manual_ring) {
            args.manual_ring = &ring;
            run_iters(exper_read_seq_manual_ring, &args, num_iters, drop_caches, !dump_result);
        } else if (manual_pool) {
            args.manual_pool = &pool;
            run_iters(exper_read_seq_manual_pool, &args, num_iters, drop_caches, !dump_result);
        } else
            run_iters(exper_read_seq, &args, num_iters, drop_caches, !dump_result);
        
        if (dump_result) {
            if (!same_buffer) {
                for (auto buf : args.rbufs)
                    std::cout << std::string(buf, buf + args.rlen) << std::endl;
            } else
                std::cout << std::string(args.rbufs[0], args.rbufs[0] + args.rlen) << std::endl;
        }

        for (int fd : fds)
            close(fd);

    } else if (exper == "write_seq") {
        unsigned nwrites = 128;
        size_t wlen = 64 * 1024;

        int open_flags = O_CREAT | O_RDWR;
        if (o_direct)
            open_flags |= O_DIRECT;

        std::vector<int> fds;
        fds.push_back(open("write_seq.dat", open_flags, S_IRUSR | S_IWUSR));
        if (multi_file) {
            for (unsigned i = 1; i < nwrites; ++i) {
                std::string filename = "write_seq_" + std::to_string(i) + ".dat";
                fds.push_back(open(filename.c_str(), open_flags, S_IRUSR | S_IWUSR));
            }
        }
        
        ExperWriteSeqArgs args(fds, rand_string(wlen), nwrites, multi_file);
        
        if (manual_ring) {
            args.manual_ring = &ring;
            run_iters(exper_write_seq_manual_ring, &args, num_iters, drop_caches, !dump_result);
        } else if (manual_pool) {
            args.manual_pool = &pool;
            run_iters(exper_write_seq_manual_pool, &args, num_iters, drop_caches, !dump_result);
        } else
            run_iters(exper_write_seq, &args, num_iters, drop_caches, !dump_result);

        if (dump_result) {
            char *rbuf = new (std::align_val_t(512)) char[wlen];
            for (unsigned i = 0; i < nwrites; ++i) {
                int fd = multi_file ? fds[i] : fds[0];
                off_t offset = multi_file ? 0 : i * wlen;
                [[maybe_unused]] ssize_t ret = pread(fd, rbuf, wlen, offset);
                std::cout << std::string(rbuf, rbuf + wlen) << std::endl;
            }
            delete[] rbuf;
        }

        for (int fd : fds)
            close(fd);

    } else if (exper == "streaming") {
        size_t block_size = 64 * 1024;
        unsigned num_blocks = 256;

        int open_flags = O_CREAT | O_RDWR;
        if (o_direct)
            open_flags |= O_DIRECT;

        int fd_in = open("streaming_in.dat", open_flags, S_IRUSR | S_IWUSR);
        int fd_out = open("streaming_out.dat", open_flags, S_IRUSR | S_IWUSR);

        std::string wcontent = rand_string(block_size);
        char *wbuf = new (std::align_val_t(512)) char[block_size];
        memcpy(wbuf, wcontent.c_str(), wcontent.length());
        for (unsigned i = 0; i < num_blocks; ++i) {
            [[maybe_unused]] ssize_t ret = pwrite(fd_in, wbuf, block_size, i * block_size);
            assert(ret == block_size);
        }
        delete[] wbuf;

        ExperStreamingArgs args(fd_in, fd_out, block_size, num_blocks, same_buffer);

        run_iters(exper_streaming, &args, num_iters, drop_caches, !dump_result);

        if (dump_result) {
            char *rbuf = new (std::align_val_t(512)) char[block_size];
            for (unsigned i = 0; i < num_blocks; ++i) {
                [[maybe_unused]] ssize_t ret = pread(fd_out, rbuf, block_size, i * block_size);
                std::cout << std::string(rbuf, rbuf + block_size) << std::endl;
            }
            delete[] rbuf;
        }

        close(fd_in);
        close(fd_out);

    } else {
        std::cerr << "Error: unrecognized experiment " << exper << std::endl;
        print_usage_exit(self);
    }

    if (manual_ring)
        io_uring_queue_exit(&ring);
    else if (manual_pool)
        pool.JoinThreads();
}


static void print_usage_exit(const char *self) {
    std::cerr << "Usage: " << self << " EXPER_NAME DBDIR_PATH NUM_ITERS"
              << " [--drop_caches] [--dump_result] [--manual_ring|pool]"
              << " [--same_buffer] [--o_direct] [--multi_file]" << std::endl;
    exit(1);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if ((argc < 4) || (argc > 8))
        print_usage_exit(argv[0]);

    std::string exper(argv[1]);
    std::string dbdir(argv[2]);
    unsigned num_iters = std::stoul(std::string(argv[3]));

    bool drop_caches = false;
    bool dump_result = false;
    bool manual_ring = false;
    bool manual_pool = false;
    bool same_buffer = false;
    bool o_direct = false;
    bool multi_file = false;
    for (int i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--dump_result") == 0) {
            dump_result = true;
            srand(1234567);     // use fixed seed for result comparison
        } else if (strcmp(argv[i], "--drop_caches") == 0)
            drop_caches = true;
        else if (strcmp(argv[i], "--manual_ring") == 0)
            manual_ring = true;
        else if (strcmp(argv[i], "--manual_pool") == 0)
            manual_pool = true;
        else if (strcmp(argv[i], "--same_buffer") == 0)
            same_buffer = true;
        else if (strcmp(argv[i], "--o_direct") == 0)
            o_direct = true;
        else if (strcmp(argv[i], "--multi_file") == 0)
            multi_file = true;
    }
    if (manual_pool && manual_ring) {
        std::cerr << "Error: --manual_ring and --manual_pool both given"
                  << std::endl;
        print_usage_exit(argv[0]);
    }

    run_exper(argv[0], dbdir, exper, num_iters, drop_caches, dump_result,
              manual_ring, manual_pool, same_buffer, o_direct, multi_file);
    return 0;
}
