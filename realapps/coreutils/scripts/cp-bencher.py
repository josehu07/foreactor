#!/usr/bin/env python3

import time
import subprocess
import os
import argparse


CP_BIN = "./coreutils-src/src/cp"
CP_GRAPH_ID = 0

URING_QUEUE = 512

NUM_ITERS = 30


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
        exit(1)

def check_dir_exists(dir_path):
    if not os.path.isdir(dir_path):
        print(f"Error: directory {dir_path} does not exist")
        exit(1)


def query_timestamp_sec():
    return time.perf_counter()


def run_cp_single(libforeactor, workdir, use_foreactor, backend=None,
                  pre_issue_depth=0):
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    envs = os.environ.copy()
    envs["LD_PRELOAD"] = libforeactor
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs[f"DEPTH_{CP_GRAPH_ID}"] = str(pre_issue_depth)
    if backend == "io_uring_default":
        envs[f"QUEUE_{CP_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{CP_GRAPH_ID}"] = "no"
    else:
        envs[f"QUEUE_{CP_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{CP_GRAPH_ID}"] = "yes"

    cmd = [CP_BIN, "--reflink=never", "--sparse=never"]
    # concurrent background I/O sometimes works better with readahead turned off
    if use_foreactor:
        cmd.append("--fadvise=random")

    indir = f"{workdir}/indir"
    outdir = f"{workdir}/outdir"

    assert os.path.isdir(indir)
    num_files = 0
    for file in os.listdir(indir):
        cmd.append(f"{workdir}/indir/{file}")
        num_files += 1
    assert num_files > 0
    
    cmd.append(f"{outdir}/")

    secs_before = query_timestamp_sec()
    subprocess.run(cmd, check=True, capture_output=True, env=envs)
    secs_after = query_timestamp_sec()

    return (secs_after - secs_before) / num_files   # return value is secs/file

def run_cp_iters(num_iters, libforeactor, workdir, use_foreactor, backend=None,
                 pre_issue_depth=0):
    result_avg_secs = 0.
    for i in range(num_iters):
        avg_secs = run_cp_single(libforeactor, workdir, use_foreactor, backend,
                                 pre_issue_depth)
        result_avg_secs += avg_secs
    avg_ms = (result_avg_secs / num_iters) * 1000
    return avg_ms


def run_exprs(libforeactor, workdir, output_log, backend, pre_issue_depth_list):
    num_iters = NUM_ITERS

    with open(output_log, 'w') as fout:
        avg_ms = run_cp_iters(num_iters, libforeactor, workdir, False)
        result = f" orig: avg {avg_ms:.3f} ms"
        fout.write(result + '\n')
        print(result)

        for pre_issue_depth in pre_issue_depth_list:
            avg_ms = run_cp_iters(num_iters, libforeactor, workdir, True,
                                  backend, pre_issue_depth)
            result = f" {pre_issue_depth:4d}: avg {avg_ms:.3f} ms"
            fout.write(result + '\n')
            print(result)


def main():
    parser = argparse.ArgumentParser(description="cp copy benchmark driver")
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-d', dest='workdir', required=True,
                        help="workdir containing files")
    parser.add_argument('-o', dest='output_log', required=True,
                        help="output log filename")
    parser.add_argument('-b', dest='backend', required=True,
                        help="io_uring_default|io_uring_sqe_async")
    parser.add_argument('pre_issue_depths', metavar='D', type=int, nargs='+',
                        help="pre_issue_depth to try")
    args = parser.parse_args()

    if args.backend == "thread_pool":
        print(f"Error: thread pool backend does not support link feature yet")
        exit(1)
    elif args.backend != "io_uring_default" and args.backend != "io_uring_sqe_async":
        print(f"Error: unrecognized backend {args.backend}")
        exit(1)

    check_file_exists(args.libforeactor)
    check_file_exists(CP_BIN)
    check_dir_exists(args.workdir)
    check_dir_exists(f"{args.workdir}/indir")
    check_dir_exists(f"{args.workdir}/outdir")

    run_exprs(args.libforeactor, args.workdir, args.output_log, args.backend,
              args.pre_issue_depths)

if __name__ == "__main__":
    main()
