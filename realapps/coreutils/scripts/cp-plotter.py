#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize


def read_avgtime_ms(results_dir, num_files, file_size, backend,
                    pre_issue_depth_str):
    with open(f"{results_dir}/cp-{file_size}-{num_files}-" + \
              f"{backend}.log") as flog:
        while True:
            line = flog.readline().strip()
            if line[:line.index(':')] == pre_issue_depth_str:
                avg_ms = float(line.split()[2])
                return avg_ms


def plot_avgtime_bars(results, file_sizes, output_prefix):
    plt.rcParams.update({'font.size': 16})
    plt.rcParams.update({'figure.figsize': (10, 6)})

    norm = Normalize(vmin=0, vmax=26)
    orig_color = "steelblue"
    orig_hatch = '//'
    cmaps = {
        "io_uring_default": cm.BuGn,
        "io_uring_sqe_async": cm.OrRd,
    }
    edge_color = "black"

    overall_max_ys = 0.

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(file_sizes))))
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

        max_ys = max(ys)
        if max_ys > overall_max_ys:
            overall_max_ys = max_ys

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(file_sizes))))
        ys = results[config]
        for x, y in zip(xs, ys):
            shifted_y = y + 0.01 * overall_max_ys
            plt.text(x, shifted_y, f"{y:.1f}", ha="center", va="bottom",
                                               fontsize=12, rotation=90)

    plt.xlabel("Total file size (bytes)")
    plt.ylabel("Completion time of cp (s)")

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/4),
                      range(len(file_sizes))))
    plt.xticks(xticks, file_sizes)

    plt.grid(axis='y', zorder=1)

    plt.ylim((0, overall_max_ys * 1.15))

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-avgtime.png", dpi=200)
    plt.close()

    print(f"PLOT avgtime")

def handle_avgtime(results_dir, output_prefix):
    NUM_FILES = 8
    FILE_SIZES = ["2G", "4G", "8G", "16G"]
    BACKENDS = ["io_uring_sqe_async"]
    PRE_ISSUE_DEPTH_LIST = [4, 16]

    results = {"original": []}
    y_scale = 1000.0

    for file_size in FILE_SIZES:
        avg_ms = read_avgtime_ms(results_dir, NUM_FILES, file_size,
                                 BACKENDS[0], "orig")
        results["original"].append(avg_ms / y_scale)

        for backend in BACKENDS:
            for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                config = f"{backend}-{pre_issue_depth}"
                if config not in results:
                    results[config] = []
                avg_ms = read_avgtime_ms(results_dir, NUM_FILES, file_size,
                                         backend, str(pre_issue_depth))
                results[config].append(avg_ms / y_scale)

    plot_avgtime_bars(results, FILE_SIZES, output_prefix)


def main():
    parser = argparse.ArgumentParser(description="cp stat result plotting")
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
