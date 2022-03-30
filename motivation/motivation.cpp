#include <new>
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


static constexpr char TMPDIR[] = "/mnt/ssd/josehu/motivation_tmpdir";

static constexpr size_t NUM_FILES = 10;
static constexpr size_t FILE_SIZE = 1024 * 1024;
static constexpr size_t READ_SIZE = 4096;

static char **bufs;

static std::mutex start_mu, finish_mu;
static std::condition_variable start_cv, finish_cv;
static std::vector<bool> thread_start;
static std::vector<bool> thread_done;

static constexpr unsigned URING_QUEUE_LEN = 256;


static int pick_rand_file(std::vector<int> *files) {
    return files->at(std::rand() % NUM_FILES);
}

static size_t pick_rand_offset() {
    return READ_SIZE * (std::rand() % (FILE_SIZE / READ_SIZE));
}


////////////////////////////////////
// App-level thread pool approach //
////////////////////////////////////

void thread_func(std::vector<int> *files, size_t id, size_t num_preads,
                 size_t num_threads) {
    {
        std::unique_lock start_lk(start_mu);
        if (!thread_start[id])
            start_cv.wait(start_lk);
    }

    for (size_t idx = id; idx < num_preads; idx += num_threads) {
        ssize_t res = pread(pick_rand_file(files), bufs[idx], READ_SIZE,
                            pick_rand_offset());
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

double preads_async_app_threads(std::vector<int>& files, size_t num_preads,
                                size_t num_threads) {
    thread_start.clear();
    thread_done.clear();
    for (size_t id = 0; id < num_threads; ++id) {
        thread_start.push_back(false);
        thread_done.push_back(false);
    }

    std::vector<std::thread> workers(num_threads);
    for (size_t id = 0; id < num_threads; ++id) {
        workers[id] = std::thread(thread_func, &files, id, num_preads,
                                  num_threads);
    }

    // Sleep for a while to ensure all workers ready, listening on cv
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Timing start...
    auto ts_beg = std::chrono::high_resolution_clock::now();

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

    // Timing end...
    auto ts_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed_us = ts_end - ts_beg;

    for (auto& t : workers)
        t.join();

    return elapsed_us.count();
}


///////////////////////////////////
// Async intf. io_uring approach //
///////////////////////////////////

double preads_async_io_uring(std::vector<int>& files, size_t num_preads) {
    struct io_uring ring;
    
    int rc = io_uring_queue_init(URING_QUEUE_LEN, &ring, 0);
    if (rc != 0)
        throw std::runtime_error("io_uring_queue_init failed");

    // Timing start...
    auto ts_beg = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_preads; ++i) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (sqe == nullptr)
            throw std::runtime_error("io_uring_get_sqe failed");

        io_uring_prep_read(sqe, pick_rand_file(&files), bufs[i], READ_SIZE,
                           pick_rand_offset());
        io_uring_sqe_set_data(sqe, (void *) i);
    }

    int submitted = io_uring_submit(&ring);
    if (submitted != static_cast<int>(num_preads))
        throw std::runtime_error("fewer than all prepared entries submitted");

    for (size_t j = 0; j < num_preads; ++j) {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
            throw std::runtime_error("io_uring_wait_cqe failed");

        if (cqe->res != READ_SIZE)
            throw std::runtime_error("io_uring pread not successful");

        size_t id = (size_t) io_uring_cqe_get_data(cqe);
        if (id >= num_preads)
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

static void make_files() {
    std::filesystem::remove_all(TMPDIR);
    std::filesystem::create_directory(TMPDIR);
    std::filesystem::current_path(TMPDIR);

    for (size_t i = 0; i < NUM_FILES; ++i) {
        std::ofstream file("tmp" + std::to_string(i));
        std::string rand_content = rand_string(FILE_SIZE);
        file << rand_content;
    }
}


static std::vector<int> open_files() {
    std::filesystem::current_path(TMPDIR);

    std::vector<int> files(NUM_FILES);
    for (size_t i = 0; i < NUM_FILES; ++i) {
        files[i] = open(("tmp" + std::to_string(i)).c_str(), 0, O_RDONLY);
        if (files[i] < 0)
            throw std::runtime_error("failed to open file");
    }

    return files;
}

static void close_files(std::vector<int>& files) {
    for (int fd : files)
        close(fd);
}


// static void drop_caches(void) {
//     int rc = system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
//     if (rc != 0)
//         throw std::runtime_error("drop_caches failed");
// }


int main(int argc, char *argv[]) {
    if (argc != 2 && argc != 3)
        throw std::runtime_error("usage: ./motivation make|run NUM_PREADS");

    // create the files for reading
    if (strcmp(argv[1], "make") == 0) {
        make_files();
        return 0;

    // benchmark three forms of asynchrony on NUM_PREADS random preads
    } else if (strcmp(argv[1], "run") == 0) {
        if (argc != 3)
            throw std::runtime_error("usage: ./motivation make|run NUM_PREADS");

        size_t num_preads = std::stoul(argv[2]);
        if (num_preads <= 0)
            throw std::runtime_error("num_preads must be positive");
        if (num_preads > URING_QUEUE_LEN) {
            throw std::runtime_error("num_preads cannot be more than " +
                                     std::to_string(URING_QUEUE_LEN));
        }

        bufs = new char *[num_preads];
        for (size_t i = 0; i < num_preads; ++i)
            bufs[i] = new (std::align_val_t(READ_SIZE)) char[READ_SIZE];

        std::vector<int> files = open_files();

        // warm up any caching involved, do not time this one
        double unused __attribute__((unused)) =
            preads_async_io_uring(files, num_preads);

        // drop_caches();
        double app_threads_unbounded_us =
            preads_async_app_threads(files, num_preads, num_preads);

        // drop_caches();
        double app_threads_bounded_us =
            preads_async_app_threads(files, num_preads, 8);

        // drop_caches();
        double io_uring_us =
            preads_async_io_uring(files, num_preads);

        std::cout << "app threads unbounded: " << app_threads_unbounded_us
                  << " us" << std::endl
                  << "app threads bounded (8): " << app_threads_bounded_us
                  << " us" << std::endl
                  << "io_uring: " << io_uring_us << " us" << std::endl;

        for (size_t i = 0; i < num_preads; ++i)
            delete[] bufs[i];
        delete[] bufs;

        close_files(files);

    } else {
        throw std::runtime_error("usage: ./motivation make|run NUM_PREADS");
    }
    
    return 0;
}
