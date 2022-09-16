#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import Normalize

import argparse
import os


def read_result_log(filename):
    results = {
        "seq": 0.,
        "rnd": dict()
    }

    with open(filename, 'r') as flog:
        curr_exper, curr_nthreads, curr_reqsize = None, 0, 0
        for line in flog.readlines():
            line = line.strip()

            if line.startswith("Exper "):
                segs = line.split()
                if segs[1] == "sequential":
                    curr_exper = "seq"
                else:
                    curr_exper = "rnd"
                    curr_nthreads = int(segs[2][segs[2].index('=')+1:])
                    curr_reqsize = int(segs[3][segs[3].index('=')+1:])
            elif line.startswith("average"):
                assert curr_exper is not None
                throughput = float(line.split()[1])
                if curr_exper == "seq":
                    results["seq"] = throughput
                else:
                    if curr_nthreads not in results["rnd"]:
                        results["rnd"][curr_nthreads] = dict()
                    results["rnd"][curr_nthreads][curr_reqsize] = throughput
                curr_exper = None

    return results


def plot_results(results, outname):
    plt.rcParams.update({'font.size': 19})
    plt.rcParams.update({'figure.figsize': (8, 3.6)})

    nthreads_list = [1, 2, 4, 8, 16, 24, 32]
    xticks_list = [1, 4, 8, 16, 24, 32]
    reqsize_list = [4096, 16384, 65536]

    name_map = {
        4096:   "4KB",
        16384:  "16KB",
        65536:  "64KB"
    }
    marker_map = {
        4096:   'o',
        16384:  'v',
        65536:  'D'
    }
    norm = Normalize(vmin=0, vmax=10)
    cmap = cm.OrRd
    color_map = {
        4096:   cmap(norm(4)),
        16384:  cmap(norm(5)),
        65536:  cmap(norm(6))
    }

    plt.axhline(y=results["seq"], label="Seq.", color="steelblue",
                                  linestyle="--", linewidth=3, zorder=3)

    for reqsize in reqsize_list[::-1]:
        xs = nthreads_list
        ys = []
        for nthreads in nthreads_list:
            ys.append(results["rnd"][nthreads][reqsize])
        
        plt.plot(xs, ys, label=name_map[reqsize], marker=marker_map[reqsize],
                         markersize=10, linewidth=2.5, color=color_map[reqsize],
                         zorder=3)
    
    plt.xlabel("Number of concurrent requests")
    plt.ylabel("Throughput (MB/s)")

    plt.xticks(xticks_list, xticks_list, fontsize=16)
    plt.yticks(fontsize=16)

    plt.ylim(bottom=0)

    plt.grid(axis='y', zorder=1)

    plt.legend(mode="expand", ncol=1, loc="center left",
               bbox_to_anchor=(1, 0.5),
               fontsize=17, frameon=False, handlelength=1.5)

    plt.tight_layout(rect=[0, 0, 0.85, 1])

    plt.savefig(outname, dpi=200)
    plt.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot motivation figure")
    parser.add_argument('--log', type=str, required=True)
    args = parser.parse_args()

    results = read_result_log(args.log)
    outname = f"{os.path.splitext(args.log)[0]}.pdf"
    plot_results(results, outname)
