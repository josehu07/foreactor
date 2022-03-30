#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import subprocess
import os
import argparse
import matplotlib.pyplot as plt


URING_QUEUE_LEN = 256
NUM_REPEATS = 3


def run_ycsbcli_single(libforeactor, dbdir, trace, drop_caches, use_foreactor,
                       pre_issue_depth=0):
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    envs = os.environ.copy()
    envs["LD_PRELOAD"] = libforeactor
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs["QUEUE_0"] = str(URING_QUEUE_LEN)
    envs["DEPTH_0"] = str(pre_issue_depth)

    cmd = ["sudo", "./ycsbcli", "-d", dbdir, "-f", trace,
           "--bg_compact_off", "--no_fill_cache"]
    if drop_caches:
        cmd.append("--drop_caches")

    result = subprocess.run(cmd, check=True, capture_output=True, env=envs)
    output = result.stdout.decode('ascii')

    rm_seen = False
    for line in output.strip().split('\n'):
        line = line.strip()
        if line.startswith("removing top/bottom-5"):
            rm_seen = True
        elif rm_seen and line.startswith("avg"):
            return float(line.split()[-2])


def run_exprs(libforeactor, dbdir, trace, drop_caches, pre_issue_depth_list):
    original_us_r = 0.0
    for i in range(NUM_REPEATS):
        original_us_r += run_ycsbcli_single(libforeactor, dbdir, trace,
                                            drop_caches, False)
    original_us = original_us_r / NUM_REPEATS

    foreactor_us_list = []
    for pre_issue_depth in pre_issue_depth_list:
        foreactor_us_r = 0.0
        for i in range(NUM_REPEATS):
            foreactor_us_r += run_ycsbcli_single(libforeactor, dbdir, trace,
                                                 drop_caches, True, pre_issue_depth)
        foreactor_us_list.append(foreactor_us_r / NUM_REPEATS)

    return original_us, foreactor_us_list


def plot_time(pre_issue_depth_list, original_us, foreactor_us_list, output_name):
    print(f'{original_us:8.3f}')
    for i in range(len(pre_issue_depth_list)):
        print(f'{pre_issue_depth_list[i]:3d} {foreactor_us_list[i]:8.3f}')

    width = 0.5

    plt.bar([0], [original_us], width,
            zorder=3, label="Original")
    plt.bar(pre_issue_depth_list, foreactor_us_list, width,
            zorder=3, label="Foreactor")

    plt.ylabel("Time (us)")
    plt.xticks([0] + pre_issue_depth_list, ['original'] + pre_issue_depth_list)
    plt.xlabel("pre_issue_depth")

    plt.grid(zorder=0, axis='y')

    plt.legend()

    plt.savefig(output_name, dpi=120)


def main():
    parser = argparse.ArgumentParser(description="LevelDB benchmark w/ foreactor")
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-d', dest='dbdir', required=True,
                        help="dbdir of LevelDB")
    parser.add_argument('-f', dest='trace', required=True,
                        help="trace file to run ycsbcli")
    parser.add_argument('-o', dest='output_name', required=True,
                        help="output plot filename")
    parser.add_argument('--drop_caches', dest='drop_caches', action='store_true',
                        help="do drop_caches per request")
    parser.add_argument('pre_issue_depths', metavar='D', type=int, nargs='+',
                        help="pre_issue_depth to try")
    args = parser.parse_args()

    original_us, foreactor_us_list = run_exprs(args.libforeactor, args.dbdir,
                                               args.trace, args.drop_caches,
                                               args.pre_issue_depths)
    plot_time(args.pre_issue_depths, original_us, foreactor_us_list,
              args.output_name)

if __name__ == "__main__":
    main()
