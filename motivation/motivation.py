import matplotlib
matplotlib.use('Agg')

import subprocess
import matplotlib.pyplot as plt
import numpy as np


NUM_FILES_LIST = list(range(8, 72, 8))
NUM_REPEATS = 25


def run_single(num_files):
    result = subprocess.run(["./motivation", str(num_files)],
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

def run_exprs(num_files_list):
    app_threads_unbounded_time, app_threads_bounded_time, io_uring_time = [], [], []

    for num_files in num_files_list:
        app_threads_unbounded_r, app_threads_bounded_r, io_uring_r = 0.0, 0.0, 0.0
        for i in range(NUM_REPEATS):
            app_threads_unbounded_t, app_threads_bounded_t, io_uring_t = run_single(num_files)
            app_threads_unbounded_r += app_threads_unbounded_t
            app_threads_bounded_r += app_threads_bounded_t
            io_uring_r += io_uring_t

        app_threads_unbounded_time.append(app_threads_unbounded_r / NUM_REPEATS)
        app_threads_bounded_time.append(app_threads_bounded_r / NUM_REPEATS)
        io_uring_time.append(io_uring_r / NUM_REPEATS)

    assert len(app_threads_unbounded_time) == len(app_threads_bounded_time)
    assert len(app_threads_unbounded_time) == len(io_uring_time)
    assert len(app_threads_unbounded_time) == len(num_files_list)
    return app_threads_unbounded_time, app_threads_bounded_time, io_uring_time


def plot_time(num_files_list, app_threads_unbounded_time, app_threads_bounded_time,
              io_uring_time):
    for i in range(len(num_files_list)):
        print(f'{num_files_list[i]:3d} {app_threads_unbounded_time[i]:8.3f}' \
              f' {app_threads_bounded_time[i]:8.3f} {io_uring_time[i]:8.3f}')

    xs = np.arange(len(num_files_list))
    width = 0.25

    plt.bar(xs-width, app_threads_unbounded_time, width,
            zorder=3, label="User-level threading (unbounded)")
    plt.bar(xs, app_threads_bounded_time, width,
            zorder=3, label="User-level threading (pool size = 8)")
    plt.bar(xs+width, io_uring_time, width,
            zorder=3, label="Using io_uring (kernel wq size = 8)")

    plt.ylabel("Time (us)")
    plt.xticks(xs, num_files_list)
    plt.xlabel("Number of concurrent preads")

    plt.grid(zorder=0, axis='y')

    plt.legend()

    plt.savefig("motivation.png", dpi=120)


def main():
    app_threads_unbounded_time, app_threads_bounded_time, io_uring_time = run_exprs(NUM_FILES_LIST)
    plot_time(NUM_FILES_LIST, app_threads_unbounded_time, app_threads_bounded_time, io_uring_time)

if __name__ == "__main__":
    main()
