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


def make_dir_tree_internal(curr_dir, tree_levels, tree_width, file_size):
    if tree_levels == 1:
        for i in range(tree_width):
            file_path = f"{curr_dir}/file_{i}.dat"
            create_random_file(file_path, file_size)
    else:
        for i in range(tree_width):
            dir_path = f"{curr_dir}/dir_{i}"
            prepare_empty_dir(dir_path)
            make_dir_tree_internal(dir_path, tree_levels-1, tree_width,
                                   file_size)

def make_tar_dir_tree(workdir, tree_levels, tree_width, file_size):
    make_dir_tree_internal(f"{workdir}/indir", tree_levels, tree_width,
                           file_size)


def main():
    parser = argparse.ArgumentParser(description="tar workload files setup")
    parser.add_argument('-d', dest='workdir', required=True,
                        help="workdir containing files")
    parser.add_argument('-s', dest='file_size', required=True, type=int,
                        help="file size in bytes")
    parser.add_argument('-l', dest='tree_levels', required=True, type=int,
                        help="number of directory tree levels")
    parser.add_argument('-w', dest='tree_width', required=True, type=int,
                        help="number of files/dirs per node")
    args = parser.parse_args()

    check_dir_exists(args.workdir)
    prepare_empty_dir(f"{args.workdir}/indir")

    if args.tree_levels < 1:
        print(f"Error: tree levels {args.tree_levels} must be >= 1")
        exit(1)
    if args.tree_width < 1:
        print(f"Error: tree width {args.tree_width} must be >= 1")
        exit(1)

    make_tar_dir_tree(args.workdir, args.tree_levels, args.tree_width,
                      args.file_size)

if __name__ == "__main__":
    main()
