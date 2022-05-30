#!/usr/bin/env python3

import subprocess
import os
import argparse


URING_QUEUE_LEN = 256
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
                       use_foreactor, pre_issue_depth=0):
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    envs = os.environ.copy()
    envs["LD_PRELOAD"] = libforeactor
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs["QUEUE_0"] = str(URING_QUEUE_LEN)
    envs["DEPTH_0"] = str(pre_issue_depth)

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


def output_filename(output_prefix, output_suffix):
    return f'{output_prefix}-{output_suffix}.log'

def run_exprs(libforeactor, dbdir, trace, mem_limit, drop_caches,
              output_prefix, pre_issue_depth_list):
    output = run_ycsbcli_single(libforeactor, dbdir, trace, mem_limit,
                                drop_caches, False)
    with open(output_filename(output_prefix, 'orig'), 'w') as fout:
        fout.write(output)

    for pre_issue_depth in pre_issue_depth_list:
        output = run_ycsbcli_single(libforeactor, dbdir, trace, mem_limit,
                                    drop_caches, True, pre_issue_depth)
        with open(output_filename(output_prefix, pre_issue_depth), 'w') as fout:
            fout.write(output)


def main():
    parser = argparse.ArgumentParser(description="LevelDB benchmark w/ foreactor")
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-d', dest='dbdir', required=True,
                        help="dbdir of LevelDB")
    parser.add_argument('-f', dest='trace', required=True,
                        help="trace file to run ycsbcli")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="prefix of output log filenames")
    parser.add_argument('--mem_limit', dest='mem_limit', required=False, default="none",
                        help="memory limit to bound page cache size")
    parser.add_argument('--drop_caches', dest='drop_caches', action='store_true',
                        help="do drop_caches per request")
    parser.add_argument('pre_issue_depths', metavar='D', type=int, nargs='+',
                        help="pre_issue_depth to try")
    args = parser.parse_args()

    if args.mem_limit != "none":
        try:
            int(args.mem_limit)
        except:
            print("Error: mem_limit must be an integer in bytes")
            exit(1)

    run_exprs(args.libforeactor, args.dbdir, args.trace, args.mem_limit,
              args.drop_caches, args.output_prefix, args.pre_issue_depths)

if __name__ == "__main__":
    main()
