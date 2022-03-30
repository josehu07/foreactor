#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import subprocess
import matplotlib.pyplot as plt
import numpy as np


NUM_PREADS_LIST = list(range(8, 44, 4))
NUM_REPEATS = 100


def make_files():
    subprocess.run(["./motivation", "make"], check=True)


def run_single(num_preads):
    result = subprocess.run(["./motivation", "run", str(num_preads)],
                            check=True, capture_output=True)
    output = result.stdout.decode('ascii')

    app_threads_unbounded_t, app_threads_bounded_t, io_uring_t = 0.0, 0.0, 0.0
    for line in output.strip().split('\n'):
        line = line.strip()
        if line.startswith("app threads unbounded"):
            app_threads_unbounded_t = float(line.split()[-2])
        elif line.startswith("app threads bounded (8)"):
            app_threads_bounded_t = float(line.split()[-2])
        elif line.startswith("io_uring"):
            io_uring_t = float(line.split()[-2])

    return app_threads_unbounded_t, app_threads_bounded_t, io_uring_t


def run_exprs(num_preads_list):
    app_threads_unbounded_time, app_threads_bounded_time, io_uring_time = [], [], []

    for num_preads in num_preads_list:
        app_threads_unbounded_l, app_threads_bounded_l, io_uring_l = [], [], []
        for i in range(NUM_REPEATS):
            app_threads_unbounded_t, app_threads_bounded_t, io_uring_t = run_single(num_preads)
            app_threads_unbounded_l.append(app_threads_unbounded_t)
            app_threads_bounded_l.append(app_threads_bounded_t)
            io_uring_l.append(io_uring_t)
        app_threads_unbounded_r = sum(sorted(app_threads_unbounded_l)[5:-5]) / (NUM_REPEATS - 10)
        app_threads_bounded_r = sum(sorted(app_threads_bounded_l)[5:-5]) / (NUM_REPEATS - 10)
        io_uring_r = sum(sorted(io_uring_l)[5:-5]) / (NUM_REPEATS - 10)

        app_threads_unbounded_time.append(app_threads_unbounded_r)
        app_threads_bounded_time.append(app_threads_bounded_r)
        io_uring_time.append(io_uring_r)

    assert len(app_threads_unbounded_time) == len(app_threads_bounded_time)
    assert len(app_threads_unbounded_time) == len(io_uring_time)
    assert len(app_threads_unbounded_time) == len(num_preads_list)
    return app_threads_unbounded_time, app_threads_bounded_time, io_uring_time


def plot_time(num_preads_list, app_threads_unbounded_time, app_threads_bounded_time,
              io_uring_time):
    for i in range(len(num_preads_list)):
        print(f'{num_preads_list[i]:3d} {app_threads_unbounded_time[i]:8.3f}' \
              f' {app_threads_bounded_time[i]:8.3f} {io_uring_time[i]:8.3f}')

    xs = np.arange(len(num_preads_list))

    # width = 0.25
    # plt.bar(xs-width, app_threads_unbounded_time, width,
    #         zorder=3, label="User-level threading (unbounded)")
    # plt.bar(xs, app_threads_bounded_time, width,
    #         zorder=3, label="User-level threading (pool size = 8)")
    # plt.bar(xs+width, io_uring_time, width,
    #         zorder=3, label="Using io_uring (kernel wq size = 8)")
    
    plt.plot(xs, app_threads_unbounded_time,
             marker='^', zorder=3, label="User-level threading (unbounded)")
    plt.plot(xs, app_threads_bounded_time,
             marker='o', zorder=3, label="User-level threading (pool size = 8)")
    plt.plot(xs, io_uring_time,
             marker='x', zorder=3, label="Using io_uring (kernel wq size = 8)")

    plt.ylabel("Completion Time (us)")
    plt.xticks(xs, num_preads_list)
    plt.xlabel("Number of pread() Requests")

    plt.grid(zorder=0, axis='y')

    plt.legend()

    plt.savefig("motivation.png", dpi=120)


def main():
    make_files()
    app_threads_unbounded_time, app_threads_bounded_time, io_uring_time = run_exprs(NUM_PREADS_LIST)
    plot_time(NUM_PREADS_LIST, app_threads_unbounded_time, app_threads_bounded_time, io_uring_time)

if __name__ == "__main__":
    main()
