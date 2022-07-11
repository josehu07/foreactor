#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


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


def create_random_file(path, file_size):
    cmd = ["head", "-c", str(file_size), "/dev/urandom"]
    result = subprocess.run(cmd, check=True, capture_output=True)
    output = result.stdout
    
    with open(path, 'wb') as f:
        f.write(output)


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

    check_dir_exists(args.workdir)
    prepare_empty_dir(f"{args.workdir}/indir")
    prepare_empty_dir(f"{args.workdir}/outdir")

    make_cp_files(args.workdir, args.file_size, args.num_files)

if __name__ == "__main__":
    main()
