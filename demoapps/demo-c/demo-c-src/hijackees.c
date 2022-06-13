#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "hijackees.h"


void exper_simple(void *args_) {
    ExperSimpleArgs *args = (ExperSimpleArgs *) args_;
    ssize_t ret __attribute__((unused));

    int fd = open(args->filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ret = pwrite(fd, args->wstr, args->wlen, 0);
    ret = pread(fd, args->rbuf0, args->rlen, 0);
    ret = pread(fd, args->rbuf1, args->rlen, args->rlen);
    close(fd);
}
