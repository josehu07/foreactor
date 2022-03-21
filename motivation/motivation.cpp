#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <chrono>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <liburing.h>


static constexpr char TMPDIR[] = "/tmp/motivation_tmpdir";

static unsigned NUM_FILES = 32;     // maybe set by argument
static constexpr size_t READ_SIZE = 4096;

static char **bufs;

static std::mutex start_mu, finish_mu;
static std::condition_variable start_cv, finish_cv;
static std::vector<bool> thread_start;
static std::vector<bool> thread_done;

static constexpr unsigned URING_QUEUE_LEN = 256;


void thread_func(std::vector<int> *files, size_t id, size_t num_threads) {
    {
        std::unique_lock start_lk(start_mu);
        if (!thread_start[id])
            start_cv.wait(start_lk);
    }

    for (size_t idx = id; idx < NUM_FILES; idx += num_threads) {
        int fd = files->at(idx);

        ssize_t res = pread(fd, bufs[idx], READ_SIZE, 0);
        if (res != READ_SIZE)
            throw std::runtime_error("app thread pread not successful");
    }

    std::unique_lock finish_lk(finish_mu);
    thread_done[id] = true;
    if (std::all_of(thread_done.begin(), thread_done.end(),
        [](bool b) { return b; })) {
        finish_cv.notify_one();
    }
}

double preads_async_app_threads(std::vector<int>& files, size_t num_threads) {
    thread_start.clear();
    thread_done.clear();
    for (size_t i = 0; i < num_threads; ++i) {
        thread_start.push_back(false);
        thread_done.push_back(false);
    }

    std::vector<std::thread> workers(num_threads);
    for (size_t i = 0; i < num_threads; ++i)
        workers[i] = std::thread(thread_func, &files, i, num_threads);

    // Timing start...
    auto ts_beg = std::chrono::high_resolution_clock::now();

    {
        std::unique_lock start_lk(start_mu);
        for (size_t i = 0; i < num_threads; ++i)
            thread_start[i] = true;
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

    // Timing end...
    auto ts_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed_us = ts_end - ts_beg;

    for (auto& t : workers)
        t.join();

    return elapsed_us.count();
}


double preads_async_io_uring(std::vector<int>& files) {
    struct io_uring ring;
    
    int rc = io_uring_queue_init(URING_QUEUE_LEN, &ring, 0);
    if (rc != 0)
        throw std::runtime_error("io_uring_queue_init failed");

    // Timing start...
    auto ts_beg = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < files.size(); ++i) {
        int fd = files[i];

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (sqe == nullptr)
            throw std::runtime_error("io_uring_get_sqe failed");

        io_uring_prep_read(sqe, fd, bufs[i], READ_SIZE, 0);
        io_uring_sqe_set_data(sqe, (void *) i);
    }

    int submitted = io_uring_submit(&ring);
    if (submitted != static_cast<int>(files.size()))
        throw std::runtime_error("fewer than all prepared entries submitted");

    for (size_t j = 0; j < files.size(); ++j) {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
            throw std::runtime_error("io_uring_wait_cqe failed");

        if (cqe->res != READ_SIZE)
            throw std::runtime_error("io_uring pread not successful");

        size_t id = (size_t) io_uring_cqe_get_data(cqe);
        if (id >= files.size())
            throw std::runtime_error("io_uring cqe invalid user data field");

        io_uring_cqe_seen(&ring, cqe);
    }

    // Timing end...
    auto ts_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed_us = ts_end - ts_beg;

    io_uring_queue_exit(&ring);

    return elapsed_us.count();
}


static const std::string rand_string(size_t length) {
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

static std::vector<int> make_files() {
    std::filesystem::remove_all(TMPDIR);
    std::filesystem::create_directory(TMPDIR);
    std::filesystem::current_path(TMPDIR);

    for (size_t i = 0; i < NUM_FILES; ++i) {
        std::ofstream file("tmp" + std::to_string(i));
        std::string rand_content = rand_string(READ_SIZE);
        file << rand_content;
    }

    std::vector<int> files(NUM_FILES);
    for (size_t i = 0; i < NUM_FILES; ++i) {
        files[i] = open(("tmp" + std::to_string(i)).c_str(), 0, O_RDONLY);
        if (files[i] < 0)
            throw std::runtime_error("failed to open file");
    }

    return files;
}


static void drop_caches(void) {
    int rc = system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    if (rc != 0)
        throw std::runtime_error("drop_caches failed");
}


int main(int argc, char *argv[]) {
    if (argc != 2)
        throw std::runtime_error("usage: ./motivation num_files");

    NUM_FILES = std::stoul(argv[1]);
    if (NUM_FILES <= 0)
        throw std::runtime_error("num_files must be positive");
    if (NUM_FILES > URING_QUEUE_LEN) {
        throw std::runtime_error("num_files cannot be more than " +
                                 std::to_string(URING_QUEUE_LEN));
    }

    bufs = new char *[NUM_FILES];
    for (size_t i = 0; i < NUM_FILES; ++i)
        bufs[i] = new char[READ_SIZE];

    std::vector<int> files = make_files();

    drop_caches();
    double app_threads_unbounded_us = preads_async_app_threads(files, NUM_FILES);

    drop_caches();
    double app_threads_bounded_us = preads_async_app_threads(files, 8);

    drop_caches();
    double io_uring_us = preads_async_io_uring(files);

    std::cout << "app threads unbounded: " << app_threads_unbounded_us << " us" << std::endl
              << "app threads bounded (8): " << app_threads_bounded_us << " us" << std::endl
              << "io_uring: " << io_uring_us << " us" << std::endl;

    for (size_t i = 0; i < NUM_FILES; ++i)
        delete[] bufs[i];
    delete[] bufs;
    
    return 0;
}
