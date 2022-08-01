#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


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


def create_random_file(path, file_size):
    cmd = ["head", "-c", str(file_size), "/dev/urandom"]

    with open(path, 'wb') as f:
        run_subprocess_cmd(cmd, outfile=f, merge=False)


def make_cp_files(workdir, file_size, num_files):
    for i in range(num_files):
        path = f"{workdir}/indir/in_{i}.dat"
        create_random_file(path, file_size)


def main():
    parser = argparse.ArgumentParser(description="cp workload files setup")
    parser.add_argument('-d', dest='workdir', required=True,
                        help="workdir containing files")
    parser.add_argument('-s', dest='file_size', required=True, type=int,
                        help="file size in bytes")
    parser.add_argument('-n', dest='num_files', required=True, type=int,
                        help="number of files per run")
    args = parser.parse_args()

    prepare_dir(args.workdir, True)
    prepare_dir(f"{args.workdir}/indir", True)
    prepare_dir(f"{args.workdir}/outdir", True)

    make_cp_files(args.workdir, args.file_size, args.num_files)

if __name__ == "__main__":
    main()
