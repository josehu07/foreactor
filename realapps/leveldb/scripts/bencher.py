#!/usr/bin/env python3

import subprocess
import os
import argparse


YCSBCLI_BIN = "./ycsbcli"
GET_GRAPH_ID = 0

URING_QUEUE = 32
CGROUP_NAME = "leveldb_group"

CACHED_ITERS = 5
DROP_CACHES_ITERS = 1


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
        exit(1)


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

def get_iostat_bio_mb_read():
    # FIXME: currently just reads stat of the first device
    cmd = ["sudo", "iostat", "-d", "-m", "1", "1"]
    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    in_dev_line, mb_read_idx = False, -1
    for line in output.split('\n'):
        line = line.strip()
        if "MB_read" in line:
            in_dev_line = True
            mb_read_idx = line.split().index("MB_read")
        elif in_dev_line:
            assert mb_read_idx > 0
            return float(line.split()[mb_read_idx])
    return None


def run_ycsbcli_single(libforeactor, dbdir, trace, mem_limit, drop_caches,
                       use_foreactor, backend=None, pre_issue_depth=0):
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    envs = os.environ.copy()
    envs["LD_PRELOAD"] = libforeactor
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs[f"DEPTH_{GET_GRAPH_ID}"] = str(pre_issue_depth)
    if backend == "io_uring_default":
        envs[f"QUEUE_{GET_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{GET_GRAPH_ID}"] = "no"
    elif backend == "io_uring_sqe_async":
        envs[f"QUEUE_{GET_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{GET_GRAPH_ID}"] = "yes"
    else:
        num_uthreads = pre_issue_depth
        if num_uthreads <= 0:
            num_uthreads = 1
        elif num_uthreads > 16:
            num_uthreads = 16
        envs[f"UTHREADS_{GET_GRAPH_ID}"] = str(num_uthreads)

    cmd = [YCSBCLI_BIN, "-d", dbdir, "-f", trace, "--bg_compact_off",
           "--no_fill_cache"]
    if drop_caches:
        cmd.append("--drop_caches")
    if mem_limit != "none":
        set_cgroup_mem_limit(int(mem_limit))
        cmd = ["sudo", "cgexec", "-g", "memory:"+CGROUP_NAME] + cmd

    mb_read_before = get_iostat_bio_mb_read()
    result = subprocess.run(cmd, check=True, capture_output=True, env=envs)
    mb_read_after = get_iostat_bio_mb_read()
    output = result.stdout.decode('ascii')
    return output, mb_read_after - mb_read_before

def get_us_result_from_output(output):
    in_timing_section = False
    sum_us, avg_us, p99_us = None, None, None

    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("Removing top-1"):
            in_timing_section = True
        elif in_timing_section and line.startswith("sum"):
            sum_us = float(line.split()[1])
        elif in_timing_section and line.startswith("avg"):
            avg_us = float(line.split()[1])
        elif in_timing_section and line.startswith("p99"):
            p99_us = float(line.split()[1])
            break

    return (sum_us, avg_us, p99_us)

def run_ycsbcli_iters(num_iters, libforeactor, dbdir, trace, mem_limit,
                      drop_caches, use_foreactor, backend=None,
                      pre_issue_depth=0):
    result_sum_us, result_avg_us, result_p99_us = 0., 0., 0.
    result_mb_read = 0.
    for i in range(num_iters):
        output, mb_read = run_ycsbcli_single(libforeactor, dbdir, trace, mem_limit,
                                             drop_caches, use_foreactor, backend,
                                             pre_issue_depth)
        sum_us, avg_us, p99_us = get_us_result_from_output(output)
        assert mb_read >= 0
        assert sum_us is not None
        assert avg_us is not None
        assert p99_us is not None
        result_sum_us += sum_us
        result_avg_us += avg_us
        result_p99_us += p99_us
        result_mb_read += mb_read

    result_sum_us /= num_iters
    result_avg_us /= num_iters
    result_p99_us /= num_iters
    result_mb_read /= num_iters
    return result_sum_us, result_avg_us, result_p99_us, result_mb_read


def run_exprs(libforeactor, dbdir, trace, mem_limit, drop_caches,
              output_log, backend, pre_issue_depth_list):
    num_iters = CACHED_ITERS if not drop_caches else DROP_CACHES_ITERS

    with open(output_log, 'w') as fout:
        sum_us, avg_us, p99_us, mb_read = run_ycsbcli_iters(num_iters, libforeactor,
                                                            dbdir, trace, mem_limit,
                                                            drop_caches, False)
        result = f" orig: sum {sum_us:.3f} avg {avg_us:.3f} p99 {p99_us:.3f} us" + \
                 f" {mb_read:.3f} MB_read"
        fout.write(result + '\n')
        print(result)

        for pre_issue_depth in pre_issue_depth_list:
            sum_us, avg_us, p99_us, mb_read = run_ycsbcli_iters(num_iters, libforeactor,
                                                                dbdir, trace, mem_limit,
                                                                drop_caches, True,
                                                                backend, pre_issue_depth)
            result = f" {pre_issue_depth:4d}: sum {sum_us:.3f} avg {avg_us:.3f}" + \
                     f" p99 {p99_us:.3f} us {mb_read:.3f} MB_read"
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

    check_file_exists(args.libforeactor)
    check_file_exists(args.trace)
    check_file_exists(YCSBCLI_BIN)

    run_exprs(args.libforeactor, args.dbdir, args.trace, args.mem_limit,
              args.drop_caches, args.output_log, args.backend, args.pre_issue_depths)

if __name__ == "__main__":
    main()
