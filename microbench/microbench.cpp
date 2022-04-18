#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <math.h>
#include <string.h>

#include "cxxopts.hpp"
#include "exper_impl.hpp"


static constexpr size_t BLOCK_SIZE = 4096;


static const std::string gen_rand_string(size_t length) {
    static constexpr char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::string str;
    str.reserve(length);

    for (size_t i = 0; i < length; ++i)
        str += alphanum[rand() % (sizeof(alphanum) - 1)];

    return str;
}

static void make_files(const std::string dir_path, size_t num_files,
                       size_t file_size) {
    std::filesystem::remove_all(dir_path);
    std::filesystem::create_directory(dir_path);
    std::filesystem::current_path(dir_path);

    for (size_t i = 0; i < num_files; ++i) {
        std::ofstream file("tmp" + std::to_string(i));
        const std::string rand_content = gen_rand_string(file_size);
        file << rand_content;
    }
}


static std::vector<int> open_files(const std::string dir_path,
                                   size_t num_files, bool direct) {
    std::filesystem::current_path(dir_path);

    int flags = O_RDWR;
    if (direct)
        flags |= O_DIRECT;

    std::vector<int> files(num_files);
    for (size_t i = 0; i < num_files; ++i) {
        files[i] = open(("tmp" + std::to_string(i)).c_str(), flags);
        if (files[i] < 0)
            throw std::runtime_error("failed to open file");
    }

    return files;
}

static void close_files(std::vector<int>& files) {
    for (int fd : files)
        close(fd);
}


static void drop_caches(void) {
    int rc = system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    if (rc != 0)
        throw std::runtime_error("drop_caches failed");
}


static int pick_file(std::vector<int>& files, size_t num_reqs, size_t idx) {
    if (num_reqs < files.size())
        return files[idx];
    size_t reqs_per_file = num_reqs / files.size();
    size_t file_idx = idx / reqs_per_file;
    return file_idx < files.size() ? files[file_idx] : files.back();
}

static off_t pick_rnd_offset(size_t file_size, size_t req_size, int fd,
                             std::unordered_map<int, std::vector<off_t>>& offsets_seen) {
    if (file_size % req_size != 0)
        throw std::runtime_error("file_size not a multiple of req_size");
    if (file_size / req_size < 16)
        throw std::runtime_error("file_size too small for random");
    if (offsets_seen[fd].size() >= file_size / req_size)
        throw std::runtime_error("unique offsets have been used up");

    off_t offset;
    do {
        offset = req_size * (std::rand() % (file_size / req_size));
    } while (std::find(offsets_seen[fd].begin(), offsets_seen[fd].end(),
                       offset) != offsets_seen[fd].end());
    
    offsets_seen[fd].push_back(offset);
    return offset;
}

static off_t pick_seq_offset(size_t file_size, size_t req_size, size_t idx) {
    if (file_size < req_size * (idx + 1))
        throw std::runtime_error("file_size too small for sequential");
    return req_size * idx;
}


static void require_args(cxxopts::Options& options, cxxopts::ParseResult& args,
                         std::vector<std::string> required) {
    for (auto& s : required) {
        if (args.count(s) == 0) {
            std::cerr << options.help();
            throw std::runtime_error("some required argument(s) not given");
        }
    }
}


static void print_time_stat(std::vector<double>& times_us) {
    double avg_us = 0.0,
           max_us = std::numeric_limits<double>::min(),
           min_us = std::numeric_limits<double>::max();
    for (double& time : times_us) {
        avg_us += time;
        max_us = time > max_us ? time : max_us;
        min_us = time < min_us ? time : min_us;
    }
    avg_us /= times_us.size();

    double stddev = 0.0;
    for (double& time : times_us)
        stddev += ((time - avg_us) * (time - avg_us)) / times_us.size();
    stddev = sqrt(stddev);

    printf("Time elapsed stat:\n");
    printf("  count   %ld\n", times_us.size());
    printf("  avg_us  %.3lf\n", avg_us);
    printf("  max_us  %.3lf\n", max_us);
    printf("  min_us  %.3lf\n", min_us);
    printf("  stddev  %.3lf\n", stddev);
}


int main(int argc, char *argv[]) {
    std::srand(std::time(0));

    // argument parsing & sanity check
    std::string dir_path, async_mode;
    size_t num_files, file_size, num_reqs, req_size,
           num_threads, uring_queue, timing_rounds, warmup_rounds;

    cxxopts::Options options("microbenchmark driver");
    options.add_options()
            ("h,help", "print help message", cxxopts::value<bool>())
            // file path
            ("d,directory", "path to temp directory", cxxopts::value<std::string>(dir_path))
            ("make", "if given, prepare files and exit", cxxopts::value<bool>())
            // workload
            ("f,num_files", "number of files to span across", cxxopts::value<size_t>(num_files))
            ("s,file_size", "file size in bytes", cxxopts::value<size_t>(file_size))
            ("w,write", "if given, pwrites; else, preads", cxxopts::value<bool>())
            ("sequential", "if given, do sequential reqs; else, random", cxxopts::value<bool>())
            ("n,num_reqs", "number of requests in function", cxxopts::value<size_t>(num_reqs))
            ("r,req_size", "size of each r/w request", cxxopts::value<size_t>(req_size))
            // async mechanism
            ("a,async", "async I/O mechanism to use", cxxopts::value<std::string>(async_mode))
            ("t,num_threads", "if thread_pool, num of threads", cxxopts::value<size_t>(num_threads))
            ("q,uring_queue", "if io_uring, capacity of queue",
                              cxxopts::value<size_t>(uring_queue)->default_value("256"))
            ("no_iosqe_async", "if io_uring, do not set IOSQE_ASYNC", cxxopts::value<bool>())
            ("fixed_buf", "if given for io_uring, use fixed buffers", cxxopts::value<bool>())
            ("fixed_file", "if given for io_uring, use fixed files", cxxopts::value<bool>())
            ("sq_poll", "if given for io_uring, use SQ polling", cxxopts::value<bool>())
            // page cache & device
            ("direct", "if given, opens with O_DIRECT", cxxopts::value<bool>())
            // ("dev_poll", "if given, do polling I/O", cxxopts::value<bool>())
            // experiment
            ("tr", "number of timing rounds", cxxopts::value<size_t>(timing_rounds))
            ("wr", "number of warmup rounds", cxxopts::value<size_t>(warmup_rounds));
    auto args = options.parse(argc, argv);

    if (args.count("help")) {
        std::cout << options.help();
        return 0;
    }

    bool flag_make = args.count("make") > 0,
         flag_write = args.count("write") > 0,
         flag_sequential = args.count("sequential") > 0,
         flag_no_iosqe_async = args.count("no_iosqe_async") > 0,
         flag_fixed_buf = args.count("fixed_buf") > 0,
         flag_fixed_file = args.count("fixed_file") > 0,
         flag_sq_poll = args.count("sq_poll") > 0,
         flag_direct = args.count("direct") > 0;

    require_args(options, args, {"directory", "num_files", "file_size"});
    if (!flag_make) {
        require_args(options, args, {"num_reqs", "req_size", "async", "tr", "wr"});
        if (async_mode == "thread_pool")
            require_args(options, args, {"num_threads"});
    }

    if (flag_sq_poll && !flag_fixed_file)
        throw std::runtime_error("--sq_poll requires --fixed_file as well");

    if (flag_sequential && num_files != 1)
        throw std::runtime_error("--sequential requires only 1 file");

    if (async_mode == "io_uring" && flag_fixed_buf && !flag_direct)
        throw std::runtime_error("io_uring --fixed_buf only works for direct I/O");

    if (flag_direct && req_size % BLOCK_SIZE != 0)
        throw std::runtime_error("request size not a multiple of 4KiB");

    // prepare the files
    if (flag_make) {
        make_files(dir_path, num_files, file_size);
        return 0;
    }

    drop_caches();
    std::vector<int> files = open_files(dir_path, num_files, flag_direct);

    // construct the request sequence
    std::unordered_map<int, std::vector<off_t>> offsets_seen;
    std::vector<Req> reqs;
    reqs.reserve(num_reqs);
    for (size_t idx = 0; idx < num_reqs; ++idx) {
        int fd = pick_file(files, num_reqs, idx);
        if (offsets_seen.find(fd) == offsets_seen.end())
            offsets_seen[fd] = std::vector<off_t>();
        off_t offset = flag_sequential ? pick_seq_offset(file_size, req_size, idx)
                                       : pick_rnd_offset(file_size, req_size, fd, offsets_seen);
        reqs.emplace_back(fd, offset, req_size, flag_write,
                          new (std::align_val_t(BLOCK_SIZE)) char[req_size]);

        if (flag_write) {
            const std::string str = gen_rand_string(req_size - 1);
            strncpy(reqs.back().buf, str.c_str(), req_size);
        }
    }

    // carry out the experiment
    std::vector<double> times_us;
    if (async_mode == "basic")
        times_us = run_exper_basic(reqs, timing_rounds, warmup_rounds);
    else if (async_mode == "thread_pool")
        times_us = run_exper_thread_pool(reqs, num_threads, timing_rounds, warmup_rounds);
    else if (async_mode == "io_uring")
        times_us = run_exper_io_uring(reqs, uring_queue, flag_fixed_buf,
                                      flag_fixed_file, flag_sq_poll, flag_no_iosqe_async,
                                      timing_rounds, warmup_rounds);
    else {
        std::cerr << "Supported async_mode: basic|thread_pool|io_uring" << std::endl;
        throw std::runtime_error("unrecognized async_mode");
    }

    print_time_stat(times_us);

    // free all buffers in reqs
    for (auto& req : reqs)
        delete[] req.buf;

    close_files(files);
    return 0;
}
