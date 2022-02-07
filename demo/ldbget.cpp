#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include <foreactor.hpp>


static constexpr char DBDIR[] = "/tmp/hintdemo_dbdir";
static constexpr int NUM_LEVELS = 3;
static constexpr int FILES_PER_LEVEL = 8;
// static constexpr size_t FILE_SIZE = 16 * 1024 * 1024;
static constexpr size_t FILE_SIZE = 4096;

static const std::string table_name(int level, int index)
{
    std::stringstream ss;
    ss << level << "-" << index << ".ldb";
    return ss.str();
}


static void drop_caches(void)
{
    system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
}


static char READ_BUF[FILE_SIZE];

static void do_get_serial(std::vector<std::vector<int>>& files)
{
    auto t1 = std::chrono::high_resolution_clock::now();
    // level-0 tables from latest to oldest
    for (int index = FILES_PER_LEVEL - 1; index >= 0; --index) {
        ssize_t ret = pread(files[0][index], READ_BUF, FILE_SIZE, 0);
        assert(ret == FILE_SIZE);
    }
    // levels 1 and beyond
    for (int level = 1; level < NUM_LEVELS; ++level) {
        int index = std::rand() % 8;    // simulate the calc of file in level
        ssize_t ret = pread(files[level][index], READ_BUF, FILE_SIZE, 0);
        assert(ret == FILE_SIZE);
    }
    auto t2 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> time_finish_syscalls = t2 - t1;
    std::cout << "Serial - total time: " << time_finish_syscalls.count()
        << " ms" << std::endl;
}

static void do_get_foreactor(std::vector<std::vector<int>>& files, int pre_issue_depth)
{
    auto t1 = std::chrono::high_resolution_clock::now();
    foreactor::IOUring ring(pre_issue_depth);
    // build the intention
    std::vector<foreactor::SyscallNode *> syscalls;
    syscalls.reserve(FILES_PER_LEVEL + NUM_LEVELS - 1);
    // level-0 tables from latest to oldest
    for (int index = FILES_PER_LEVEL - 1; index >= 0; --index) {
        syscalls.push_back(
            new foreactor::SyscallPread(files[0][index], READ_BUF, FILE_SIZE, 0));
    }
    // levels 1 and beyond
    for (int level = 1; level < NUM_LEVELS; ++level) {
        int index = std::rand() % 8;    // simulate the calc of file in level
        syscalls.push_back(
            new foreactor::SyscallPread(files[level][index], READ_BUF, FILE_SIZE, 0));
    }
    foreactor::DepGraphEnter(syscalls, pre_issue_depth, ring);
    auto t2 = std::chrono::high_resolution_clock::now();

    // make the syscalls in order
    for (auto syscall : syscalls) {
        long ret = syscall->Issue();
        assert(ret == FILE_SIZE);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    foreactor::DepGraphLeave(syscalls);

    std::chrono::duration<double, std::milli> time_build_intention = t2 - t1;
    std::chrono::duration<double, std::milli> time_finish_syscalls = t3 - t2;
    std::cout << "foreactor (" << pre_issue_depth << ") - total time: "
        << time_build_intention.count() + time_finish_syscalls.count()
        << " ms" << std::endl;
    std::cout << "  build intention: " << time_build_intention.count()
        << " ms" << std::endl;
    std::cout << "  finish syscalls: " << time_finish_syscalls.count()
        << " ms" << std::endl;
}


int main(void)
{
    std::filesystem::current_path(DBDIR);

    std::vector<std::vector<int>> files;
    for (int level = 0; level < NUM_LEVELS; ++level) {
        files.push_back(std::vector<int>());
        for (int index = 0; index < FILES_PER_LEVEL; ++index) {
            int fd = open(table_name(level, index).c_str(), 0, O_RDONLY);
            assert(fd > 0);
            files.back().push_back(fd);
        }
    }

    drop_caches();
    do_get_serial(files);
    drop_caches();
    do_get_foreactor(files, 2);
    drop_caches();
    do_get_foreactor(files, 4);
    drop_caches();
    do_get_foreactor(files, 8);

    for (auto l : files)
        for (auto fd : l)
            close(fd);
}
