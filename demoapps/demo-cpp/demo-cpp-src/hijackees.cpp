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
    [[maybe_unused]] ssize_t ret =
        pwrite(fd, args->wcontent.c_str(), args->wlen, 0);
    assert(ret == static_cast<ssize_t>(args->wlen));
    ret = pread(fd, args->rbuf0, args->rlen, 0);
    assert(ret == static_cast<ssize_t>(args->rlen));
    ret = pread(fd, args->rbuf1, args->rlen, args->rlen);
    assert(ret == static_cast<ssize_t>(args->rlen));
    close(fd);
}
