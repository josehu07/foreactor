#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


CP_PREPARE_PY = "./scripts/cp-prepare.py"
CP_BENCHER_PY = "./scripts/cp-bencher.py"
CP_PLOTTER_PY = "./scripts/cp-plotter.py"

CP_FILE_SIZES = {
    "16M":  16   * 1024 * 1024,
    "32M":  32   * 1024 * 1024,
    "64M":  64   * 1024 * 1024,
    "128M": 128  * 1024 * 1024,
    "256M": 256  * 1024 * 1024,
}
CP_NUM_FILES = 4

CP_FIGURES = ["avgtime"]
CP_BACKENDS = ["io_uring_default"]
CP_PRE_ISSUE_DEPTH_LIST = [4, 8, 16, 32, 64]

DU_PREPARE_PY = "./scripts/du-prepare.py"
DU_BENCHER_PY = "./scripts/du-bencher.py"
DU_PLOTTER_PY = "./scripts/du-plotter.py"

DU_FILE_COUNTS = {
    "1k":  1  * 1000,
    "3k":  3  * 1000,
    "5k":  5  * 1000,
    "7k":  7  * 1000,
    "9k":  9  * 1000,
}
DU_NUM_DIRS = 4
DU_FILE_SIZE = 10

DU_FIGURES = ["avgtime"]
DU_BACKENDS = ["io_uring_sqe_async"]
DU_PRE_ISSUE_DEPTH_LIST = [4, 8, 16, 32, 64]


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


def run_prepare_cp(workdir_prefix, file_size, file_size_abbr, num_files):
    workdir = f"{workdir_prefix}/cp_{file_size_abbr}_{num_files}"
    prepare_dir(workdir, True)

    cmd = ["python3", CP_PREPARE_PY, "-d", workdir, "-s", str(file_size),
           "-n", str(num_files)]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def prepare_all_cp(workdir_prefix, file_sizes, num_files):
    for file_size_abbr, file_size in file_sizes.items():
        run_prepare_cp(workdir_prefix, file_size, file_size_abbr, num_files)
        print(f"MADE {file_size_abbr} {num_files}")


def run_prepare_du(workdir_prefix, num_dirs, file_count, file_count_abbr,
                   file_size):
    workdir = f"{workdir_prefix}/du_{num_dirs}_{file_count_abbr}"
    prepare_dir(workdir, True)

    cmd = ["python3", DU_PREPARE_PY, "-d", workdir, "-s", str(file_size),
           "-r", str(num_dirs), "-n", str(file_count)]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def prepare_all_du(workdir_prefix, num_dirs, file_counts, file_size):
    for file_count_abbr, file_count in file_counts.items():
        run_prepare_du(workdir_prefix, num_dirs, file_count, file_count_abbr,
                       file_size)
        print(f"MADE {num_dirs} {file_count_abbr}")


def run_bench_cp(libforeactor, results_dir, workdir_prefix, file_size_abbr,
                 num_files, backend, pre_issue_depth_list):
    workdir = f"{workdir_prefix}/cp_{file_size_abbr}_{num_files}"
    output_log = f"{results_dir}/cp-{file_size_abbr}-{num_files}-" + \
                 f"{backend}.log"

    cmd = ["python3", CP_BENCHER_PY, "-l", libforeactor, "-d", workdir,
           "-o", output_log, "-b", backend]
    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def run_all_cp(libforeactor, results_dir, workdir_prefix, file_sizes,
               num_files, backends, pre_issue_depth_list):
    for file_size_abbr, file_size in file_sizes.items():
        for backend in backends:
            output = run_bench_cp(libforeactor, results_dir,
                                  workdir_prefix,
                                  file_size_abbr, num_files,
                                  backend, pre_issue_depth_list)
            print(f"RUN {file_size_abbr} {num_files} {backend}")
            print(output.rstrip())


def run_bench_du(libforeactor, results_dir, workdir_prefix, num_dirs,
                 file_count_abbr, backend, pre_issue_depth_list):
    workdir = f"{workdir_prefix}/du_{num_dirs}_{file_count_abbr}"
    output_log = f"{results_dir}/du-{num_dirs}-{file_count_abbr}-" + \
                 f"{backend}.log"

    cmd = ["python3", DU_BENCHER_PY, "-l", libforeactor, "-d", workdir,
           "-o", output_log, "-b", backend]
    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def run_all_du(libforeactor, results_dir, workdir_prefix, num_dirs,
               file_counts, backends, pre_issue_depth_list):
    for file_count_abbr, file_count in file_counts.items():
        for backend in backends:
            output = run_bench_du(libforeactor, results_dir,
                                  workdir_prefix,
                                  num_dirs, file_count_abbr,
                                  backend, pre_issue_depth_list)
            print(f"RUN {num_dirs} {file_count_abbr} {backend}")
            print(output.rstrip())


def run_plot_cp(results_dir, figure):
    cmd = ["python3", CP_PLOTTER_PY, "-m", figure, "-r", results_dir,
           "-o", f"{results_dir}/cp"]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def plot_all_cp(results_dir, figures):
    for figure in figures:
        run_plot_cp(results_dir, figure)
        print(f"PLOT {figure}")


def run_plot_du(results_dir, figure):
    cmd = ["python3", DU_PLOTTER_PY, "-m", figure, "-r", results_dir,
           "-o", f"{results_dir}/du"]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def plot_all_du(results_dir, figures):
    for figure in figures:
        run_plot_du(results_dir, figure)
        print(f"PLOT {figure}")


def check_arg_given(parser, args, argname):
    assert type(argname) == str
    if not hasattr(args, argname) or getattr(args, argname) is None:
        print(f"Error: option {argname} must be given")
        parser.print_help()
        exit(1)

def main():
    parser = argparse.ArgumentParser(description="Run all coreutils experiments")
    parser.add_argument('-m', dest='mode', required=True,
                        help="cp-prepare|cp-bencher|cp-plotter|"
                             "du-prepare|du-bencher|du-plotter")
    parser.add_argument('-d', dest='workdir_prefix', required=False,
                        help="required for prepare/bencher; absolute path prefix of workdirs")
    parser.add_argument('-l', dest='libforeactor', required=False,
                        help="required for bencher; absolute path to libforeactor.so")
    parser.add_argument('-r', dest='results_dir', required=False,
                        help="required for bencher/plotter; directory to hold result logs")
    args = parser.parse_args()
    
    # change to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    if args.mode == "cp-prepare":
        check_arg_given(parser, args, "workdir_prefix")
        check_file_exists(CP_PREPARE_PY)
        check_dir_exists(args.workdir_prefix)
        prepare_all_cp(args.workdir_prefix, CP_FILE_SIZES, CP_NUM_FILES)

    elif args.mode == "cp-bencher":
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(CP_BENCHER_PY)
        prepare_dir(args.results_dir, False)
        run_all_cp(args.libforeactor, args.results_dir, args.workdir_prefix,
                   CP_FILE_SIZES, CP_NUM_FILES, CP_BACKENDS,
                   CP_PRE_ISSUE_DEPTH_LIST)

    elif args.mode == "cp-plotter":
        check_arg_given(parser, args, "results_dir")
        check_file_exists(CP_PLOTTER_PY)
        check_dir_exists(args.results_dir)
        plot_all_cp(args.results_dir, CP_FIGURES)
        # configs to plot are controlled in the plotter script

    elif args.mode == "du-prepare":
        check_arg_given(parser, args, "workdir_prefix")
        check_file_exists(DU_PREPARE_PY)
        check_dir_exists(args.workdir_prefix)
        prepare_all_du(args.workdir_prefix, DU_NUM_DIRS, DU_FILE_COUNTS,
                       DU_FILE_SIZE)

    elif args.mode == "du-bencher":
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(DU_BENCHER_PY)
        prepare_dir(args.results_dir, False)
        run_all_du(args.libforeactor, args.results_dir, args.workdir_prefix,
                   DU_NUM_DIRS, DU_FILE_COUNTS, DU_BACKENDS,
                   DU_PRE_ISSUE_DEPTH_LIST)

    elif args.mode == "du-plotter":
        check_arg_given(parser, args, "results_dir")
        check_file_exists(DU_PLOTTER_PY)
        check_dir_exists(args.results_dir)
        plot_all_du(args.results_dir, DU_FIGURES)
        # configs to plot are controlled in the plotter script

    else:
        print(f"Error: unrecognized mode {args.mode}")
        exit(1)

if __name__ == "__main__":
    main()