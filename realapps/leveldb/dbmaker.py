#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


YCSBCLI_BIN = "./ycsbcli"
LEVELDBUTIL_BIN = "./leveldb-src/build/leveldbutil"

SAMEKEY_NUMOPS_PER_RUN = 100
YCSBRUN_NUMOPS_SCALE = 1


def run_ycsbcli(dbdir, trace, value_size, memtable_limit, filesize_limit,
                bg_compact_off):
    cmd = [YCSBCLI_BIN, "-d", dbdir, "-v", str(value_size), "-f", trace,
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


def run_leveldbutil(dbdir, filenum):
    ldb_file = f"{dbdir}/{filenum:06d}.ldb"
    cmd = [LEVELDBUTIL_BIN, "dump", ldb_file]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def get_file_middle_key(dump_content):
    keys = []   # naturally sorted within a ldb file
    for line in dump_content.strip().split('\n'):
        key = line[line.index("'")+1:line.index("@")-2]
        keys.append(key)

    assert len(keys) > 0
    return keys[len(keys) // 2]

def get_file_closest_key(dump_content, target_key):
    for line in dump_content.strip().split('\n'):
        key = line[line.index("'")+1:line.index("@")-2]
        if key > target_key:
            return key
    return None


def get_l0_workload_keys(stats, dbdir):
    lines = stats.split('\n')
    in_l0_section = False
    l0_filenums = []

    for line in lines:
        if line.startswith("--- level 0 ---"):
            in_l0_section = True
        elif line.startswith("--- level 1 ---"):
            break
        elif in_l0_section:
            fnum = int(line[:line.index(":")])
            l0_filenums.append(fnum)

    l0_filenums.sort(reverse=True)
    l0_workload_keys = []
    for fnum in l0_filenums:
        dump_content = run_leveldbutil(dbdir, fnum)
        key = get_file_middle_key(dump_content)
        l0_workload_keys.append(key)

    return l0_workload_keys

def get_level_workload_key(stats, dbdir, level, last_l0_key):
    lines = stats.split('\n')
    in_level_section = False
    last_key_max_seen = None

    for line in lines:
        if line.startswith(f"--- level {level} ---"):
            in_level_section = True
        elif in_level_section:
            if line.startswith("--- level "):
                break
            fnum = int(line[:line.index(":")])
            line = line.strip().split(" .. ")[1]
            key_max = line[line.index("'")+1:line.index("@")-2]
            last_key_max_seen = key_max
            if key_max > last_l0_key:
               dump_content = run_leveldbutil(dbdir, fnum) 
               key = get_file_closest_key(dump_content, last_l0_key)
               assert key is not None
               return key

    if last_key_max_seen is None:
        # corner case where one level in the middle is completely empty
        return last_l0_key
    else:
        return last_key_max_seen

def generate_workloads(stats, dbdir, ycsb_bin, ycsb_workload, num_records,
                       num_l0_tables, output_prefix):
    print("Workload keys --")

    # generate workload of reading the key that approximately triggers N preads
    l0_workload_keys = get_l0_workload_keys(stats, dbdir)
    for idx, l0_key in enumerate(l0_workload_keys):
        n = idx + 1
        with open(f"{output_prefix}-{num_l0_tables}-samekey-{n}.txt", 'w') as ftxt:
            for i in range(SAMEKEY_NUMOPS_PER_RUN):
                ftxt.write(f"READ {l0_key}\n")
        print(f" {n}: {l0_key}")
    last_l0_key = l0_workload_keys[-1]

    # for deeper levels, pick the smallest key larger than the last l0 key
    level = 1
    deepest_level = get_deepest_level(stats)
    while level <= deepest_level:
        level_key = get_level_workload_key(stats, dbdir, level, last_l0_key)
        n = len(l0_workload_keys) + level
        with open(f"{output_prefix}-{num_l0_tables}-samekey-{n}.txt", 'w') as ftxt:
            for i in range(SAMEKEY_NUMOPS_PER_RUN):
                ftxt.write(f"READ {level_key}\n")
        print(f" {n}: {level_key}")
        level += 1

    # finally, generate the general YCSB run trace
    num_operations = num_records * YCSBRUN_NUMOPS_SCALE
    ycsb_output = run_ycsb_bin(ycsb_bin, "run", ycsb_workload,
                               ["-p", f"recordcount={num_records}",
                                "-p", f"operationcount={num_operations}"])
    run_trace = f"{output_prefix}-{num_l0_tables}-ycsbrun.txt"
    convert_trace(ycsb_output, run_trace)


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
        exit(1)

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

    check_file_exists(args.ycsb_bin)
    check_file_exists(args.ycsb_workload)
    check_file_exists(YCSBCLI_BIN)
    check_file_exists(LEVELDBUTIL_BIN)

    stats, num_records = make_db_image(args.dbdir, args.ycsb_bin, args.ycsb_workload,
                                       args.value_size, args.mlim, args.flim,
                                       args.num_l0_tables, args.output_prefix)
    generate_workloads(stats, args.dbdir, args.ycsb_bin, args.ycsb_workload,
                       num_records, args.num_l0_tables, args.output_prefix)

if __name__ == "__main__":
    main()
