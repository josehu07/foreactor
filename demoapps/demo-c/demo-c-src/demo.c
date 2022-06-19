#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "hijackees.h"


static char *rand_string(size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    char *str = malloc(sizeof(char) * (length + 1));
    assert(str != NULL);
    for (size_t i = 0; i < length; ++i)
        str[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    str[length] = '\0';
    return str;
}

static void cmd_drop_caches(void) {
    int rc __attribute__((unused)) =
        system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    assert(rc == 0);
}

static void print_usage_exit(const char *self);


void run_iters(ExperFunc exper_func, void *exper_args, unsigned num_iters,
               bool drop_caches, bool print_time) {
    double elapsed_us[num_iters];
    for (unsigned i = 0; i < num_iters; ++i) {
        if (drop_caches)    // drain page cache before each run?
            cmd_drop_caches();

        struct timespec start_ts, pause_ts;
        clock_gettime(CLOCK_MONOTONIC, &start_ts);
        exper_func(exper_args);
        clock_gettime(CLOCK_MONOTONIC, &pause_ts);

        uint64_t nsecs = (pause_ts.tv_sec - start_ts.tv_sec) * 1e9
                         + (pause_ts.tv_nsec - start_ts.tv_nsec);
        elapsed_us[i] = (double) nsecs / 1000.;
    }

    if (print_time) {
        double sum_us = 0.;
        int skip = 3, cnt = 0;
        int show = num_iters > 3 ? num_iters - 3 : 0;

        fprintf(stderr, "  Time elapsed: [ ");
        if (show > 0)
            fprintf(stderr, "... ");

        for (unsigned i = 0; i < num_iters; ++i) {
            double us = elapsed_us[i];
            if (show <= 0)
                fprintf(stderr, "%.3lf ", us);
            else
                show--;
            if (skip <= 0) {
                sum_us += us;
                cnt++;
            } else
                skip--;
        }
        fprintf(stderr, "] ");

        if (cnt > 0)
            fprintf(stderr, "avg %.3lf ", sum_us / cnt);
        fprintf(stderr, "us\n");
    }
}

void run_exper(const char *self, const char *dbdir, const char *exper,
               unsigned num_iters, bool drop_caches, bool dump_result) {
    int ret __attribute__((unused)) = chdir(dbdir);
    assert(ret == 0);

    if (strcmp(exper, "simple") == 0) {
        ExperSimpleArgs args = {
            .filename = "simple.dat",
            .wstr = rand_string(8192),
            .wlen = 8192,
            .rlen = 4096,
            .rbuf0 = malloc(sizeof(char) * 4096),
            .rbuf1 = malloc(sizeof(char) * 4096)
        };

        run_iters(exper_simple, &args, num_iters, drop_caches, !dump_result);

        if (dump_result)
            printf("%s\n%s\n", args.rbuf0, args.rbuf1);

        free(args.wstr);
        free(args.rbuf0);
        free(args.rbuf1);
    }
}


static void print_usage_exit(const char *self) {
    fprintf(stderr, "Usage: %s EXPER_NAME DBDIR_PATH NUM_ITERS "
                    "[--drop_caches] [--dump_result]\n", self);
    exit(1);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if ((argc < 4) || (argc > 6))
        print_usage_exit(argv[0]);

    const char *exper = argv[1];
    const char *dbdir = argv[2];
    unsigned num_iters = strtoul(argv[3], NULL, 10);

    bool drop_caches = false;
    bool dump_result = false;
    for (int i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--dump_result") == 0) {
            dump_result = true;
            srand(1234567);     // use fixed seed for result comparison
        } else if (strcmp(argv[i], "--drop_caches") == 0) {
            drop_caches = true;
        } else {
            fprintf(stderr, "Error: unrecognized option %s\n", argv[i]);
            print_usage_exit(argv[0]);
        }
    }

    run_exper(argv[0], dbdir, exper, num_iters, drop_caches, dump_result);
    return 0;
}
