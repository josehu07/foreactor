#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize


def read_avg_us(results_dir, degree, workload_name, backend,
                pre_issue_depth_str):
    with open(f"{results_dir}/bpt-{degree}-{workload_name}-{backend}.log") as flog:
        for line in flog:
            line = line.strip()
            if len(line) > 0 and line[:line.index(':')] == pre_issue_depth_str:
                avg_us = float(line.split()[4])
                return avg_us


def plot_throughput_bars(results, degrees, x_label, y_label,
                         output_prefix, title_suffix):
    plt.rcParams.update({'font.size': 17})
    plt.rcParams.update({'figure.figsize': (4.2, 4)})

    print(results)

    norm = Normalize(vmin=0, vmax=420)
    orig_color = "steelblue"
    orig_hatch = '//'
    cmap = cm.OrRd
    edge_color = "black"

    overall_max_ys = 0.

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(degrees))))
        ys = results[config]

        color = orig_color
        hatch = orig_hatch
        label = config
        if config != "original":
            pre_issue_depth = int(config)
            color = cmap(norm(pre_issue_depth))
            hatch = ''
            label = f"foreactor-{pre_issue_depth}"

        plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
                        edgecolor=edge_color, zorder=3)

        max_ys = max(ys)
        if max_ys > overall_max_ys:
            overall_max_ys = max_ys

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(degrees))))
        ys = results[config]
        for x, y in zip(xs, ys):
            shifted_y = y + 0.01 * overall_max_ys
            plt.text(x, shifted_y, f"{y:.1f}", ha="center", va="bottom",
                                               fontsize=12, rotation=90)

    plt.xlabel(x_label)
    plt.ylabel(y_label)

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/4),
                      range(len(degrees))))
    plt.xticks(xticks, degrees, fontsize=16)
    plt.yticks(fontsize=16)

    plt.gca().yaxis.get_major_locator().set_params(integer=True)

    # plt.grid(axis='y', zorder=1)

    plt.ylim((0, overall_max_ys * 1.22))

    # plt.legend(mode="expand", ncol=3, loc="lower left",
    #            bbox_to_anchor=(0, 1.01, 1, 0.2),
    #            fontsize=16)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.pdf", dpi=200)
    plt.close()

    print(f"PLOT {title_suffix}")

    # replot a wider figure for the legend
    plt.rcParams.update({'figure.figsize': (9, 4)})

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(degrees))))
        ys = results[config]

        color = orig_color
        hatch = orig_hatch
        label = config
        if config != "original":
            pre_issue_depth = int(config)
            color = cmap(norm(pre_issue_depth))
            hatch = ''
            label = f"foreactor-{pre_issue_depth}"
        
        plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
                        edgecolor=edge_color, zorder=3)

    plt.legend(mode="expand", ncol=3, loc="lower left",
               bbox_to_anchor=(0.1, 1.01, 0.8, 0.2),
               fontsize=16, frameon=False)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-legend.pdf", dpi=200)
    plt.close()


def handle_load_throughput(results_dir, output_prefix):
    DEGREES = [64, 128, 256, 510]
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [64, 256]
    NUM_LOAD_KEYS = 128 * 128 * 100

    results = {"original": []}

    y_scale = 1000000.

    for degree in DEGREES:    
        avg_us = read_avg_us(results_dir, degree, "load", BACKEND, "orig")
        results["original"].append(
            (NUM_LOAD_KEYS * 1000000. / y_scale) / avg_us)

        for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
            config = f"{pre_issue_depth}"
            if config not in results:
                results[config] = []
            avg_us = read_avg_us(results_dir, degree, "load", BACKEND,
                                 str(pre_issue_depth))
            results[config].append(
                (NUM_LOAD_KEYS * 1000000. / y_scale) / avg_us)

    plot_throughput_bars(results, DEGREES,
                         "Tree degree",
                         "Throughput (m records/s)",
                         output_prefix, f"load")

def handle_scan_throughput(results_dir, output_prefix):
    DEGREES = [64, 128, 256, 510]
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [64, 256]
    APPROX_SCAN_KEYS = int(128 * 128 * 100 * 0.9)

    results = {"original": []}

    y_scale = 1000000.

    for degree in DEGREES:    
        avg_us = read_avg_us(results_dir, degree, "scan", BACKEND, "orig")
        results["original"].append(
            (APPROX_SCAN_KEYS * 1000000. / y_scale) / avg_us)

        for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
            config = f"{pre_issue_depth}"
            if config not in results:
                results[config] = []
            avg_us = read_avg_us(results_dir, degree, "scan", BACKEND,
                                 str(pre_issue_depth))
            results[config].append(
                (APPROX_SCAN_KEYS * 1000000. / y_scale) / avg_us)

    plot_throughput_bars(results, DEGREES,
                         "Tree degree",
                         "Throughput (m records/s)",
                         output_prefix, f"scan")


def main():
    parser = argparse.ArgumentParser(description="BPTree result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="load_throughput|scan_throughput")
    parser.add_argument('-r', dest='results_dir', required=True,
                        help="input result logs directory")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="output plot filename prefix")
    args = parser.parse_args()

    if args.mode == 'load_throughput':
        handle_load_throughput(args.results_dir, args.output_prefix)
    elif args.mode == 'scan_throughput':
        handle_scan_throughput(args.results_dir, args.output_prefix)
    else:
        print(f"Error: mode {args.mode} unrecognized")
        exit(1)

if __name__ == "__main__":
    main()
