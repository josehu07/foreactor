#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


PREPARE_PY = "./scripts/prepare.py"
BENCHER_PY = "./scripts/bencher.py"
PLOTTER_PY = "./scripts/plotter.py"

DEGREES = [64, 128, 256, 510]   # 510 is the max allowed

BACKEND = "io_uring_sqe_async"

PRE_ISSUE_DEPTH_LIST_FOR_LOADS = [64, 256]
MEM_PERCENTAGE_FOR_LOADS = 100

PRE_ISSUE_DEPTH_LIST_FOR_SCANS = [64, 256]
MEM_PERCENTAGE_FOR_SCANS = 100

ALL_FIGURES = ["load_throughput", "scan_throughput"]


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


def run_subprocess_cmd(cmd, outfile=None, merge=False, env=None):
    try:
        result = None
        if outfile is None and not merge:
            result = subprocess.run(cmd, env=env, check=True,
                                         capture_output=True)
        elif outfile is None:
            result = subprocess.run(cmd, env=env, check=True,
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT)
        elif not merge:
            result = subprocess.run(cmd, env=env, check=True,
                                         stdout=outfile)
        else:
            result = subprocess.run(cmd, env=env, check=True,
                                         stdout=outfile,
                                         stderr=subprocess.STDOUT)
        output = None
        if result.stdout is not None:
            output = result.stdout.decode('ascii')
        return output
    except subprocess.CalledProcessError as err:
        print(f"Error: subprocess returned exit status {err.returncode}")
        print(f"  command: {' '.join(err.cmd)}")
        if err.stderr is not None:
            print(f"  stderr: {err.stderr.decode('ascii')}")
        exit(1)


def prepare_workloads(workloads_dir, degrees):
    output_prefix = f"{workloads_dir}/trace"
    cmd = ["python3", PREPARE_PY, "-d", ','.join(list(map(str, degrees))),
           "-o", output_prefix]
    print(f"MAKING {','.join(map(str, degrees))}")
    return run_subprocess_cmd(cmd, merge=False)


def run_bench(libforeactor, dbfile, workload, output_log, backend,
              pre_issue_depth_list, mem_percentage=100, remove_exist=False):
    cmd = ["python3", BENCHER_PY, "-l", libforeactor, "-f", dbfile,
           "-w", workload, "-o", output_log, "-b", backend]
    if remove_exist:
        cmd.append("--remove_exist")

    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    return run_subprocess_cmd(cmd, merge=False)

def run_bench_bptcli(libforeactor, workloads_dir, results_dir, dbfile_prefix,
                     degree, workload_name, backend, pre_issue_depth_list,
                     mem_percentage):
    dbfile = f"{dbfile_prefix}/bptree-{degree}.dat"
    workload = f"{workloads_dir}/trace-{degree}-{workload_name}.txt"
    output_log = f"{results_dir}/bpt-{degree}-{workload_name}-{backend}.log"

    print(f"RUNNING {degree} {workload_name}")
    output = run_bench(libforeactor, dbfile, workload, output_log, backend,
                       pre_issue_depth_list, mem_percentage=mem_percentage,
                       remove_exist=(workload_name == "load"))
    print(output.rstrip())

def run_all_loads(libforeactor, workloads_dir, results_dir, dbfile_prefix,
                  degrees, backend, pre_issue_depth_list, mem_percentage):
    for degree in degrees:
        run_bench_bptcli(libforeactor, workloads_dir, results_dir,
                         dbfile_prefix, degree, "load", backend,
                         pre_issue_depth_list, mem_percentage)

def run_all_scans(libforeactor, workloads_dir, results_dir, dbfile_prefix,
                  degrees, backend, pre_issue_depth_list, mem_percentage):
    for degree in degrees:
        run_bench_bptcli(libforeactor, workloads_dir, results_dir,
                         dbfile_prefix, degree, "scan", backend,
                         pre_issue_depth_list, mem_percentage)


def plot_all_figs(results_dir, figures):
    for figure in figures:
        cmd = ["python3", PLOTTER_PY, "-m", figure, "-r", results_dir,
               "-o", f"{results_dir}/bpt"]
        run_subprocess_cmd(cmd, merge=False)
        print(f"PLOT {figure}")


def check_arg_given(parser, args, argname):
    assert type(argname) == str
    if not hasattr(args, argname) or getattr(args, argname) is None:
        print(f"Error: option {argname} must be given")
        parser.print_help()
        exit(1)

def main():
    parser = argparse.ArgumentParser(description="Run all BPTree experiments")
    parser.add_argument('-m', dest='mode', required=True,
                        help="prepare|bencher|plotter")
    parser.add_argument('-w', dest='workloads_dir', required=False,
                        help="required for prepare/bencher; directory to hold workload traces")
    parser.add_argument('-d', dest='dbfile_prefix', required=False,
                        help="required for bencher; absolute path to working directory")
    parser.add_argument('-l', dest='libforeactor', required=False,
                        help="required for bencher; absolute path to libforeactor.so")
    parser.add_argument('-r', dest='results_dir', required=False,
                        help="required for bencher/plotter; directory to hold result logs")
    args = parser.parse_args()

    # change to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    if args.mode == "prepare":
        check_arg_given(parser, args, "workloads_dir")
        check_file_exists(PREPARE_PY)
        prepare_dir(args.workloads_dir, False)
        prepare_workloads(args.workloads_dir, DEGREES)

    elif args.mode == "bencher":
        check_arg_given(parser, args, "dbfile_prefix")
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "workloads_dir")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(BENCHER_PY)
        check_dir_exists(args.dbfile_prefix)
        check_dir_exists(args.workloads_dir)
        prepare_dir(args.results_dir, False)
        run_all_loads(args.libforeactor, args.workloads_dir, args.results_dir,
                      args.dbfile_prefix, DEGREES, BACKEND,
                      PRE_ISSUE_DEPTH_LIST_FOR_LOADS,
                      MEM_PERCENTAGE_FOR_LOADS)
        run_all_scans(args.libforeactor, args.workloads_dir, args.results_dir,
                      args.dbfile_prefix, DEGREES, BACKEND,
                      PRE_ISSUE_DEPTH_LIST_FOR_SCANS,
                      MEM_PERCENTAGE_FOR_SCANS)

    elif args.mode == "plotter":
        check_arg_given(parser, args, "results_dir")
        check_file_exists(PLOTTER_PY)
        check_dir_exists(args.results_dir)
        plot_all_figs(args.results_dir, ALL_FIGURES)

    else:
        print(f"Error: unrecognized mode {args.mode}")
        exit(1)

if __name__ == "__main__":
    main()
