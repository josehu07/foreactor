#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <chrono>

#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

#include <foreactor.hpp>

namespace fa = foreactor;


static constexpr char DBDIR[] = "/tmp/demo_dbdir";
static constexpr int NUM_LEVELS = 4;
static constexpr int FILES_PER_LEVEL = 8;
static constexpr size_t FILE_SIZE = 4096;

static const std::string table_name(int level, int index) {
    std::stringstream ss;
    ss << level << "-" << index << ".ldb";
    return ss.str();
}


static void drop_caches(void) {
    system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
}


static char READ_BUF[FILE_SIZE];
static std::vector<char *> result_serial;
static std::vector<char *> result_foreactor;

static std::vector<std::vector<int>> open_selective(void) {
    std::vector<std::vector<int>> files;
    for (int level = 0; level < NUM_LEVELS; ++level) {
        files.push_back(std::vector<int>());
        for (int index = 0; index < FILES_PER_LEVEL; ++index) {
            if (level == 0 && index != FILES_PER_LEVEL / 2) {
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


void do_get_serial(bool check_result) {
    auto files = open_selective();

    // make the syscalls to mimic LevelDB::Get
    auto t1 = std::chrono::high_resolution_clock::now();
    // level-0 tables from latest to oldest, not hit
    for (int index = FILES_PER_LEVEL - 1; index >= 0; --index) {
        if (files[0][index] < 0)
            files[0][index] = open(table_name(0, index).c_str(), 0, O_RDONLY);
        ssize_t ret = pread(files[0][index], READ_BUF, FILE_SIZE, 0);
        if (check_result) {
            assert(ret == FILE_SIZE);
            result_serial.push_back(new char[FILE_SIZE]);
            memcpy(result_serial.back(), READ_BUF, FILE_SIZE);
        }
    }
    // key found at level-1
    for (int level = 1; level < 2; ++level) {
        int index = level % 8;    // simulate the calc of file in level
        if (files[level][index] < 0)
            files[level][index] = open(table_name(level, index).c_str(), 0, O_RDONLY);
        ssize_t ret = pread(files[level][index], READ_BUF, FILE_SIZE, 0);
        if (check_result) {
            assert(ret == FILE_SIZE);
            result_serial.push_back(new char[FILE_SIZE]);
            memcpy(result_serial.back(), READ_BUF, FILE_SIZE);
        }
    }
    auto t2 = std::chrono::high_resolution_clock::now();

    if (!check_result) {
        std::chrono::duration<double, std::milli> time_finish_syscalls = t2 - t1;
        std::cout << "Serial - total time: " << time_finish_syscalls.count() << " ms" << std::endl;
    }

    close_all(files);
}


void build_get_scgraph(fa::SCGraph *scgraph, std::vector<std::vector<int>>& files,
                       uint64_t (*GenNodeId)(int, int, int)) {
    fa::SyscallNode *last_node = nullptr;
    // level-0 tables from latest to oldest
    for (int index = FILES_PER_LEVEL - 1; index >= 0; --index) {
        int fd = files[0][index];
        if (fd < 0) {
            // need to open
            auto node_open = new fa::SyscallOpen(table_name(0, index), 0, O_RDONLY);
            auto node_pread = new fa::SyscallPread(-1, FILE_SIZE, 0,
                                                   std::vector{false, true, true});
            assert(scgraph->AddNode(GenNodeId(0, index, 0), node_open));
            assert(scgraph->AddNode(GenNodeId(0, index, 1), node_pread));
            if (last_node != nullptr)
                last_node->SetNext(node_open, /*weak_edge*/ true);
            node_open->SetNext(node_pread, /*weak_edge*/ false);
            last_node = node_pread;
        } else {
            // already open
            auto node_pread = new fa::SyscallPread(fd, FILE_SIZE, 0);
            assert(scgraph->AddNode(GenNodeId(0, index, 1), node_pread));
            if (last_node != nullptr)
                last_node->SetNext(node_pread, /*weak_edge*/ true);
            last_node = node_pread;
        }
    }
    // levels 1 and beyond
    for (int level = 1; level < NUM_LEVELS; ++level) {
        int index = level % 8;    // simulate the calc of file in level
        int fd = files[level][index];
        if (fd < 0) {
            // need to open
            auto node_open = new fa::SyscallOpen(table_name(level, index), 0, O_RDONLY);
            auto node_pread = new fa::SyscallPread(-1, FILE_SIZE, 0,
                                                   std::vector{false, true, true});
            assert(scgraph->AddNode(GenNodeId(level, index, 0), node_open));
            assert(scgraph->AddNode(GenNodeId(level, index, 1), node_pread));
            if (last_node != nullptr)
                last_node->SetNext(node_open, /*weak_edge*/ true);
            node_open->SetNext(node_pread, /*weak_edge*/ false);
            last_node = node_pread;
        } else {
            // already open
            auto node_pread = new fa::SyscallPread(fd, FILE_SIZE, 0);
            assert(scgraph->AddNode(GenNodeId(level, index, 1), node_pread));
            if (last_node != nullptr)
                last_node->SetNext(node_pread, /*weak_edge*/ true);
            last_node = node_pread;
        }
    }
}

void do_get_foreactor(int pre_issue_depth, fa::IOUring *ring, bool check_result) {
    auto files = open_selective();

    auto GenNodeId = [](int level, int index, int op) -> uint64_t {
        constexpr int fmax = NUM_LEVELS * FILES_PER_LEVEL;
        int fid = level * FILES_PER_LEVEL + index;
        return op * fmax + fid;
    };

    // build the syscall graph
    auto t1 = std::chrono::high_resolution_clock::now();    
    fa::SCGraph scgraph(ring, pre_issue_depth);
    build_get_scgraph(&scgraph, files, GenNodeId);

    // make the syscalls to mimic LevelDB::Get
    auto t2 = std::chrono::high_resolution_clock::now();
    // level-0 tables from latest to oldest, not hit
    for (int index = FILES_PER_LEVEL - 1; index >= 0; --index) {
        auto *node_pread = static_cast<fa::SyscallPread *>(
            scgraph.GetSyscallNode(GenNodeId(0, index, 1)));
        if (files[0][index] < 0) {
            auto *node_open = static_cast<fa::SyscallOpen *>(
                scgraph.GetSyscallNode(GenNodeId(0, index, 0)));
            files[0][index] = node_open->Issue();
            node_pread->SetArgFd(files[0][index]);
        }
        ssize_t ret = node_pread->Issue(READ_BUF);
        if (check_result) {
            assert(ret == FILE_SIZE);
            result_foreactor.push_back(new char[FILE_SIZE]);
            memcpy(result_foreactor.back(), READ_BUF, FILE_SIZE);
        }
    }
    // key found at level-1
    for (int level = 1; level < 2; ++level) {
        int index = level % 8;    // simulate the calc of file in level
        auto *node_pread = static_cast<fa::SyscallPread *>(
            scgraph.GetSyscallNode(GenNodeId(level, index, 1)));
        if (files[level][index] < 0) {
            auto *node_open = static_cast<fa::SyscallOpen *>(
                scgraph.GetSyscallNode(GenNodeId(level, index, 0)));
            files[level][index] = node_open->Issue();
            node_pread->SetArgFd(files[level][index]);
        }
        ssize_t ret = node_pread->Issue(READ_BUF);
        if (check_result) {
            assert(ret == FILE_SIZE);
            result_foreactor.push_back(new char[FILE_SIZE]);
            memcpy(result_foreactor.back(), READ_BUF, FILE_SIZE);
        }
    }
    auto t3 = std::chrono::high_resolution_clock::now();

    if (check_result) {
        std::cout << "  check result ";
        for (size_t i = 0; i < result_serial.size(); ++i) {
            if (i >= result_foreactor.size()) {
                std::cout << "FAILED, " << i << "-th read not found" << std::endl;
                return;
            }
            if (memcmp(result_serial[i], result_foreactor[i], FILE_SIZE) != 0) {
                std::cout << "FAILED, " << i << "-th read data mismatch" << std::endl;
                return;
            }
        }
        std::cout << "PASSED" << std::endl;
        for (char *data : result_foreactor)
            delete[] data;
        result_foreactor.clear();
    } else {
        std::chrono::duration<double, std::milli> time_build_intention = t2 - t1;
        std::chrono::duration<double, std::milli> time_finish_syscalls = t3 - t2;
        std::cout << "foreactor (" << pre_issue_depth << ") - total time: "
            << time_build_intention.count() + time_finish_syscalls.count() << " ms" << std::endl;
        std::cout << "  build scgraph:   " << time_build_intention.count() << " ms" << std::endl;
        std::cout << "  finish syscalls: " << time_finish_syscalls.count() << " ms" << std::endl;
    }

    close_all(files);
}


int main(void) {
    std::filesystem::current_path(DBDIR);

    fa::IOUring ring(16);

    std::cout << "Correctness check --" << std::endl;
    do_get_serial(true);
    do_get_foreactor(4, &ring, true);
    std::cout << std::endl;

    std::cout << "Performance timing --" << std::endl;
    drop_caches();
    do_get_serial(false);
    drop_caches();
    do_get_foreactor(2, &ring, false);
    drop_caches();
    do_get_foreactor(4, &ring, false);
    drop_caches();
    do_get_foreactor(8, &ring, false);
}
