#!/usr/bin/env python3

import time
import subprocess
import os
import argparse


BPTCLI_BIN = "./bptcli"
SCAN_GRAPH_ID = 0
LOAD_GRAPH_ID = 1

URING_QUEUE = 512
CGROUP_NAME = "bptree_group"

NUM_LOAD_ITERS = 1
NUM_SCAN_ITERS = 5


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
        exit(1)

def check_dir_exists(dir_path):
    if not os.path.isdir(dir_path):
        print(f"Error: directory {dir_path} does not exist")
        exit(1)


def run_subprocess_cmd(cmd, outfile=None, merge=False, env=None):
    try:
        result = None
        if outfile is None and not merge:
            result = subprocess.run(cmd, env=env, check=True,
                                         capture_output=True)
        elif outfile is None:
            result = subprocess.run(cmd, env=env, check=True,
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT)
        elif not merge:
            result = subprocess.run(cmd, env=env, check=True,
                                         stdout=outfile)
        else:
            result = subprocess.run(cmd, env=env, check=True,
                                         stdout=outfile,
                                         stderr=subprocess.STDOUT)
        output = None
        if result.stdout is not None:
            output = result.stdout.decode('ascii')
        return output
    except subprocess.CalledProcessError as err:
        print(f"Error: subprocess returned exit status {err.returncode}")
        print(f"  command: {' '.join(err.cmd)}")
        if err.stderr is not None:
            print(f"  stderr: {err.stderr.decode('ascii')}")
        exit(1)

def query_timestamp_sec():
    return time.perf_counter()


def set_cgroup_mem_limit(mem_limit):
    cmd = ["sudo", "lscgroup"]
    output = run_subprocess_cmd(cmd, merge=False)
    cgroup_found = False
    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("memory:") and CGROUP_NAME in line:
            cgroup_found = True
            break

    if not cgroup_found:    # create cgroup
        cmd = ["sudo", "cgcreate", "-g", "memory:"+CGROUP_NAME]
        run_subprocess_cmd(cmd, merge=False)

    cmd = ["sudo", "cgset", "-r", "memory.limit_in_bytes="+str(mem_limit),
           CGROUP_NAME]
    run_subprocess_cmd(cmd, merge=False)

def compose_bptcli_cmd_env(libforeactor, dbfile, trace, mem_limit,
                           use_foreactor, backend, pre_issue_depth):
    envs = os.environ.copy()
    envs["LD_PRELOAD"] = libforeactor
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs[f"DEPTH_{SCAN_GRAPH_ID}"] = str(pre_issue_depth)
    envs[f"DEPTH_{LOAD_GRAPH_ID}"] = str(pre_issue_depth)
    if backend == "io_uring_default":
        envs[f"QUEUE_{SCAN_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"QUEUE_{LOAD_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{SCAN_GRAPH_ID}"] = "no"
        envs[f"SQE_ASYNC_FLAG_{LOAD_GRAPH_ID}"] = "no"
    elif backend == "io_uring_sqe_async":
        envs[f"QUEUE_{SCAN_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"QUEUE_{LOAD_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{SCAN_GRAPH_ID}"] = "yes"
        envs[f"SQE_ASYNC_FLAG_{LOAD_GRAPH_ID}"] = "yes"
    else:
        num_uthreads = pre_issue_depth
        if num_uthreads <= 0:
            num_uthreads = 1
        elif num_uthreads > 16:
            num_uthreads = 16
        envs[f"UTHREADS_{SCAN_GRAPH_ID}"] = str(num_uthreads)
        envs[f"UTHREADS_{LOAD_GRAPH_ID}"] = str(num_uthreads)

    cmd = [BPTCLI_BIN, "-f", dbfile, "-w", trace]
    if mem_limit != "none":
        set_cgroup_mem_limit(int(mem_limit))
        cmd = ["sudo", "cgexec", "-g", "memory:"+CGROUP_NAME] + cmd

    return cmd, envs


def run_bptcli_single(libforeactor, dbfile, trace, mem_limit, use_foreactor,
                      backend=None, pre_issue_depth=0):
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")
    cmd, env = compose_bptcli_cmd_env(libforeactor, dbfile, trace, mem_limit,
                                      use_foreactor, backend, pre_issue_depth)
    output = run_subprocess_cmd(cmd, merge=True, env=env)
    return output

def get_us_result_from_output(output):
    in_timing_section = False
    sum_us, avg_us, p99_us = None, None, None

    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("Time elapsed stats:"):
            in_timing_section = True
        elif in_timing_section and line.startswith("sum"):
            sum_us = float(line.split()[1])
        elif in_timing_section and line.startswith("avg"):
            avg_us = float(line.split()[1])
        elif in_timing_section and line.startswith("p99"):
            p99_us = float(line.split()[1])
            break

    return (sum_us, avg_us, p99_us)


def run_bptcli_iters(num_iters, libforeactor, dbfile, trace, mem_limit,
                     use_foreactor, backend=None, pre_issue_depth=0,
                     remove_exist=False):
    result_sum_us, result_avg_us = 0., 0.
    for i in range(num_iters):
        if remove_exist and os.path.isfile(dbfile):
            os.remove(dbfile)
        
        output = run_bptcli_single(libforeactor, dbfile, trace, mem_limit,
                                   use_foreactor, backend=backend,
                                   pre_issue_depth=pre_issue_depth)
        sum_us, avg_us, _ = get_us_result_from_output(output)
        if sum_us is not None:
            assert avg_us is not None
            result_sum_us += sum_us
            result_avg_us += avg_us
    
    result_sum_us /= num_iters
    result_avg_us /= num_iters
    return (result_sum_us, result_avg_us)

def run_exprs(libforeactor, dbfile, trace, mem_limit, output_log, backend,
              pre_issue_depth_list, remove_exist):
    num_iters = NUM_LOAD_ITERS if "load" in trace else NUM_SCAN_ITERS

    with open(output_log, 'w') as fout:
        sum_us, avg_us = \
            run_bptcli_iters(num_iters, libforeactor, dbfile, trace, mem_limit,
                             False, remove_exist=remove_exist)
        result = f" orig: sum_us {sum_us:.3f} avg_us {avg_us:.3f}"
        fout.write(result+'\n')
        print(result)

        for pre_issue_depth in pre_issue_depth_list:
            sum_us, avg_us = \
                run_bptcli_iters(num_iters, libforeactor, dbfile, trace, mem_limit,
                                 True, backend=backend,
                                 pre_issue_depth=pre_issue_depth,
                                 remove_exist=remove_exist)
            result = f" {pre_issue_depth:4d}:" + \
                     f" sum_us {sum_us:.3f} avg_us {avg_us:.3f}"
            fout.write(result+'\n')
            print(result)


def main():
    parser = argparse.ArgumentParser(description="BPTree benchmark driver")
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-f', dest='dbfile', required=True,
                        help="backing file of B+ tree")
    parser.add_argument('-w', dest='trace', required=True,
                        help="trace file to execute")
    parser.add_argument('-o', dest='output_log', required=True,
                        help="output log filename")
    parser.add_argument('-b', dest='backend', required=True,
                        help="io_uring_default|io_uring_sqe_async|thread_pool")
    parser.add_argument('-m', dest='mem_limit', required=False, default="none",
                        help="memory limit to bound page cache size")
    parser.add_argument('--remove_exist', dest='remove_exist', action='store_true',
                        help="if given, removes the dbfile before each run")
    parser.add_argument('pre_issue_depths', metavar='D', type=int, nargs='+',
                        help="list of pre_issue_depth to try")
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
    check_file_exists(BPTCLI_BIN)

    run_exprs(args.libforeactor, args.dbfile, args.trace, args.mem_limit,
              args.output_log, args.backend, args.pre_issue_depths,
              args.remove_exist)

if __name__ == "__main__":
    main()
