#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


def run_ycsbcli(dbdir, trace, value_size, memtable_limit, filesize_limit,
                bg_compact_off):
    cmd = ["./ycsbcli", "-d", dbdir, "-v", str(value_size), "-f", trace,
           "--wait_before_close"]
    if bg_compact_off:
        cmd.append("--bg_compact_off")
    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output


def get_num_level_tables(output):
    lines = output.split('\n')
    curr_level = -1
    curr_cnt = 0
    num_level_tables = []

    for line in lines:
        if line.startswith("--- level "):
            if curr_level >= 0:
                assert len(num_level_tables) == curr_level
                num_level_tables.append(curr_cnt)
            curr_level = int(line.strip().split()[-2])
            curr_cnt = 0
        elif curr_level >= 0:
            if len(line.strip()) > 0:
                curr_cnt += 1

    assert len(num_level_tables) == curr_level
    num_level_tables.append(curr_cnt)
    return num_level_tables

def get_num_l0_tables(output):
    return get_num_level_tables(output)[0]

def get_deepest_level(output):
    num_level_tables = get_num_level_tables(output)
    for level in range(len(num_level_tables)-1, -1, -1):
        if num_level_tables[level] > 0:
            return level
    return -1


def get_full_stat_section(output):
    lines = output.split('\n')
    stat_lines = []
    in_stat_section = False

    for line in lines:
        if line.startswith("Num files at level --"):
            in_stat_section = True
        if in_stat_section:
            stat_lines.append(line.strip())

    return '\n'.join(stat_lines)


def run_ycsb_bin(ycsb_bin, ycsb_action, ycsb_workload, extra_args=[]):
    cmd = [ycsb_bin, ycsb_action, "basic", "-P", ycsb_workload]
    cmd += extra_args

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def convert_trace(ycsb_output, output_trace):
    with open(output_trace, 'w') as fout:
        for line in ycsb_output.split('\n'):
            segs = line.strip().split()
            if len(segs) < 1:
                continue
            elif segs[0] == "READ" or segs[0] == "INSERT" or segs[0] == "UPDATE":
                # opcode key
                fout.write(f"{segs[0]} {segs[2]}\n")
            elif segs[0] == "SCAN":
                # scan key length
                fout.write(f"{segs[0]} {segs[2]} {segs[3]}\n")

def take_trace_records(ftrace, tmpfile, num_records):
    num_taken = 0
    with open(tmpfile, 'w') as ftmp:
        while num_taken < num_records:
            line = ftrace.readline()
            if len(line) == 0:
                print("Error: mega trace not large enough")
                exit(1)
            ftmp.write(line.strip() + '\n')
            num_taken += 1

def make_db_image(dbdir, ycsb_bin, ycsb_workload, value_size, memtable_limit,
                  filesize_limit, num_l0_tables, output_prefix):
    # clean up old files
    if os.path.isdir(dbdir):
        for file in os.listdir(dbdir):
            path = os.path.join(dbdir, file)
            try:
                shutil.rmtree(path)
            except OSError:
                os.remove(path)

    approx_num_records_per_l0_table = memtable_limit // value_size

    # generate the mega YCSB load trace
    max_num_records = approx_num_records_per_l0_table * num_l0_tables * 3
    ycsb_output = run_ycsb_bin(ycsb_bin, "load", ycsb_workload,
                               ["-p", f"recordcount={max_num_records}"])
    mega_trace = f"{output_prefix}-{num_l0_tables}-mega.txt"
    convert_trace(ycsb_output, mega_trace)

    num_records = 0
    with open(mega_trace, 'r') as ftrace:
        tmpfile = "/tmp/makedb.tmp.txt"

        # load sufficient number of records to form the base image
        num_base_records = approx_num_records_per_l0_table * num_l0_tables * 2
        take_trace_records(ftrace, tmpfile, num_base_records)
        output = run_ycsbcli(dbdir, tmpfile, value_size, memtable_limit,
                             filesize_limit, False)
        num_records += num_base_records

        # repeatedly feed in samples of records, while turning off background
        # compaction of everything except memtable, until the desired number of
        # level-0 tables is reached
        while get_num_l0_tables(output) < num_l0_tables:
            num_new_records = approx_num_records_per_l0_table // 2
            take_trace_records(ftrace, tmpfile, num_new_records)
            output = run_ycsbcli(dbdir, tmpfile, value_size, memtable_limit,
                                 filesize_limit, True)
            num_records += num_new_records

        stats = get_full_stat_section(output)
        print(stats)

        return stats, num_records


def get_l0_workload_keys(stats):
    lines = stats.split('\n')
    in_l0_section = False
    l0_workload_keys = []

    for line in lines:
        if line.startswith("--- level 0 ---"):
            in_l0_section = True
        elif line.startswith("--- level 1 ---"):
            return l0_workload_keys
        elif in_l0_section:
            key = line[line.index("'")+1:line.index("@")-2]
            l0_workload_keys.append(key)

def get_level_workload_key(stats, level, last_l0_key):
    lines = stats.split('\n')
    in_level_section = False
    last_level_key_seen = None

    for line in lines:
        if line.startswith(f"--- level {level} ---"):
            in_level_section = True
        elif in_level_section:
            if line.startswith("--- level "):
                break
            key = line[line.index("'")+1:line.index("@")-2]
            if key > last_l0_key:
                return key
            last_level_key_seen = key

    return last_level_key_seen

def generate_workloads(stats, ycsb_bin, ycsb_workload, num_records, output_prefix):
    print("Workload keys --")

    num_l0_tables = get_num_l0_tables(stats)

    # generate workload of reading the key that approximately triggers N preads
    l0_workload_keys = get_l0_workload_keys(stats)
    for idx, l0_key in enumerate(l0_workload_keys):
        n = idx + 1
        with open(f"{output_prefix}-{num_l0_tables}-samekey-{n}.txt", 'w') as ftxt:
            for i in range(100):
                ftxt.write(f"READ {l0_key}\n")
        print(f" {n}: {l0_key}")
    last_l0_key = l0_workload_keys[-1]

    # for deeper levels, pick the smallest key larger than the last l0 key
    level = 1
    deepest_level = get_deepest_level(stats)
    while level <= deepest_level:
        level_key = get_level_workload_key(stats, level, last_l0_key)
        n = len(l0_workload_keys) + level
        with open(f"{output_prefix}-{num_l0_tables}-samekey-{n}.txt", 'w') as ftxt:
            for i in range(100):
                ftxt.write(f"READ {level_key}\n")
        print(f" {n}: {level_key}")
        level += 1

    # finally, generate the general YCSB run trace
    num_run_records = num_records
    if num_run_records > 1000:
        num_run_records = 1000
    ycsb_output = run_ycsb_bin(ycsb_bin, "run", ycsb_workload,
                               ["-p", f"recordcount={num_records}",
                                "-p", f"operationcount={num_run_records}"])
    run_trace = f"{output_prefix}-{num_l0_tables}-ycsbrun.txt"
    convert_trace(ycsb_output, run_trace)


def main():
    parser = argparse.ArgumentParser(description="LevelDB database image setup")
    parser.add_argument('-d', dest='dbdir', required=True,
                        help="dbdir of LevelDB")
    parser.add_argument('-y', dest='ycsb_bin', required=True,
                        help="path to YCSB binary")
    parser.add_argument('-w', dest='ycsb_workload', required=True,
                        help="base YCSB workload property file")
    parser.add_argument('-v', dest='value_size', required=True, type=int,
                        help="value size in bytes")
    parser.add_argument('-t', dest='num_l0_tables', required=True, type=int,
                        help="target number of L0 tables")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="path prefix of output workloads")
    parser.add_argument('--mlim', dest='mlim', required=False, type=int,
                        default=4194304, help="memtable size limit")
    parser.add_argument('--flim', dest='flim', required=False, type=int,
                        default=2097152, help="sstable filesize limit")
    args = parser.parse_args()

    stats, num_records = make_db_image(args.dbdir, args.ycsb_bin, args.ycsb_workload,
                                       args.value_size, args.mlim, args.flim,
                                       args.num_l0_tables, args.output_prefix)
    generate_workloads(stats, args.ycsb_bin, args.ycsb_workload, num_records,
                       args.output_prefix)

if __name__ == "__main__":
    main()
