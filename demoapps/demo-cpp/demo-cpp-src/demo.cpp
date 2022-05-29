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

#include "hijackees.hpp"


static constexpr int NUM_RUN_ITERS = 5;


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
    std::cerr << "Usage: " << self << " EXPER_NAME DBDIR_PATH"
              << " [--drop_caches] [--dump_result]" << std::endl;
    exit(1);
}

static void cmd_drop_caches(void) {
    [[maybe_unused]] int rc =
        system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    assert(rc == 0);
}


void run_iters(ExperFunc exper_func, ExperArgs *exper_args,
               bool drop_caches) {
    std::vector<double> elapsed_us;
    for (int i = 0; i < NUM_RUN_ITERS; ++i) {
        if (drop_caches)    // drain page cache before each run?
            cmd_drop_caches();
        
        auto t1 = std::chrono::high_resolution_clock::now();
        exper_func(exper_args);
        auto t2 = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::micro> duration = t2 - t1;
        elapsed_us.push_back(duration.count());
    }

    std::cerr << "  Time elapsed: [ ";
    for (double& us : elapsed_us)
        std::cerr << us << " ";
    std::cerr << "] us" << std::endl;
}

void run_exper(const char *self, std::string& dbdir, std::string& exper,
               bool drop_caches, bool dump_result) {
    std::filesystem::current_path(dbdir);

    if (exper == "simple") {
        ExperFunc func = exper_simple;
        ExperSimpleArgs args("simple.dat", rand_string(8192));
        run_iters(func, &args, drop_caches);
        if (dump_result) {
            std::cout << std::string(args.rbuf0, args.rbuf0 + args.rlen)
                      << std::endl
                      << std::string(args.rbuf1, args.rbuf1 + args.rlen)
                      << std::endl;
        }

    } else
        print_usage_exit(self);
}


int main(int argc, char *argv[]) {
    srand(time(NULL));

    if ((argc < 3) || (argc > 5))
        print_usage_exit(argv[0]);

    std::string exper(argv[1]);
    std::string dbdir(argv[2]);

    bool drop_caches = false;
    bool dump_result = false;
    for (int i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--drop_caches") == 0)
            drop_caches = true;
        else if (strcmp(argv[i], "--dump_result") == 0)
            dump_result = true;
    }

    run_exper(argv[0], dbdir, exper, drop_caches, dump_result);
    return 0;
}
