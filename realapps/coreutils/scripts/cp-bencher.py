#!/usr/bin/env python3

import time
import subprocess
import os
import argparse


CP_BIN = "./coreutils-src/src/cp"
CP_GRAPH_ID = 0

URING_QUEUE = 512

NUM_ITERS = 3


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


def compose_cp_cmd_env(libforeactor, workdir, workdir_out, use_foreactor,
                       allow_reflink, backend, pre_issue_depth):
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

    cmd = [CP_BIN, "--sparse=never"]
    if not allow_reflink:
        cmd.append("--reflink=never")
    # concurrent background I/O sometimes works better with readahead turned off
    if use_foreactor:
        cmd.append("--fadvise=random")

    indir = f"{workdir}/indir"
    outdir = f"{workdir_out}/outdir"

    assert os.path.isdir(indir)
    for file in os.listdir(indir):
        cmd.append(f"{indir}/{file}")
    
    cmd.append(f"{outdir}/")

    return cmd, envs


def run_cp_single(libforeactor, workdir, workdir_out, use_foreactor,
                  allow_reflink=False, backend=None, pre_issue_depth=0):
    os.system("ulimit -n 65536")
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    cmd, env = compose_cp_cmd_env(libforeactor, workdir, workdir_out, use_foreactor,
                                  allow_reflink, backend, pre_issue_depth)

    secs_before = query_timestamp_sec()
    run_subprocess_cmd(cmd, merge=False, env=env)
    secs_after = query_timestamp_sec()
    return (secs_after - secs_before)

def run_cp_iters(num_iters, libforeactor, workdir, workdir_out, use_foreactor,
                 allow_reflink=False, backend=None, pre_issue_depth=0):
    result_avg_secs = 0.
    for i in range(num_iters):
        avg_secs = run_cp_single(libforeactor, workdir, workdir_out, use_foreactor,
                                 allow_reflink=allow_reflink, backend=backend,
                                 pre_issue_depth=pre_issue_depth)
        result_avg_secs += avg_secs
    avg_ms = (result_avg_secs / num_iters) * 1000
    return avg_ms


def run_exprs(libforeactor, workdir, workdir_out, output_log, backend,
              pre_issue_depth_list):
    num_iters = NUM_ITERS

    with open(output_log, 'w') as fout:
        avg_ms = run_cp_iters(num_iters, libforeactor, workdir, workdir_out, False,
                              allow_reflink=False)
        result = f" orig: avg {avg_ms:.3f} ms"
        fout.write(result + '\n')
        print(result)

        avg_ms = run_cp_iters(num_iters, libforeactor, workdir, workdir_out, False,
                              allow_reflink=True)
        result = f" wcfr: avg {avg_ms:.3f} ms"
        fout.write(result + '\n')
        print(result)

        for pre_issue_depth in pre_issue_depth_list:
            avg_ms = run_cp_iters(num_iters, libforeactor, workdir, workdir_out, True,
                                  backend=backend, pre_issue_depth=pre_issue_depth)
            result = f" {pre_issue_depth:4d}: avg {avg_ms:.3f} ms"
            fout.write(result + '\n')
            print(result)


def get_iostat_snapshot(dev_name):
    cmd = ["sudo", "iostat", "-p", "-m", "1", "1"]
    output = run_subprocess_cmd(cmd, merge=False)

    in_avg_cpu = False
    cpu_util, mb_read, mb_wrtn = 0., 0., 0.
    for line in output.strip().split('\n'):
        line = line.strip()
        if not in_avg_cpu and line.startswith('avg-cpu'):
            in_avg_cpu = True
        elif in_avg_cpu:
            segs = line.split()
            cpu_ur, cpu_ni, cpu_sy = float(segs[0]), float(segs[1]), float(segs[2])
            cpu_util = cpu_ur + cpu_ni + cpu_sy
            in_avg_cpu = False
        elif line.startswith(dev_name):
            segs = line.split()
            mb_read, mb_wrtn = float(segs[5]), float(segs[6])

    return cpu_util, mb_read, mb_wrtn

def get_top_cpu_util(pid, interval, count):
    assert count > 1
    cmd = ["sudo", "top", "-b", "-n", str(count), "-d", str(interval), "-p", str(pid)]
    output = run_subprocess_cmd(cmd, merge=False)

    cpu_utils = []
    for line in output.strip().split('\n'):
        line = line.strip()
        # io_uring kernel threads' cpu usage may not be charged under pid, so anyway
        # we read the global cpu usage
        if line.startswith("%Cpu(s)"):
            segs = line[line.index(':')+1:].strip().split(',')
            cpu_ur, cpu_sy, cpu_ni = \
                float(segs[0][:-3]), float(segs[1][:-3]), float(segs[2][:-3])
            cpu_utils.append(cpu_ur + cpu_ni + cpu_sy)
    
    return sum(cpu_utils[1:]) / (len(cpu_utils) - 1)

def gen_cp_util(dev_name, libforeactor, workdir, use_foreactor, backend=None,
                pre_issue_depth=0):
    os.system("ulimit -n 65536")
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    cmd, env = compose_cp_cmd_env(libforeactor, workdir, use_foreactor, backend,
                                  pre_issue_depth)

    num_iters = 10
    cpu_utils, disk_r_utils, disk_w_utils = [], [], []
    for _ in range(num_iters):
        # read disk counter before run
        _, mb_read_before, mb_wrtn_before = get_iostat_snapshot(dev_name)
        secs_before = query_timestamp_sec()

        # spawn process, do top alongside to sample cpu util
        proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.DEVNULL)
        cpu_util = get_top_cpu_util(proc.pid, 0.1, 10)   # assume at least lasts 1s
        proc.wait()

        # read disk counter after run
        secs_after = query_timestamp_sec()
        _, mb_read_after, mb_wrtn_after = get_iostat_snapshot(dev_name)

        cpu_utils.append(cpu_util)

        elapsed_secs = secs_after - secs_before
        disk_r_util = (mb_read_after - mb_read_before) / elapsed_secs
        disk_w_util = (mb_wrtn_after - mb_wrtn_before) / elapsed_secs
        disk_r_utils.append(disk_r_util)
        disk_w_utils.append(disk_w_util)

    cpu_util = sum(cpu_utils) / len(cpu_utils)
    disk_r_util = sum(disk_r_utils) / len(disk_r_utils)
    disk_w_util = sum(disk_w_utils) / len(disk_w_utils)
    return cpu_util, disk_r_util, disk_w_util

def run_utils(libforeactor, workdir, output_log, backend, pre_issue_depth_list,
              dev_name):
    with open(output_log, 'w') as fout:
        cpu_util, disk_r_util, disk_w_util = \
            gen_cp_util(dev_name, libforeactor, workdir, False)
        result = f" orig: cpu {cpu_util:.2f}% disk_r {disk_r_util:.2f} " + \
                 f"disk_w {disk_w_util:.2f} MB/s"
        fout.write(result + '\n')
        print(result)

        for pre_issue_depth in pre_issue_depth_list:
            cpu_util, disk_r_util, disk_w_util = \
                gen_cp_util(dev_name, libforeactor, workdir, True, backend,
                            pre_issue_depth)
            result = f" {pre_issue_depth:4d}: cpu {cpu_util:.2f}% " + \
                     f"disk_r {disk_r_util:.2f} disk_w {disk_w_util:.2f} MB/s"
            fout.write(result + '\n')
            print(result)


def main():
    parser = argparse.ArgumentParser(description="cp copy benchmark driver")
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-d', dest='workdir', required=True,
                        help="workdir containing files")
    parser.add_argument('--dout', dest='workdir_out', required=False,
                        help="specify if want separate output workdir")
    parser.add_argument('-o', dest='output_log', required=True,
                        help="output log filename")
    parser.add_argument('-b', dest='backend', required=True,
                        help="io_uring_default|io_uring_sqe_async")
    parser.add_argument('--util_dev', dest='util_dev', required=False,
                        help="run utilization report mode on disk dev")
    parser.add_argument('pre_issue_depths', metavar='D', type=int, nargs='+',
                        help="pre_issue_depth to try")
    args = parser.parse_args()

    if args.backend == "thread_pool":
        print(f"Error: thread pool backend does not support link feature yet")
        exit(1)
    elif args.backend != "io_uring_default" and args.backend != "io_uring_sqe_async":
        print(f"Error: unrecognized backend {args.backend}")
        exit(1)

    workdir_out = args.workdir
    if hasattr(args, "workdir_out") and getattr(args, "workdir_out") is not None:
        workdir_out = args.workdir_out
        if not os.path.isdir(f"{workdir_out}/outdir"):
            os.mkdir(f"{workdir_out}/outdir")

    check_file_exists(args.libforeactor)
    check_file_exists(CP_BIN)
    check_dir_exists(args.workdir)
    check_dir_exists(f"{args.workdir}/indir")
    check_dir_exists(f"{workdir_out}/outdir")

    if args.util_dev is None:
        run_exprs(args.libforeactor, args.workdir, workdir_out, args.output_log,
                  args.backend, args.pre_issue_depths)
    else:
        run_utils(args.libforeactor, args.workdir, args.output_log,
                  args.backend, args.pre_issue_depths, args.util_dev)

if __name__ == "__main__":
    main()
