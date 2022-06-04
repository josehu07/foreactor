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


struct ExperReadSeqArgs : ExperArgs {
    const int fd;
    std::vector<char *> rbufs;
    const size_t rlen;
    const unsigned nreads;
    struct io_uring *manual_ring;
    ThreadPool *manual_pool;

    ExperReadSeqArgs(int fd, size_t rlen, unsigned nreads)
        : fd(fd), rlen(rlen), nreads(nreads) {
        for (unsigned i = 0; i < nreads; ++i)
            rbufs.push_back(new char[rlen]);
    }
    ~ExperReadSeqArgs() {
        for (auto buf : rbufs)
            delete[] buf;
    }
};

void exper_read_seq(void *args);
void exper_read_seq_same_buffer(void *args);
void exper_read_seq_manual_ring(void *args);
void exper_read_seq_manual_pool(void *args);


struct ExperWriteSeqArgs : ExperArgs {
    const int fd;
    std::vector<std::string> wcontents;
    const size_t wlen;
    const unsigned nwrites;
    struct io_uring *manual_ring;
    ThreadPool *manual_pool;

    ExperWriteSeqArgs(int fd, std::string wcontent, unsigned nwrites)
        : fd(fd), wlen(wcontent.length()), nwrites(nwrites) {
        for (unsigned i = 0; i < nwrites; ++i)
            wcontents.push_back(wcontent);
    }
    ~ExperWriteSeqArgs() {}
};

void exper_write_seq(void *args);
void exper_write_seq_manual_ring(void *args);
void exper_write_seq_manual_pool(void *args);


#endif
