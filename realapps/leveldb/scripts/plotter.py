#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize


def read_samekey_us(results_dir, value_size, approx_num_preads, backend, drop_caches,
                    pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-samekey-{approx_num_preads}-{backend}-" + \
              f"{'drop_caches' if drop_caches else 'cached'}.log", 'r') as flog:
        for line in flog:
            line = line.strip()
            if len(line) > 0 and line[:line.index(':')] == pre_issue_depth_str:
                avg_us = float(line.split()[4])
                return avg_us

def read_ycsb_c_run_us(results_dir, value_size, ycsb_distribution, backend,
                       mem_percentage, pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-ycsb-c-{ycsb_distribution}-{backend}-" + \
              f"mem_{mem_percentage}-threads_0.log", 'r') as flog:
        for line in flog:
            line = line.strip()
            if len(line) > 0 and line[:line.index(':')] == pre_issue_depth_str:
                avg_us = float(line.split()[4])
                p99_us = float(line.split()[6])
                return avg_us, p99_us

def read_multithread_ops(results_dir, value_size, ycsb_distribution, num_threads, backend,
                         mem_percentage, pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-ycsb-c-{ycsb_distribution}-{backend}-" + \
              f"mem_{mem_percentage}-threads_{num_threads}.log", 'r') as flog:
        for line in flog:
            line = line.strip()
            if len(line) > 0 and line[:line.index(':')] == pre_issue_depth_str:
                sum_ops = float(line.split()[2])
                return sum_ops

def read_timer_fractions(results_dir, segments, value_size, num_l0_tables,
                         ycsb_distribution, backend, mem_percentage,
                         pre_issue_depth_str):
    # benchmarking with detailed timers on will results in substantially
    # different performance numbers; thus, the only meaningful information
    # is the fraction of time spent in each segment
    fractions = None
    with open(f"{results_dir}/ldb-{value_size}-{num_l0_tables}-{ycsb_distribution}-"
              f"ycsbrun-{backend}-mem_{mem_percentage}-with_timer.log") as flog:
        pass    # TODO
    assert len(fractions) == len(segments)
    assert sum(fractions) <= 1.
    # we then multiply these fractions with the original timing numbers
    # when run without timers
    avg_us, _ = read_ycsb_c_run_us(results_dir, value_size, ycsb_distribution,
                                   backend, mem_percentage, pre_issue_depth_str)
    segments_us = [fractions[i] * avg_us for i in range(len(fractions))]
    return fractions, segments_us


# def plot_grouped_bars(results, x_list, x_label, y_label, output_prefix,
#                       title_suffix):
#     plt.rcParams.update({'font.size': 16})
#     plt.rcParams.update({'figure.figsize': (15, 7)})

#     norm = Normalize(vmin=5, vmax=30)
#     orig_color = "steelblue"
#     orig_hatch = '//'
#     cmaps = {
#         "thread_pool": cm.BuGn,
#         "io_uring_sqe_async": cm.OrRd,
#     }
#     edge_color = "black"

#     for idx, config in enumerate(results.keys()):
#         xs = list(map(lambda x: x * (len(results)+1.2) + idx,
#                       range(len(x_list))))
#         ys = results[config]

#         color = orig_color
#         hatch = orig_hatch
#         label = config
#         if config != "original":
#             segs = config.split('-')
#             backend, pre_issue_depth = segs[0], int(segs[1])
#             color = cmaps[backend](norm(pre_issue_depth))
#             hatch = ''
#             if config.startswith("io_uring"):
#                 label = f"foreactor-io_uring-{pre_issue_depth}"
#             else:
#                 label = f"foreactor-thread_pool-{pre_issue_depth}"
        
#         plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
#                         edgecolor=edge_color, zorder=3)

#         for x, y in zip(xs, ys):
#             plt.text(x, y, f"{y:.1f}", ha="center", va="bottom",
#                            fontsize=10, rotation=15)

#     plt.xlabel(x_label)
#     plt.ylabel(y_label)

#     xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/2),
#                       range(len(x_list))))
#     plt.xticks(xticks, x_list)

#     plt.grid(axis='y', zorder=1)

#     plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

#     plt.tight_layout()

#     plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
#     plt.close()
#     print(f"PLOT {title_suffix}")

def plot_series_lines(results, x_list, x_label, y_label, output_prefix,
                      title_suffix):
    plt.rcParams.update({'font.size': 16})
    plt.rcParams.update({'figure.figsize': (12, 7)})

    norm = Normalize(vmin=5, vmax=22)
    orig_color = "steelblue"
    cmaps = {
        "thread_pool": cm.BuGn,
        "io_uring_sqe_async": cm.OrRd,
    }
    orig_marker = 'o'
    markers = {
        "thread_pool": 's',
        "io_uring_sqe_async": '^',
    }

    xs = list(range(len(x_list)))
    overall_max_ys = 0.

    for config in results.keys():
        ys = results[config]

        color = orig_color
        marker = orig_marker
        label = config
        if config != "original":
            segs = config.split('-')
            backend, pre_issue_depth = segs[0], int(segs[1])
            color = cmaps[backend](norm(pre_issue_depth))
            marker = markers[backend]
            if config.startswith("io_uring"):
                label = f"foreactor-io_uring-{pre_issue_depth}"
            else:
                label = f"foreactor-thread_pool-{pre_issue_depth}"
        
        plt.plot(xs, ys, label=label, color=color, marker=marker,
                         linewidth=2.5, markersize=8, zorder=3)

        max_ys = max(ys)
        if max_ys > overall_max_ys:
            overall_max_ys = max_ys

    for config in results.keys():
        ys = results[config]
        for x, y in zip(xs, ys):
            shifted_y = y + 0.03 * (y / overall_max_ys) * overall_max_ys
            plt.text(x, shifted_y, f"{y:.1f}", ha="center", va="bottom",
                                               fontsize=12)

    plt.xlabel(x_label)
    plt.ylabel(y_label)

    plt.xticks(xs, x_list)

    plt.grid(axis='y', zorder=1)

    plt.ylim((0, overall_max_ys * 1.1))

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_mem_ratio(results_dir, output_prefix):
    VALUE_SIZES = ["16K"]
    MEM_PERCENTAGES = [5, 10, 25, 50, 100]
    YCSB_DISTRIBUTION = "zipfian"
    BACKENDS = ["io_uring_sqe_async"]
    PRE_ISSUE_DEPTH_LIST = [16]

    for value_size in VALUE_SIZES:
        results = {"original": []}
        x_list = []
        
        for mem_percentage in MEM_PERCENTAGES:
            x_list.append(str(mem_percentage))

            avg_us, _ = read_ycsb_c_run_us(results_dir, value_size,
                                           YCSB_DISTRIBUTION, BACKENDS[0],
                                           mem_percentage, "orig")
            results["original"].append(avg_us)

            for backend in BACKENDS:
                for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                    config = f"{backend}-{pre_issue_depth}"
                    if config not in results:
                        results[config] = []
                    avg_us, _ = read_ycsb_c_run_us(results_dir, value_size,
                                                   YCSB_DISTRIBUTION, backend,
                                                   mem_percentage,
                                                   str(pre_issue_depth))
                    results[config].append(avg_us)

        plot_series_lines(results, x_list,
                          "Available memory vs. DB volume (%)",
                          "Avg. time per Get request (us)",
                          output_prefix, f"mem_ratio-req_{value_size}")

def handle_req_size(results_dir, output_prefix):
    VALUE_SIZES = ["256B", "1K", "4K", "16K", "64K"]
    MEM_PERCENTAGES = [10]
    YCSB_DISTRIBUTION = "zipfian"
    BACKENDS = ["io_uring_sqe_async"]
    PRE_ISSUE_DEPTH_LIST = [16]

    for mem_percentage in MEM_PERCENTAGES:
        results = {"original": []}
        x_list = []

        for value_size in VALUE_SIZES:
            x_list.append(value_size)

            avg_us, _ = read_ycsb_c_run_us(results_dir, value_size,
                                           YCSB_DISTRIBUTION, BACKENDS[0],
                                           mem_percentage, "orig")
            results["original"].append(avg_us)

            for backend in BACKENDS:
                for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                    config = f"{backend}-{pre_issue_depth}"
                    if config not in results:
                        results[config] = []
                    avg_us, _ = read_ycsb_c_run_us(results_dir, value_size,
                                                   YCSB_DISTRIBUTION, backend,
                                                   mem_percentage,
                                                   str(pre_issue_depth))
                    results[config].append(avg_us)

        plot_series_lines(results, x_list,
                          "Record value size (bytes)",
                          "Avg. time per Get request (us)",
                          output_prefix, f"req_size-mem_{mem_percentage}")

def handle_tail_lat(results_dir, output_prefix):
    VALUE_SIZES = ["256B", "1K", "4K", "16K", "64K"]
    MEM_PERCENTAGES = [10]
    YCSB_DISTRIBUTION = "zipfian"
    BACKENDS = ["io_uring_sqe_async"]
    PRE_ISSUE_DEPTH_LIST = [16]

    for mem_percentage in MEM_PERCENTAGES:
        results = {"original": []}
        x_list = []

        for value_size in VALUE_SIZES:
            x_list.append(value_size)

            _, p99_us = read_ycsb_c_run_us(results_dir, value_size,
                                           YCSB_DISTRIBUTION, BACKENDS[0],
                                           mem_percentage, "orig")
            results["original"].append(p99_us)

            for backend in BACKENDS:
                for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
                    config = f"{backend}-{pre_issue_depth}"
                    if config not in results:
                        results[config] = []
                    _, p99_us = read_ycsb_c_run_us(results_dir, value_size,
                                                   YCSB_DISTRIBUTION, backend,
                                                   mem_percentage,
                                                   str(pre_issue_depth))
                    results[config].append(p99_us)

        plot_series_lines(results, x_list,
                          "Record value size (bytes)",
                          "P99 tail latency of Get request (us)",
                          output_prefix, f"tail_lat-mem_{mem_percentage}")


def plot_heat_map(dist_results, distributions, x_list, y_list, x_label, y_label,
                  output_prefix, title_suffix):
    plt.rcParams.update({'font.size': 12})
    plt.rcParams.update({'figure.figsize': (10, 4)})

    vmin, vmax = -0.2, 0.5

    sp_idx = 0
    for distribution, results in zip(distributions, dist_results):
        subplot_id = int(f"1{len(distributions)}{sp_idx+1}")
        plt.subplot(subplot_id)

        # plot better improvement as greener
        plt.imshow(np.transpose(results), origin="upper", cmap="RdYlGn",
                                          vmin=vmin, vmax=vmax)

        for i in range(len(x_list)):
            for j in range(len(y_list)):
                plt.text(i, j, f"{results[i, j]*100:.1f}",
                               ha="center", va="center", fontsize=11)

        plt.xlabel(x_label)
        plt.ylabel(y_label)

        plt.xticks(list(range(len(x_list))), x_list)
        plt.yticks(list(range(len(y_list))), y_list)

        plt.title(distribution)

        sp_idx += 1

    plt.subplots_adjust(right=0.9)
    fig = plt.figure(1)
    cbar_ax = fig.add_axes([0.92, 0.15, 0.02, 0.7])
    cbar_ticks = list(np.arange(vmin+0.1, vmax, 0.1))
    cbar_labels = list(map(lambda p: f"{p*100:.0f}%", cbar_ticks))
    cbar = plt.colorbar(cax=cbar_ax, ticks=cbar_ticks, shrink=0.6)
    cbar.ax.set_yticklabels(cbar_labels)

    plt.savefig(f"{output_prefix}-{title_suffix}.png",
                dpi=200, bbox_inches='tight')
    plt.close()
    print(f"PLOT heat_map")

def handle_heat_map(results_dir, output_prefix):
    VALUE_SIZES = ["256B", "1K", "4K", "16K", "64K"]
    MEM_PERCENTAGES = [5, 10, 25, 50, 100]
    YCSB_DISTRIBUTIONS = ["zipfian", "uniform"]
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH = 16

    dist_results = []

    for ycsb_distribution in YCSB_DISTRIBUTIONS:
        results = np.empty([len(MEM_PERCENTAGES), len(VALUE_SIZES)])

        for xi, mem_percentage in enumerate(MEM_PERCENTAGES):
            for yi, value_size in enumerate(VALUE_SIZES):
                original_us, _ = read_ycsb_c_run_us(results_dir, value_size,
                                                    ycsb_distribution, BACKEND,
                                                    mem_percentage, "orig")
                foreactor_us, _ = read_ycsb_c_run_us(results_dir, value_size,
                                                     ycsb_distribution, BACKEND,
                                                     mem_percentage,
                                                     str(PRE_ISSUE_DEPTH))
                
                improvement = (original_us - foreactor_us) / original_us
                results[xi, yi] = improvement

        dist_results.append(results)

    plot_heat_map(dist_results, YCSB_DISTRIBUTIONS,
                  MEM_PERCENTAGES, VALUE_SIZES,
                  "Available memory vs. DB volume (%)",
                  "Record value size (bytes)",
                  output_prefix, f"heat_map")


# def plot_controlled(results, approx_nums_preads, cached_modes, output_prefix,
#                     title_suffix):
#     plt.rcParams.update({'font.size': 18})
#     plt.rcParams.update({'figure.figsize': (20, 7)})

#     norm = Normalize(vmin=0, vmax=20)
#     orig_color = "steelblue"
#     cmaps = {
#         "thread_pool": cm.BuGn,
#         "io_uring_sqe_async": cm.OrRd,
#     }
#     orig_marker = 'x'
#     markers = {
#         "thread_pool": '^',
#         "io_uring_sqe_async": 'o',
#     }

#     xs = approx_nums_preads

#     ymin, ymax = 0, 0
#     for l in results.values():
#         m = max(l)
#         if m > ymax:
#             ymax = m
#     ymax *= 1.05

#     for sp_idx, cached_str in enumerate(cached_modes):
#         subplot_id = int(f"1{len(cached_modes)}{sp_idx+1}")
#         plt.subplot(subplot_id)

#         for config in results:
#             if config[config.rindex('-')+1:] == cached_str:
#                 ys = results[config]

#                 color = orig_color
#                 marker = orig_marker
#                 label = config[:config.rindex('-')]
#                 if not config.startswith("original"):
#                     segs = config.split('-')
#                     backend, pre_issue_depth = segs[0], int(segs[1])
#                     color = cmaps[backend](norm(pre_issue_depth))
#                     marker = markers[backend]
#                     if config.startswith("io_uring"):
#                         label = f"foreactor-io_uring-{pre_issue_depth}"
#                     else:
#                         label = f"foreactor-thread_pool-{pre_issue_depth}"
                
#                 plt.plot(xs, ys, label=label, marker=marker, markersize=8,
#                                  color=color, linewidth=2, zorder=3)

#         plt.xlabel("Approximate #preads")
#         if sp_idx == 0:
#             plt.ylabel("Avg. time of Get request (us)")
#         else:
#             plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

#         plt.xticks(list(range(1, max(approx_nums_preads)+1)))

#         plt.ylim((ymin, ymax))

#         plt.grid(axis='y', zorder=1)

#         plt.title(cached_str.replace('_', '-').capitalize())

#     plt.tight_layout()

#     plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
#     plt.close()
#     print(f"PLOT {title_suffix}")

# def handle_controlled(results_dir, output_prefix):
#     VALUE_SIZES = ["16K"]
#     NUM_L0_TABLES = 12
#     YCSB_DISTRIBUTION = "uniform"
#     BACKENDS = ["io_uring_sqe_async"]
#     PRE_ISSUE_DEPTH_LIST = [15]

#     for value_size in VALUE_SIZES:
#         valid_points = []
#         approx_nums_preads = list(range(1, NUM_L0_TABLES+2))
#         results = {"original-cached": [], "original-non_cached": []}

#         # plot cached first to filter out invalid points
#         for drop_caches in (False, True):
#             cached_str = "cached" if not drop_caches else "non_cached"

#             for approx_num_preads in approx_nums_preads:
#                 avg_us = read_samekey_us(results_dir, value_size,
#                                          NUM_L0_TABLES, YCSB_DISTRIBUTION,
#                                          approx_num_preads, BACKENDS[0],
#                                          drop_caches, "orig")
#                 results[f"original-{cached_str}"].append(avg_us)

#                 for backend in BACKENDS:
#                     for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
#                         config = f"{backend}-{pre_issue_depth}-{cached_str}"
#                         if config not in results:
#                             results[config] = []
#                         avg_us = read_samekey_us(results_dir, value_size,
#                                                  NUM_L0_TABLES,
#                                                  YCSB_DISTRIBUTION,
#                                                  approx_num_preads,
#                                                  backend, drop_caches,
#                                                  str(pre_issue_depth))
#                         results[config].append(avg_us)

#             # filter out invalid points where the number of preads is
#             # apparently inaccurate
#             if not drop_caches:
#                 assert len(valid_points) == 0
#                 last_us = 0.
#                 for idx in range(len(approx_nums_preads)):
#                     avg_us = results[f"original-{cached_str}"][idx]
#                     if avg_us >= last_us:
#                         valid_points.append(idx)
#                         last_us = avg_us
#             else:
#                 assert len(valid_points) > 0

#             approx_nums_preads = [n for i, n in enumerate(approx_nums_preads) \
#                                   if i in valid_points]
#             for k in results:
#                 results[k] = [us for i, us in enumerate(results[k]) \
#                               if i in valid_points]

#         plot_controlled(results, approx_nums_preads, ["cached", "non_cached"],
#                         output_prefix, f"controlled-req_{value_size}")


def plot_multithread(results, num_threads, output_prefix, title_suffix):
    pass    # TODO

def handle_multithread(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "16K"
    NUM_L0_TABLES = 12
    YCSB_DISTRIBUTION = "uniform"
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [8, 15]
    MEM_PERCENTAGE = 100
    NUMS_THREADS = [2, 4, 8]

    results = {"original": []}

    for num_threads in NUMS_THREADS:
        sum_ops = read_multithread_ops(results_dir, VALUE_SIZE_ABBR,
                                       NUM_L0_TABLES, YCSB_DISTRIBUTION,
                                       num_threads, BACKEND, MEM_PERCENTAGE,
                                       "orig")
        results["original"].append(sum_ops)

        for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
            if str(pre_issue_depth) not in results:
                results[str(pre_issue_depth)] = []
            sum_ops = read_multithread_ops(results_dir, VALUE_SIZE_ABBR,
                                           NUM_L0_TABLES, YCSB_DISTRIBUTION,
                                           num_threads, BACKEND, MEM_PERCENTAGE,
                                           str(pre_issue_depth))
            results[str(pre_issue_depth)].append(sum_ops)

    plot_multithread(results, num_threads, output_prefix, f"multithread")


def handle_with_writes(results_dir, output_prefix):
    pass


def plot_breakdown(fractions_map, segments_us_map, segments, output_prefix,
                   title_suffix):
    pass    # TODO

def handle_breakdown(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "16K"
    NUM_L0_TABLES = 12
    YCSB_DISTRIBUTION = "uniform"
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [8, 15]
    MEM_PERCENTAGE = 60

    SEGMENTS = ["TODO"]     # TODO

    fractions, segments_us = read_timer_fractions(results_dir, SEGMENTS,
                                                  VALUE_SIZE_ABBR, NUM_L0_TABLES,
                                                  YCSB_DISTRIBUTION, BACKEND,
                                                  MEM_PERCENTAGE, "orig")
    fractions_map = {"original": fractions}
    segments_us_map = {"original": segments_us}

    for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
        fractions, segments_us = read_timer_fractions(results_dir, SEGMENTS,
                                                      VALUE_SIZE_ABBR, NUM_L0_TABLES,
                                                      YCSB_DISTRIBUTION, BACKEND,
                                                      MEM_PERCENTAGE,
                                                      str(pre_issue_depth))
        fractions_map[str(pre_issue_depth)] = fractions
        segments_us_map[str(pre_issue_depth)] = segments_us

    plot_breakdown(fractions_map, segments_us_map, SEGMENTS, output_prefix,
                   f"breakdown")


def main():
    parser = argparse.ArgumentParser(description="LevelDB result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="mem_ratio|req_size|heat_map|controlled|" + \
                             "breakdown|multithread|with_writes")
    parser.add_argument('-r', dest='results_dir', required=True,
                        help="input result logs directory")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="output plot filename prefix")
    args = parser.parse_args()

    if args.mode == 'mem_ratio':
        handle_mem_ratio(args.results_dir, args.output_prefix)
    elif args.mode == 'req_size':
        handle_req_size(args.results_dir, args.output_prefix)
    elif args.mode == 'tail_lat':
        handle_tail_lat(args.results_dir, args.output_prefix)
    elif args.mode == 'heat_map':
        handle_heat_map(args.results_dir, args.output_prefix)
    # elif args.mode == 'controlled':
    #     handle_controlled(args.results_dir, args.output_prefix)
    elif args.mode == 'multithread':
        handle_multithread(args.results_dir, args.output_prefix)
    elif args.mode == 'with_writes':
        handle_with_writes(args.results_dir, args.output_prefix)
    elif args.mode == 'breakdown':
        handle_breakdown(args.results_dir, args.output_prefix)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
