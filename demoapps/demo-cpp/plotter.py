import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
import argparse


def parse_exper_results(log_file):
    results = {
        'expers': [],
        'bars': []
    }

    with open(log_file, 'r') as flog:
        curr_exper = None
        curr_configs, curr_times = [], []

        def add_curr_exper():
            assert curr_exper is not None
            results['expers'].append(curr_exper)
            results['bars'].append({
                'configs': curr_configs,
                'times': curr_times
            })

        line = flog.readline()
        while line:
            line = line.strip()

            if line.startswith("running") and "-original" in line:
                if curr_exper is not None:
                    add_curr_exper()
                exper_base = line[line.find('running ')+8:line.find('-original')].strip()
                exper_args = line[line.find('-original')+9:line.find('...')].strip()
                curr_exper = exper_base + ' ' + exper_args
                curr_configs = ['original']
                curr_times = []
            elif line.startswith("running") and "-config" in line:
                if 'io_uring_default' in line:
                    curr_configs.append('io_uring_default')
                elif 'io_uring_sqe_async' in line:
                    curr_configs.append('io_uring_sqe_async')
                elif 'thread_pool' in line:
                    curr_configs.append('thread_pool')
                elif 'manual_ring' in line:
                    curr_configs.append('manual_ring')
                elif 'manual_pool' in line:
                    curr_configs.append('manual_pool')
                else:
                    print(f"Error: unrecognized config {line}")
                    exit(1)
            elif line.startswith("Time elapsed:"):
                time_us = float(line.split()[-2])
                time_ms = time_us / 1000.0
                curr_times.append(time_ms)

            line = flog.readline()

        add_curr_exper()

    return results


def plot_exper_results(results, output_prefix):
    assert len(results['expers']) == len(results['bars'])

    for i in range(len(results['expers'])):
        exper = results['expers'][i]
        configs = results['bars'][i]['configs']
        times = results['bars'][i]['times']

        xs = list(range(len(configs)))
        ys = times
        xlabels = configs
        width = 0.5

        print(f"{exper}: {times}")

        plt.bar(xs[:1], ys[:1], zorder=3, width=width, color='darkorange')
        plt.bar(xs[1:], ys[1:], zorder=3, width=width, color='steelblue')
        
        plt.xlabel("Configs")
        plt.ylabel("Time (ms)")

        plt.title(exper)

        plt.xticks(xs, xlabels, rotation=20)

        plt.grid(zorder=1, axis='y')

        plt.tight_layout()

        exper_str = '_'.join(exper.split())
        plt.savefig(f"{output_prefix}-{exper_str}.png", dpi=120)
        plt.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Plot timing results")
    parser.add_argument('--log_file', type=str, required=True)
    parser.add_argument('--output_prefix', type=str, required=True)
    args = parser.parse_args()

    results = parse_exper_results(args.log_file)
    plot_exper_results(results, args.output_prefix)
