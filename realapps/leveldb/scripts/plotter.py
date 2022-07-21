#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize


def read_samekey_us(results_dir, value_size, num_l0_tables, approx_num_preads,
                    backend, drop_caches, pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-{num_l0_tables}-samekey-"
              f"{approx_num_preads}-{backend}-"
              f"{'drop_caches' if drop_caches else 'cached'}.log") as flog:
        while True:
            line = flog.readline().strip()
            if line[:line.index(':')] == pre_issue_depth_str:
                avg_us = float(line.split()[4])
                return avg_us

def read_ycsbrun_us(results_dir, value_size, num_l0_tables, backend,
                    mem_percentage, pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-{num_l0_tables}-ycsbrun-"
              f"{backend}-mem_{mem_percentage}.log") as flog:
        while True:
            line = flog.readline().strip()
            if line[:line.index(':')] == pre_issue_depth_str:
                avg_us = float(line.split()[4])
                return avg_us


def plot_grouped_bars(results, x_list, x_label, output_prefix, title_suffix):
    plt.rcParams.update({'font.size': 18})
    plt.rcParams.update({'figure.figsize': (20, 7)})

    norm = Normalize(vmin=5, vmax=20)
    orig_color = "steelblue"
    orig_hatch = '//'
    cmaps = {
        "thread_pool": cm.BuGn,
        "io_uring_sqe_async": cm.OrRd,
    }
    edge_color = "black"

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(x_list))))
        ys = results[config]

        color = orig_color
        hatch = orig_hatch
        label = config
        if config != "original":
            segs = config.split('-')
            backend, pre_issue_depth = segs[0], int(segs[1])
            color = cmaps[backend](norm(pre_issue_depth))
            hatch = ''
            if config.startswith("io_uring"):
                label = f"foreactor-io_uring-{pre_issue_depth}"
            else:
                label = f"foreactor-thread_pool-{pre_issue_depth}"
        
        plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
                        edgecolor=edge_color, zorder=3)

        for x, y in zip(xs, ys):
            plt.text(x, y, f"{y:.1f}", ha="center", va="bottom",
                           fontsize=11)

    plt.xlabel(x_label)
    plt.ylabel("Avg. time per Get request (us)")

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/2),
                      range(len(x_list))))
    plt.xticks(xticks, x_list)

    plt.grid(axis='y', zorder=1)

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_mem_ratio(results_dir, output_prefix):
    VALUE_SIZES = ["16K"]
    MEM_PERCENTAGES = [20, 40, 60, 80, 100]
    NUM_L0_TABLES = 12
    BACKENDS = ["io_uring_sqe_async"]
    PRE_ISSUE_DEPTH_LIST = [8, 12, 15]

    for value_size in VALUE_SIZES:
        results = {"original": []}

        for mem_percentage in MEM_PERCENTAGES:
            avg_us = read_ycsbrun_us(results_dir, value_size, NUM_L0_TABLES,
                                     BACKENDS[0], mem_percentage, "orig")
            results["original"].append(avg_us)

            for backend in BACKENDS:
                for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                    config = f"{backend}-{pre_issue_depth}"
                    if config not in results:
                        results[config] = []
                    avg_us = read_ycsbrun_us(results_dir, value_size,
                                             NUM_L0_TABLES, backend,
                                             mem_percentage,
                                             str(pre_issue_depth))
                    results[config].append(avg_us)

        plot_grouped_bars(results, MEM_PERCENTAGES,
                          "Available memory vs. database volume (%)",
                          output_prefix, f"mem_ratio-req_{value_size}")

def handle_req_size(results_dir, output_prefix):
    VALUE_SIZES = ["1K", "4K", "16K", "64K", "256K"]
    MEM_PERCENTAGES = [20]
    NUM_L0_TABLES = 12
    BACKENDS = ["io_uring_sqe_async"]
    PRE_ISSUE_DEPTH_LIST = [8, 12, 15]

    for mem_percentage in MEM_PERCENTAGES:
        results = {"original": []}

        for value_size in VALUE_SIZES:
            avg_us = read_ycsbrun_us(results_dir, value_size, NUM_L0_TABLES,
                                     BACKENDS[0], mem_percentage, "orig")
            results["original"].append(avg_us)

            for backend in BACKENDS:
                for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                    config = f"{backend}-{pre_issue_depth}"
                    if config not in results:
                        results[config] = []
                    avg_us = read_ycsbrun_us(results_dir, value_size,
                                             NUM_L0_TABLES, backend,
                                             mem_percentage,
                                             str(pre_issue_depth))
                    results[config].append(avg_us)

        plot_grouped_bars(results, VALUE_SIZES,
                          "Database image value size (bytes)",
                          output_prefix, f"req_size-mem_{mem_percentage}")


def plot_heat_map(results, x_list, y_list, x_label, y_label, output_prefix):
    plt.rcParams.update({'font.size': 10})
    plt.rcParams.update({'figure.figsize': (4, 4)})

    # plot better improvement as greener
    vmin, vmax = -0.2, 0.4
    plt.imshow(np.transpose(results), origin="upper", cmap="RdYlGn",
                                      vmin=vmin, vmax=vmax)

    for i in range(len(x_list)):
        for j in range(len(y_list)):
            plt.text(i, j, f"{results[i, j]*100:.1f}",
                           ha="center", va="center", fontsize=10)

    plt.xlabel(x_label)
    plt.ylabel(y_label)

    plt.xticks(list(range(len(x_list))), x_list)
    plt.yticks(list(range(len(y_list))), y_list)

    cbar_ticks = list(np.arange(vmin+0.1, vmax, 0.1))
    cbar_labels = list(map(lambda p: f"{p*100:.0f}%", cbar_ticks))
    cbar = plt.colorbar(ticks=cbar_ticks, shrink=0.6)
    cbar.ax.set_yticklabels(cbar_labels)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-heat_map.png", dpi=200)
    plt.close()
    print(f"PLOT heat_map")

def handle_heat_map(results_dir, output_prefix):
    VALUE_SIZES = ["1K", "4K", "16K", "64K", "256K"]
    MEM_PERCENTAGES = [20, 40, 60, 80, 100]
    NUM_L0_TABLES = 12
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH = 15

    results = np.empty([len(MEM_PERCENTAGES), len(VALUE_SIZES)])

    for xi, mem_percentage in enumerate(MEM_PERCENTAGES):
        for yi, value_size in enumerate(VALUE_SIZES):
            original_us = read_ycsbrun_us(results_dir, value_size,
                                          NUM_L0_TABLES, BACKEND,
                                          mem_percentage, "orig")
            foreactor_us = read_ycsbrun_us(results_dir, value_size,
                                           NUM_L0_TABLES, BACKEND,
                                           mem_percentage,
                                           str(PRE_ISSUE_DEPTH))
            
            improvement = (original_us - foreactor_us) / original_us
            results[xi, yi] = improvement

    plot_heat_map(results, MEM_PERCENTAGES, VALUE_SIZES,
                  "Available memory vs. database volume (%)",
                  "Database image value size (bytes)",
                  output_prefix)


def plot_controlled(results, approx_nums_preads, cached_modes, output_prefix,
                    title_suffix):
    plt.rcParams.update({'font.size': 18})
    plt.rcParams.update({'figure.figsize': (20, 7)})

    norm = Normalize(vmin=0, vmax=20)
    orig_color = "steelblue"
    cmaps = {
        "thread_pool": cm.BuGn,
        "io_uring_sqe_async": cm.OrRd,
    }
    orig_marker = 'x'
    markers = {
        "io_uring_sqe_async": 'o',
        "thread_pool": '^',
    }

    xs = approx_nums_preads

    ymin, ymax = 0, 0
    for l in results.values():
        m = max(l)
        if m > ymax:
            ymax = m
    ymax *= 1.05

    for sp_idx, cached_str in enumerate(cached_modes):
        subplot_id = int(f"1{len(cached_modes)}{sp_idx+1}")
        plt.subplot(subplot_id)

        for config in results:
            if config[config.rindex('-')+1:] == cached_str:
                ys = results[config]

                color = orig_color
                marker = orig_marker
                label = config[:config.rindex('-')]
                if not config.startswith("original"):
                    segs = config.split('-')
                    backend, pre_issue_depth = segs[0], int(segs[1])
                    color = cmaps[backend](norm(pre_issue_depth))
                    marker = markers[backend]
                    if config.startswith("io_uring"):
                        label = f"foreactor-io_uring-{pre_issue_depth}"
                    else:
                        label = f"foreactor-thread_pool-{pre_issue_depth}"
                
                plt.plot(xs, ys, label=label, marker=marker, markersize=8,
                                 color=color, linewidth=2, zorder=3)

        plt.xlabel("Approximate #preads")
        if sp_idx == 0:
            plt.ylabel("Avg. time of Get request (us)")
        else:
            plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

        plt.xticks(list(range(1, max(approx_nums_preads)+1)))

        plt.ylim((ymin, ymax))

        plt.grid(axis='y', zorder=1)

        plt.title(cached_str.replace('_', '-').capitalize())

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_controlled(results_dir, output_prefix):
    VALUE_SIZES = ["16K"]
    NUM_L0_TABLES = 12
    BACKENDS = ["io_uring_sqe_async", "thread_pool"]
    PRE_ISSUE_DEPTH_LIST = [8, 15]

    for value_size in VALUE_SIZES:
        valid_points = []
        approx_nums_preads = list(range(1, NUM_L0_TABLES+2))
        results = {"original-cached": [], "original-non_cached": []}

        # plot cached first to filter out invalid points
        for drop_caches in (False, True):
            cached_str = "cached" if not drop_caches else "non_cached"

            for approx_num_preads in approx_nums_preads:
                avg_us = read_samekey_us(results_dir, value_size,
                                         NUM_L0_TABLES, approx_num_preads,
                                         BACKENDS[0], drop_caches, "orig")
                results[f"original-{cached_str}"].append(avg_us)

                for backend in BACKENDS:
                    for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                        config = f"{backend}-{pre_issue_depth}-{cached_str}"
                        if config not in results:
                            results[config] = []
                        avg_us = read_samekey_us(results_dir, value_size,
                                                 NUM_L0_TABLES,
                                                 approx_num_preads,
                                                 backend, drop_caches,
                                                 str(pre_issue_depth))
                        results[config].append(avg_us)

            # filter out invalid points where the number of preads is
            # apparently inaccurate
            if not drop_caches:
                assert len(valid_points) == 0
                last_us = 0.
                for idx in range(len(approx_nums_preads)):
                    avg_us = results[f"original-{cached_str}"][idx]
                    if avg_us >= last_us:
                        valid_points.append(idx)
                        last_us = avg_us
            else:
                assert len(valid_points) > 0

            approx_nums_preads = [n for i, n in enumerate(approx_nums_preads) \
                                  if i in valid_points]
            for k in results:
                results[k] = [us for i, us in enumerate(results[k]) \
                              if i in valid_points]

        plot_controlled(results, approx_nums_preads, ["cached", "non_cached"],
                        output_prefix, f"controlled-req_{value_size}")


def main():
    parser = argparse.ArgumentParser(description="LevelDB result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="mem_ratio|req_size|heat_map|controlled")
    parser.add_argument('-r', dest='results_dir', required=True,
                        help="input result logs directory")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="output plot filename prefix")
    args = parser.parse_args()

    if args.mode == 'mem_ratio':
        handle_mem_ratio(args.results_dir, args.output_prefix)
    elif args.mode == 'req_size':
        handle_req_size(args.results_dir, args.output_prefix)
    elif args.mode == 'heat_map':
        handle_heat_map(args.results_dir, args.output_prefix)
    elif args.mode == 'controlled':
        handle_controlled(args.results_dir, args.output_prefix)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
