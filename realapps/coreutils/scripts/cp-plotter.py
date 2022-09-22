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


def plot_throughput_bars(results, file_sizes, output_prefix):
    plt.rcParams.update({'font.size': 17})
    plt.rcParams.update({'figure.figsize': (4.8, 4)})

    norm = Normalize(vmin=0, vmax=26)
    orig_color = "steelblue"
    orig_hatch = '//'
    wcfr_color = "lightcyan"
    wcfr_hatch = '\\\\'
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

        color = orig_color if config == "original" else wcfr_color
        hatch = orig_hatch if config == "original" else wcfr_hatch
        label = config
        if config != "original" and config != "copy_file_range":
            segs = config.split('-')
            backend, pre_issue_depth = segs[0], int(segs[1])
            color = cmaps[backend](norm(pre_issue_depth))
            hatch = ''
            label = f"foreactor-{pre_issue_depth}"
        
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

    plt.xlabel("File size (bytes)")
    plt.ylabel("Throughput (copied MB/s)")

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/4),
                      range(len(file_sizes))))
    plt.xticks(xticks, file_sizes, fontsize=16)
    plt.yticks(fontsize=16)

    # plt.grid(axis='y', zorder=1)

    plt.ylim((0, overall_max_ys * 1.24))

    # plt.legend(mode="expand", ncol=3, loc="lower left",
    #            bbox_to_anchor=(0, 1.01, 1, 0.2),
    #            fontsize=16)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-throughput.pdf", dpi=200)
    plt.close()

    print(f"PLOT throughput")

    # replot a wider figure for the legend
    # plt.rcParams.update({'figure.figsize': (9, 4)})

    # for idx, config in enumerate(results.keys()):
    #     xs = list(map(lambda x: x * (len(results)+1.2) + idx,
    #                   range(len(file_sizes))))
    #     ys = results[config]

    #     color = orig_color if config == "original" else wcfr_color
    #     hatch = orig_hatch if config == "original" else wcfr_hatch
    #     label = config
    #     if config != "original" and config != "copy_file_range":
    #         segs = config.split('-')
    #         backend, pre_issue_depth = segs[0], int(segs[1])
    #         color = cmaps[backend](norm(pre_issue_depth))
    #         hatch = ''
    #         label = f"foreactor-{pre_issue_depth}"
        
    #     plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
    #                     edgecolor=edge_color, zorder=3)

    # plt.legend(mode="expand", ncol=2, loc="lower left",
    #            bbox_to_anchor=(0.15, 1.01, 0.7, 0.2),
    #            fontsize=16, frameon=False)

    # plt.tight_layout()

    # plt.savefig(f"{output_prefix}-legend.pdf", dpi=200)
    # plt.close()

def handle_throughput(results_dir, output_prefix):
    NUM_FILES = 8
    FILE_SIZES = {
        "2G": 2 * 1024 * 1024 * 1024,
        "4G": 4 * 1024 * 1024 * 1024,
        "8G": 8 * 1024 * 1024 * 1024,
        "16G": 16 * 1024 * 1024 * 1024
    }
    BACKENDS = ["io_uring_sqe_async"]
    PRE_ISSUE_DEPTH_LIST = [4, 16]

    results = {"original": [], "copy_file_range": []}
    y_scale = 1024 * 1024.0

    for file_size in FILE_SIZES:
        avg_ms = read_avgtime_ms(results_dir, NUM_FILES, file_size,
                                 BACKENDS[0], "orig")
        results["original"].append(
            (FILE_SIZES[file_size] / y_scale) / (avg_ms / 1000.0))

        avg_ms = read_avgtime_ms(results_dir, NUM_FILES, file_size,
                                 BACKENDS[0], "wcfr")
        results["copy_file_range"].append(
            (FILE_SIZES[file_size] / y_scale) / (avg_ms / 1000.0))

        for backend in BACKENDS:
            for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                config = f"{backend}-{pre_issue_depth}"
                if config not in results:
                    results[config] = []
                avg_ms = read_avgtime_ms(results_dir, NUM_FILES, file_size,
                                         backend, str(pre_issue_depth))
                results[config].append(
                    (FILE_SIZES[file_size] / y_scale) / (avg_ms / 1000.0))

    plot_throughput_bars(results, FILE_SIZES, output_prefix)


def main():
    parser = argparse.ArgumentParser(description="cp stat result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which figure(s) to plot: throughput")
    parser.add_argument('-r', dest='results_dir', required=True,
                        help="input result logs directory")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="output plot filename prefix")
    args = parser.parse_args()

    if args.mode == 'throughput':
        handle_throughput(args.results_dir, args.output_prefix)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
