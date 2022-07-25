#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


DBMAKER_PY = "./scripts/dbmaker.py"
BENCHER_PY = "./scripts/bencher.py"
PLOTTER_PY = "./scripts/plotter.py"

VALUE_SIZES = {
    "1K":   1024,
    # "2K":   2 * 1024,
    "4K":   4 * 1024,
    # "8K":   8 * 1024,
    "16K":  16 * 1024,
    # "32K":  32 * 1024,
    "64K":  64 * 1024,
    # "128K": 128 * 1024,
    "256K": 256 * 1024,
    # "512K": 512 * 1024,
    # "1M":   1024 * 1024,
}
VALUE_SIZES_FOR_SAMEKEY = {
    "16K":  16 * 1024,
}
VALUE_SIZE_ABBR_FOR_BREAKDOWN = "16K"

NUMS_L0_TABLES = [12]
NUMS_L0_TABLES_FOR_SAMEKEY = [12]
NUM_L0_TABLES_FOR_BREAKDOWN = 12

YCSB_DISTRIBUTIONS = ["zipfian", "uniform"]
YCSB_DISTRIBUTIONS_FOR_SAMEKEY = ["uniform"]
YCSB_DISTRIBUTION_FOR_BREAKDOWN = "uniform"

# BACKENDS = ["io_uring_sqe_async", "thread_pool"]
BACKENDS = ["io_uring_sqe_async"]
BACKEND_FOR_BREAKDOWN = "io_uring_sqe_async"

# PRE_ISSUE_DEPTH_LIST = [4, 8, 12, 15]
PRE_ISSUE_DEPTH_LIST = [8, 15]

MEM_PERCENTAGES = [100, 80, 60, 40, 20]
MEM_PERCENTAGE_FOR_BREAKDOWN = 60

# GET_FIGURES = ["mem_ratio", "req_size", "heat_map", "controlled"]
GET_FIGURES = ["mem_ratio", "req_size", "heat_map"]
GET_BREAKDOWN_FIGURES = ["breakdown"]


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: file {path} does not exist")
        exit(1)

def check_dir_exists(dir_path):
    if not os.path.isdir(dir_path):
        print(f"Error: directory {dir_path} does not exist")
        exit(1)

def prepare_dir(dir_path, empty=False):
    if os.path.isdir(dir_path):
        if empty:
            for file in os.listdir(dir_path):
                path = os.path.join(dir_path, file)
                try:
                    shutil.rmtree(path)
                except OSError:
                    os.remove(path)
    else:
        os.mkdir(dir_path)


def run_makedb(workloads_dir, dbdir_prefix, value_size, value_size_abbr,
               ycsb_bin, ycsb_workload, num_l0_tables, ycsb_distribution,
               gen_samekey_workloads):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}_{num_l0_tables}_" + \
            f"{ycsb_distribution}"
    output_prefix = f"{workloads_dir}/trace-{value_size_abbr}-{num_l0_tables}-" + \
                    f"{ycsb_distribution}"

    cmd = ["python3", DBMAKER_PY, "-d", dbdir, "-v", str(value_size),
           "-y", ycsb_bin, "-w", ycsb_workload, "-t", str(num_l0_tables),
           "-z", ycsb_distribution, "-o", output_prefix]
    if gen_samekey_workloads:
        cmd.append("--gen_samekey")

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def make_db_images(workloads_dir, dbdir_prefix, ycsb_bin, ycsb_workload,
                   value_sizes, value_sizes_for_samekey, nums_l0_tables,
                   num_l0_tables_for_samekey, ycsb_distributions,
                   ycsb_distributions_for_samekey):
    for value_size_abbr, value_size in value_sizes.items():
        for num_l0_tables in nums_l0_tables:
            for ycsb_distribution in ycsb_distributions:
                gen_samekey_workloads = \
                    (value_size_abbr in value_sizes_for_samekey) and \
                    (num_l0_tables in num_l0_tables_for_samekey) and \
                    (ycsb_distribution in ycsb_distributions_for_samekey)
                run_makedb(workloads_dir, dbdir_prefix, value_size,
                           value_size_abbr, ycsb_bin, ycsb_workload,
                           num_l0_tables, ycsb_distribution, gen_samekey_workloads)
                print(f"MADE {value_size_abbr} {num_l0_tables} {ycsb_distribution}")


def get_dbdir_bytes(dbdir):
    cmd = ["du", "-s", "-b", dbdir]
    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    size_bytes = int(output.strip().split()[0])
    return size_bytes

def run_bench(libforeactor, dbdir, workload, output_log, backend,
              pre_issue_depth_list, drop_caches, mem_percentage=100):
    cmd = ["python3", BENCHER_PY, "-l", libforeactor, "-d", dbdir,
           "-f", workload, "-o", output_log, "-b", backend]
    if drop_caches:
        cmd.append("--drop_caches")
    
    assert mem_percentage > 0 and mem_percentage <= 100
    mem_bytes = int(get_dbdir_bytes(dbdir) * mem_percentage / 100)
    cmd += ["--mem_limit", str(mem_bytes)]

    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    result = subprocess.run(cmd, check=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT)
    output = result.stdout.decode('ascii')
    return output


def run_bench_samekey(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                      value_size_abbr, num_l0_tables, ycsb_distribution,
                      approx_num_preads, backend, pre_issue_depth_list,
                      drop_caches):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}_{num_l0_tables}_" + \
            f"{ycsb_distribution}"
    workload = f"{workloads_dir}/trace-{value_size_abbr}-{num_l0_tables}-" + \
               f"{ycsb_distribution}-samekey-{approx_num_preads}.txt"
    output_log = f"{results_dir}/ldb-{value_size_abbr}-{num_l0_tables}-" + \
                 f"{ycsb_distribution}-samekey-{approx_num_preads}-{backend}-" + \
                 f"{'drop_caches' if drop_caches else 'cached'}.log"
    return run_bench(libforeactor, dbdir, workload, output_log, backend,
                     pre_issue_depth_list, drop_caches)

def run_all_samekey(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                    value_sizes, nums_l0_tables, ycsb_distributions, backends,
                    pre_issue_depth_list):
    for value_size_abbr, value_size in value_sizes.items():
        for num_l0_tables in nums_l0_tables:
            for ycsb_distribution in ycsb_distributions:
                for approx_num_preads in range(1, num_l0_tables+2+1):   # FIXME
                    for backend in backends:
                        for drop_caches in (False, True):
                            output = run_bench_samekey(libforeactor, workloads_dir,
                                                       results_dir, dbdir_prefix,
                                                       value_size_abbr, num_l0_tables,
                                                       ycsb_distribution,
                                                       approx_num_preads, backend,
                                                       pre_issue_depth_list,
                                                       drop_caches)
                            print(f"RUN {value_size_abbr} {num_l0_tables} " + \
                                  f"{ycsb_distribution} {approx_num_preads} " + \
                                  f"{backend} {drop_caches}")
                            print(output.rstrip())


def run_bench_ycsbrun(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                      value_size_abbr, num_l0_tables, ycsb_distribution,
                      backend, pre_issue_depth_list, mem_percentage):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}_{num_l0_tables}_" + \
            f"{ycsb_distribution}"
    workload = f"{workloads_dir}/trace-{value_size_abbr}-{num_l0_tables}-" + \
               f"{ycsb_distribution}-ycsbrun.txt"
    output_log = f"{results_dir}/ldb-{value_size_abbr}-{num_l0_tables}-" + \
                 f"{ycsb_distribution}-ycsbrun-{backend}-mem_{mem_percentage}.log"
    return run_bench(libforeactor, dbdir, workload, output_log, backend,
                     pre_issue_depth_list, False, mem_percentage)

def run_all_ycsbrun(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                    value_sizes, nums_l0_tables, ycsb_distributions, backends,
                    pre_issue_depth_list, mem_percentages):
    for value_size_abbr, value_size in value_sizes.items():
        for num_l0_tables in nums_l0_tables:
            for ycsb_distribution in ycsb_distributions:
                for backend in backends:
                    for mem_percentage in mem_percentages:
                        output = run_bench_ycsbrun(libforeactor, workloads_dir,
                                                   results_dir, dbdir_prefix,
                                                   value_size_abbr, num_l0_tables,
                                                   ycsb_distribution, backend,
                                                   pre_issue_depth_list,
                                                   mem_percentage)
                        print(f"RUN {value_size_abbr} {num_l0_tables} " + \
                              f"{ycsb_distribution} ycsbrun {backend} " + \
                              f"mem_{mem_percentage}")
                        print(output.rstrip())


def run_plotter(results_dir, figure):
    cmd = ["python3", PLOTTER_PY, "-m", figure, "-r", results_dir,
           "-o", f"{results_dir}/ldb"]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def plot_all_figs(results_dir, figures):
    for figure in figures:
        run_plotter(results_dir, figure)
        print(f"PLOT {figure}")


def run_bench_with_timer(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                         value_size_abbr, num_l0_tables, ycsb_distribution,
                         backend, pre_issue_depth_list, mem_percentage):
    print(f"Note: please ensure that libforeactor.so is re-compiled with " + \
          f"`make clean && make timer`!")
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}_{num_l0_tables}_" + \
            f"{ycsb_distribution}"
    workload = f"{workloads_dir}/trace-{value_size_abbr}-{num_l0_tables}-" + \
               f"{ycsb_distribution}-ycsbrun.txt"
    output_log = f"{results_dir}/ldb-{value_size_abbr}-{num_l0_tables}-" + \
                 f"{ycsb_distribution}-ycsbrun-{backend}-mem_{mem_percentage}-" + \
                 f"with_timer.log"
    return run_bench(libforeactor, dbdir, workload, output_log, backend,
                     pre_issue_depth_list, False, mem_percentage)


def check_arg_given(parser, args, argname):
    assert type(argname) == str
    if not hasattr(args, argname) or getattr(args, argname) is None:
        print(f"Error: option {argname} must be given")
        parser.print_help()
        exit(1)

def main():
    parser = argparse.ArgumentParser(description="Run all LevelDB experiments")
    parser.add_argument('-m', dest='mode', required=True,
                        help="dbmaker|bencher|plotter")
    parser.add_argument('-d', dest='dbdir_prefix', required=False,
                        help="required for dbmaker/bencher; absolute path prefix of dbdirs")
    parser.add_argument('-y', dest='ycsb_bin', required=False,
                        help="required for dbmaker; path to YCSB binary")
    parser.add_argument('-p', dest='ycsb_workload', required=False,
                        help="required for dbmaker; path to YCSB workload property file")
    parser.add_argument('-l', dest='libforeactor', required=False,
                        help="required for bencher; absolute path to libforeactor.so")
    parser.add_argument('-w', dest='workloads_dir', required=False,
                        help="required for dbmaker/bencher; directory to hold generated traces")
    parser.add_argument('-r', dest='results_dir', required=False,
                        help="required for bencher/plotter; directory to hold result logs")
    args = parser.parse_args()
    
    # change to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    if args.mode == "dbmaker":
        check_arg_given(parser, args, "dbdir_prefix")
        check_arg_given(parser, args, "ycsb_bin")
        check_arg_given(parser, args, "ycsb_workload")
        check_arg_given(parser, args, "workloads_dir")
        check_file_exists(DBMAKER_PY)
        check_dir_exists(args.dbdir_prefix)
        prepare_dir(args.workloads_dir, False)
        make_db_images(args.workloads_dir, args.dbdir_prefix, args.ycsb_bin,
                       args.ycsb_workload, VALUE_SIZES, VALUE_SIZES_FOR_SAMEKEY,
                       NUMS_L0_TABLES, NUMS_L0_TABLES_FOR_SAMEKEY,
                       YCSB_DISTRIBUTIONS, YCSB_DISTRIBUTIONS_FOR_SAMEKEY)

    elif args.mode == "bencher":
        check_arg_given(parser, args, "dbdir_prefix")
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "workloads_dir")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(BENCHER_PY)
        check_dir_exists(args.dbdir_prefix)
        check_dir_exists(args.workloads_dir)
        prepare_dir(args.results_dir, False)
        run_all_ycsbrun(args.libforeactor, args.workloads_dir, args.results_dir,
                        args.dbdir_prefix, VALUE_SIZES, NUMS_L0_TABLES, YCSB_DISTRIBUTIONS,
                        BACKENDS, PRE_ISSUE_DEPTH_LIST, MEM_PERCENTAGES)
        run_all_samekey(args.libforeactor, args.workloads_dir, args.results_dir,
                        args.dbdir_prefix, VALUE_SIZES_FOR_SAMEKEY, NUMS_L0_TABLES_FOR_SAMEKEY,
                        YCSB_DISTRIBUTIONS_FOR_SAMEKEY, BACKENDS, PRE_ISSUE_DEPTH_LIST)

    elif args.mode == "plotter":
        check_arg_given(parser, args, "results_dir")
        check_file_exists(PLOTTER_PY)
        check_dir_exists(args.results_dir)
        plot_all_figs(args.results_dir, GET_FIGURES)
        # configs to plot are controlled in the plotter script

    elif args.mode == "breakdown":
        check_arg_given(parser, args, "dbdir_prefix")
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "workloads_dir")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(BENCHER_PY)
        check_file_exists(PLOTTER_PY)
        check_dir_exists(args.dbdir_prefix)
        check_dir_exists(args.workloads_dir)
        check_dir_exists(args.results_dir)
        run_bench_with_timer(args.libforeactor, args.workloads_dir, args.results_dir,
                             args.dbdir_prefix, VALUE_SIZE_ABBR_FOR_BREAKDOWN,
                             NUM_L0_TABLES_FOR_BREAKDOWN, YCSB_DISTRIBUTION_FOR_BREAKDOWN,
                             BACKEND_FOR_BREAKDOWN, PRE_ISSUE_DEPTH_LIST,
                             MEM_PERCENTAGE_FOR_BREAKDOWN)
        plot_all_figs(args.results_dir, GET_BREAKDOWN_FIGURES)

    else:
        print(f"Error: unrecognized mode {args.mode}")
        exit(1)

if __name__ == "__main__":
    main()
