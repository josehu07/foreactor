#include <vector>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "hijackees.hpp"


void exper_simple(void *args_) {
    ExperSimpleArgs *args = reinterpret_cast<ExperSimpleArgs *>(args_);
    [[maybe_unused]] ssize_t ret;
    
    int fd = open(args->filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ret = pwrite(fd, args->wcontent.c_str(), args->wlen, 0);
    ret = pread(fd, args->rbuf0, args->rlen, 0);
    ret = pread(fd, args->rbuf1, args->rlen, args->rlen);
    close(fd);
}


void exper_branching(void *args_) {
    ExperBranchingArgs *args = reinterpret_cast<ExperBranchingArgs *>(args_);
    [[maybe_unused]] ssize_t ret;

    int fd = args->fd;
    if (args->fd < 0) {
        fd = open(args->filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        ret = pwrite(fd, args->wcontent0.c_str(), args->wlen, 0);
    } else {
        ret = pwrite(fd, args->wcontent1.c_str(), args->wlen, 0);
    }

    ret = pwrite(fd, args->wcontent2.c_str(), args->wlen, 0);

    ssize_t useful_ret = 0;
    if (args->read && !args->readtwice) {
        ret = pread(fd, args->rbuf0, args->rlen, 0);
    } else if (args->read && args->readtwice) {
        if (!args->moveoff) {
            ret = pread(fd, args->rbuf0, args->rlen, 0);
            ret = pread(fd, args->rbuf1, args->rlen, 0);
        } else {
            ret = pread(fd, args->rbuf0, args->rlen, 0);
            useful_ret = pread(fd, args->rbuf1, args->rlen, args->rlen);
        }
    }

    if (useful_ret != static_cast<ssize_t>(args->rlen))
        ret = pwrite(fd, args->wcontent1.c_str(), args->wlen, 0);

    close(fd);
}


void exper_looping(void *args_) {
    ExperLoopingArgs *args = reinterpret_cast<ExperLoopingArgs *>(args_);
    [[maybe_unused]] ssize_t ret;

    int fd = open(args->filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    for (unsigned i = 0; i < args->nrepeats; ++i) {
        for (unsigned j = 0; j < args->nwrites; ++j)
            ret = pwrite(fd, args->wcontent.c_str(), args->wlen, j * args->wlen);

        for (unsigned j = 0; j < args->nreadsd2; ++j) {
            ret = pread(fd, args->rbufs[j*2], args->rlen, (j*2) * args->rlen);
            ret = pread(fd, args->rbufs[j*2+1], args->rlen, (j*2+1) * args->rlen);
        }
    }

    close(fd);
}


void exper_read_seq(void *args_) {
    ExperReadSeqArgs *args = reinterpret_cast<ExperReadSeqArgs *>(args_);
    [[maybe_unused]] ssize_t ret;

    for (unsigned i = 0; i < args->nreads; ++i)
        ret = pread(args->fd, args->rbuf, args->rlen, i * args->rlen);
}


void exper_write_seq(void *args_) {
    ExperWriteSeqArgs *args = reinterpret_cast<ExperWriteSeqArgs *>(args_);
    [[maybe_unused]] ssize_t ret;

    for (unsigned i = 0; i < args->nwrites; ++i)
        ret = pwrite(args->fd, args->wcontent.c_str(), args->wlen, i * args->wlen);
}
