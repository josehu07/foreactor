#include <functional>
#include <vector>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <liburing.h>

#include "thread_pool.hpp"


#ifndef __DEMO_HIJACKEES_H__
#define __DEMO_HIJACKEES_H__


struct ExperArgs {};

typedef std::function<void(void *)> ExperFunc;


struct ExperSimpleArgs : ExperArgs {
    const std::string filename;
    const std::string wcontent;
    const size_t wlen;
    const size_t rlen;
    char * const rbuf0;
    char * const rbuf1;

    ExperSimpleArgs(std::string filename, std::string wcontent)
        : filename(filename), wcontent(wcontent), wlen(wcontent.length()),
          rlen(wlen / 2), rbuf0(new char[rlen]), rbuf1(new char[rlen]) {
        assert(rbuf0 != nullptr);
        assert(rbuf1 != nullptr);
    }
    ~ExperSimpleArgs() {
        delete[] rbuf0;
        delete[] rbuf1;
    }
};

void exper_simple(void *args);


struct ExperBranchingArgs : ExperArgs {
    const int fd;
    const std::string filename;
    const std::string wcontent0;
    const std::string wcontent1;
    const std::string wcontent2;
    const size_t wlen;
    const size_t rlen;
    char * const rbuf0;
    char * const rbuf1;
    const bool read;
    const bool readtwice;
    const bool moveoff;

    ExperBranchingArgs(std::string filename, std::string wcontent0,
                       std::string wcontent1, std::string wcontent2)
        : fd(-1), filename(filename), wcontent0(wcontent0),
          wcontent1(wcontent1), wcontent2(wcontent2),
          wlen(wcontent0.length()), rlen(wlen / 2),
          rbuf0(new char[rlen]), rbuf1(new char[rlen]), read(true),
          readtwice(true), moveoff(true) {
        assert(rbuf0 != nullptr);
        assert(rbuf1 != nullptr);
    }
    ~ExperBranchingArgs() {
        delete[] rbuf0;
        delete[] rbuf1;
    }
};

void exper_branching(void *args);


struct ExperLoopingArgs : ExperArgs {
    const std::string filename;
    const std::string wcontent;
    const size_t wlen;
    const size_t rlen;
    const unsigned nwrites;
    const unsigned nreadsd2;
    const unsigned nrepeats;
    std::vector<char *> rbufs;

    ExperLoopingArgs(std::string filename, std::string wcontent,
                     unsigned nwrites, unsigned nreadsd2, unsigned nrepeats)
        : filename(filename), wcontent(wcontent), wlen(wcontent.length()),
          rlen((nwrites * wlen) / (nreadsd2 * 2)), nwrites(nwrites),
          nreadsd2(nreadsd2), nrepeats(nrepeats) {
        for (unsigned i = 0; i < 2 * nreadsd2; ++i)
            rbufs.push_back(new char[rlen]);
    }
    ~ExperLoopingArgs() {
        for (auto buf : rbufs)
            delete[] buf;
    }
};

void exper_looping(void *args);


struct ExperWeakEdgeArgs : ExperArgs {
    const int fd;
    const std::vector<std::string> wcontents;
    const size_t len;
    char * const rbuf0;
    char * const rbuf1;
    const unsigned nrepeats;

    ExperWeakEdgeArgs(int fd, std::vector<std::string> wcontents)
        : fd(fd), wcontents(wcontents), len(wcontents[0].length()),
          rbuf0(new char[len]), rbuf1(new char[len]),
          nrepeats(wcontents.size()) {
        assert(rbuf0 != nullptr);
        assert(rbuf1 != nullptr);
    }
    ~ExperWeakEdgeArgs() {
        delete[] rbuf0;
        delete[] rbuf1;
    }
};

void exper_weak_edge(void *args);


struct ExperCrossingArgs : ExperArgs {
    const int fd;
    const size_t len;
    char * const rbuf;
    const unsigned nblocks;

    ExperCrossingArgs(int fd, size_t len, unsigned nblocks)
        : fd(fd), len(len), rbuf(new char[len]), nblocks(nblocks) {
        assert(rbuf != nullptr);
    }
    ~ExperCrossingArgs() {
        delete[] rbuf;
    }
};

void exper_crossing(void *args);


struct ExperReadSeqArgs : ExperArgs {
    const std::vector<int> fds;
    std::vector<char *> rbufs;
    const size_t rlen;
    const unsigned nreads;
    const bool same_buffer;
    const bool multi_file;
    struct io_uring *manual_ring;
    ThreadPool *manual_pool;

    ExperReadSeqArgs(std::vector<int> fds, size_t rlen, unsigned nreads,
                     bool same_buffer, bool multi_file)
        : fds(fds), rlen(rlen), nreads(nreads), same_buffer(same_buffer),
          multi_file(multi_file) {
        for (unsigned i = 0; i < nreads; ++i)
            rbufs.push_back(new (std::align_val_t(512)) char[rlen]);
    }
    ~ExperReadSeqArgs() {
        for (auto buf : rbufs)
            delete[] buf;
    }
};

void exper_read_seq(void *args);
void exper_read_seq_manual_ring(void *args);
void exper_read_seq_manual_pool(void *args);


struct ExperWriteSeqArgs : ExperArgs {
    const std::vector<int> fds;
    std::vector<char *> wbufs;
    const size_t wlen;
    const unsigned nwrites;
    const bool multi_file;
    struct io_uring *manual_ring;
    ThreadPool *manual_pool;

    ExperWriteSeqArgs(std::vector<int> fds, std::string wcontent,
                      unsigned nwrites, bool multi_file)
        : fds(fds), wlen(wcontent.length()), nwrites(nwrites),
          multi_file(multi_file) {
        for (unsigned i = 0; i < nwrites; ++i) {
            wbufs.push_back(new (std::align_val_t(512)) char[wlen]);
            memcpy(wbufs[i], wcontent.c_str(), wcontent.length());
        }
    }
    ~ExperWriteSeqArgs() {
        for (auto buf : wbufs)
            delete[] buf;
    }
};

void exper_write_seq(void *args);
void exper_write_seq_manual_ring(void *args);
void exper_write_seq_manual_pool(void *args);


struct ExperStreamingArgs : ExperArgs {
    const int fd_in;
    const int fd_out;
    const size_t block_size;
    const unsigned num_blocks;
    std::vector<char *> bufs;
    const bool single_buf;

    ExperStreamingArgs(int fd_in, int fd_out, size_t block_size,
                       unsigned num_blocks, bool single_buf)
        : fd_in(fd_in), fd_out(fd_out), block_size(block_size),
          num_blocks(num_blocks), single_buf(single_buf) {
        for (unsigned i = 0; i < num_blocks; ++i)
            bufs.push_back(new (std::align_val_t(512)) char[block_size]);
    }
    ~ExperStreamingArgs() {
        for (auto buf : bufs)
            delete[] buf;
    }
};

void exper_streaming(void *args);


#endif
