#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


YCSBCLI_BIN = "./ycsbcli"
LEVELDBUTIL_BIN = "./leveldb-src/build/leveldbutil"

TARGET_DATABASE_VOLUME = 1024 * 1024 * 1024     # make ~1GB per database image
TARGET_WORKLOAD_VOLUME = 100 * 1024 * 1024      # read out ~100MB per workload

YCSB_LOAD_WORKLOAD = "c"
YCSB_RUN_POSSIBLE_WORKLOADS = {"a", "b", "c", "d", "e", "f"}
YCSB_RUN_POSSIBLE_DISTRIBUTIONS = {"zipfian", "uniform"}

SAMEKEY_NUMOPS_PER_RUN = 100

NUM_L0_TABLES = 12
NUM_RECORDS_FILENAME = "num_init_records.txt"


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
        exit(1)

def check_dir_exists(dir_path):
    if not os.path.isdir(dir_path):
        print(f"Error: directory {dir_path} does not exist")
        exit(1)


def run_ycsbcli(dbdir, trace, value_size, memtable_limit, filesize_limit,
                bg_compact_off, print_stat_exit=False):
    if print_stat_exit:
        cmd = [YCSBCLI_BIN, "-d", dbdir, "--print_stat_exit"]
    else:
        cmd = [YCSBCLI_BIN, "-d", dbdir, "-v", str(value_size), "-f", trace,
               "--mlim", str(memtable_limit), "--flim", str(filesize_limit),
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


def run_ycsb_bin(ycsb_bin, ycsb_action, ycsb_workload, extra_args, out_file):
    cmd = [ycsb_bin, ycsb_action, "basic", "-P", ycsb_workload]
    cmd += extra_args
    cmd += ["-p", "fieldcount=1", "-p", "fieldlength=1"]

    with open(out_file, 'w') as fout:
        result = subprocess.run(cmd, check=True, stdout=fout,
                                stderr=subprocess.STDOUT)

def convert_trace(ycsb_output, output_trace):
    with open(ycsb_output, 'r') as fin, open(output_trace, 'w') as fout:
        for line in fin:
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

def make_db_image(dbdir, tmpdir, ycsb_bin, ycsb_workload, value_size,
                  memtable_limit, filesize_limit, output_prefix):
    # clean up old files
    if os.path.isdir(dbdir):
        for file in os.listdir(dbdir):
            path = os.path.join(dbdir, file)
            try:
                shutil.rmtree(path)
            except OSError:
                os.remove(path)

    approx_num_records = TARGET_DATABASE_VOLUME // value_size
    approx_num_records_per_l0_table = memtable_limit // value_size

    # generate the mega YCSB load trace
    max_num_records = int(approx_num_records * 1.2)
    tmpfile_ycsb = f"{tmpdir}/leveldb_prepare.tmp1.txt"
    tmpfile_mega = f"{tmpdir}/leveldb_prepare.tmp2.txt"
    run_ycsb_bin(ycsb_bin, "load", ycsb_workload,
                 ["-p", f"recordcount={max_num_records}"], tmpfile_ycsb)
    convert_trace(tmpfile_ycsb, tmpfile_mega)

    num_records = 0
    with open(tmpfile_mega, 'r') as ftrace:
        tmpfile_load = f"{tmpdir}/leveldb_prepare.tmp3.txt"

        # load sufficient number of records to form the base image
        take_trace_records(ftrace, tmpfile_load, approx_num_records)
        output = run_ycsbcli(dbdir, tmpfile_load, value_size,
                             memtable_limit, filesize_limit, False)
        num_records = approx_num_records

        # repeatedly feed in samples of records, while turning off background
        # compaction of everything except memtable, until the desired dynamic
        # number of level-0 tables is reached
        while get_num_l0_tables(output) < NUM_L0_TABLES:
            num_new_records = approx_num_records_per_l0_table // 2
            take_trace_records(ftrace, tmpfile_load, num_new_records)
            output = run_ycsbcli(dbdir, tmpfile_load, value_size,
                                 memtable_limit, filesize_limit, True)
            num_records += num_new_records
    
    # store the number of initial records in a special file for convenience
    with open(f"{dbdir}/{NUM_RECORDS_FILENAME}", 'w') as fnum:
        fnum.write(str(num_records))


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

def get_init_num_records(dbdir):
    with open(f"{dbdir}/{NUM_RECORDS_FILENAME}", 'r') as fnum:
        return int(fnum.read().strip())

def generate_workloads(dbdir, tmpdir, ycsb_bin, ycsb_run_workloads, value_size,
                       ycsb_distribution, output_prefix, gen_samekey_workloads,
                       max_threads):
    stats = get_full_stat_section(
        run_ycsbcli(dbdir, "dummy", value_size, 0, 0, True, True))
    num_records = get_init_num_records(dbdir)

    if gen_samekey_workloads:
        print("Workload keys --")
        samekey_trace = lambda n: f"{output_prefix}-samekey-{n}-{t}.txt"

        # generate workload of reading the key that approximately triggers N preads
        # through level 0
        l0_workload_keys = get_l0_workload_keys(stats, dbdir)
        for idx, l0_key in enumerate(l0_workload_keys):
            n = idx + 1
            with open(samekey_trace(n), 'w') as ftxt:
                for _ in range(SAMEKEY_NUMOPS_PER_RUN):
                    ftxt.write(f"READ {l0_key}\n")
            print(f" {n}: {l0_key}")
        last_l0_key = l0_workload_keys[-1]

        # for deeper levels, pick the smallest key larger than the last l0 key
        level = 1
        deepest_level = get_deepest_level(stats)
        while level <= deepest_level:
            level_key = get_level_workload_key(stats, dbdir, level, last_l0_key)
            n = len(l0_workload_keys) + level
            with open(samekey_trace(n), 'w') as ftxt:
                for _ in range(SAMEKEY_NUMOPS_PER_RUN):
                    ftxt.write(f"READ {level_key}\n")
            print(f" {n}: {level_key}")
            level += 1

    for t in range(max_threads):
        tmpfile = f"{tmpdir}/leveldb_prepare.tmp3.txt"
        num_operations = TARGET_WORKLOAD_VOLUME // value_size
        for ycsb_workload in ycsb_run_workloads:
            workload_name = ycsb_workload[-1:]  # last character of property filename
            run_ycsb_bin(ycsb_bin, "run", ycsb_workload,
                         ["-p", f"recordcount={num_records}",
                          "-p", f"operationcount={num_operations}",
                          "-p", f"requestdistribution={ycsb_distribution}"],
                         tmpfile)
            run_trace = f"{output_prefix}-ycsb-{workload_name}-" + \
                        f"{ycsb_distribution}-{t}.txt"
            convert_trace(tmpfile, run_trace)


def get_comma_separated_list(arg, argname, possible_set):
    l = arg.strip().split(',')
    if len(l) == 0:
        print(f"Error: empty comma-separated list argument {argname}")
        exit(1)
    else:
        for e in l:
            if e not in possible_set:
                print(f"Error: invalid comma-separated element {e}")
                exit(1)
    return l

def main():
    parser = argparse.ArgumentParser(description="LevelDB database image setup")
    parser.add_argument('-d', dest='dbdir', required=True,
                        help="dbdir of LevelDB")
    parser.add_argument('-t', dest='tmpdir', required=True,
                        help="directory to hold temp files, space >= 10GB")
    parser.add_argument('-y', dest='ycsb_dir', required=True,
                        help="path to official YCSB directory")
    parser.add_argument('-v', dest='value_size', required=True, type=int,
                        help="value size in bytes")
    parser.add_argument('-w', dest='ycsb_workloads', required=True,
                        help="comma-separated list, e.g. a,b,c")
    parser.add_argument('-z', dest='ycsb_distributions', required=True,
                        help="comma-separated list, e.g. zipfian,uniform")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="path prefix of output workloads")
    parser.add_argument('--max_threads', dest='max_threads', type=int, default=1,
                        help="if > 1, generate these many distinct traces")
    parser.add_argument('--skip_load', dest='skip_load', action='store_true',
                        help="if given, skip loading and only generate workloads")
    parser.add_argument('--gen_samekey', dest='gen_samekey', action='store_true',
                        help="if given, also generate samekey workloads")
    parser.add_argument('--mlim', dest='mlim', required=False, type=int,
                        default=4194304, help="memtable size limit")
    parser.add_argument('--flim', dest='flim', required=False, type=int,
                        default=2097152, help="sstable filesize limit")
    args = parser.parse_args()

    check_dir_exists(args.tmpdir)
    check_dir_exists(args.ycsb_dir)

    run_workloads = get_comma_separated_list(args.ycsb_workloads,
                                             "ycsb_workloads",
                                             YCSB_RUN_POSSIBLE_WORKLOADS)
    run_distributions = get_comma_separated_list(args.ycsb_distributions,
                                                 "ycsb_distributions",
                                                 YCSB_RUN_POSSIBLE_DISTRIBUTIONS)

    ycsb_bin = f"{args.ycsb_dir}/bin/ycsb"
    load_workload = f"{args.ycsb_dir}/workloads/workload{YCSB_LOAD_WORKLOAD}"
    run_workloads = [f"{args.ycsb_dir}/workloads/workload{r}" \
                     for r in run_workloads]
    check_file_exists(load_workload)
    for run_workload in run_workloads:
        check_file_exists(run_workload)
    
    check_file_exists(YCSBCLI_BIN)
    check_file_exists(LEVELDBUTIL_BIN)

    if args.gen_samekey and args.max_threads > 1:
        print(f"Error: cannot give gen_samekey when preparing multithreading")
        exit(1)

    if not args.skip_load:
        make_db_image(args.dbdir, args.tmpdir, ycsb_bin, load_workload,
                      args.value_size, args.mlim, args.flim, args.output_prefix)

    for ycsb_distribution in run_distributions:
        generate_workloads(args.dbdir, args.tmpdir, ycsb_bin, run_workloads,
                           args.value_size, ycsb_distribution,
                           args.output_prefix, args.gen_samekey,
                           args.max_threads)

if __name__ == "__main__":
    main()
