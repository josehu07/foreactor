#include <vector>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "hijackees.hpp"


void exper_simple(void *args_) {
    ExperSimpleArgs *args = reinterpret_cast<ExperSimpleArgs *>(args_);
    
    int fd = open(args->filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    pwrite(fd, args->wcontent.c_str(), args->wlen, 0);
    pread(fd, args->rbuf0, args->rlen, 0);
    pread(fd, args->rbuf1, args->rlen, args->rlen);
    close(fd);
}


void exper_branching(void *args_) {
    ExperBranchingArgs *args = reinterpret_cast<ExperBranchingArgs *>(args_);

    int fd = args->fd;
    if (args->fd < 0) {
        fd = open(args->filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        pwrite(fd, args->wcontent0.c_str(), args->wlen, 0);
    } else {
        pwrite(fd, args->wcontent1.c_str(), args->wlen, 0);
    }

    pwrite(fd, args->wcontent2.c_str(), args->wlen, 0);

    ssize_t ret = 0;
    if (args->read && !args->readtwice) {
        pread(fd, args->rbuf0, args->rlen, 0);
    } else if (args->read && args->readtwice) {
        if (!args->moveoff) {
            pread(fd, args->rbuf0, args->rlen, 0);
            pread(fd, args->rbuf1, args->rlen, 0);
        } else {
            pread(fd, args->rbuf0, args->rlen, 0);
            ret = pread(fd, args->rbuf1, args->rlen, args->rlen);
        }
    }

    if (ret != static_cast<ssize_t>(args->rlen)) {
        ret = pwrite(fd, args->wcontent1.c_str(), args->wlen, 0);
        assert(ret == static_cast<ssize_t>(args->wlen));
    }

    close(fd);
}


void exper_looping(void *args_) {
    ExperLoopingArgs *args = reinterpret_cast<ExperLoopingArgs *>(args_);

    int fd = open(args->filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    for (unsigned i = 0; i < args->nrepeats; ++i) {
        for (unsigned j = 0; j < args->nwrites; ++j)
            pwrite(fd, args->wcontent.c_str(), args->wlen, j * args->wlen);

        for (unsigned j = 0; j < args->nreadsd2; ++j) {
            pread(fd, args->rbufs[j*2], args->rlen, (j*2) * args->rlen);
            pread(fd, args->rbufs[j*2+1], args->rlen, (j*2+1) * args->rlen);
        }
    }

    close(fd);
}
