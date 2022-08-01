#!/usr/bin/env python3

import time
import subprocess
import os
import argparse


DU_BIN = "./coreutils-src/src/du"
DU_GRAPH_ID = 1

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


def run_du_single(libforeactor, workdir, use_foreactor, backend=None,
                  pre_issue_depth=0):
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    envs = os.environ.copy()
    envs["LD_PRELOAD"] = libforeactor
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs[f"DEPTH_{DU_GRAPH_ID}"] = str(pre_issue_depth)
    if backend == "io_uring_default":
        envs[f"QUEUE_{DU_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{DU_GRAPH_ID}"] = "no"
    elif backend == "io_uring_sqe_async":
        envs[f"QUEUE_{DU_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{DU_GRAPH_ID}"] = "yes"
    else:
        num_uthreads = pre_issue_depth
        if num_uthreads <= 0:
            num_uthreads = 1
        elif num_uthreads > 16:
            num_uthreads = 16
        envs[f"UTHREADS_{DU_GRAPH_ID}"] = str(num_uthreads)

    cmd = [DU_BIN, "-s", "-h"]
    
    indir = f"{workdir}/indir"

    assert os.path.isdir(indir)
    num_dirs = 0
    for root_dir in os.listdir(indir):
        cmd.append(f"{workdir}/indir/{root_dir}")
        num_dirs += 1
    assert num_dirs > 0
    
    secs_before = query_timestamp_sec()
    run_subprocess_cmd(cmd, merge=False, env=envs)
    secs_after = query_timestamp_sec()
    return (secs_after - secs_before) / num_dirs    # return value is secs/dir

def run_du_iters(num_iters, libforeactor, workdir, use_foreactor, backend=None,
                 pre_issue_depth=0):
    result_avg_secs = 0.
    for i in range(num_iters):
        avg_secs = run_du_single(libforeactor, workdir, use_foreactor, backend,
                                 pre_issue_depth)
        result_avg_secs += avg_secs
    avg_ms = (result_avg_secs / num_iters) * 1000
    return avg_ms


def run_exprs(libforeactor, workdir, output_log, backend, pre_issue_depth_list):
    num_iters = NUM_ITERS

    with open(output_log, 'w') as fout:
        avg_ms = run_du_iters(num_iters, libforeactor, workdir, False)
        result = f" orig: avg {avg_ms:.3f} ms"
        fout.write(result + '\n')
        print(result)

        for pre_issue_depth in pre_issue_depth_list:
            avg_ms = run_du_iters(num_iters, libforeactor, workdir, True,
                                    backend, pre_issue_depth)
            result = f" {pre_issue_depth:4d}: avg {avg_ms:.3f} ms"
            fout.write(result + '\n')
            print(result)


def main():
    parser = argparse.ArgumentParser(description="du stat benchmark driver")
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-d', dest='workdir', required=True,
                        help="workdir containing files")
    parser.add_argument('-o', dest='output_log', required=True,
                        help="output log filename")
    parser.add_argument('-b', dest='backend', required=True,
                        help="io_uring_default|io_uring_sqe_async|thread_pool")
    parser.add_argument('pre_issue_depths', metavar='D', type=int, nargs='+',
                        help="pre_issue_depth to try")
    args = parser.parse_args()

    if args.backend != "io_uring_default" and args.backend != "io_uring_sqe_async" \
       and args.backend != "thread_pool":
        print(f"Error: unrecognized backend {args.backend}")
        exit(1)

    check_file_exists(args.libforeactor)
    check_file_exists(DU_BIN)
    check_dir_exists(args.workdir)
    check_dir_exists(f"{args.workdir}/indir")

    run_exprs(args.libforeactor, args.workdir, args.output_log, args.backend,
              args.pre_issue_depths)

if __name__ == "__main__":
    main()
