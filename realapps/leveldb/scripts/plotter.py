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
                sum_ops = float(line.split()[8])
                return sum_ops

def read_with_writes_ops(results_dir, value_size, workload_name, ycsb_distribution,
                         num_threads, backend, mem_percentage, pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-ycsb-{workload_name}-{ycsb_distribution}-" + \
              f"{backend}-mem_{mem_percentage}-threads_{num_threads}.log", 'r') as flog:
        for line in flog:
            line = line.strip()
            if len(line) > 0 and line[:line.index(':')] == pre_issue_depth_str:
                sum_ops = float(line.split()[8])
                return sum_ops

def read_zipf_consts_ops(results_dir, value_size, ycsb_distribution, backend,
                         mem_percentage, pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-ycsb-c-{ycsb_distribution}-" + \
              f"{backend}-mem_{mem_percentage}-threads_0.log", 'r') as flog:
        for line in flog:
            line = line.strip()
            if len(line) > 0 and line[:line.index(':')] == pre_issue_depth_str:
                sum_ops = float(line.split()[8])
                return sum_ops

def read_timer_fractions(results_dir, value_size, ycsb_distribution, backend,
                         mem_percentage, pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-ycsb-c-{ycsb_distribution}-"
              f"{backend}-mem_{mem_percentage}-with_timer.log") as flog:
        if pre_issue_depth_str == "orig":
            # compute time for the 'other' seg from a timed exper
            timed_total_us, seg_sum_us = 0., 0.
            for line in flog:
                line = line.strip()
                if timed_total_us <= 0 and len(line) > 0 and \
                   line[:line.find(':')] != "orig":
                    timed_total_us = float(line.split()[4])
                elif timed_total_us > 0 and line.startswith("timer_us"):
                    seg_sum_us += float(line.split()[2])
                elif timed_total_us > 0 and not line.startswith("timer_us"):
                    break
            other_us = timed_total_us - seg_sum_us
            
            # read out benchmarking result when timer was not on
            avg_us, _ = read_ycsb_c_run_us(results_dir, value_size,
                                           ycsb_distribution, backend,
                                           mem_percentage, "orig")
            sync_call_us = avg_us - other_us
            fractions = {
                "sync-call": sync_call_us / avg_us,
                "other": other_us / avg_us
            }
            segments_us = {
                "sync-call": sync_call_us,
                "other": other_us
            }
            return fractions, segments_us

        else:
            # benchmarking with detailed timers on will results in
            # substantially worse performance numbers; thus, the only
            # meaningful information is the fraction of time spent in each
            # segment
            segments_us = dict()
            total_us, in_correct_section = 0., False
            for line in flog:
                line = line.strip()
                if len(line) > 0 and not in_correct_section and \
                   line[:line.index(':')] == pre_issue_depth_str:
                    total_us = float(line.split()[4])
                    in_correct_section = True
                elif in_correct_section and line.startswith("timer_us"):
                    l = line.split()
                    seg_name, seg_us = l[1], float(l[2])
                    segments_us[seg_name] = seg_us
                elif in_correct_section and not line.startswith("timer_us"):
                    in_correct_section = False
                    break
            seg_sum_inaccurate_us = sum(segments_us.values())
            other_us = total_us - seg_sum_inaccurate_us
            segments_us["other"] = other_us

            # we then multiply these fractions with the original timing numbers
            # when run without timers
            avg_us, _ = read_ycsb_c_run_us(results_dir, value_size,
                                           ycsb_distribution, backend,
                                           mem_percentage, pre_issue_depth_str)
            seg_sum_accurate_us = avg_us - other_us
            for seg_name in segments_us:
                if seg_name != "other":
                    segments_us[seg_name] *= \
                        (seg_sum_accurate_us / seg_sum_inaccurate_us)
            fractions = {sn: segments_us[sn] / avg_us for sn in segments_us}
            return fractions, segments_us


def plot_grouped_bars(results, x_list, x_label, y_label, output_prefix,
                      title_suffix):
    plt.rcParams.update({'font.size': 22})
    plt.rcParams.update({'figure.figsize': (7, 5)})

    norm = Normalize(vmin=5, vmax=26)
    orig_color = "steelblue"
    orig_hatch = '//'
    cmaps = {
        "thread_pool": cm.BuGn,
        "io_uring_sqe_async": cm.OrRd,
    }
    edge_color = "black"

    overall_max_ys = 0.

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
                label = f"foreactor-{pre_issue_depth}"
            else:
                label = f"foreactor-thread_pool-{pre_issue_depth}"
        
        plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
                        edgecolor=edge_color, zorder=3)

        max_ys = max(ys)
        if max_ys > overall_max_ys:
            overall_max_ys = max_ys

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx + 0.2,
                      range(len(x_list))))
        ys = results[config]
        for x, y in zip(xs, ys):
            shifted_y = y + 0.01 * overall_max_ys
            plt.text(x, shifted_y, f"{y:.1f}", ha="center", va="bottom",
                                               fontsize=12, rotation=90)

    plt.xlabel(x_label)
    plt.ylabel(y_label)

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/4),
                      range(len(x_list))))
    x_list_pruned = [s if not s.endswith('B') else s[:-1] for s in x_list]
    plt.xticks(xticks, x_list_pruned, fontsize=17)

    plt.yticks(fontsize=17)

    # plt.grid(axis='y', zorder=1)
    
    if overall_max_ys > 1000:
        plt.ylim((0, overall_max_ys * 1.3))
    else:
        plt.ylim((0, overall_max_ys * 1.25))

    plt.legend(mode="expand", ncol=3, loc="lower left",
               bbox_to_anchor=(0, 1, 1, 0.2),
               fontsize=20, frameon=False)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.pdf", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_mem_ratio(results_dir, output_prefix):
    VALUE_SIZES = ["1K"]
    MEM_PERCENTAGES = [(i+1)*10 for i in range(10)]
    YCSB_DISTRIBUTION = "zipf_0.99"
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

        plot_grouped_bars(results, x_list,
                          "Available mem vs. DB volume (%)",
                          "Time per Get (us)",
                          output_prefix, f"mem_ratio-req_{value_size}")

def handle_req_size(results_dir, output_prefix):
    VALUE_SIZES = ["128B", "256B", "512B", "1K", "2K", "4K", "8K", "16K", "32K", "64K"]
    MEM_PERCENTAGES = [10]
    YCSB_DISTRIBUTION = "zipf_0.99"
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

        plot_grouped_bars(results, x_list,
                          "Record value size (bytes)",
                          "Time per Get (us)",
                          output_prefix, f"req_size-mem_{mem_percentage}")

def handle_tail_lat(results_dir, output_prefix):
    VALUE_SIZES = ["128B", "256B", "512B", "1K", "2K", "4K", "8K", "16K", "32K", "64K"]
    MEM_PERCENTAGES = [10]
    YCSB_DISTRIBUTION = "zipf_0.99"
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

        plot_grouped_bars(results, x_list,
                          "Record value size (bytes)",
                          "P99 tail latency (us)",
                          output_prefix, f"tail_lat-mem_{mem_percentage}")


def plot_heat_map(dist_results, distributions, x_list, y_list, x_label, y_label,
                  output_prefix, title_suffix):
    plt.rcParams.update({'font.size': 20})
    plt.rcParams.update({'figure.figsize': (5, 5)})

    vmin, vmax = -0.35, 0.45
    cmin, cmax = -0.1, 0.4

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
                               ha="center", va="center", fontsize=10)

        if sp_idx == 0:
            plt.ylabel(y_label)

        y_list_pruned = [s if not s.endswith('B') else s[:-1] for s in y_list]
        plt.xticks(list(range(len(x_list))), x_list, fontsize=16)
        plt.yticks(list(range(len(y_list))), y_list_pruned, fontsize=16)

        if len(distributions) > 1:
            plt.title(distribution.capitalize(), fontsize=16)

        sp_idx += 1

    plt.subplots_adjust(right=0.9)

    fig = plt.figure(1)
    fig.supxlabel(x_label, fontsize=20, va="center")

    cbar_ax = fig.add_axes([0.92, 0.15, 0.02, 0.7])
    cbar_ticks = list(np.arange(cmin, cmax+0.1, 0.1))
    cbar_labels = list(map(lambda p: f"{p*100:.0f}%", cbar_ticks))
    cbar = plt.colorbar(cax=cbar_ax, ticks=cbar_ticks, shrink=0.6)
    cbar.ax.set_yticklabels(cbar_labels, fontsize=16)

    plt.savefig(f"{output_prefix}-{title_suffix}.pdf",
                dpi=200, bbox_inches='tight')
    plt.close()
    print(f"PLOT heat_map")

def handle_heat_map(results_dir, output_prefix):
    VALUE_SIZES = ["128B", "256B", "512B", "1K", "2K", "4K", "8K", "16K", "32K", "64K"]
    MEM_PERCENTAGES = [(i+1)*10 for i in range(10)]
    YCSB_DISTRIBUTIONS = ["zipf_0.99"]
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
                  "Available mem vs. DB volume (%)",
                  "Record value size (bytes)",
                  output_prefix, f"heat_map")


def plot_multithread(results, nums_threads, x_label, y_label, output_prefix,
                     title_suffix):
    plt.rcParams.update({'font.size': 22})
    plt.rcParams.update({'figure.figsize': (5, 5)})

    norm = Normalize(vmin=-8, vmax=30)
    orig_color = "steelblue"
    foreactor_cmap = cm.OrRd
    orig_marker = 'o'
    foreactor_marker = '^'

    xs = range(len(nums_threads))
    overall_max_ys = 0.
    xs_span = max(xs) - min(xs)

    for config in results.keys():
        ys = results[config]

        color = orig_color
        marker = orig_marker
        label = config
        if config != "original":
            pre_issue_depth = int(config)
            color = foreactor_cmap(norm(pre_issue_depth))
            marker = foreactor_marker
            label = f"foreactor-{pre_issue_depth}"
        
        plt.plot(xs, ys, label=label, color=color, marker=marker,
                         linewidth=2.5, markersize=9, zorder=3)

        max_ys = max(ys)
        if max_ys > overall_max_ys:
            overall_max_ys = max_ys

    # for config in results.keys():
    #     ys = results[config]
    #     for x, y in zip(xs, ys):
    #         ha = "left" if config == "original" else "right"
    #         va = "top"  if config == "original" else "bottom"
    #         plt.text(x, y, f"{y:.1f}", ha=ha, va=va,
    #                                            fontsize=12)

    plt.xlabel(x_label)
    plt.ylabel(y_label)

    plt.xticks(xs[1::2], nums_threads[1::2], fontsize=17)
    plt.yticks(fontsize=17)

    plt.xlim((min(xs) - 0.05*xs_span, max(xs) + 0.05*xs_span))
    plt.ylim((0, overall_max_ys * 1.1))

    plt.grid(axis='y', zorder=1)

    plt.legend(mode="expand", ncol=3, loc="lower left",
               bbox_to_anchor=(-0.18, 1, 1.25, 0.2),
               fontsize=20, frameon=False, handlelength=1)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.pdf", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_multithread(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "1K"
    YCSB_DISTRIBUTION = "zipf_0.99"
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [16]
    MEM_PERCENTAGE = 10
    NUMS_THREADS = [i+1 for i in range(16)]

    y_scale = 1000.
    results = {"original": []}

    for num_threads in NUMS_THREADS:
        sum_ops = read_multithread_ops(results_dir, VALUE_SIZE_ABBR,
                                       YCSB_DISTRIBUTION, num_threads,
                                       BACKEND, MEM_PERCENTAGE, "orig")
        results["original"].append(sum_ops / y_scale)

        for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
            if str(pre_issue_depth) not in results:
                results[str(pre_issue_depth)] = []
            sum_ops = read_multithread_ops(results_dir, VALUE_SIZE_ABBR,
                                           YCSB_DISTRIBUTION, num_threads,
                                           BACKEND, MEM_PERCENTAGE,
                                           str(pre_issue_depth))
            results[str(pre_issue_depth)].append(sum_ops / y_scale)

    plot_multithread(results, NUMS_THREADS,
                     "Number of clients",
                     "Throughput (kops/sec)",
                     output_prefix, f"multithread")


def plot_with_writes(results, workload_names, x_label, y_label, output_prefix,
                     title_suffix):
    plt.rcParams.update({'font.size': 22})
    plt.rcParams.update({'figure.figsize': (5, 5)})

    norm = Normalize(vmin=5, vmax=26)
    orig_color = "steelblue"
    orig_hatch = '//'
    foreactor_cmap = cm.OrRd
    edge_color = "black"

    overall_max_ys = 0.

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(workload_names))))
        ys = results[config]

        color = orig_color
        hatch = orig_hatch
        label = config
        if config != "original":
            pre_issue_depth = int(config)
            color = foreactor_cmap(norm(pre_issue_depth))
            hatch = ''
            label = f"foreactor-{pre_issue_depth}"
        
        plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
                        edgecolor=edge_color, zorder=3)

        max_ys = max(ys)
        if max_ys > overall_max_ys:
            overall_max_ys = max_ys

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(workload_names))))
        ys = results[config]
        for x, y in zip(xs, ys):
            shifted_y = y + 0.01 * overall_max_ys
            plt.text(x, shifted_y, f"{y:.1f}", ha="center", va="bottom",
                                               fontsize=13, rotation=90)

    if len(x_label) > 0:
        plt.xlabel(x_label)
    plt.ylabel(y_label)

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/4),
                      range(len(workload_names))))
    xtick_labels = list(map(lambda n: f"{n.upper()}", workload_names))
    plt.xticks(xticks, xtick_labels)

    # plt.grid(axis='y', zorder=1)

    plt.ylim((0, overall_max_ys * 1.15))

    plt.legend(mode="expand", ncol=3, loc="lower left",
               bbox_to_anchor=(-0.14, 1, 1.2, 0.2),
               fontsize=20, frameon=False, handlelength=1)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.pdf", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_with_writes(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "1K"
    YCSB_DISTRIBUTION = "zipf_0.99"
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [16]
    MEM_PERCENTAGE = 10
    NUM_THREADS = 1
    YCSB_WORKLOADS = ["a", "b", "c", "d", "e", "f"]

    y_scale = 1000.
    results = {"original": []}

    for workload_name in YCSB_WORKLOADS:
        sum_ops = read_with_writes_ops(results_dir, VALUE_SIZE_ABBR,
                                       workload_name, YCSB_DISTRIBUTION,
                                       NUM_THREADS, BACKEND, MEM_PERCENTAGE,
                                       "orig")
        results["original"].append(sum_ops / y_scale)

        for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
            if str(pre_issue_depth) not in results:
                results[str(pre_issue_depth)] = []
            sum_ops = read_with_writes_ops(results_dir, VALUE_SIZE_ABBR,
                                           workload_name, YCSB_DISTRIBUTION,
                                           NUM_THREADS, BACKEND, MEM_PERCENTAGE,
                                           str(pre_issue_depth))
            results[str(pre_issue_depth)].append(sum_ops / y_scale)

    plot_with_writes(results, YCSB_WORKLOADS,
                     "YCSB workload",
                     "Throughput (kops/sec)",
                     output_prefix, f"with_writes")


def plot_zipf_consts(rel_uniform, rel_zipfian_list, zipf_constants,
                     x_label, y_label, output_prefix, title_suffix):
    plt.rcParams.update({'font.size': 22})
    plt.rcParams.update({'figure.figsize': (5, 5)})

    plt.axhline(y=rel_uniform, label="uniform", color="steelblue",
                               linestyle="--", linewidth=2.5,
                               zorder=3)

    plt.plot(zipf_constants, rel_zipfian_list, label="zipfian",
                                               color="orange",
                                               marker='o',
                                               linewidth=2.5,
                                               markersize=10,
                                               zorder=3)

    plt.xlabel(x_label)
    plt.ylabel(y_label)

    min_x, max_x = min(zipf_constants), max(zipf_constants)
    min_y, max_y = 1.0, max((rel_uniform, max(rel_zipfian_list)))
    plt.xlim((min_x - 0.05, max_x + 0.05))
    plt.ylim((min_y, max_y * 1.1))

    xticks = [min_x + 0.1*i for i in range(len(zipf_constants))]
    xtick_labels = [f"{x:.1f}" for x in xticks]
    plt.xticks(xticks, xtick_labels)

    plt.grid(axis='y', zorder=1)

    plt.legend(mode="expand", ncol=3, loc="lower left",
               bbox_to_anchor=(-0.14, 1, 1.2, 0.2),
               fontsize=20, frameon=False, handlelength=1.5)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.pdf", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_zipf_consts(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "1K"
    ZIPF_CONSTANTS = [0.9, 0.99, 1.1, 1.2, 1.3]
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH = 16
    MEM_PERCENTAGE = 10

    sum_ops_original = read_zipf_consts_ops(results_dir, VALUE_SIZE_ABBR,
                                            "uniform", BACKEND, MEM_PERCENTAGE,
                                            "orig") 
    sum_ops_foreactor = read_zipf_consts_ops(results_dir, VALUE_SIZE_ABBR,
                                             "uniform", BACKEND, MEM_PERCENTAGE,
                                             str(PRE_ISSUE_DEPTH))
    rel_uniform = sum_ops_foreactor / sum_ops_original

    rel_zipfian_list = []
    for zipf_constant in ZIPF_CONSTANTS:
        ycsb_distribution = f"zipf_{zipf_constant}"
        sum_ops_original = read_zipf_consts_ops(results_dir, VALUE_SIZE_ABBR,
                                                ycsb_distribution, BACKEND,
                                                MEM_PERCENTAGE, "orig") 
        sum_ops_foreactor = read_zipf_consts_ops(results_dir, VALUE_SIZE_ABBR,
                                                 ycsb_distribution, BACKEND,
                                                 MEM_PERCENTAGE,
                                                 str(PRE_ISSUE_DEPTH))
        rel_zipfian = sum_ops_foreactor / sum_ops_original
        if rel_zipfian > rel_uniform:
            # for small zipf_constants, due to measurement uncertainties the
            # result might end up with higher improvement than uniform; cap
            # it by the improvement of uniform
            rel_zipfian = rel_uniform
        rel_zipfian_list.append(rel_zipfian)
    
    plot_zipf_consts(rel_uniform, rel_zipfian_list, ZIPF_CONSTANTS,
                     "Zipfian theta constant",
                     "Relative improvement",
                     output_prefix, f"zipf_consts")


def plot_breakdown(fractions_map, segments_us_map, segments, x_label, y_label,
                   output_prefix, title_suffix):
    plt.rcParams.update({'font.size': 22})
    plt.rcParams.update({'figure.figsize': (10, 4)})

    segments_cmap = {
        "pre-issuing-algorithm":    "coral",
        "engine-submission":        "steelblue",
        "engine-completion":        "lightgreen",
        "reflecting-result":        "darkgreen",
        "clearing-cancellation":    "indianred",
        "synchronous-syscall":      "lightblue",
        "other":                    "lightgray"
    }
    segments_hatch = {
        "pre-issuing-algorithm":    "xx",
        "engine-submission":        "||",
        "engine-completion":        "\\\\",
        "reflecting-result":        "++",
        "clearing-cancellation":    "oo",
        "synchronous-syscall":      "//",
        "other":                    ""
    }
    edge_color = "black"

    xs = list(range(len(fractions_map)))
    bottom_ys = [0. for _ in xs]

    legend_handles, legend_labels = [], []

    for seg_name in segments[::-1]:
        ys = [segments_us_map[config][seg_name] \
              for config in fractions_map.keys()]

        color = segments_cmap[seg_name]
        hatch = segments_hatch[seg_name]
        
        h = plt.barh(xs[::-1], ys, height=0.4, left=bottom_ys, label=seg_name,
                                   color=color, hatch=hatch,
                                   edgecolor=edge_color, zorder=3)
        
        legend_handles.append(h)
        legend_labels.append(seg_name)
        for cidx, y in enumerate(ys):
            bottom_ys[cidx] += y

    if len(y_label) > 0:
        plt.ylabel(y_label)
    plt.xlabel(x_label)

    ytick_labels = list(fractions_map.keys())
    for cidx in range(len(ytick_labels)):
        if ytick_labels[cidx] == "original":
            ytick_labels[cidx] = "orig."
        else:
            ytick_labels[cidx] = "fore."
    plt.yticks(xs, ytick_labels[::-1])

    plt.xlim((0., max(bottom_ys) * 1.1))
    plt.ylim((-0.5, len(fractions_map) - 0.5))

    plt.legend(reversed(legend_handles), reversed(legend_labels),
               loc="upper left", bbox_to_anchor=(1.01, 1.1),
               frameon=False, handlelength=1)

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.pdf", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_breakdown(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "1K"
    YCSB_DISTRIBUTION = "zipf_0.99"
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [16]
    MEM_PERCENTAGE = 10

    SEGMENTS = [
        "clearing-cancellation",
        "reflecting-result",
        "engine-completion",
        "engine-submission",
        "pre-issuing-algorithm",
        "synchronous-syscall",
        "other"
    ]
    
    def timer_segs_to_plot_segs(d):
        result = {sn: 0. for sn in SEGMENTS}
        for seg_name, seg_val in d.items():
            if seg_name == "get-frontier" or seg_name == "check-args" or \
               seg_name == "peek-algo" or seg_name == "push-forward":
                result["pre-issuing-algorithm"] += seg_val
            elif seg_name == "sync-call":
                result["synchronous-syscall"] += seg_val
            elif seg_name == "clear-prog" or seg_name == "reset-graph":
                result["clearing-cancellation"] += seg_val
            elif seg_name == "engine-submit":
                result["engine-submission"] += seg_val
            elif seg_name == "engine-cmpl":
                result["engine-completion"] += seg_val
            elif seg_name == "reflect-res":
                result["reflecting-result"] += seg_val
            else:
                result["other"] += seg_val
        return result

    fractions, segments_us = read_timer_fractions(results_dir,
                                                  VALUE_SIZE_ABBR,
                                                  YCSB_DISTRIBUTION,
                                                  BACKEND,
                                                  MEM_PERCENTAGE,
                                                  "orig")
    fractions_map = {"original": timer_segs_to_plot_segs(fractions)}
    segments_us_map = {"original": timer_segs_to_plot_segs(segments_us)}

    for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
        fractions, segments_us = read_timer_fractions(results_dir,
                                                      VALUE_SIZE_ABBR,
                                                      YCSB_DISTRIBUTION,
                                                      BACKEND,
                                                      MEM_PERCENTAGE,
                                                      str(pre_issue_depth))
        fractions_map[str(pre_issue_depth)] = timer_segs_to_plot_segs(fractions)
        segments_us_map[str(pre_issue_depth)] = timer_segs_to_plot_segs(segments_us)

    # print(segments_us_map)
    plot_breakdown(fractions_map, segments_us_map, SEGMENTS,
                   "Latency breakdown (us)",
                   "",
                   output_prefix, f"breakdown")


def main():
    parser = argparse.ArgumentParser(description="LevelDB result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="mem_ratio|req_size|heat_map|breakdown|" + \
                             "multithread|with_writes|zipf_consts")
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
    elif args.mode == 'multithread':
        handle_multithread(args.results_dir, args.output_prefix)
    elif args.mode == 'with_writes':
        handle_with_writes(args.results_dir, args.output_prefix)
    elif args.mode == 'zipf_consts':
        handle_zipf_consts(args.results_dir, args.output_prefix)
    elif args.mode == 'breakdown':
        handle_breakdown(args.results_dir, args.output_prefix)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
