#!/usr/bin/env python3

import os
import argparse
import random


MAX_KEY = 10000000

NUM_LOAD_KEYS = 128 * 128 * 100

NUM_SCANS_OPS = 10
SCAN_RANGE_LEN = 9000000


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
        exit(1)

def check_dir_exists(dir_path):
    if not os.path.isdir(dir_path):
        print(f"Error: directory {dir_path} does not exist")
        exit(1)


def generate_load_traces(output_prefix, degree, num_loads):
    trace = f"{output_prefix}-{degree}-load.txt"
    with open(trace, 'w') as ftrace:
        ftrace.write(f"DEGREE {degree}\n")
        ftrace.write(f"LOAD {num_loads}\n")
        for i in range(num_loads):
            key = random.randint(0, MAX_KEY)
            ftrace.write(f"K {key}\n")
        ftrace.write(f"ENDLOAD {num_loads}\n")

def generate_scan_traces(output_prefix, degree, num_scans, scan_range):
    trace = f"{output_prefix}-{degree}-scan.txt"
    with open(trace, 'w') as ftrace:
        ftrace.write(f"DEGREE {degree}\n")
        for i in range(num_scans):
            key = random.randint(0, MAX_KEY - SCAN_RANGE_LEN - 1)
            ftrace.write(f"SCAN {key} {key + SCAN_RANGE_LEN}\n")


def get_comma_separated_list(arg, argname):
    l = arg.strip().split(',')
    if len(l) == 0:
        print(f"Error: empty comma-separated list argument {argname}")
        exit(1)
    try:
        l = list(map(int, l))
    except:
        print(f"Error: failed to convert {argname} to list of integers")
        exit(1)
    return l

def main():
    parser = argparse.ArgumentParser(description="BPTree workload preparer")
    parser.add_argument('-d', dest='degrees', required=True,
                        help="degree of B+ tree, comma-separated list")
    parser.add_argument('-o', dest='output_prefix', required=True,
                        help="path prefix of output workloads")
    args = parser.parse_args()
    
    degrees = get_comma_separated_list(args.degrees, "degrees")
    for degree in degrees:
        generate_load_traces(args.output_prefix, degree, NUM_LOAD_KEYS)
        generate_scan_traces(args.output_prefix, degree, NUM_SCANS_OPS,
                             SCAN_RANGE_LEN)
    
if __name__ == "__main__":
    main()
