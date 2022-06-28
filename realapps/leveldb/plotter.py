#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize


def read_samekey_us(input_logs_dir, value_size, num_l0_tables, approx_num_preads,
                    backend, drop_caches, pre_issue_depth_str):
    with open(f"{input_logs_dir}/result-{value_size}-{num_l0_tables}-samekey-"
              f"{approx_num_preads}-{backend}-"
              f"{'drop_caches' if drop_caches else 'cached'}.log") as flog:
        while True:
            line = flog.readline().strip()
            if line[:line.index(':')] == pre_issue_depth_str:
                avg_us = float(line.split()[4])
                return avg_us

def plot_samekey_setup(results, approx_nums_preads, output_prefix, db_setup, drop_caches):
    plt.rcParams.update({'font.size': 16})
    plt.rcParams.update({'figure.figsize': (12, 7)})

    norm = Normalize(vmin=-5, vmax=20)
    orig_color = "steelblue"
    cmaps = {
        "io_uring_sqe_async": cm.BuGn,
        "thread_pool": cm.OrRd,
    }
    orig_marker = 'x'
    markers = {
        "io_uring_sqe_async": 'o',
        "thread_pool": '^',
    }

    xs = approx_nums_preads
    
    for config in results:
        ys = results[config]

        color = orig_color
        marker = orig_marker

        if config != "original":
            segs = config.split('-')
            backend, pre_issue_depth = segs[0], int(segs[1])
            color = cmaps[backend](norm(pre_issue_depth))
            marker = markers[backend]
        
        plt.plot(xs, ys, label=config, marker=marker, markersize=6, color=color,
                         zorder=1)

    plt.xlabel("Approx. #preads")
    plt.ylabel("Time per Get (us)")

    plt.xticks(list(range(1, max(approx_nums_preads) + 1)))

    cache_str = "drop_caches" if drop_caches else "cached"
    plt.title(f"Database image: {db_setup} {cache_str}")

    plt.grid(axis='y')

    plt.legend(loc="center left", bbox_to_anchor=(1.1, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-{db_setup}-{cache_str}.png", dpi=120)
    plt.close()
    print(f"PLOT {db_setup}-{cache_str}")

def handle_samekey(input_logs_dir, output_prefix):
    value_sizes = ["256B", "512B", "1K", "2K", "4K", "8K", "16K", "32K", "64K", "128K",
                   "256K", "512K", "1M"]
    nums_l0_tables = [8, 12]
    backends = ["io_uring_sqe_async", "thread_pool"]
    pre_issue_depth_list = [4, 8, 12, 15]
    
    valid_points = dict()

    for drop_caches in (True, False):   # plot drop_caches first to filter out invalid points
        for value_size in value_sizes:
            for num_l0_tables in nums_l0_tables:
                approx_nums_preads = list(range(1, num_l0_tables+2+1))      # FIXME
                results = {"original": []}

                for approx_num_preads in approx_nums_preads:
                    avg_us = read_samekey_us(input_logs_dir, value_size,
                                             num_l0_tables, approx_num_preads,
                                             backends[0], drop_caches, "orig")
                    results["original"].append(avg_us)

                    for backend in backends:
                        for pre_issue_depth in pre_issue_depth_list:
                            config = f"{backend}-{pre_issue_depth}"
                            if config not in results:
                                results[config] = []
                            avg_us = read_samekey_us(input_logs_dir, value_size,
                                                     num_l0_tables, approx_num_preads,
                                                     backend, drop_caches,
                                                     str(pre_issue_depth))
                            results[config].append(avg_us)

                db_setup = f"{value_size}-{num_l0_tables}"

                # filter out invalid points where the number of preads is incorrect
                if drop_caches:
                    assert db_setup not in valid_points
                    valid_points[db_setup] = []
                    last_us = 0.
                    for idx in range(len(approx_nums_preads)):
                        avg_us = results["original"][idx]
                        if avg_us >= last_us:
                            valid_points[db_setup].append(idx)
                            last_us = avg_us
                else:
                    assert db_setup in valid_points

                approx_nums_preads = [n for i, n in enumerate(approx_nums_preads) \
                                      if i in valid_points[db_setup]]
                for k in results:
                    results[k] = [us for i, us in enumerate(results[k]) \
                                  if i in valid_points[db_setup]]

                plot_samekey_setup(results, approx_nums_preads, output_prefix,
                                   db_setup, drop_caches)


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
    plt.rcParams.update({'figure.figsize': (20, 7)})

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
        
        plt.bar(xs, ys, width=1, label=config, color=color, zorder=1)

    plt.xlabel("Database Image")
    plt.ylabel("Avg. Time per Get (us)")

    plt.title(f"{title}")

    xticks = list(map(lambda x: x * (len(results)+2) + (len(results)/2), range(len(db_setups))))
    plt.xticks(xticks, db_setups)

    plt.grid(axis='y')

    plt.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))

    plt.tight_layout()

    plt.savefig(f"{output_prefix}-ycsbrun-{title}.png", dpi=120)
    plt.close()
    print(f"PLOT ycsbrun-{title}")

def handle_ycsbrun(input_logs_dir, output_prefix):
    value_sizes = ["256B", "1K", "4K", "16K", "64K", "256K", "1M"]
    nums_l0_tables = [8, 12]
    backends = ["io_uring_sqe_async"]
    pre_issue_depth_list = [4, 8, 12]
    mem_percentages = [100, 75, 50, 25]

    for mem_percentage in mem_percentages:
        db_setups = []
        results = {"original": []}

        for value_size in value_sizes:
            for num_l0_tables in nums_l0_tables:
                avg_us = read_ycsbrun_us(input_logs_dir, value_size, num_l0_tables,
                                         backends[0], mem_percentage, "orig")
                results["original"].append(avg_us)
                db_setups.append(f"{value_size}-{num_l0_tables}")

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
    parser = argparse.ArgumentParser(description="LevelDB result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which figures to plot: samekey|ycsbrun")
    parser.add_argument('-i', dest='input_logs_dir', required=True,
                        help="input logs directory")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="output plot filename prefix")
    args = parser.parse_args()

    if args.mode == 'samekey':
        handle_samekey(args.input_logs_dir, args.output_prefix)
    elif args.mode == 'ycsbrun':
        handle_ycsbrun(args.input_logs_dir, args.output_prefix)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
