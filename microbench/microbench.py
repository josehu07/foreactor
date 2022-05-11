#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import subprocess
import matplotlib.pyplot as plt
import numpy as np


NUM_FILES = 64
FILE_SIZE = 128 * 1024 * 1024
FILE_SIZE_WHEN_LIMIT = 16 * 1024 * 1024
NUM_REQS_LIST = list(range(4, 68, 4))
REQ_SIZE_LIST = [(4**i) * 1024 for i in range(1, 7, 2)]
REQ_SIZE_LIST_WHEN_LIMIT = [(4**i) * 1024 for i in range(1, 5, 2)]
ASYNC_MODES = ["sync", "thread_pool_unbounded", "thread_pool_nproc", "thread_pool_4xsocks",
               "io_uring_default", "io_uring_sqeasync", "io_uring_sqpoll", "io_uring_sqeasync_sqpoll"]
LINE_STYLES = [':', '--', '--', '--',
               '-', '-', '-', '-']


def get_nproc():
    cmd = ["sudo", "nproc"]
    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return int(output)

NPROC = get_nproc()
CGROUP_NAME = "microbench_group"

def calculate_mem_limit(mem_percentage, num_reqs, req_size):
    mem_limit = int(num_reqs * FILE_SIZE_WHEN_LIMIT * (mem_percentage / 100.))
    mem_limit += num_reqs * req_size    # space for user buffers
    return mem_limit

def set_cgroup_mem_limit(mem_limit):
    cmd = ["sudo", "lscgroup"]
    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    cgroup_found = False
    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("memory:") and CGROUP_NAME in line:
            cgroup_found = True
            break

    if not cgroup_found:    # create cgroup
        cmd = ["sudo", "cgcreate", "-g", "memory:"+CGROUP_NAME]
        subprocess.run(cmd, check=True)

    cmd = ["sudo", "cgset", "-r", "memory.limit_in_bytes="+str(mem_limit),
           CGROUP_NAME]
    subprocess.run(cmd, check=True)


def make_files(dir_path):
    cmd = ["sudo", "./microbench",
           "-d", dir_path,
           "-f", str(NUM_FILES),
           "-s", str(FILE_SIZE),
           "--make"]
    subprocess.run(cmd, check=True)


def run_single(dir_path, num_reqs, req_size, rw_mode, file_src, page_cache,
               async_mode, timing_rounds, warmup_rounds):
    cmd = ["sudo", "./microbench",
           "-d", dir_path]
    if file_src == "single":
        cmd += ["-f", "1"]
    else:
        cmd += ["-f", str(NUM_FILES)]
    cmd += ["-n", str(num_reqs),
            "-r", str(req_size)]

    if rw_mode == "write":
        cmd += ["--write"]

    if page_cache == "direct":
        cmd += ["--direct",
                "-s", str(FILE_SIZE)]
    elif page_cache != "unlimited":
        if file_src == "single":
            raise ValueError("single file mem limit mode not supported")
        mem_percentage = int(page_cache)
        mem_limit = calculate_mem_limit(mem_percentage, num_reqs, req_size)
        set_cgroup_mem_limit(mem_limit)
        cmd = ["sudo", "cgexec", "-g", "memory:"+CGROUP_NAME] + cmd
        cmd += ["--shuffle_offset",
                "-s", str(FILE_SIZE_WHEN_LIMIT)]
    else:
        cmd += ["-s", str(FILE_SIZE)]

    if async_mode == "sync":
        cmd += ["-a", "basic"]
    elif async_mode == "thread_pool_unbounded":
        cmd += ["-a", "thread_pool",
                "-t", str(num_reqs)]
    elif async_mode == "thread_pool_nproc":
        num_threads = num_reqs if num_reqs < NPROC else NPROC
        cmd += ["-a", "thread_pool",
                "-t", str(num_threads)]
    elif async_mode == "thread_pool_4xsocks":
        num_threads = num_reqs if num_reqs < 8 else 8   # TODO: fetch this number
        cmd += ["-a", "thread_pool",
                "-t", str(num_threads)]
    elif async_mode == "io_uring_default":
        cmd += ["-a", "io_uring"]
    elif async_mode == "io_uring_sqeasync":
        cmd += ["-a", "io_uring",
                "--iosqe_async"]
    elif async_mode == "io_uring_sqpoll":
        cmd += ["-a", "io_uring",
                "--fixed_file", "--sq_poll"]
    elif async_mode == "io_uring_sqeasync_sqpoll":
        cmd += ["-a", "io_uring",
                "--iosqe_async", "--fixed_file", "--sq_poll"]
    else:
        raise ValueError("unrecognized async_mode " + async_mode)

    if async_mode.startswith("io_uring") and page_cache == "direct":
        cmd += ["--fixed_buf"]

    cmd += ["--tr", str(timing_rounds),
            "--wr", str(warmup_rounds)]

    print(cmd)
    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    print(output)

    avg_us, stddev = 0., 0.
    for line in output.strip().split('\n'):
        line = line.strip()
        if line.startswith("avg_us"):
            avg_us = float(line.split()[1])
        elif line.startswith("stddev"):
            stddev = float(line.split()[1])
            break

    return avg_us, stddev


def plot_results(req_size, avg_us_l, stddev_l, output_prefix):
    for i in range(len(ASYNC_MODES)):
        xs = NUM_REQS_LIST
        ys = avg_us_l[i]

        plt.plot(xs, ys,
                 zorder=3, label=ASYNC_MODES[i], linestyle=LINE_STYLES[i])

    plt.xlabel("Length of Syscall Sequence")
    plt.xticks(NUM_REQS_LIST, NUM_REQS_LIST)
    plt.ylabel("Completion Time (us)")

    plt.grid(zorder=0, axis='y')

    plt.legend()

    plt.savefig(f"{output_prefix}-{req_size}.png", dpi=120)
    plt.close()


def run_expers(dir_path, rw_mode, file_src, page_cache,
               timing_rounds, warmup_rounds, output_prefix):
    req_size_list = REQ_SIZE_LIST
    if page_cache != "unlimited" and page_cache != "direct":
        req_size_list = REQ_SIZE_LIST_WHEN_LIMIT
        
    for req_size in req_size_list:
        avg_us_l, stddev_l = [], []

        for async_mode in ASYNC_MODES:
            avg_us_l.append([])
            stddev_l.append([])

            for num_reqs in NUM_REQS_LIST:
                print(f"Exper: req_size {req_size}  #reqs {num_reqs}  mode {async_mode}")
                avg_us, stddev = run_single(dir_path, num_reqs, req_size, rw_mode,
                                            file_src, page_cache, async_mode,
                                            timing_rounds, warmup_rounds)
                avg_us_l[-1].append(avg_us)
                stddev_l[-1].append(stddev)

        plot_results(req_size, avg_us_l, stddev_l, output_prefix)


def main():
    parser = argparse.ArgumentParser(description="Small-scale async I/O microbenchmark")
    parser.add_argument('-d', dest='dir_path', required=True,
                        help="path to data directory")
    parser.add_argument('--make', dest='make_files', default=False, action='store_true',
                        help="if given, prepare the data files")
    parser.add_argument('--rdwr', dest='rw_mode', type=str, default="read",
                        help="read/write mode: read|write")
    parser.add_argument('--file', dest='file_src', type=str, default="different",
                        help="file source mode: single|different")
    parser.add_argument('--mem', dest='page_cache', type=str, default="unlimited",
                        help="page cache mode: direct|unlimited|<memlimit_percentage>")
    parser.add_argument('--tr', dest='timing_rounds', type=int, default=10000,
                        help="number of timing rounds")
    parser.add_argument('--wr', dest='warmup_rounds', type=int, default=1000,
                        help="number of warmup rounds")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="prefix of output files")
    args = parser.parse_args()

    if args.page_cache != "direct" and args.page_cache != "unlimited":
        try:
            int(args.page_cache)
        except:
            raise ValueError("mem limit must be a percentage int in [1, 100]")

    if args.make_files:
        make_files(args.dir_path)
    else:
        run_expers(args.dir_path, args.rw_mode, args.file_src, args.page_cache,
                   args.timing_rounds, args.warmup_rounds, args.output_prefix)

if __name__ == "__main__":
    main()
