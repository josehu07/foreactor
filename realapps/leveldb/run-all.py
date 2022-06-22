#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


def run_makedb(dbdir_prefix, value_size, value_size_abbr, ycsb_bin, ycsb_workload,
               num_l0_tables):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}_{num_l0_tables}"
    output_prefix = f"workloads/trace-{value_size_abbr}"

    cmd = ["python3", "makedb.py", "-d", dbdir, "-v", str(value_size),
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
    cmd = ["python3", "bench.py", "-l", libforeactor, "-d", dbdir,
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


def run_all(libforeactor, dbdir_prefix, ycsb_bin, ycsb_workload):
    value_sizes = {
        "256B": 256,
        "1K":   1024,
        "4K":   4 * 1024,
        "16K":  16 * 1024,
        "64K":  64 * 1024,
        "128K": 128 * 1024,
        "256K": 256 * 1024,
        "512K": 512 * 1024,
        "1M":   1024 * 1024,
    }
    nums_l0_tables = [8, 12]
    backends = ["io_uring_default", "io_uring_sqe_async", "thread_pool"]
    pre_issue_depth_list = [0, 4, 8, 12, 15]

    make_db_images(dbdir_prefix, ycsb_bin, ycsb_workload, value_sizes,
                   nums_l0_tables)

    run_all_samekey(libforeactor, dbdir_prefix, value_sizes, nums_l0_tables,
                    backends, pre_issue_depth_list)
    run_all_ycsbrun(libforeactor, dbdir_prefix, value_sizes, nums_l0_tables,
                    backends, pre_issue_depth_list)


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
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-d', dest='dbdir_prefix', required=True,
                        help="absolute path prefix of dbdir")
    parser.add_argument('-y', dest='ycsb_bin', required=True,
                        help="path to YCSB binary")
    parser.add_argument('-w', dest='ycsb_workload', required=True,
                        help="path to YCSB workload property file")
    args = parser.parse_args()
    
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    prepare_empty_dir("workloads")
    prepare_empty_dir("results")

    run_all(args.libforeactor, args.dbdir_prefix, args.ycsb_bin, args.ycsb_workload)

if __name__ == "__main__":
    main()
