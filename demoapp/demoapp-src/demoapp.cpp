#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>
#include <chrono>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "ldb_get.hpp"


static constexpr int OPEN_SKIP_WHICH = 12;
static constexpr int NUM_RUN_ITERS = 5;


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

static void drop_caches(void) {
    int rc __attribute__((unused)) =
        system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    assert(rc == 0);
}

static std::vector<std::vector<int>> open_all(bool selective) {
    std::vector<std::vector<int>> files;
    for (int level = 0; level < NUM_LEVELS; ++level) {
        files.push_back(std::vector<int>());
        for (int index = 0; index < FILES_PER_LEVEL; ++index) {
            if ((!selective) || (index != OPEN_SKIP_WHICH)) {
                int fd = open(table_name(level, index).c_str(), 0, O_RDONLY);
                assert(fd > 0);
                files.back().push_back(fd);
            } else
                files.back().push_back(-1);
        }
    }
    return files;
}

static void close_all(std::vector<std::vector<int>> files) {
    for (auto& level : files) {
        for (int fd : level) {
            if (fd > 0)
                close(fd);
        }
    }
}


void make_db_img() {
    std::filesystem::remove_all(DBDIR);
    std::filesystem::create_directory(DBDIR);
    std::filesystem::current_path(DBDIR);

    for (int level = 0; level < NUM_LEVELS; ++level) {
        for (int index = 0; index < FILES_PER_LEVEL; ++index) {
            std::ofstream table(table_name(level, index));
            std::string rand_content = rand_string(FILE_SIZE);
            table << rand_content;
        }
    }
}

void run_ldb_get(bool dump_result, bool selective_open = false,
                 bool do_drop_caches = false) {
    std::filesystem::current_path(DBDIR);
    std::vector<std::string> bytes;

    std::vector<double> elapsed_us;
    for (int i = 0; i < NUM_RUN_ITERS; ++i) {
        std::vector<std::vector<int>> files = open_all(selective_open);

        if (do_drop_caches)     // drain page cache before each run?
            drop_caches();
        
        auto t1 = std::chrono::high_resolution_clock::now();
        bytes = ldb_get(files);
        auto t2 = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::micro> duration = t2 - t1;
        elapsed_us.push_back(duration.count());

        close_all(files);
    }

    std::cerr << "Time elapsed: [ ";
    for (double& us : elapsed_us)
        std::cerr << us << " ";
    std::cerr << "] us" << std::endl;

    if (dump_result) {
        for (std::string& s : bytes)
            std::cout << s << std::endl;
    }
}


int main(int argc, char *argv[]) {
    if ((argc < 2) || (argc > 4)) {
        std::cerr << "Usage: " << argv[0] << " make|dump|run"
                  << " [--selective_open] [--drop_caches]" << std::endl;
        return -1;
    }

    if (strcmp(argv[1], "make") == 0) {
        make_db_img();
        return 0;
    } else if (strcmp(argv[1], "run") == 0) {
        bool selective_open = false;
        bool do_drop_caches = false;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--selective_open") == 0)
                selective_open = true;
            else if (strcmp(argv[i], "--drop_caches") == 0)
                do_drop_caches = true;
        }
        run_ldb_get(false, selective_open, do_drop_caches);
        return 0;
    } else if (strcmp(argv[1], "dump") == 0) {
        run_ldb_get(true);
        return 0;
    } else {
        std::cerr << "Usage: " << argv[0] << " make|dump|run"
                  << " [--selective_open] [--drop_caches]" << std::endl;
        return -1;
    }
}
