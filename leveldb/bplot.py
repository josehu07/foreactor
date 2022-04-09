#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt


def read_avg_time_for_depth(input_logs):
    assert len(input_logs) > 0
    input_prefix = input_logs[0][:input_logs[0].rindex('-')]
    for input_log in input_logs[1:]:
        assert input_log.startswith(input_prefix)

    depth_avg_time = dict()

    for input_log in input_logs:
        pre_issue_depth = input_log[input_log.rindex('-')+1:input_log.rindex('.')]
        in_timing_section = False

        with open(input_log, 'r') as flog:
            for line in flog.readlines():
                line = line.strip()
                if line.startswith("Removing top/bottom-5:"):
                    in_timing_section = True
                elif in_timing_section and line.startswith("avg "):
                    time = float(line.split()[1])
                    depth_avg_time[pre_issue_depth] = time
                    break

    return depth_avg_time

def plot_avg_time_for_depth(depth_avg_time, output_name):
    width = 0.5
    xs = range(len(depth_avg_time))

    depths = [int(d) for d in depth_avg_time.keys() if d != 'orig']
    depths.sort()
    depths = ['orig'] + [str(d) for d in depths]
    ys = [depth_avg_time[d] for d in depths]

    plt.bar(xs[:1], ys[:1], width,
            zorder=3, label="Original")
    plt.bar(xs[1:], ys[1:], width,
            zorder=3, label="Foreactor")

    for x, y in zip(xs, ys):
        plt.text(x, y, f'{y:.3f}',
                 va='bottom', ha='center')

    plt.ylabel("Time per Get (us)")
    plt.xticks(xs, depths)
    plt.xlabel("pre_issue_depth")

    plt.grid(zorder=0, axis='y')

    plt.legend()

    plt.savefig(output_name, dpi=120)
    plt.close()

def do_depth(input_logs, output_name):
    depth_avg_time = read_avg_time_for_depth(input_logs)
    plot_avg_time_for_depth(depth_avg_time, output_name)


def do_mem(input_logs, output_name):
    # TODO: implement this
    return


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
                    break

    print("Note: the largest value in each experiment is omitted to remove the"
          " setup overhead on the first request\n")
    for l in depth_times.values():
        del l[-1]
    return depth_times

def plot_all_times_as_cdf(depth_times, output_name):
    max_time = max((l[-1] for l in depth_times.values()))

    depths = [int(d) for d in depth_times.keys() if d != 'orig']
    depths.sort()
    depths = ['orig'] + [str(d) for d in depths]

    print(f"{'depth':>5s} {'95':>8s} {'99':>8s} {'99.999':>8s}")
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

        ys = [cum_cnt / ys[-1] for cum_cnt in ys]   # convert to percentage
        tail_95 = times[int(len(times) * 0.95) - 1]
        tail_99 = times[int(len(times) * 0.99) - 1]
        tail_99_999 = times[int(len(times) * 0.99999) - 1]
        print(f"{depth:>5s} {tail_95:>8.3f} {tail_99:>8.3f} {tail_99_999:>8.3f}")

        p = plt.plot(xs, ys,
                     zorder=3, label=depth)

        plt.vlines(x=xs[-1], ymin=0.97, ymax=1.03,
                   color=p[0].get_color())

    plt.ylabel("CDF Percentage (%)")
    plt.xlabel("Time per Get (us)")

    plt.grid(zorder=0, axis='x')

    plt.legend()

    plt.savefig(output_name, dpi=120)
    plt.close()

def do_cdf(input_logs, output_name):
    depth_times = read_all_times_for_cdf(input_logs)
    plot_all_times_as_cdf(depth_times, output_name)


def do_tail(input_logs, output_name):
    # TODO: implement this
    return


def read_times_on_samekey(input_logs):
    assert len(input_logs) > 0
    for input_log in input_logs:
        assert "samekey-read" in input_log

    depth_nread_times = dict()

    for input_log in input_logs:
        rdash = input_log.rindex('-')
        pre_issue_depth = input_log[rdash+1:input_log.rindex('.')]
        num_preads = int(input_log[input_log.rindex('-', 0, rdash)+1:rdash])
        in_timing_section = False

        with open(input_log, 'r') as flog:
            for line in flog.readlines():
                line = line.strip()
                if line.startswith("Removing top/bottom-5:"):
                    in_timing_section = True
                elif in_timing_section and line.startswith("avg "):
                    time = float(line.split()[1])
                    if pre_issue_depth not in depth_nread_times:
                        depth_nread_times[pre_issue_depth] = []
                    while num_preads > len(depth_nread_times[pre_issue_depth]):
                        depth_nread_times[pre_issue_depth].append(0)
                    depth_nread_times[pre_issue_depth][num_preads - 1] = time
                    break

    return depth_nread_times

def plot_times_on_samekey(depth_nread_times, output_name):
    depths = [int(d) for d in depth_nread_times.keys() if d != 'orig']
    depths.sort()
    depths = ['orig'] + [str(d) for d in depths]

    xs = range(1, len(depth_nread_times['orig']) + 1)

    for depth in depths:
        plt.plot(xs, depth_nread_times[depth],
                 zorder=3, label=depth, marker='o')

    plt.ylabel("Time of Get (us)")
    plt.xticks(xs, xs)
    plt.xlabel("Number of preads")

    plt.grid(zorder=0, axis='y')
    
    plt.legend()

    plt.savefig(output_name, dpi=120)
    plt.close()

def do_samekey(input_logs, output_name):
    depth_nread_times = read_times_on_samekey(input_logs)
    plot_times_on_samekey(depth_nread_times, output_name)


def main():
    parser = argparse.ArgumentParser(description="Benchmark result plotting")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which figure to plot: depth|mem|cdf|tail|samekey")
    parser.add_argument('-o', dest='output_name', required=True,
                        help="output plot filename")
    parser.add_argument('input_logs', metavar='I', type=str, nargs='+',
                        help="list of input benchmark result logs")
    args = parser.parse_args()

    if args.mode == 'depth':
        do_depth(args.input_logs, args.output_name)
    elif args.mode == 'mem':
        do_mem(args.input_logs, args.output_name)
    elif args.mode == 'cdf':
        do_cdf(args.input_logs, args.output_name)
    elif args.mode == 'tail':
        do_tail(args.input_logs, args.output_name)
    elif args.mode == 'samekey':
        do_samekey(args.input_logs, args.output_name)
    else:
        print(f'Error: mode {args.mode} unrecognized')
        exit(1)

if __name__ == "__main__":
    main()
