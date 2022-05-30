#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt


TIMERS = [
    "pool-flush",
    "pool-clear",
    "clear-prog",
    "peek-algo",
    "sync-call",
    "ring-submit",
    "ring-cmpl"
]

def calc_breakdown(input_log):
    num_reqs = 0
    avg_time = 0.0
    breakdown_times, breakdown_cnts = dict(), dict()
    in_timing_section = False

    with open(input_log, 'r') as flog:
        for line in flog.readlines():
            line = line.strip()

            if line.startswith("Finished "):
                num_reqs = int(line.split()[1])
            elif line.startswith("Time elapsed stats:"):
                in_timing_section = True
            elif in_timing_section and line.startswith("avg "):
                avg_time = float(line.split()[1])
                in_timing_section = False
            elif line.startswith('# '):
                segs = line.split()
                name, cnt, avg = segs[1], int(segs[4]), float(segs[6])
                for timer in TIMERS:
                    if timer in name:
                        breakdown_times[timer] = cnt * avg / num_reqs
                        breakdown_cnts[timer] = cnt / num_reqs
                        break

    print(f"Req avg. time:  {avg_time:.3f} us")
    print("Breakdown")
    print(f"  {'timer':>12s}  {'avg_us/req':>10s}  {'cnt/req':>8s}")
    for timer in TIMERS:
        print(f"  {timer:>12s}  {breakdown_times[timer]:>10.3f}"
              f"  {breakdown_cnts[timer]:>8.2f}")

    return breakdown_times, breakdown_cnts

def plot_breakdown(breakdown_times, breakdown_cnts, output_name):
    width = 0.35

    accum_us, bottom_us = 0.0, dict()
    for timer in TIMERS[::-1]:
        bottom_us[timer] = accum_us
        accum_us += breakdown_times[timer]
    for timer in TIMERS:
        plt.bar([0], [breakdown_times[timer]], width,
                bottom=bottom_us[timer], zorder=3, label=timer)

    estimated_orig_us = breakdown_times["sync-call"] * \
                        (breakdown_cnts["sync-call"] + breakdown_cnts["ring-cmpl"])
    plt.bar([1], [estimated_orig_us], width,
            zorder=3, label="original")

    plt.ylabel("Timer per Get Breakdown (us)")
    plt.xticks([0, 1], ["foreactor", "original"])
    plt.xlim(-0.5, 1.5)

    plt.grid(zorder=0, axis='y')

    plt.legend(loc='center left', bbox_to_anchor=(1, 0.5))

    plt.tight_layout()
    plt.savefig(output_name, dpi=120)
    plt.close()


def input_to_output_name(input_log):
    assert input_log.endswith("timer.log")
    return input_log[:-3] + "png"

def main():
    parser = argparse.ArgumentParser(description="Latency breakdown calculator")
    parser.add_argument('input_logs', metavar='I', type=str, nargs='+',
                        help="list of input result logs benchmarked with timer on")
    args = parser.parse_args()

    for input_log in args.input_logs:
        print("\n" + input_log + ":")
        breakdown_times, breakdown_cnts = calc_breakdown(input_log)
        plot_breakdown(breakdown_times, breakdown_cnts,
                       input_to_output_name(input_log))

if __name__ == "__main__":
    main()
