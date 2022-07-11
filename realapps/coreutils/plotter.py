#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize


def read_ycsbrun_us(input_logs_dir, value_size, num_l0_tables, backend, mem_percentage,
                    pre_issue_depth_str):
    with open(f"{input_logs_dir}/result-{value_size}-{num_l0_tables}-ycsbrun-"
              f"{backend}-mem_{mem_percentage}.log") as flog:
        while True:
            line = flog.readline().strip()
            if line[:line.index(':')] == pre_issue_depth_str:
                avg_us = float(line.split()[4])
                return avg_us

def plot_ycsbrun_bars(results, db_setups, output_prefix, title):
    plt.rcParams.update({'font.size': 16})
    # plt.rcParams.update({'figure.figsize': (20, 7)})
    plt.rcParams.update({'figure.figsize': (12, 7)})

    norm = Normalize(vmin=-5, vmax=20)
    orig_color = "steelblue"
    cmaps = {
        "io_uring_sqe_async": cm.BuGn,
        "thread_pool": cm.OrRd,
    }

    for idx, config in enumerate(results.keys()):
        xs = list(map(lambda x: x * (len(results)+2) + idx, range(len(db_setups))))
        ys = results[config]

        color = orig_color

        if config != "original":
            segs = config.split('-')
            backend, pre_issue_depth = segs[0], int(segs[1])
            color = cmaps[backend](norm(pre_issue_depth))
        
        plt.bar(xs, ys, width=0.7, label=config, color=color, zorder=3)

    plt.xlabel("Database Image")
    plt.ylabel("Avg. Time per Get (us)")

    plt.title(f"{title}")

    xticks = list(map(lambda x: x * (len(results)+2) + (len(results)/2), range(len(db_setups))))
    plt.xticks(xticks, db_setups)

    plt.grid(axis='y', zorder=1)

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-ycsbrun-{title}.png", dpi=120)
    plt.close()
    print(f"PLOT ycsbrun-{title}")

def handle_avgtime(input_logs_dir, output_prefix):
    file_sizes = ["128M", "512M", "2G"]
    num_files = 4
    backends = ["io_uring_default", "io_uring_sqe_async"]
    pre_issue_depth_list = [2, 4, 8, 16, 32]

    for file_size in file_sizes:
        results = {"original": []}

        avg_us = read_avgtime_us(input_logs_dir, value_size, num_l0_tables,
                                 backends[0], mem_percentage, "orig")
        results["original"].append(avg_us)

        for backend in backends:
            for pre_issue_depth in pre_issue_depth_list:
                config = f"{backend}-{pre_issue_depth}"
                if config not in results:
                    results[config] = []
                avg_us = read_ycsbrun_us(input_logs_dir, value_size, num_l0_tables,
                                         backend, mem_percentage, str(pre_issue_depth))
                results[config].append(avg_us)

        plot_ycsbrun_bars(results, db_setups, output_prefix, f"mem_{mem_percentage}")


def main():
    parser = argparse.ArgumentParser(description="cp copy result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which figures to plot: avgtime")
    parser.add_argument('-i', dest='input_logs_dir', required=True,
                        help="input logs directory")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="output plot filename prefix")
    args = parser.parse_args()

    if args.mode == 'avgtime':
        handle_avgtime(args.input_logs_dir, args.output_prefix)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
