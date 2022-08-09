#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


CP_PREPARE_PY = "./scripts/cp-prepare.py"
CP_BENCHER_PY = "./scripts/cp-bencher.py"
CP_PLOTTER_PY = "./scripts/cp-plotter.py"

CP_NUM_FILES = 8
CP_TOTAL_SIZES = {
    "2G":  2  * 1024 * 1024 * 1024,
    "4G":  4  * 1024 * 1024 * 1024,
    "8G":  8  * 1024 * 1024 * 1024,
    "16G": 16 * 1024 * 1024 * 1024,
}

CP_BACKENDS = ["io_uring_sqe_async"]
CP_PRE_ISSUE_DEPTH_LIST = [4, 16]

CP_TOTAL_SIZE_ABBR_FOR_UTIL = "2G"
CP_BACKEND_FOR_UTIL = "io_uring_sqe_async"

CP_FIGURES = ["avgtime"]

DU_PREPARE_PY = "./scripts/du-prepare.py"
DU_BENCHER_PY = "./scripts/du-bencher.py"
DU_PLOTTER_PY = "./scripts/du-plotter.py"

DU_NUM_DIRS = 100
DU_FILE_COUNTS = {
    "100k": 100 * 1000,
    "200k": 200 * 1000,
    "400k": 400 * 1000,
    "800k": 800 * 1000,
}

DU_BACKENDS = ["io_uring_sqe_async"]
DU_PRE_ISSUE_DEPTH_LIST = [4, 16]

DU_FILE_COUNT_ABBR_FOR_UTIL = "400k"
DU_BACKEND_FOR_UTIL = "io_uring_sqe_async"

DU_FIGURES = ["avgtime"]


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


def run_prepare_cp(workdir_prefix, file_size, file_size_abbr, num_files):
    workdir = f"{workdir_prefix}/cp_{file_size_abbr}_{num_files}"
    prepare_dir(workdir, True)

    cmd = ["python3", CP_PREPARE_PY, "-d", workdir, "-s", str(file_size),
           "-n", str(num_files)]
    return run_subprocess_cmd(cmd, merge=False)

def prepare_all_cp(workdir_prefix, file_sizes, num_files):
    for file_size_abbr, file_size in file_sizes.items():
        print(f"MAKING {file_size_abbr} {num_files}")
        run_prepare_cp(workdir_prefix, file_size, file_size_abbr, num_files)


def run_prepare_du(workdir_prefix, num_dirs, file_count, file_count_abbr):
    workdir = f"{workdir_prefix}/du_{num_dirs}_{file_count_abbr}"
    prepare_dir(workdir, True)

    cmd = ["python3", DU_PREPARE_PY, "-d", workdir, "-r", str(num_dirs),
           "-n", str(file_count)]
    return run_subprocess_cmd(cmd, merge=False)

def prepare_all_du(workdir_prefix, num_dirs, file_counts):
    for file_count_abbr, file_count in file_counts.items():
        print(f"MAKING {num_dirs} {file_count_abbr}")
        run_prepare_du(workdir_prefix, num_dirs, file_count, file_count_abbr)


def run_bench_cp(libforeactor, results_dir, workdir_prefix, file_size_abbr,
                 num_files, backend, pre_issue_depth_list):
    workdir = f"{workdir_prefix}/cp_{file_size_abbr}_{num_files}"
    output_log = f"{results_dir}/cp-{file_size_abbr}-{num_files}-" + \
                 f"{backend}.log"

    cmd = ["python3", CP_BENCHER_PY, "-l", libforeactor, "-d", workdir,
           "-o", output_log, "-b", backend]
    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    return run_subprocess_cmd(cmd, merge=False)

def run_all_cp(libforeactor, results_dir, workdir_prefix, file_sizes,
               num_files, backends, pre_issue_depth_list):
    for file_size_abbr, file_size in file_sizes.items():
        for backend in backends:
            print(f"RUNNING {file_size_abbr} {num_files} {backend}")
            output = run_bench_cp(libforeactor, results_dir,
                                  workdir_prefix,
                                  file_size_abbr, num_files,
                                  backend, pre_issue_depth_list)
            print(output.rstrip())


def run_bench_du(libforeactor, results_dir, workdir_prefix, num_dirs,
                 file_count_abbr, backend, pre_issue_depth_list):
    workdir = f"{workdir_prefix}/du_{num_dirs}_{file_count_abbr}"
    output_log = f"{results_dir}/du-{num_dirs}-{file_count_abbr}-" + \
                 f"{backend}.log"

    cmd = ["python3", DU_BENCHER_PY, "-l", libforeactor, "-d", workdir,
           "-o", output_log, "-b", backend]
    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    return run_subprocess_cmd(cmd, merge=False)

def run_all_du(libforeactor, results_dir, workdir_prefix, num_dirs,
               file_counts, backends, pre_issue_depth_list):
    for file_count_abbr, file_count in file_counts.items():
        for backend in backends:
            print(f"RUNNING {num_dirs} {file_count_abbr} {backend}")
            output = run_bench_du(libforeactor, results_dir,
                                  workdir_prefix,
                                  num_dirs, file_count_abbr,
                                  backend, pre_issue_depth_list)
            print(output.rstrip())


def plot_all_cp(results_dir, figures):
    for figure in figures:
        cmd = ["python3", CP_PLOTTER_PY, "-m", figure, "-r", results_dir,
               "-o", f"{results_dir}/cp"]
        run_subprocess_cmd(cmd, merge=False)
        print(f"PLOT {figure}")

def plot_all_du(results_dir, figures):
    for figure in figures:
        cmd = ["python3", DU_PLOTTER_PY, "-m", figure, "-r", results_dir,
               "-o", f"{results_dir}/du"]
        run_subprocess_cmd(cmd, merge=False)
        print(f"PLOT {figure}")


def run_cp_util(libforeactor, results_dir, workdir_prefix, dev_name,
                file_size_abbr, num_files, backend, pre_issue_depth_list):
    workdir = f"{workdir_prefix}/cp_{file_size_abbr}_{num_files}"
    output_log = f"{results_dir}/cp-{file_size_abbr}-{num_files}-" + \
                 f"{backend}-util.log"

    cmd = ["python3", CP_BENCHER_PY, "-l", libforeactor, "-d", workdir,
           "-o", output_log, "-b", backend, "--util_dev", dev_name]
    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    print(f"RUNNING {file_size_abbr} {num_files} {backend} utilization")
    output = run_subprocess_cmd(cmd, merge=False)
    print(output)

def run_du_util(libforeactor, results_dir, workdir_prefix, dev_name,
                num_dirs, file_count_abbr, backend, pre_issue_depth_list):
    workdir = f"{workdir_prefix}/du_{num_dirs}_{file_count_abbr}"
    output_log = f"{results_dir}/du-{num_dirs}-{file_count_abbr}-" + \
                 f"{backend}-util.log"

    cmd = ["python3", DU_BENCHER_PY, "-l", libforeactor, "-d", workdir,
           "-o", output_log, "-b", backend, "--util_dev", dev_name]
    cmd += list(map(lambda d: str(d), pre_issue_depth_list))

    print(f"RUNNING {num_dirs} {file_count_abbr} {backend} utilization")
    output = run_subprocess_cmd(cmd, merge=False)
    print(output)


def check_arg_given(parser, args, argname):
    assert type(argname) == str
    if not hasattr(args, argname) or getattr(args, argname) is None:
        print(f"Error: option {argname} must be given")
        parser.print_help()
        exit(1)

def main():
    parser = argparse.ArgumentParser(description="Run all coreutils experiments")
    parser.add_argument('-m', dest='mode', required=True,
                        help="cp-prepare|cp-bencher|cp-plotter|cp-utilization"
                             "du-prepare|du-bencher|du-plotter|du-utilization")
    parser.add_argument('-d', dest='workdir_prefix', required=False,
                        help="required for prepare/bencher; absolute path prefix of workdirs")
    parser.add_argument('-l', dest='libforeactor', required=False,
                        help="required for bencher; absolute path to libforeactor.so")
    parser.add_argument('-r', dest='results_dir', required=False,
                        help="required for bencher/plotter; directory to hold result logs")
    parser.add_argument('--util_dev', dest='util_dev', required=False,
                        help="required for utilization; disk device name")
    args = parser.parse_args()
    
    # change to the directory containing this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(script_dir)

    if args.mode == "cp-prepare":
        check_arg_given(parser, args, "workdir_prefix")
        check_file_exists(CP_PREPARE_PY)
        check_dir_exists(args.workdir_prefix)
        prepare_all_cp(args.workdir_prefix, CP_TOTAL_SIZES, CP_NUM_FILES)

    elif args.mode == "cp-bencher":
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "workdir_prefix")
        check_arg_given(parser, args, "results_dir")
        check_file_exists(CP_BENCHER_PY)
        prepare_dir(args.results_dir, False)
        run_all_cp(args.libforeactor, args.results_dir, args.workdir_prefix,
                   CP_TOTAL_SIZES, CP_NUM_FILES, CP_BACKENDS,
                   CP_PRE_ISSUE_DEPTH_LIST)

    elif args.mode == "cp-plotter":
        check_arg_given(parser, args, "results_dir")
        check_file_exists(CP_PLOTTER_PY)
        check_dir_exists(args.results_dir)
        plot_all_cp(args.results_dir, CP_FIGURES)
        # configs to plot are controlled in the plotter script
    
    elif args.mode == "cp-utilization":
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "workdir_prefix")
        check_arg_given(parser, args, "results_dir")
        check_arg_given(parser, args, "util_dev")
        check_file_exists(CP_BENCHER_PY)
        prepare_dir(args.results_dir, False)
        run_cp_util(args.libforeactor, args.results_dir, args.workdir_prefix,
                    args.util_dev, CP_TOTAL_SIZE_ABBR_FOR_UTIL, CP_NUM_FILES,
                    CP_BACKEND_FOR_UTIL, CP_PRE_ISSUE_DEPTH_LIST)

    elif args.mode == "du-prepare":
        check_arg_given(parser, args, "workdir_prefix")
        check_file_exists(DU_PREPARE_PY)
        check_dir_exists(args.workdir_prefix)
        prepare_all_du(args.workdir_prefix, DU_NUM_DIRS, DU_FILE_COUNTS)

    elif args.mode == "du-bencher":
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "workdir_prefix")
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

    elif args.mode == "du-utilization":
        check_arg_given(parser, args, "libforeactor")
        check_arg_given(parser, args, "workdir_prefix")
        check_arg_given(parser, args, "results_dir")
        check_arg_given(parser, args, "util_dev")
        check_file_exists(DU_BENCHER_PY)
        prepare_dir(args.results_dir, False)
        run_du_util(args.libforeactor, args.results_dir, args.workdir_prefix,
                    args.util_dev, DU_NUM_DIRS, DU_FILE_COUNT_ABBR_FOR_UTIL,
                    DU_BACKEND_FOR_UTIL, DU_PRE_ISSUE_DEPTH_LIST)

    else:
        print(f"Error: unrecognized mode {args.mode}")
        exit(1)

if __name__ == "__main__":
    main()
