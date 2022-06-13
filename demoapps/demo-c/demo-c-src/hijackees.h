#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>


#pragma once


typedef void (*ExperFunc)(void *);


typedef struct ExperSimpleArgs {
    const char *filename;
    char *wstr;
    size_t wlen;
    size_t rlen;
    char *rbuf0;
    char *rbuf1;
} ExperSimpleArgs;

void exper_simple(void *args);
