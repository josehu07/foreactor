#!/usr/bin/env python3

import matplotlib
matplotlib.use('Agg')

import argparse
import matplotlib.pyplot as plt
import numpy as np


BLOCK_SIZE = 4096
VALUE_SIZE = 10000


def read_block_log(input_log):
    block_log = []
    table_map = dict()

    in_req = True
    in_stat, level_num, level_idx = False, 0, 0

    with open(input_log, 'r') as flog:
        for line in flog.readlines():
            line = line.strip()
            if len(line) == 0:
                continue

            if in_req and line.startswith('req '):
                segs = line.split()
                assert len(segs) == 2
                block_log.append({
                    'key': segs[1],
                    'fnums': [],
                    'foffs': [],
                    'sizes': []
                })
            elif in_req and line.startswith('Finished '):
                in_req = False
            elif in_req:
                segs = line.split()
                assert len(segs) == 3
                block_log[-1]['fnums'].append(int(segs[0]))
                block_log[-1]['foffs'].append(int(segs[1]))
                block_log[-1]['sizes'].append(int(segs[2]))
            elif line.startswith('SSTables --'):
                in_stat = True
            elif in_stat and line.startswith('--- level '):
                segs = line.split()
                assert len(segs) == 4
                level_num = int(segs[2])
                level_idx = 0
            elif in_stat:
                fnum = int(line[:line.index(':')])
                table_map[fnum] = (level_num, level_idx)
                level_idx += 1

    return block_log, table_map


def plot_distribution(block_log, table_map, dist_output_name):
    num_l0_tables, max_level = 0, 0
    for level_num, level_idx in table_map.values():
        if level_num > max_level:
            max_level = level_num
        if level_num == 0:
            num_l0_tables += 1

    max_preads = num_l0_tables + max_level
    dist = [0 for i in range(max_preads)]

    for req in block_log:
        num_preads = len(req['fnums'])
        dist[num_preads - 1] += 1

    width = 0.5
    xs = list(range(1, len(dist) + 1 ))

    plt.bar(xs, dist, width,
            zorder=3)

    plt.ylabel("Request Count")
    plt.xticks(xs, xs)
    plt.xlabel("Number of preads")

    plt.grid(zorder=0, axis='y')

    plt.savefig(dist_output_name, dpi=120)
    plt.close()

def plot_heat_map(block_log, table_map, heat_output_name):
    level_files = []
    for level_num, level_idx in table_map.values():
        if level_num + 1 > len(level_files):
            level_files.append(0)
        if level_idx + 1 > level_files[level_num]:
            level_files[level_num] = level_idx + 1

    block_heat = {tup: [] for tup in table_map.values()}
    for req in block_log:
        for fnum, foff, size in zip(req['fnums'], req['foffs'], req['sizes']):
            tup = table_map[fnum]

            end_off = foff + size - 1
            beg_block = foff // BLOCK_SIZE
            end_block = end_off // BLOCK_SIZE

            while end_block + 1 > len(block_heat[tup]):
                block_heat[tup].append(0)
            for block in range(beg_block, end_block + 1):
                block_heat[tup][block] += 1

    max_local_blocks = max((len(l) for l in block_heat.values()))
    data = []
    for level_num in range(len(level_files)):
        for level_idx in range(level_files[level_num] - 1, -1, -1):
            heat = block_heat[(level_num, level_idx)]
            heat += [0] * (max_local_blocks - len(heat))
            data.append(heat)
    data = np.transpose(np.array(data))

    plt.imshow(data,
               interpolation='none', origin='lower',
               aspect=0.03, cmap='binary')

    plt.ylabel("Block Offset")
    plt.xticks(range(data.shape[1]), range(data.shape[1]),
               rotation='vertical')
    plt.xlabel("Table File")

    for i in range(data.shape[1] - 1):
        xoff = i + 0.5
        plt.axvline(x=xoff, color='black', linewidth=0.2)

    # for i in range(data.shape[0]):
    #     for j in range(data.shape[1]):
    #         plt.text(i, j, data[i, j],
    #                  va='center', ha='center', color='w')

    plt.savefig(heat_output_name, dpi=120)
    plt.close()

# def plot_dist_land_file(block_log, table_map, heat_output_name):
#     level_files = []
#     for level_num, level_idx in table_map.values():
#         if level_num + 1 > len(level_files):
#             level_files.append(0)
#         if level_idx + 1 > level_files[level_num]:
#             level_files[level_num] = level_idx + 1

#     dist = [0 for i in range(sum(level_files))]

#     def tup_to_fidx(tup):
#         fidx = 0
#         for i in range(tup[0]):
#             fidx += level_files[i]
#         return fidx + tup[1]

#     for req in block_log:
#         land_file = req['fnums'][-1]
#         land_file = tup_to_fidx(table_map[land_file])
#         dist[land_file] += 1

#     width = 0.5
#     xs = list(range(1, len(dist) + 1 ))

#     plt.bar(xs, dist, width, zorder=3)

#     plt.ylabel("Request Count")
#     plt.xticks(xs, xs)
#     plt.xlabel("Land File")

#     plt.grid(zorder=0, axis='y')

#     plt.savefig(heat_output_name, dpi=120)
#     plt.close()


def main():
    parser = argparse.ArgumentParser(description="Distribution log plotting")
    parser.add_argument('-i', dest='input_log', required=True,
                        help="input log filename")
    parser.add_argument('--dist', dest='dist_output_name', required=True,
                        help="output distribution plot filename")
    parser.add_argument('--heat', dest='heat_output_name', required=True,
                        help="output heat map plot filename")
    args = parser.parse_args()

    block_log, table_map = read_block_log(args.input_log)
    plot_distribution(block_log, table_map, args.dist_output_name)
    plot_heat_map(block_log, table_map, args.heat_output_name)
    

if __name__ == "__main__":
    main()
