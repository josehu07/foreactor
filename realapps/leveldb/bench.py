#!/usr/bin/env python3

import subprocess
import os
import argparse


URING_QUEUE = 16
CGROUP_NAME = "leveldb_group"


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


def run_ycsbcli_single(libforeactor, dbdir, trace, mem_limit, drop_caches,
                       use_foreactor, backend=None, pre_issue_depth=0):
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    envs = os.environ.copy()
    envs["LD_PRELOAD"] = libforeactor
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs["DEPTH_0"] = str(pre_issue_depth)
    if backend == "io_uring_default":
        envs["QUEUE_0"] = str(URING_QUEUE)
        envs["SQE_ASYNC_FLAG_0"] = "no"
    elif backend == "io_uring_sqe_async":
        envs["QUEUE_0"] = str(URING_QUEUE)
        envs["SQE_ASYNC_FLAG_0"] = "yes"
    else:
        num_uthreads = pre_issue_depth      # use uthreads == depth
        if num_uthreads <= 0:
            num_uthreads = 1
        envs["UTHREADS_0"] = str(num_uthreads)

    cmd = ["./ycsbcli", "-d", dbdir, "-f", trace,
           "--bg_compact_off", "--no_fill_cache"]
    if drop_caches:
        cmd.append("--drop_caches")
    if mem_limit != "none":
        set_cgroup_mem_limit(int(mem_limit))
        cmd = ["sudo", "cgexec", "-g", "memory:"+CGROUP_NAME] + cmd

    result = subprocess.run(cmd, check=True, capture_output=True, env=envs)
    output = result.stdout.decode('ascii')
    return output

def get_avg_us_from_output(output):
    in_timing_section = False
    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("Removing top-1"):
            in_timing_section = True
        elif in_timing_section and line.startswith("avg"):
            return float(line.split()[1])
    return None

def run_ycsbcli_iters(num_iters, libforeactor, dbdir, trace, mem_limit,
                      drop_caches, use_foreactor, backend=None,
                      pre_issue_depth=0):
    result_us = 0
    for i in range(num_iters):
        output = run_ycsbcli_single(libforeactor, dbdir, trace, mem_limit,
                                    drop_caches, use_foreactor, backend,
                                    pre_issue_depth)
        avg_us = get_avg_us_from_output(output)
        assert avg_us is not None
        result_us += avg_us
    return result_us / num_iters


def run_exprs(libforeactor, dbdir, trace, mem_limit, drop_caches,
              output_log, backend, pre_issue_depth_list):
    num_iters = 5 if not drop_caches else 1

    with open(output_log, 'w') as fout:
        result_us = run_ycsbcli_iters(num_iters, libforeactor, dbdir, trace,
                                      mem_limit, drop_caches, False)
        result = f" orig: {result_us:.3f} us"
        fout.write(result + '\n')
        print(result)

        for pre_issue_depth in pre_issue_depth_list:
            result_us = run_ycsbcli_iters(num_iters, libforeactor, dbdir, trace,
                                          mem_limit, drop_caches, True, backend,
                                          pre_issue_depth)
            result = f" {pre_issue_depth:4d}: {result_us:.3f} us"
            fout.write(result + '\n')
            print(result)


def main():
    parser = argparse.ArgumentParser(description="LevelDB benchmark driver")
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-d', dest='dbdir', required=True,
                        help="dbdir of LevelDB")
    parser.add_argument('-f', dest='trace', required=True,
                        help="trace file to run ycsbcli")
    parser.add_argument('-o', dest='output_log', required=True,
                        help="output log filename")
    parser.add_argument('-b', dest='backend', required=True,
                        help="io_uring_default|io_uring_sqe_async|thread_pool")
    parser.add_argument('--mem_limit', dest='mem_limit', required=False, default="none",
                        help="memory limit to bound page cache size")
    parser.add_argument('--drop_caches', dest='drop_caches', action='store_true',
                        help="do drop_caches per request")
    parser.add_argument('pre_issue_depths', metavar='D', type=int, nargs='+',
                        help="pre_issue_depth to try")
    args = parser.parse_args()

    if args.backend != "io_uring_default" and args.backend != "io_uring_sqe_async" \
       and args.backend != "thread_pool":
        print(f"Error: unrecognized backend {args.backend}")
        exit(1)

    if args.mem_limit != "none":
        try:
            int(args.mem_limit)
        except:
            print("Error: mem_limit must be an integer in bytes")
            exit(1)

    run_exprs(args.libforeactor, args.dbdir, args.trace, args.mem_limit,
              args.drop_caches, args.output_log, args.backend, args.pre_issue_depths)

if __name__ == "__main__":
    main()
