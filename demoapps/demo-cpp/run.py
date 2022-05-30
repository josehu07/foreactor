#!/usr/bin/env python3

import argparse
import os
import subprocess


def gen_env(libforeactor, queue, depths):
    env = dict()
    env.update(os.environ)

    env["LD_PRELOAD"] = libforeactor
    
    for (i, d) in enumerate(depths):
        env[f"QUEUE_{i}"] = str(queue)
        env[f"DEPTH_{i}"] = str(d)
    
    env["USE_FOREACTOR"] = "no"
    return env

def run_cmd(cmd, env):
    # print(f" cmd: {cmd}")
    result = subprocess.run(cmd, stdout=subprocess.PIPE, check=True, env=env)
    return result.stdout.decode('utf-8')


def run_dump(name, dbdir, libforeactor, queue, depths, args=[]):
    cmd = ['./demo', name, dbdir, '1', '--dump_result']
    cmd += args

    env = gen_env(libforeactor, queue, depths)

    print(f" dumping {name}-original...")
    env["USE_FOREACTOR"] = "no"
    output_original = run_cmd(cmd, env)
    
    print(f" dumping {name}-foreactor...")
    env["USE_FOREACTOR"] = "yes"
    output_foreactor = run_cmd(cmd, env)

    print(f"  comparing outputs... ", end='')
    if output_original != output_foreactor:
        print("FAILED")
        print("  original:")
        print(output_original)
        print("  foreactor:")
        print(output_foreactor)
    else:
        print("CORRECT")

def run_demo(name, dbdir, libforeactor, queue, depths, args=[]):
    cmd = ['./demo', name, dbdir, '5']
    cmd += args

    env = gen_env(libforeactor, queue, depths)

    print(f" running {name}-original...")
    env["USE_FOREACTOR"] = "no"
    output_original = run_cmd(cmd, env)
    if len(output_original) > 0:
        print(output_original)

    print(f" running {name}-foreactor...")
    env["USE_FOREACTOR"] = "yes"
    output_foreactor = run_cmd(cmd, env)
    if len(output_foreactor) > 0:
        print(output_foreactor)


def run_all(dbdir, libforeactor):
    queue = 256
    depths = [8, 8, 32, 128, 128]

    print("Checking correctness ---")
    run_dump("simple", dbdir, libforeactor, queue, depths)
    run_dump("branching", dbdir, libforeactor, queue, depths)
    run_dump("looping", dbdir, libforeactor, queue, depths)

    print("\nRunning normal experiments ---")
    run_demo("simple", dbdir, libforeactor, queue, depths)
    run_demo("branching", dbdir, libforeactor, queue, depths)
    run_demo("looping", dbdir, libforeactor, queue, depths)
    run_demo("read_seq", dbdir, libforeactor, queue, depths)
    run_demo("write_seq", dbdir, libforeactor, queue, depths)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run all demo workloads")
    parser.add_argument('--dbdir', type=str, required=True)
    args = parser.parse_args()

    mydir = os.path.dirname(os.path.realpath(__file__))
    dbdir = os.path.realpath(args.dbdir)
    libforeactor = os.path.realpath(os.path.join(
        mydir, "../../libforeactor/libforeactor.so"))

    if not os.path.isfile(libforeactor):
        print(f"Error: {libforeactor} not found")
        exit(1)

    run_all(dbdir, libforeactor)
