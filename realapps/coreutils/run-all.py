#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


PREPARE_PY = "./prepare.py"
BENCHER_PY = "./bencher.py"

FILE_SIZES = {
    "8M":   8 * 1024 * 1024,
    "32M":  32 * 1024 * 1024,
    "128M": 128 * 1024 * 1024,
    # "512M": 512 * 1024 * 1024,
}
NUM_FILES = 4

BACKENDS = ["io_uring_default", "io_uring_sqe_async"]
PRE_ISSUE_DEPTH_LIST = [2, 4, 8, 16, 32]


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
        exit(1)

def check_dir_exists(dir_path):
    if not os.path.isdir(dir_path):
        print(f"Error: directory {dir_path} does not exist")
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


def run_prepare(workdir_prefix, file_size, file_size_abbr, num_files):
    workdir = f"{workdir_prefix}/cp_{file_size_abbr}_{num_files}"
    prepare_empty_dir(workdir)

    cmd = ["python3", PREPARE_PY, "-d", workdir, "-s", str(file_size),
           "-n", str(num_files)]

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def prepare_all(workdir_prefix, file_sizes, num_files):
    for file_size_abbr, file_size in file_sizes.items():
        run_prepare(workdir_prefix, file_size, file_size_abbr, num_files)
        print(f"MADE {file_size_abbr} {num_files}")


def run_bench_cp(libforeactor, workdir_prefix, file_size_abbr, num_files,
                 backend, pre_issue_depth_list):
    workdir = f"{workdir_prefix}/cp_{file_size_abbr}_{num_files}"
    output_log = f"results/result-{file_size_abbr}-{num_files}-" + \
                 f"{backend}.log"

    cmd = ["python3", BENCHER_PY, "-l", libforeactor, "-d", workdir,
           "-o", output_log, "-b", backend]
    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout.decode('ascii')
    return output

def run_all_cp(libforeactor, workdir_prefix, file_sizes, num_files,
               backends, pre_issue_depth_list):
    for file_size_abbr, file_size in file_sizes.items():
        for backend in backends:
            output = run_bench_cp(libforeactor, workdir_prefix,
                                  file_size_abbr, num_files, backend,
                                  pre_issue_depth_list)
            print(f"RUN {file_size_abbr} {num_files} {backend}")
            print(output.rstrip())


def main():
    parser = argparse.ArgumentParser(description="Run all cp copy experiments")
    parser.add_argument('-m', dest='mode', required=True,
                        help="which phase to run: prepare|bencher")
    parser.add_argument('-d', dest='workdir_prefix', required=True,
                        help="absolute path prefix of workdir")
    parser.add_argument('-l', dest='libforeactor', required=False,
                        help="required for bencher; absolute path to libforeactor.so")
    args = parser.parse_args()
    
    # change to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    check_dir_exists(args.workdir_prefix)

    if args.mode == "prepare":
        check_file_exists(PREPARE_PY)
        prepare_all(args.workdir_prefix, FILE_SIZES, NUM_FILES)

    elif args.mode == "bencher":
        check_file_exists(BENCHER_PY)
        prepare_empty_dir("results")

        assert args.libforeactor is not None

        run_all_cp(args.libforeactor, args.workdir_prefix, FILE_SIZES, NUM_FILES,
                   BACKENDS, PRE_ISSUE_DEPTH_LIST)

    else:
        print(f"Error: unrecognized mode {args.mode}")
        exit(1)

if __name__ == "__main__":
    main()
