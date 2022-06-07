#include <vector>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <liburing.h>

#include "hijackees.hpp"
#include "thread_pool.hpp"


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


void exper_weak_edge(void *args_) {
    ExperWeakEdgeArgs *args = reinterpret_cast<ExperWeakEdgeArgs *>(args_);
    [[maybe_unused]] ssize_t ret;

    for (unsigned i = 0; i < args->nrepeats; ++i) {
        ret = pwrite(args->fd, args->wcontents[i].c_str(), args->len, 0);
        ret = pread(args->fd, args->rbuf0, args->len, 0);
        if (i == 3)     // let's pretend this is some runtime-dependent condition,
            return;     // making the edge to next node a weak edge
        ret = pread(args->fd, args->rbuf1, args->len, 0);
    }
}


void exper_crossing(void *args_) {
    ExperCrossingArgs *args = reinterpret_cast<ExperCrossingArgs *>(args_);
    [[maybe_unused]] ssize_t ret;

    for (unsigned i = 0; i < args->nblocks; ++i) {
        ret = pread(args->fd, args->rbuf, args->len, i * args->len);
        for (unsigned j = 0; j < args->len; ++j)
            args->rbuf[j] = '7';
        ret = pwrite(args->fd, args->rbuf, args->len, i * args->len);
    }
}


void exper_read_seq(void *args_) {
    ExperReadSeqArgs *args = reinterpret_cast<ExperReadSeqArgs *>(args_);
    [[maybe_unused]] ssize_t ret;

    for (unsigned i = 0; i < args->nreads; ++i) {
        int fd = args->multi_file ? args->fds[i] : args->fds[0];
        char *buf = args->same_buffer ? args->rbufs[0] : args->rbufs[i];
        off_t offset = args->multi_file ? 0 : i * args->rlen;
        ret = pread(fd, buf, args->rlen, offset);
    }
}

void exper_read_seq_manual_ring(void *args_) {
    ExperReadSeqArgs *args = reinterpret_cast<ExperReadSeqArgs *>(args_);
    assert(args->manual_ring != nullptr);

    for (unsigned i = 0; i < args->nreads; ++i) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(args->manual_ring);
        assert(sqe != nullptr);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(i));

        int fd = args->multi_file ? args->fds[i] : args->fds[0];
        char *buf = args->same_buffer ? args->rbufs[0] : args->rbufs[i];
        off_t offset = args->multi_file ? 0 : i * args->rlen;
        io_uring_prep_read(sqe, fd, buf, args->rlen, offset);
        
        sqe->flags |= IOSQE_ASYNC;
    }

    io_uring_submit(args->manual_ring);

    for (unsigned i = 0; i < args->nreads; ++i) {
        struct io_uring_cqe *cqe;
        [[maybe_unused]] int ret = io_uring_wait_cqe(args->manual_ring, &cqe);
        assert(ret == 0);
        io_uring_cqe_seen(args->manual_ring, cqe);
    }
}

void exper_read_seq_manual_pool(void *args_) {
    ExperReadSeqArgs *args = reinterpret_cast<ExperReadSeqArgs *>(args_);
    assert(args->manual_pool != nullptr);

    std::vector<ThreadPoolSQEntry> entries;
    entries.reserve(args->nreads);

    for (unsigned i = 1; i < args->nreads; ++i) {
        int fd = args->multi_file ? args->fds[i] : args->fds[0];
        char *buf = args->same_buffer ? args->rbufs[0] : args->rbufs[i];
        off_t offset = args->multi_file ? 0 : i * args->rlen;
        entries.emplace_back(ThreadPoolSQEntry{
            .entry_id = i,
            .sc_type = SC_PREAD,
            .fd = fd,
            .offset = offset,
            .buf = reinterpret_cast<uint64_t>(buf),
            .rw_len = args->rlen
        });
    }

    args->manual_pool->SubmitBulk(entries);

    [[maybe_unused]] ssize_t ret = pread(args->fds[0], args->rbufs[0], args->rlen, 0);

    for (unsigned i = 1; i < args->nreads; ++i)
        args->manual_pool->CompleteOne();
}


void exper_write_seq(void *args_) {
    ExperWriteSeqArgs *args = reinterpret_cast<ExperWriteSeqArgs *>(args_);
    [[maybe_unused]] ssize_t ret;

    for (unsigned i = 0; i < args->nwrites; ++i) {
        int fd = args->multi_file ? args->fds[i] : args->fds[0];
        off_t offset = args->multi_file ? 0 : i * args->wlen;
        ret = pwrite(fd, args->wbufs[i], args->wlen, offset);
    }
}

void exper_write_seq_manual_ring(void *args_) {
    ExperWriteSeqArgs *args = reinterpret_cast<ExperWriteSeqArgs *>(args_);
    assert(args->manual_ring != nullptr);

    for (unsigned i = 0; i < args->nwrites; ++i) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(args->manual_ring);
        assert(sqe != nullptr);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(i));

        int fd = args->multi_file ? args->fds[i] : args->fds[0];
        off_t offset = args->multi_file ? 0 : i * args->wlen;
        io_uring_prep_write(sqe, fd, args->wbufs[i], args->wlen, offset);
        
        sqe->flags |= IOSQE_ASYNC;
    }

    io_uring_submit(args->manual_ring);

    for (unsigned i = 0; i < args->nwrites; ++i) {
        struct io_uring_cqe *cqe;
        [[maybe_unused]] int ret = io_uring_wait_cqe(args->manual_ring, &cqe);
        assert(ret == 0);
        io_uring_cqe_seen(args->manual_ring, cqe);
    }
}

void exper_write_seq_manual_pool(void *args_) {
    ExperWriteSeqArgs *args = reinterpret_cast<ExperWriteSeqArgs *>(args_);
    assert(args->manual_pool != nullptr);

    std::vector<ThreadPoolSQEntry> entries;
    entries.reserve(args->nwrites);

    for (unsigned i = 0; i < args->nwrites; ++i) {
        int fd = args->multi_file ? args->fds[i] : args->fds[0];
        off_t offset = args->multi_file ? 0 : i * args->wlen;
        entries.emplace_back(ThreadPoolSQEntry{
            .entry_id = i,
            .sc_type = SC_PWRITE,
            .fd = fd,
            .offset = offset,
            .buf = reinterpret_cast<uint64_t>(args->wbufs[i]),
            .rw_len = args->wlen
        });
    }

    args->manual_pool->SubmitBulk(entries);

    for (unsigned i = 0; i < args->nwrites; ++i)
        args->manual_pool->CompleteOne();
}
