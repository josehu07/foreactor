#!/usr/bin/env python3

import fractions
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

def read_with_writes_ops(results_dir, value_size, workload_name, ycsb_distribution, backend,
                         mem_percentage, pre_issue_depth_str):
    with open(f"{results_dir}/ldb-{value_size}-ycsb-{workload_name}-{ycsb_distribution}-" + \
              f"{backend}-mem_{mem_percentage}-threads_0.log", 'r') as flog:
        for line in flog:
            line = line.strip()
            if len(line) > 0 and line[:line.index(':')] == pre_issue_depth_str:
                sum_ops = float(line.split()[8])
                return sum_ops

def read_timer_fractions(results_dir, value_size, ycsb_distribution, backend,
                         mem_percentage, pre_issue_depth_str):
    # benchmarking with detailed timers on will results in substantially
    # different performance numbers; thus, the only meaningful information
    # is the fraction of time spent in each segment
    fractions = dict()
    with open(f"{results_dir}/ldb-{value_size}-ycsb-c-{ycsb_distribution}-"
              f"{backend}-mem_{mem_percentage}-with_timer.log") as flog:
        if pre_issue_depth_str == "orig":
            orig_total_us, last_total_us, sync_call_us = 0., 0., 0.
            for line in flog:
                line = line.strip()
                if last_total_us <= 0 and len(line) > 0 and \
                   line[:line.find(':')] == "orig":
                    orig_total_us = float(line.split()[4])
                elif last_total_us <= 0 and len(line) > 0 and \
                     line[:line.find(':')] != "orig":
                    last_total_us = float(line.split()[4])
                elif last_total_us > 0 and line.startswith("timer_us sync-call"):
                    sync_call_us = float(line.split()[2])
                    break
            if sync_call_us <= 0:
                print(f"Error: did not find any timer segment information in log")
                exit(1)
            sync_call_us += orig_total_us - last_total_us
            other_us = orig_total_us - sync_call_us
            fractions = {
                "sync-call": sync_call_us / orig_total_us,
                "other": other_us / orig_total_us
            }

        else:
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
                    fractions[seg_name] = seg_us / total_us
                elif in_correct_section and not line.startswith("timer_us"):
                    in_correct_section = False
                    break
            other_fraction = 1. - sum(fractions.values())
            fractions["other"] = other_fraction

    assert len(fractions) > 0
    assert sum(fractions.values()) == 1
    
    # we then multiply these fractions with the original timing numbers
    # when run without timers
    avg_us, _ = read_ycsb_c_run_us(results_dir, value_size, ycsb_distribution,
                                   backend, mem_percentage, pre_issue_depth_str)
    segments_us = {sn: fractions[sn] * avg_us for sn in fractions}
    return fractions, segments_us


def plot_grouped_bars(results, x_list, x_label, y_label, output_prefix,
                      title_suffix):
    plt.rcParams.update({'font.size': 16})
    plt.rcParams.update({'figure.figsize': (14, 7)})

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
                label = f"foreactor-io_uring-{pre_issue_depth}"
            else:
                label = f"foreactor-thread_pool-{pre_issue_depth}"
        
        plt.bar(xs, ys, width=1, label=label, color=color, hatch=hatch,
                        edgecolor=edge_color, zorder=3)

        max_ys = max(ys)
        if max_ys > overall_max_ys:
            overall_max_ys = max_ys

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+1.2) + idx,
                      range(len(x_list))))
        ys = results[config]
        for x, y in zip(xs, ys):
            shifted_y = y + 0.02 * (y / overall_max_ys) * overall_max_ys
            plt.text(x, shifted_y, f"{y:.1f}", ha="center", va="bottom",
                                               fontsize=12, rotation=90)

    plt.xlabel(x_label)
    plt.ylabel(y_label)

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/4),
                      range(len(x_list))))
    plt.xticks(xticks, x_list)

    plt.grid(axis='y', zorder=1)

    plt.ylim((0, overall_max_ys * 1.15))

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_mem_ratio(results_dir, output_prefix):
    VALUE_SIZES = ["1K"]
    MEM_PERCENTAGES = [(i+1)*10 for i in range(10)]
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

        plot_grouped_bars(results, x_list,
                          "Available memory vs. DB volume (%)",
                          "Avg. time per Get request (us)",
                          output_prefix, f"mem_ratio-req_{value_size}")

def handle_req_size(results_dir, output_prefix):
    VALUE_SIZES = ["128B", "256B", "512B", "1K", "2K", "4K", "8K", "16K", "32K", "64K"]
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

        plot_grouped_bars(results, x_list,
                          "Record value size (bytes)",
                          "Avg. time per Get request (us)",
                          output_prefix, f"req_size-mem_{mem_percentage}")

def handle_tail_lat(results_dir, output_prefix):
    VALUE_SIZES = ["128B", "256B", "512B", "1K", "2K", "4K", "8K", "16K", "32K", "64K"]
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

        plot_grouped_bars(results, x_list,
                          "Record value size (bytes)",
                          "P99 tail latency of Get request (us)",
                          output_prefix, f"tail_lat-mem_{mem_percentage}")


def plot_heat_map(dist_results, distributions, x_list, y_list, x_label, y_label,
                  output_prefix, title_suffix):
    plt.rcParams.update({'font.size': 16})
    plt.rcParams.update({'figure.figsize': (16, 6)})

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
                               ha="center", va="center", fontsize=10)

        if sp_idx == 0:
            plt.ylabel(y_label)

        plt.xticks(list(range(len(x_list))), x_list)
        plt.yticks(list(range(len(y_list))), y_list)

        plt.title(distribution.capitalize(), fontsize=16)

        sp_idx += 1

    plt.subplots_adjust(right=0.9)

    fig = plt.figure(1)
    fig.supxlabel(x_label, fontsize=16, va="center")

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
    VALUE_SIZES = ["128B", "256B", "512B", "1K", "2K", "4K", "8K", "16K", "32K", "64K"]
    MEM_PERCENTAGES = [(i+1)*10 for i in range(10)]
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


def plot_multithread(results, nums_threads, x_label, y_label, output_prefix,
                     title_suffix):
    plt.rcParams.update({'font.size': 16})
    plt.rcParams.update({'figure.figsize': (12, 7)})

    norm = Normalize(vmin=-5, vmax=30)
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
            label = f"foreactor-io_uring-{pre_issue_depth}"
        
        plt.plot(xs, ys, label=label, color=color, marker=marker,
                         linewidth=2.5, markersize=10, zorder=3)

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

    plt.xticks(xs, nums_threads)

    plt.xlim((min(xs) - 0.05*xs_span, max(xs) + 0.05*xs_span))
    plt.ylim((0, overall_max_ys * 1.1))

    plt.grid(axis='y', zorder=1)

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_multithread(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "1K"
    YCSB_DISTRIBUTION = "zipfian"
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [4, 16]
    MEM_PERCENTAGE = 10
    NUMS_THREADS = [i+1 for i in range(8)]

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
                     "Number of threads",
                     "Throughput (thousand ops/sec)",
                     output_prefix, f"multithread")


def plot_with_writes(results, workload_names, x_label, y_label, output_prefix,
                     title_suffix):
    plt.rcParams.update({'font.size': 16})
    plt.rcParams.update({'figure.figsize': (12, 7)})

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
            label = f"foreactor-io_uring-{pre_issue_depth}"
        
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
            shifted_y = y + 0.02 * (y / overall_max_ys) * overall_max_ys
            plt.text(x, shifted_y, f"{y:.1f}", ha="center", va="bottom",
                                               fontsize=12, rotation=90)

    if len(x_label) > 0:
        plt.xlabel(x_label)
    plt.ylabel(y_label)

    xticks = list(map(lambda x: x * (len(results)+1.2) + (len(results)/4),
                      range(len(workload_names))))
    xtick_labels = list(map(lambda n: f"YCSB-{n.upper()}", workload_names))
    plt.xticks(xticks, xtick_labels)

    plt.grid(axis='y', zorder=1)

    plt.ylim((0, overall_max_ys * 1.1))

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_with_writes(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "1K"
    YCSB_DISTRIBUTION = "zipfian"
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [16]
    MEM_PERCENTAGE = 10
    YCSB_WORKLOADS = ["a", "b", "c", "d", "e", "f"]

    y_scale = 1000.
    results = {"original": []}

    for workload_name in YCSB_WORKLOADS:
        sum_ops = read_with_writes_ops(results_dir, VALUE_SIZE_ABBR,
                                       workload_name, YCSB_DISTRIBUTION,
                                       BACKEND, MEM_PERCENTAGE, "orig")
        results["original"].append(sum_ops / y_scale)

        for pre_issue_depth in PRE_ISSUE_DEPTH_LIST:
            if str(pre_issue_depth) not in results:
                results[str(pre_issue_depth)] = []
            sum_ops = read_with_writes_ops(results_dir, VALUE_SIZE_ABBR,
                                           workload_name, YCSB_DISTRIBUTION,
                                           BACKEND, MEM_PERCENTAGE,
                                           str(pre_issue_depth))
            results[str(pre_issue_depth)].append(sum_ops / y_scale)

    plot_with_writes(results, YCSB_WORKLOADS,
                     "",
                     "Throughput (thousand ops/sec)",
                     output_prefix, f"with_writes")


def plot_breakdown(fractions_map, segments_us_map, segments, x_label, y_label,
                   output_prefix, title_suffix):
    plt.rcParams.update({'font.size': 16})
    plt.rcParams.update({'figure.figsize': (10, 7)})

    segments_cmap = {
        "peeking-algorithm":        "coral",
        "engine-submission":        "steelblue",
        "engine-completion":        "lightgreen",
        "reflecting-result":        "darkgreen",
        "clearing-cancellation":    "indianred",
        "synchronous-syscall":      "lightblue",
        "other":                    "lightgray"
    }
    segments_hatch = {
        "peeking-algorithm":        "xx",
        "engine-submission":        "||",
        "engine-completion":        "\\\\",
        "reflecting-result":        "++",
        "clearing-cancellation":    "oo",
        "synchronous-syscall":      "//",
        "other":                    ""
    }
    edge_color = "dimgray"

    xs = list(range(len(fractions_map)))
    bottom_ys = [0. for _ in xs]

    legend_handles, legend_labels = [], []

    for seg_name in segments[::-1]:
        ys = [segments_us_map[config][seg_name] \
              for config in fractions_map.keys()]

        color = segments_cmap[seg_name]
        hatch = segments_hatch[seg_name]
        
        h = plt.bar(xs, ys, width=0.4, bottom=bottom_ys, label=seg_name,
                            color=color, hatch=hatch, edgecolor=edge_color,
                            zorder=3)
        
        legend_handles.append(h)
        legend_labels.append(seg_name)
        for cidx, y in enumerate(ys):
            bottom_ys[cidx] += y

    if len(x_label) > 0:
        plt.xlabel(x_label)
    plt.ylabel(y_label)

    xtick_labels = list(fractions_map.keys())
    for cidx in range(len(xtick_labels)):
        if xtick_labels[cidx] != "original":
            xtick_labels[cidx] = "foreactor"
    plt.xticks(xs, xtick_labels)

    plt.grid(axis='y', zorder=1)

    plt.xlim((-0.8, len(fractions_map) - 0.2))
    plt.ylim((0., max(bottom_ys) * 1.1))

    plt.legend(reversed(legend_handles), reversed(legend_labels),
               loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{title_suffix}.png", dpi=200)
    plt.close()
    print(f"PLOT {title_suffix}")

def handle_breakdown(results_dir, output_prefix):
    VALUE_SIZE_ABBR = "1K"
    YCSB_DISTRIBUTION = "zipfian"
    BACKEND = "io_uring_sqe_async"
    PRE_ISSUE_DEPTH_LIST = [16]
    MEM_PERCENTAGE = 10

    SEGMENTS = [
        "peeking-algorithm",
        "engine-submission",
        "engine-completion",
        "reflecting-result",
        "clearing-cancellation",
        "synchronous-syscall",
        "other"
    ]
    
    def timer_segs_to_plot_segs(d):
        result = {sn: 0. for sn in SEGMENTS}
        for seg_name, seg_val in d.items():
            if seg_name == "get-frontier" or seg_name == "check-args" or \
               seg_name == "peek-algo" or seg_name == "push-forward":
                result["peeking-algorithm"] += seg_val
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

    plot_breakdown(fractions_map, segments_us_map, SEGMENTS,
                   "",
                   "Latency breakdown (us)",
                   output_prefix, f"breakdown")


def main():
    parser = argparse.ArgumentParser(description="LevelDB result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="mem_ratio|req_size|heat_map|breakdown|" + \
                             "multithread|with_writes")
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
    elif args.mode == 'breakdown':
        handle_breakdown(args.results_dir, args.output_prefix)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
