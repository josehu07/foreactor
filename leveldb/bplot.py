#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt


def read_time_depth(input_logs):
    assert len(input_logs) == 1
    original_us = 0.0
    pre_issue_depth_list, foreactor_us_list = [], []

    with open(input_logs[0], 'r') as flog:
        for line in flog.readlines():
            line = line.strip()
            if len(line) == 0:
                continue

            segs = line.split()
            assert len(segs) == 2
            if segs[0] == 'orig':
                original_us = float(segs[1])
            else:
                pre_issue_depth_list.append(int(segs[0]))
                foreactor_us_list.append(float(segs[1]))

    return pre_issue_depth_list, original_us, foreactor_us_list

def plot_time_depth(pre_issue_depth_list, original_us, foreactor_us_list,
                    output_name):
    width = 0.5
    xs = list(range(1, len(pre_issue_depth_list) + 1))

    plt.bar([0], [original_us], width,
            zorder=3, label="Original")
    plt.bar(xs, foreactor_us_list, width,
            zorder=3, label="Foreactor")

    plt.text(0, original_us, f'{original_us:.3f}',
             va='bottom', ha='center')
    for x, y in zip(xs, foreactor_us_list):
        plt.text(x, y, f'{y:.3f}',
                 va='bottom', ha='center')

    plt.ylabel("Time per Get (us)")
    plt.xticks([0] + xs, ['original'] + pre_issue_depth_list)
    plt.xlabel("pre_issue_depth")

    plt.grid(zorder=0, axis='y')

    plt.legend()

    plt.savefig(output_name, dpi=120)

def do_depth(input_logs, output_name):
    pre_issue_depth_list, original_us, foreactor_us_list = \
        read_time_depth(input_logs)
    plot_time_depth(pre_issue_depth_list, original_us, foreactor_us_list,
                    output_name)


def do_mem(input_logs, output_name):
    return


def do_skew(input_logs, output_name):
    return


def do_tail(input_logs, output_name):
    return


def main():
    parser = argparse.ArgumentParser(description="Benchmark result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which figure to plot: depth, mem, skew, or tail")
    parser.add_argument('-o', dest='output_name', required=True,
                        help="output plot filename")
    parser.add_argument('input_logs', metavar='I', type=str, nargs='+',
                        help="list of input benchmark result logs")
    args = parser.parse_args()

    if args.mode == 'depth':
        do_depth(args.input_logs, args.output_name)
    elif args.mode == 'mem':
        do_mem(args.input_logs, args.output_name)
    elif args.mode == 'skew':
        do_skew(args.input_logs, args.output_name)
    elif args.mode == 'tail':
        do_tail(args.input_logs, args.output_name)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
