#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


CP_PREPARE_PY = "./scripts/cp-prepare.py"
CP_BENCHER_PY = "./scripts/cp-bencher.py"

CP_FILE_SIZES = {
    "8M":   8 * 1024 * 1024,
    "32M":  32 * 1024 * 1024,
    "128M": 128 * 1024 * 1024,
}
CP_NUM_FILES = 4

DU_PREPARE_PY = "./scripts/du-prepare.py"
DU_BENCHER_PY = "./scripts/du-bencher.py"

DU_NUM_DIRS = 10
DU_FILE_COUNTS = {
    "1k": 1000,
    "3k": 3000,
    "5k": 5000,
}
DU_FILE_SIZE = 5

BACKENDS = ["io_uring_default", "io_uring_sqe_async"]
PRE_ISSUE_DEPTH_LIST = [2, 4, 8, 16, 32, 64]


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


def check_arg_given(parser, args, argname):
    assert type(argname) == str
    if not hasattr(args, argname) or getattr(args, argname) is None:
        print(f"Error: option {argname} must be given")
        parser.print_help()
        exit(1)

def main():
    parser = argparse.ArgumentParser(description="Run all cp copy experiments")
    parser.add_argument('-m', dest='mode', required=True,
                        help="cp-prepare|cp-bencher|du-prepare|du-bencher")
    parser.add_argument('-d', dest='workdir_prefix', required=True,
                        help="absolute path prefix of workdir")
    parser.add_argument('-l', dest='libforeactor', required=False,
                        help="required for bencher; absolute path to libforeactor.so")
    parser.add_argument('-r', dest='results_dir', required=False,
                        help="required for bencher; directory to hold result logs")
    args = parser.parse_args()
    
    # change to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    check_dir_exists(args.workdir_prefix)

    if args.mode == "cp-prepare":
        check_file_exists(CP_PREPARE_PY)
        prepare_all_cp(args.workdir_prefix, CP_FILE_SIZES, CP_NUM_FILES)

    elif args.mode == "cp-bencher":
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(CP_BENCHER_PY)
        prepare_dir(args.results_dir, False)
        run_all_cp(args.libforeactor, args.results_dir, args.workdir_prefix,
                   CP_FILE_SIZES, CP_NUM_FILES, BACKENDS, PRE_ISSUE_DEPTH_LIST)

    elif args.mode == "du-prepare":
        check_file_exists(DU_PREPARE_PY)
        prepare_all_du(args.workdir_prefix, DU_NUM_DIRS, DU_FILE_COUNTS,
                       DU_FILE_SIZE)

    elif args.mode == "du-bencher":
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(DU_BENCHER_PY)
        prepare_dir(args.results_dir, False)
        run_all_du(args.libforeactor, args.results_dir, args.workdir_prefix,
                   DU_NUM_DIRS, DU_FILE_COUNTS, BACKENDS, PRE_ISSUE_DEPTH_LIST)

    else:
        print(f"Error: unrecognized mode {args.mode}")
        exit(1)

if __name__ == "__main__":
    main()
