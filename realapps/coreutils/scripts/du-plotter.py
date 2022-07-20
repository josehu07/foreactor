#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize


def read_avgtime_ms(results_dir, num_dirs, file_count, backend,
                    pre_issue_depth_str):
    with open(f"{results_dir}/du-{num_dirs}-{file_count}-" + \
              f"{backend}.log") as flog:
        while True:
            line = flog.readline().strip()
            if line[:line.index(':')] == pre_issue_depth_str:
                avg_ms = float(line.split()[2])
                return avg_ms

def plot_avgtime_bars(results, file_counts, output_prefix):
    plt.rcParams.update({'font.size': 18})
    plt.rcParams.update({'figure.figsize': (20, 7)})

    norm = Normalize(vmin=0, vmax=100)
    orig_color = "steelblue"
    orig_hatch = '//'
    cmaps = {
        # "io_uring_default": cm.BuGn,
        "io_uring_default": cm.OrRd,
        "io_uring_sqe_async": cm.OrRd,
    }
    edge_color = "black"

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(file_counts))))
        ys = results[config]

        color = orig_color
        hatch = orig_hatch
        label = config
        if config != "original":
            segs = config.split('-')
            backend, pre_issue_depth = segs[0], int(segs[1])
            color = cmaps[backend](norm(pre_issue_depth))
            hatch = ''
            label = f"foreactor-io_uring-{pre_issue_depth}"
        
        plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
                        edgecolor=edge_color, zorder=3)

        for x, y in zip(xs, ys):
            plt.text(x, y, f"{y:.1f}", ha="center", va="bottom",
                           fontsize=14)

    plt.xlabel("Total #files")
    plt.ylabel("Completion time of du (ms)")

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/2),
                  range(len(file_counts))))
    plt.xticks(xticks, file_counts)

    plt.grid(axis='y', zorder=1)

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-avgtime.png", dpi=200)
    plt.close()

    print(f"PLOT avgtime")

def handle_avgtime(results_dir, output_prefix):
    NUM_DIRS = 4
    FILE_COUNTS = ["1k", "3k", "5k", "7k", "9k"]
    BACKENDS = ["io_uring_sqe_async"]
    PRE_ISSUE_DEPTH_LIST = [4, 16, 64]

    results = {"original": []}

    for file_count in FILE_COUNTS:
        avg_ms = read_avgtime_ms(results_dir, NUM_DIRS, file_count,
                                 BACKENDS[0], "orig")
        results["original"].append(avg_ms)

        for backend in BACKENDS:
            for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                config = f"{backend}-{pre_issue_depth}"
                if config not in results:
                    results[config] = []
                avg_ms = read_avgtime_ms(results_dir, NUM_DIRS, file_count,
                                         backend, str(pre_issue_depth))
                results[config].append(avg_ms)

    plot_avgtime_bars(results, FILE_COUNTS, output_prefix)


def main():
    parser = argparse.ArgumentParser(description="du stat result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which figure(s) to plot: avgtime")
    parser.add_argument('-r', dest='results_dir', required=True,
                        help="input result logs directory")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="output plot filename prefix")
    args = parser.parse_args()

    if args.mode == 'avgtime':
        handle_avgtime(args.results_dir, args.output_prefix)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()