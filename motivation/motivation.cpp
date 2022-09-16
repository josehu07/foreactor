#include <iostream>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <thread>
#include <future>
#include <stdexcept>
#include <chrono>
#include <new>
#include <cstdlib>
#include <cassert>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cxxopts.hpp"


static unsigned NPROC = 16;
static size_t BLKSIZE = 4096;


static const std::string rand_string(size_t length) {
    static constexpr char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string str;
    str.reserve(length);
    std::srand(time(NULL));
    for (size_t i = 0; i < length; ++i)
        str += alphanum[std::rand() % (sizeof(alphanum) - 1)];
    return str;
}

static void create_file(std::filesystem::path path, size_t size) {
    assert(size >= 65536);

    if (std::filesystem::exists(path)) {
        if (!std::filesystem::is_regular_file(path)) {
            throw std::runtime_error(
                  path.string() + " exists and is non-regular file");
        } else if (std::filesystem::file_size(path) != size) {
            throw std::runtime_error(
                  path.string() + " exists and size does not match");
        }
    } else {
        std::ofstream ofs(path);
        const std::string str = rand_string(65536);
        while (size > 0) {
            size_t write_size = size < 65536 ? size : 65536;
            ofs.write(str.data(), write_size);
            size -= write_size;
        }
    }
}

[[maybe_unused]]
static void cmd_drop_caches(void) {
    [[maybe_unused]] int rc =
        system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    assert(rc == 0);
}


class ExperThreadPool {
    private:
        std::vector<std::thread> workers;
        bool working = false;

        int fd = -1;
        size_t filesize = 0;

        // (id, fd, filesize) -> void
        typedef std::function<void(int, int, size_t)> WorkloadFunc;
        // (elapsed_us) -> throughput
        typedef std::function<double(double)> ThroughputCalcFunc;

        void WorkerThreadFunc(int id, WorkloadFunc workload_func,
                              std::promise<void>&& init_promise,
                              std::future<void>&& work_future,
                              std::promise<double>&& us_promise) {
            // signal main thread for initialization complete
            init_promise.set_value();

            // wait on workload kick-off signal
            work_future.wait();

            // run workload func
            auto start = std::chrono::high_resolution_clock::now();
            workload_func(id, fd, filesize);
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed_us =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    end - start).count();

            // report elapsed_us
            us_promise.set_value(elapsed_us);
        }

        void StartThreads(size_t nthreads, WorkloadFunc workload_func,
                          std::vector<std::promise<double>> us_promises) {
            assert(!working);

            // spawn worker threads
            std::vector<std::future<void>> init_futures;
            std::vector<std::promise<void>> work_promises;

            for (int id = 0; id < static_cast<int>(nthreads); ++id) {
                std::promise<void> init_promise;
                init_futures.push_back(init_promise.get_future());
                work_promises.emplace_back();

                workers.push_back(
                    std::thread(&ExperThreadPool::WorkerThreadFunc,
                                this, id, workload_func,
                                std::move(init_promise),
                                std::move(work_promises[id].get_future()),
                                std::move(us_promises[id])));

                // set core affinity to core with the same index as thread ID
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(id % NPROC, &cpuset);
                [[maybe_unused]] int ret =
                    pthread_setaffinity_np(workers.back().native_handle(),
                                           sizeof(cpu_set_t), &cpuset);
                assert(ret == 0);
            }

            // wait for everyone finishes initialization
            for (auto&& init_future : init_futures)
                init_future.wait();
            working = true;

            // kick off workload execution
            for (auto&& work_promise : work_promises)
                work_promise.set_value();
        }

        void JoinThreads() {
            assert(working);

            // wait and join all worker threads
            for (auto&& worker: workers)
                worker.join();

            // cleanup workers vector
            workers.clear();
            working = false;
        }

    public:
        ExperThreadPool(std::string& path, size_t filesize)
                : filesize(filesize) {
            // open file in O_DIRECT mode
            fd = open(path.c_str(), O_DIRECT | O_RDWR);
            if (fd <= 0) {
                throw std::runtime_error(
                      "failed to open " + path + " in O_DIRECT mode: "
                      + std::to_string(fd) + " " + std::strerror(errno));
            }
        }

        ~ExperThreadPool() {
            close(fd);
        }

        // returns sum of throughput across all threads
        double RunExper(size_t nthreads, WorkloadFunc workload_func,
                        ThroughputCalcFunc throughput_calc_func) {
            assert(nthreads > 0);
            std::vector<std::promise<double>> us_promises(nthreads);

            std::vector<std::future<double>> us_futures;
            for (auto&& us_promise : us_promises)
                us_futures.push_back(us_promise.get_future());

            // start worker threads executing workload_func
            StartThreads(nthreads, workload_func, std::move(us_promises));
            JoinThreads();

            // aggregate throughput of each thread
            double sum_throughput = 0.;
            for (auto&& us_future : us_futures) {
                double elapsed_us = us_future.get();
                sum_throughput += throughput_calc_func(elapsed_us);
            }
            return sum_throughput;
        }
};


[[maybe_unused]]
static void run_sequential_exper(std::string& path, const size_t filesize,
                                 const size_t reqsize, const bool is_read,
                                 const unsigned num_iters) {
    assert(reqsize >= BLKSIZE);
    char *buf = new (std::align_val_t(BLKSIZE)) char[reqsize];
    assert(buf != nullptr);
    if (!is_read) {
        std::string str = rand_string(reqsize);
        memcpy(buf, str.c_str(), reqsize - 1);
        buf[reqsize - 1] = '\0';
    }

    auto workload_func = [&](int id, int fd, size_t filesize) {
        size_t offset = 0;
        while (offset < filesize) {
            size_t remain = filesize - offset;
            size_t count = remain < reqsize ? remain : reqsize;

            ssize_t ret;
            if (is_read)
                ret = pread(fd, buf, count, offset);
            else
                ret = pwrite(fd, buf, count, offset);
            assert(ret >= 0);

            offset += ret;
        }
    };

    auto throughput_calc_func = [&](double elapsed_us) -> double {
        return static_cast<double>(filesize) / elapsed_us;
    };

    ExperThreadPool etp(path, filesize);
    std::cout << "Exper sequential " << (is_read ? "read" : "write")
              << " reqsize=" << reqsize << std::endl;

    double avg_mb_s = 0.;
    for (unsigned i = 0; i < num_iters; ++i) {
        // cmd_drop_caches();
        double mb_s = etp.RunExper(1, workload_func, throughput_calc_func);
        std::cout << "  iter " << i << " " << std::fixed << std::setw(8)
                  << std::setprecision(3) << mb_s << " MB/s" << std::endl;
        avg_mb_s += mb_s;
    }

    avg_mb_s /= num_iters;
    std::cout << " average " << std::fixed << std::setw(8)
              << std::setprecision(3) << avg_mb_s << " MB/s" << std::endl;

    delete[] buf;
}

[[maybe_unused]]
static void run_mixed_rand_exper(std::string& path, const size_t filesize,
                                 const size_t nthreads, const size_t reqsize,
                                 const size_t num_reqs,
                                 const unsigned num_iters) {
    assert(reqsize >= BLKSIZE);
    std::vector<char *> bufs;
    std::string str = rand_string(reqsize);
    for (size_t id = 0; id < nthreads; ++id) {
        char *buf = new (std::align_val_t(BLKSIZE)) char[reqsize];
        assert(buf != nullptr);
        memcpy(buf, str.c_str(), reqsize - 1);
        buf[reqsize - 1] = '\0';
        bufs.push_back(buf);
    }

    auto workload_func = [&](int id, int fd, size_t filesize) {
        for (size_t n = 0; n < num_reqs; ++n) {
            const size_t nchunks = filesize / reqsize;
            size_t offset = (std::rand() % nchunks) * reqsize;
            bool is_read = (std::rand() % 2) == 0;
            
            ssize_t ret;
            if (is_read)
                ret = pread(fd, bufs[id], reqsize, offset);
            else
                ret = pwrite(fd, bufs[id], reqsize, offset);
            assert(ret >= 0);
        }
    };

    auto throughput_calc_func = [&](double elapsed_us) -> double {
        return (static_cast<double>(reqsize) * num_reqs) / elapsed_us;
    };

    ExperThreadPool etp(path, filesize);
    std::cout << "Exper mixed_rand nthreads=" << nthreads
              << " reqsize=" << reqsize << std::endl;

    double avg_mb_s = 0.;
    for (unsigned i = 0; i < num_iters; ++i) {
        // cmd_drop_caches();
        std::srand(time(NULL));
        double mb_s = etp.RunExper(nthreads, workload_func, throughput_calc_func);
        std::cout << "  iter " << i << " " << std::fixed << std::setw(8)
                  << std::setprecision(3) << mb_s << " MB/s" << std::endl;
        avg_mb_s += mb_s;
    }

    avg_mb_s /= num_iters;
    std::cout << " average " << std::fixed << std::setw(8)
              << std::setprecision(3) << avg_mb_s << " MB/s" << std::endl;

    for (char *buf : bufs)
        delete[] buf;
}


static void print_usage_exit(cxxopts::Options& options) {
    std::cerr << options.help();
    exit(1);
}

int main(int argc, char *argv[]) {
    constexpr size_t filesize = 1024 * 1024 * 1024;     // 1GB
    constexpr unsigned num_iters = 3;
    constexpr size_t num_rand_reqs = 32768;
    const std::vector<size_t> rand_nthreads {1, 2, 4, 8, 16, 24, 32};
    const std::vector<size_t> rand_reqsizes {4096, 16384, 65536};
    
    std::string path;

    cxxopts::Options options("motivation experiment");
    options.add_options()
        ("p,path", "path to test file", cxxopts::value<std::string>(path));
    auto args = options.parse(argc, argv);

    if (path.length() == 0) {
        std::cerr << "Error: path option is empty" << std::endl;
        print_usage_exit(options);
    }

    try {
        create_file(path, filesize);
    } catch (const std::exception& e) {
        std::cerr << "Error: create_file: " << e.what() << std::endl;
        return 2;
    }

    NPROC = std::thread::hardware_concurrency();
    assert(NPROC > 0);
    struct stat sbuf;
    int ret = stat(path.c_str(), &sbuf);
    assert(ret == 0);
    BLKSIZE = sbuf.st_blksize;

    try {
        // run_sequential_exper(path, filesize, 1048576, true, num_iters);
        run_sequential_exper(path, filesize, 1048576, false, num_iters);
        
        for (const size_t reqsize : rand_reqsizes) {
            for (const size_t nthreads : rand_nthreads) {
                run_mixed_rand_exper(path, filesize, nthreads, reqsize,
                                     num_rand_reqs, num_iters);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: run_exper: " << e.what() << std::endl;
        return 3;
    }

    return 0;
}
