#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt


def read_avg_time_for_depth(input_logs):
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

def plot_avg_time_for_depth(pre_issue_depth_list, original_us,
                            foreactor_us_list, output_name):
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
    plt.close()

def do_depth(input_logs, output_name):
    pre_issue_depth_list, original_us, foreactor_us_list = \
        read_avg_time_for_depth(input_logs)
    plot_avg_time_for_depth(pre_issue_depth_list, original_us,
                            foreactor_us_list, output_name)


def read_all_times_for_cdf(input_logs):
    assert len(input_logs) > 0
    input_prefix = input_logs[0][:input_logs[0].rindex('-')]
    for input_log in input_logs[1:]:
        assert input_log.startswith(input_prefix)

    depth_times = dict()

    for input_log in input_logs:
        pre_issue_depth = input_log[input_log.rindex('-')+1:input_log.rindex('.')]
        in_times_line = False

        with open(input_log, 'r') as flog:
            for line in flog.readlines():
                line = line.strip()
                if line.startswith("Sorted time elapsed:"):
                    in_times_line = True
                elif in_times_line:
                    times = [float(t) for t in line.split()]
                    times.sort()
                    depth_times[pre_issue_depth] = times
                    in_times_line = False

    print("Note: the largest value in each experiment is omitted to remove the\n"
          "      setup overhead on the first request")
    for l in depth_times.values():
        del l[-1]
    return depth_times

def plot_all_times_as_cdf(depth_times, output_name):
    max_time = max((l[-1] for l in depth_times.values()))
    xs = [float(t) for t in range(0, int(max_time) + 10, 10)]

    depths = [int(d) for d in depth_times.keys() if d != 'orig']
    depths.sort()
    depths = [str(d) for d in depths]
    depths = ['orig'] + depths

    for depth in depths:
        times = depth_times[depth]
        idx, xs, ys = 0, [], []
        for t in range(0, int(max_time) + 10, 10):
            xs.append(float(t))

            cum_cnt = 0 if len(ys) == 0 else ys[-1]
            while idx < len(times) and times[idx] < t:
                cum_cnt += 1
                idx += 1

            ys.append(cum_cnt)
            if idx >= len(times):
                break

        # convert to percentage
        ys = [cum_cnt / ys[-1] for cum_cnt in ys]

        plt.plot(xs, ys,
                 zorder=3, label=depth)

    plt.ylabel("CDF Percentage (%)")
    plt.xlabel("Time per Get (us)")

    plt.legend()

    plt.savefig(output_name, dpi=120)
    plt.close()

def do_cdf(input_logs, output_name):
    depth_times = read_all_times_for_cdf(input_logs)
    plot_all_times_as_cdf(depth_times, output_name)


def do_mem(input_logs, output_name):
    return


def do_skew(input_logs, output_name):
    return


def do_tail(input_logs, output_name):
    return


def main():
    parser = argparse.ArgumentParser(description="Benchmark result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which figure to plot: depth|cdf|mem|skew|tail")
    parser.add_argument('-o', dest='output_name', required=True,
                        help="output plot filename")
    parser.add_argument('input_logs', metavar='I', type=str, nargs='+',
                        help="list of input benchmark result logs")
    args = parser.parse_args()

    if args.mode == 'depth':
        do_depth(args.input_logs, args.output_name)
    elif args.mode == 'cdf':
        do_cdf(args.input_logs, args.output_name)
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
