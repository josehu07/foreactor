#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


PREPARE_PY = "./scripts/prepare.py"
BENCHER_PY = "./scripts/bencher.py"
PLOTTER_PY = "./scripts/plotter.py"

VALUE_SIZES = {
    "128B": 128,
    "256B": 256,
    "512B": 512,
    "1K":   1024,
    "2K":   2 * 1024,
    "4K":   4 * 1024,
    "8K":   8 * 1024,
    "16K":  16 * 1024,
    "32K":  32 * 1024,
    "64K":  64 * 1024,
}
YCSB_DISTRIBUTIONS = ["zipfian", "uniform"]
BACKENDS = ["io_uring_sqe_async"]
PRE_ISSUE_DEPTH_LIST = [16]
MEM_PERCENTAGES = [100-i*10 for i in range(10)]

VALUE_SIZES_FOR_SAMEKEY = {}
BACKENDS_FOR_SAMEKEY = ["io_uring_sqe_async"]

VALUE_SIZE_ABBR_FOR_MULTITHREAD = "1K"
YCSB_DISTRIBUTION_FOR_MULTITHREAD = "zipfian"
BACKEND_FOR_MULTITHREAD = "io_uring_sqe_async"
PRE_ISSUE_DEPTH_LIST_FOR_MULTITHREAD = [4, 16]
MEM_PERCENTAGE_FOR_MULTITHREAD = 10
MULTITHREAD_NUMS_THREADS = [i+1 for i in range(8)]

VALUE_SIZE_ABBR_FOR_WITH_WRITES = "1K"
YCSB_DISTRIBUTION_FOR_WITH_WRITES = "zipfian"
BACKEND_FOR_WITH_WRITES = "io_uring_sqe_async"
PRE_ISSUE_DEPTH_LIST_FOR_WITH_WRITES = [4, 16]
MEM_PERCENTAGE_FOR_WITH_WRITES = 10
WITH_WRITES_WORKLOADS = ["a", "b", "d", "e", "f"]

VALUE_SIZE_ABBR_FOR_BREAKDOWN = "1K"
YCSB_DISTRIBUTION_FOR_BREAKDOWN = "zipfian"
BACKEND_FOR_BREAKDOWN = "io_uring_sqe_async"
MEM_PERCENTAGE_FOR_BREAKDOWN = 10

GET_FIGURES = ["mem_ratio", "req_size", "tail_lat", "heat_map",
               "multithread", "with_writes"]
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


def run_prepare(workloads_dir, dbdir_prefix, value_size, value_size_abbr,
                ycsb_dir, tmpdir, ycsb_workloads, ycsb_distributions,
                gen_samekey_workloads=False, max_threads=1):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}"
    output_prefix = f"{workloads_dir}/trace-{value_size_abbr}"

    cmd = ["python3", PREPARE_PY, "-d", dbdir, "-t", tmpdir, "-v", str(value_size),
           "-y", ycsb_dir, "-o", output_prefix,
           "-w", ','.join(ycsb_workloads), "-z", ','.join(ycsb_distributions)]
    if gen_samekey_workloads:
        cmd.append("--gen_samekey")
    if max_threads > 1:
        cmd += ["--max_threads", str(max_threads)]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def prepare_db_and_workloads(workloads_dir, dbdir_prefix, ycsb_dir, tmpdir,
                             value_sizes, value_sizes_for_samekey,
                             ycsb_c_run_distributions,
                             value_size_abbr_for_multithread,
                             ycsb_distribution_for_multithread, max_threads,
                             value_size_abbr_for_with_writes,
                             ycsb_distribution_for_with_writes, write_workloads):
    for value_size_abbr, value_size in value_sizes.items():
        print(f"MAKING {value_size_abbr}")

        run_prepare(workloads_dir, dbdir_prefix, value_size,
                    value_size_abbr, ycsb_dir, tmpdir,
                    ["c"], ycsb_c_run_distributions,
                    gen_samekey_workloads=(value_size_abbr in value_sizes_for_samekey))

        if value_size_abbr == value_size_abbr_for_with_writes:
            run_prepare(workloads_dir, dbdir_prefix, value_size,
                        value_size_abbr, ycsb_dir, tmpdir,
                        write_workloads, [ycsb_distribution_for_with_writes])

        if value_size_abbr == value_size_abbr_for_multithread:
            run_prepare(workloads_dir, dbdir_prefix, value_size,
                        value_size_abbr, ycsb_dir, tmpdir,
                        ["c"], [ycsb_distribution_for_multithread],
                        max_threads=max_threads)


def get_dbdir_bytes(dbdir):
    cmd = ["du", "-s", "-b", dbdir]
    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    size_bytes = int(output.strip().split()[0])
    return size_bytes

def run_bench(libforeactor, dbdir, workload, output_log, backend,
              pre_issue_depth_list, drop_caches, mem_percentage=100,
              num_threads=0, with_writes=False, with_timer=False):
    cmd = ["python3", BENCHER_PY, "-l", libforeactor, "-d", dbdir,
           "-f", workload, "-o", output_log, "-b", backend, "-t", str(num_threads)]
    if with_writes:
        cmd.append("--with_writes")
    if with_timer:
        cmd.append("--with_timer")
    if drop_caches:
        cmd.append("--drop_caches")
    
    assert mem_percentage > 0 and mem_percentage <= 100
    mem_bytes = int(get_dbdir_bytes(dbdir) * mem_percentage / 100)
    cmd += ["-m", str(mem_bytes)]

    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output


def run_bench_samekey(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                      value_size_abbr, approx_num_preads, backend,
                      pre_issue_depth_list, drop_caches):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}"
    workload = f"{workloads_dir}/trace-{value_size_abbr}-samekey-" + \
               f"{approx_num_preads}-0.txt"
    output_log = f"{results_dir}/ldb-{value_size_abbr}-samekey-{approx_num_preads}-" + \
                 f"{backend}-{'drop_caches' if drop_caches else 'cached'}.log"

    print(f"RUNNING {value_size_abbr} samekey {approx_num_preads} {backend} "
          f"{'drop_caches' if drop_caches else 'cached'}")
    output = run_bench(libforeactor, dbdir, workload, output_log, backend,
                       pre_issue_depth_list, drop_caches)
    print(output.rstrip())

def run_all_samekey(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                    value_sizes, backends, pre_issue_depth_list):
    for value_size_abbr, _ in value_sizes.items():
        for approx_num_preads in range(1, 16):      # FIXME
            for backend in backends:
                for drop_caches in (False, True):
                    run_bench_samekey(libforeactor, workloads_dir, results_dir,
                                      dbdir_prefix, value_size_abbr, approx_num_preads,
                                      backend, pre_issue_depth_list, drop_caches)


def run_bench_ycsb(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                   value_size_abbr, workload_name, ycsb_distribution, backend,
                   pre_issue_depth_list, mem_percentage, num_threads=0,
                   with_writes=False):
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}"
    workload = f"{workloads_dir}/trace-{value_size_abbr}-ycsb-{workload_name}-" + \
               f"{ycsb_distribution}"
    if num_threads == 0:
        workload += "-0.txt"
    output_log = f"{results_dir}/ldb-{value_size_abbr}-ycsb-{workload_name}-" + \
                 f"{ycsb_distribution}-{backend}-mem_{mem_percentage}-" + \
                 f"threads_{num_threads}.log"

    print(f"RUNNING {value_size_abbr} ycsb {workload_name} {ycsb_distribution} " + \
          f"{backend} mem_{mem_percentage} threads_{num_threads}")
    output = run_bench(libforeactor, dbdir, workload, output_log, backend,
                       pre_issue_depth_list, False, mem_percentage=mem_percentage,
                       num_threads=num_threads, with_writes=with_writes)
    print(output.rstrip())

def run_all_ycsb_c_run(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                       value_sizes, ycsb_distributions, backends,
                       pre_issue_depth_list, mem_percentages):
    for value_size_abbr, _ in value_sizes.items():
        for ycsb_distribution in ycsb_distributions:
            for backend in backends:
                for mem_percentage in mem_percentages:
                    run_bench_ycsb(libforeactor, workloads_dir, results_dir,
                                   dbdir_prefix, value_size_abbr, "c",
                                   ycsb_distribution, backend, pre_issue_depth_list,
                                   mem_percentage)

def run_all_multithread(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                        value_size_abbr, ycsb_distribution, backend,
                        pre_issue_depth_list, mem_percentage, nums_threads):
    for num_threads in nums_threads:
        run_bench_ycsb(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                       value_size_abbr, "c", ycsb_distribution, backend,
                       pre_issue_depth_list, mem_percentage, num_threads=num_threads)

def run_all_with_writes(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                        value_size_abbr, ycsb_distribution, backend,
                        pre_issue_depth_list, mem_percentage, write_workloads):
    for workload_name in write_workloads:
        run_bench_ycsb(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                       value_size_abbr, workload_name, ycsb_distribution, backend,
                       pre_issue_depth_list, mem_percentage, with_writes=True)


def run_bench_with_timer(libforeactor, workloads_dir, results_dir, dbdir_prefix,
                         value_size_abbr, ycsb_distribution, backend,
                         pre_issue_depth_list, mem_percentage):
    print(f"Note: please make sure that libforeactor.so is re-compiled with " + \
          f"`make clean && make timer`!")
    dbdir = f"{dbdir_prefix}/leveldb_{value_size_abbr}"
    workload = f"{workloads_dir}/trace-{value_size_abbr}-ycsb-c-" + \
               f"{ycsb_distribution}-0.txt"
    output_log = f"{results_dir}/ldb-{value_size_abbr}-ycsb-c-" + \
                 f"{ycsb_distribution}-{backend}-mem_{mem_percentage}-" + \
                 f"with_timer.log"

    print(f"RUNNING {value_size_abbr} ycsb c {ycsb_distribution} {backend} " + \
          f"mem_{mem_percentage} with_timer")
    run_bench(libforeactor, dbdir, workload, output_log, backend,
              pre_issue_depth_list, False, mem_percentage=mem_percentage,
              with_timer=True)
    print(" DONE")


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


def check_arg_given(parser, args, argname):
    assert type(argname) == str
    if not hasattr(args, argname) or getattr(args, argname) is None:
        print(f"Error: option {argname} must be given")
        parser.print_help()
        exit(1)

def main():
    parser = argparse.ArgumentParser(description="Run all LevelDB experiments")
    parser.add_argument('-m', dest='mode', required=True,
                        help="prepare|bencher|plotter")
    parser.add_argument('-y', dest='ycsb_dir', required=False,
                        help="required for prepare; path to official YCSB directory")
    parser.add_argument('-t', dest='tmpdir', required=False,
                        help="required for prepare; directory to hold temp files, "
                             "available space must be >= 10GB")
    parser.add_argument('-d', dest='dbdir_prefix', required=False,
                        help="required for prepare/bencher; absolute path prefix of dbdirs")
    parser.add_argument('-w', dest='workloads_dir', required=False,
                        help="required for prepare/bencher; directory to hold generated traces")
    parser.add_argument('-l', dest='libforeactor', required=False,
                        help="required for bencher; absolute path to libforeactor.so")
    parser.add_argument('-r', dest='results_dir', required=False,
                        help="required for bencher/plotter; directory to hold result logs")
    args = parser.parse_args()
    
    # change to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    if args.mode == "prepare":
        check_arg_given(parser, args, "tmpdir")
        check_arg_given(parser, args, "dbdir_prefix")
        check_arg_given(parser, args, "ycsb_dir")
        check_arg_given(parser, args, "workloads_dir")
        check_file_exists(PREPARE_PY)
        check_dir_exists(args.tmpdir)
        check_dir_exists(args.dbdir_prefix)
        check_dir_exists(args.ycsb_dir)
        prepare_dir(args.workloads_dir, False)
        prepare_db_and_workloads(args.workloads_dir, args.dbdir_prefix, args.ycsb_dir,
                                 args.tmpdir, VALUE_SIZES,
                                 VALUE_SIZES_FOR_SAMEKEY,
                                 YCSB_DISTRIBUTIONS,
                                 VALUE_SIZE_ABBR_FOR_MULTITHREAD,
                                 YCSB_DISTRIBUTION_FOR_MULTITHREAD,
                                 max(MULTITHREAD_NUMS_THREADS),
                                 VALUE_SIZE_ABBR_FOR_WITH_WRITES,
                                 YCSB_DISTRIBUTION_FOR_WITH_WRITES,
                                 WITH_WRITES_WORKLOADS)

    elif args.mode == "bencher":
        check_arg_given(parser, args, "dbdir_prefix")
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "workloads_dir")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(BENCHER_PY)
        check_dir_exists(args.dbdir_prefix)
        check_dir_exists(args.workloads_dir)
        prepare_dir(args.results_dir, False)
        run_all_ycsb_c_run(args.libforeactor,
                           args.workloads_dir, args.results_dir,
                           args.dbdir_prefix, VALUE_SIZES,
                           YCSB_DISTRIBUTIONS,
                           BACKENDS,
                           PRE_ISSUE_DEPTH_LIST,
                           MEM_PERCENTAGES)
        run_all_multithread(args.libforeactor, args.workloads_dir, args.results_dir,
                            args.dbdir_prefix, VALUE_SIZE_ABBR_FOR_MULTITHREAD,
                            YCSB_DISTRIBUTION_FOR_MULTITHREAD,
                            BACKEND_FOR_MULTITHREAD,
                            PRE_ISSUE_DEPTH_LIST_FOR_MULTITHREAD,
                            MEM_PERCENTAGE_FOR_MULTITHREAD,
                            MULTITHREAD_NUMS_THREADS)
        run_all_with_writes(args.libforeactor, args.workloads_dir, args.results_dir,
                            args.dbdir_prefix, VALUE_SIZE_ABBR_FOR_WITH_WRITES,
                            YCSB_DISTRIBUTION_FOR_WITH_WRITES,
                            BACKEND_FOR_WITH_WRITES,
                            PRE_ISSUE_DEPTH_LIST_FOR_WITH_WRITES,
                            MEM_PERCENTAGE_FOR_WITH_WRITES,
                            WITH_WRITES_WORKLOADS)
        # run_all_samekey(args.libforeactor,
        #                 args.workloads_dir, args.results_dir,
        #                 args.dbdir_prefix, VALUE_SIZES_FOR_SAMEKEY,
        #                 BACKENDS_FOR_SAMEKEY,
        #                 PRE_ISSUE_DEPTH_LIST)

    elif args.mode == "plotter":
        check_arg_given(parser, args, "results_dir")
        check_file_exists(PLOTTER_PY)
        check_dir_exists(args.results_dir)
        plot_all_figs(args.results_dir, GET_FIGURES)
        # configs to plot are controlled in the plotter script

    elif args.mode == "breakdown":
        # requires libforeactor to be compiled with timer option on
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
                             YCSB_DISTRIBUTION_FOR_BREAKDOWN,
                             BACKEND_FOR_BREAKDOWN,
                             PRE_ISSUE_DEPTH_LIST,
                             MEM_PERCENTAGE_FOR_BREAKDOWN)
        plot_all_figs(args.results_dir, GET_BREAKDOWN_FIGURES)

    else:
        print(f"Error: unrecognized mode {args.mode}")
        exit(1)

if __name__ == "__main__":
    main()
