import matplotlib
matplotlib.use('Agg')

import subprocess
import os
import matplotlib.pyplot as plt
import numpy as np


PRE_ISSUE_DEPTH_LIST = [1, 2, 3, 4, 5, 6, 7, 8]
NUM_REPEATS = 10


def run_ycsbcli_single(use_foreactor, uring_queue_len=0, pre_issue_depth=0):
    subprocess.run(["sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'"], check=True)

    envs = os.environ.copy()
    envs["LD_PRELOAD"] = "/home/josehu/Repos/sysopt/foreactor/foreactor/libforeactor.so"
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs["QUEUE_0"] = str(uring_queue_len)
    envs["DEPTH_0"] = str(pre_issue_depth)

    cmd = ["./ycsbcli", "-d", "/mnt/optane-ssd/josehu/leveldb_dbdir", "-f", "single-get.txt",
           "--bg_compact_off", "--no_fill_cache"]
    result = subprocess.run(cmd, check=True, capture_output=True, env=envs)
    output = result.stdout.decode('ascii')

    rm_seen = False
    for line in output.strip().split('\n'):
        line = line.strip()
        if line.startswith("removing top/bottom-5"):
            rm_seen = True
        elif rm_seen and line.startswith("avg"):
            return float(line.split()[-2])

def run_exprs(pre_issue_depth_list):
    original_us_r = 0.0
    for i in range(NUM_REPEATS):
        original_us_r += run_ycsbcli_single(False)
    original_us = original_us_r / NUM_REPEATS

    foreactor_us_list = []
    for pre_issue_depth in pre_issue_depth_list:
        foreactor_us_r = 0.0
        for i in range(NUM_REPEATS):
            foreactor_us_r += run_ycsbcli_single(True, 32, pre_issue_depth)
        foreactor_us_list.append(foreactor_us_r / NUM_REPEATS)

    return original_us, foreactor_us_list


def plot_time(pre_issue_depth_list, original_us, foreactor_us_list):
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

    plt.savefig("bench-single-cached.png", dpi=120)


def main():
    original_us, foreactor_us_list = run_exprs(PRE_ISSUE_DEPTH_LIST)
    plot_time(PRE_ISSUE_DEPTH_LIST, original_us, foreactor_us_list)

if __name__ == "__main__":
    main()
