#include <new>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <string.h>

#include "cxxopts.hpp"
#include "exper_impl.hpp"


static size_t pick_rand_offset(size_t file_size, size_t req_size) {
    if (file_size % req_size != 0)
        throw std::runtime_error("file_size not a multiple of req_size");
    if ((file_size / req_size) < 16)
        throw std::runtime_error("file_size too small");
    if (req_size % 4096 != 0)
        throw std::runtime_error("req_size not a multiple of 4KiB");
    return req_size * (std::rand() % (file_size / req_size));
}

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


static void drop_caches(void) {
    int rc = system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    if (rc != 0)
        throw std::runtime_error("drop_caches failed");
}


int main(int argc, char *argv[]) {
    
    
    return 0;
}
