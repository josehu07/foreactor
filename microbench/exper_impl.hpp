#include <vector>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>


#ifndef __EXPER_IMPL_H__
#define __EXPER_IMPL_H__


struct Req {
    int fd;
    off_t offset;
    size_t count;
    bool write;
    char *buf;

    Req(int fd, off_t offset, size_t count, bool write, char *buf)
        : fd(fd), offset(offset), count(count), write(write), buf(buf) {}
};


void run_exper_basic(std::vector<Req>& reqs,
                     size_t timing_rounds, size_t warmup_rounds);
void run_exper_thread_pool(std::vector<Req>& reqs, size_t num_threads,
                           size_t timing_rounds, size_t warmup_rounds);
void run_exper_io_uring(std::vector<Req>& reqs, size_t queue_len,
                        size_t timing_rounds, size_t warmup_rounds);


#endif
