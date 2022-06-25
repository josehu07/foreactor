#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


DBMAKER_PY = "./dbmaker.py"
BENCHER_PY = "./bencher.py"

VALUE_SIZES = {
    "256B": 256,
    "512B": 512,
    "1K":   1024,
    "2K":   2 * 1024,
    "4K":   4 * 1024,
    "8K":   8 * 1024,
    "16K":  16 * 1024,
    "32K":  32 * 1024,
    "64K":  64 * 1024,
    "128K": 128 * 1024,
    "256K": 256 * 1024,
    "512K": 512 * 1024,
    "1M":   1024 * 1024,
}
NUMS_L0_TABLES = [8, 12]
BACKENDS = ["io_uring_default", "io_uring_sqe_async", "thread_pool"]
PRE_ISSUE_DEPTH_LIST = [0, 4, 8, 12, 15]


def run_makedb(dbdir_prefix, value_size, value_size_abbr, ycsb_bin, ycsb_workload,
               num_l0_tables):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}_{num_l0_tables}"
    output_prefix = f"workloads/trace-{value_size_abbr}"

    cmd = ["python3", DBMAKER_PY, "-d", dbdir, "-v", str(value_size),
           "-y", ycsb_bin, "-w", ycsb_workload, "-t", str(num_l0_tables),
           "-o", output_prefix]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def make_db_images(dbdir_prefix, ycsb_bin, ycsb_workload, value_sizes,
                   nums_l0_tables):
    for value_size_abbr, value_size in value_sizes.items():
        for num_l0_tables in nums_l0_tables:
            run_makedb(dbdir_prefix, value_size, value_size_abbr, ycsb_bin,
                       ycsb_workload, num_l0_tables)
            print(f"MADE {value_size_abbr} {num_l0_tables}")


def run_bench(libforeactor, dbdir, workload, output_log, backend,
              pre_issue_depth_list, drop_caches):
    cmd = ["python3", BENCHER_PY, "-l", libforeactor, "-d", dbdir,
           "-f", workload, "-o", output_log, "-b", backend]
    cmd += list(map(lambda d: str(d), pre_issue_depth_list))
    if drop_caches:
        cmd.append("--drop_caches")

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def run_bench_samekey(libforeactor, dbdir_prefix, value_size_abbr, num_l0_tables,
                      approx_num_preads, backend, pre_issue_depth_list, drop_caches):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}_{num_l0_tables}"
    workload = f"workloads/trace-{value_size_abbr}-{num_l0_tables}-samekey-" + \
               f"{approx_num_preads}.txt"
    output_log = f"results/result-{value_size_abbr}-{num_l0_tables}-samekey-" + \
                 f"{approx_num_preads}-{backend}-" + \
                 f"{'drop_caches' if drop_caches else 'cached'}.log"
    return run_bench(libforeactor, dbdir, workload, output_log, backend,
                     pre_issue_depth_list, drop_caches)

def run_bench_specific(libforeactor, dbdir_prefix, value_size_abbr, num_l0_tables,
                       workload_abbr, backend, pre_issue_depth_list, drop_caches):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}_{num_l0_tables}"
    workload = f"workloads/trace-{value_size_abbr}-{num_l0_tables}-" + \
               f"{workload_abbr}.txt"
    output_log = f"results/result-{value_size_abbr}-{num_l0_tables}-" + \
                 f"{workload_abbr}-{backend}-" + \
                 f"{'drop_caches' if drop_caches else 'cached'}.log"
    return run_bench(libforeactor, dbdir, workload, output_log, backend,
                     pre_issue_depth_list, drop_caches)

def run_all_samekey(libforeactor, dbdir_prefix, value_sizes, nums_l0_tables,
                    backends, pre_issue_depth_list):
    for value_size_abbr, value_size in value_sizes.items():
        for num_l0_tables in nums_l0_tables:
            for approx_num_preads in range(1, num_l0_tables+2+1):   # FIXME
                for backend in backends:
                    for drop_caches in (False, True):
                        output = run_bench_samekey(libforeactor, dbdir_prefix,
                                                   value_size_abbr, num_l0_tables,
                                                   approx_num_preads, backend,
                                                   pre_issue_depth_list, drop_caches)
                        print(f"RUN {value_size_abbr} {num_l0_tables} " + \
                              f"{approx_num_preads} {backend} {drop_caches}")
                        print(output.rstrip())

def run_all_ycsbrun(libforeactor, dbdir_prefix, value_sizes, nums_l0_tables,
                   backends, pre_issue_depth_list):
    for value_size_abbr, value_size in value_sizes.items():
        for num_l0_tables in nums_l0_tables:
            for backend in backends:
                for drop_caches in (False, True):
                    output = run_bench_specific(libforeactor, dbdir_prefix,
                                                value_size_abbr, num_l0_tables,
                                                "ycsbrun", backend,
                                                pre_issue_depth_list, drop_caches)
                    print(f"RUN {value_size_abbr} {num_l0_tables} " + \
                          f"ycsbrun {backend} {drop_caches}")
                    print(output.rstrip())


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
        exit(1)

def prepare_empty_dir(dir_path):
    if os.path.isdir(dir_path):
        for file in os.listdir(dir_path):
            path = os.path.join(dir_path, file)
            try:
                shutil.rmtree(path)
            except OSError:
                os.remove(path)
    else:
        os.mkdir(dir_path)

def main():
    parser = argparse.ArgumentParser(description="Run all LevelDB experiments")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which phase to run: dbmaker|bencher")
    parser.add_argument('-d', dest='dbdir_prefix', required=True,
                        help="absolute path prefix of dbdir")
    parser.add_argument('-y', dest='ycsb_bin', required=False,
                        help="required for dbmaker; path to YCSB binary")
    parser.add_argument('-w', dest='ycsb_workload', required=False,
                        help="required for dbmaker; path to YCSB workload property file")
    parser.add_argument('-l', dest='libforeactor', required=False,
                        help="required for bencher; absolute path to libforeactor.so")
    args = parser.parse_args()
    
    # change to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    if args.mode == "dbmaker":
        check_file_exists(DBMAKER_PY)
        prepare_empty_dir("workloads")

        assert args.ycsb_bin is not None
        assert args.ycsb_workload is not None

        make_db_images(args.dbdir_prefix, args.ycsb_bin, args.ycsb_workload,
                       VALUE_SIZES, NUMS_L0_TABLES)

    elif args.mode == "bencher":
        check_file_exists(BENCHER_PY)
        prepare_empty_dir("results")

        assert args.libforeactor is not None

        run_all_samekey(args.libforeactor, args.dbdir_prefix, VALUE_SIZES,
                        NUMS_L0_TABLES, BACKENDS, PRE_ISSUE_DEPTH_LIST)
        run_all_ycsbrun(args.libforeactor, args.dbdir_prefix, VALUE_SIZES,
                        NUMS_L0_TABLES, BACKENDS, PRE_ISSUE_DEPTH_LIST)

    else:
        print(f"Error: unrecognized mode {args.mode}")
        exit(1)

if __name__ == "__main__":
    main()
