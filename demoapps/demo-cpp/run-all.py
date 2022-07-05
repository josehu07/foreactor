#!/usr/bin/env python3

import argparse
import os
import subprocess


def gen_env(libforeactor, use_foreactor, backend):
    DEPTHS = [8, 2, 8, 32, 2, 16, 128, 128, 16, 16]
    QUEUE = 256
    UTHREADS = 8

    env = dict()
    env.update(os.environ)

    env["LD_PRELOAD"] = libforeactor
    
    if not use_foreactor:
        env["USE_FOREACTOR"] = "no"
    else:
        env["USE_FOREACTOR"] = "yes"
    
    for (i, d) in enumerate(DEPTHS):
        env[f"DEPTH_{i}"] = str(d)
        if backend is None or backend == 'io_uring_default':
            env[f"QUEUE_{i}"] = str(QUEUE)
            env[f"SQE_ASYNC_FLAG_{i}"] = "no"
        elif backend == 'io_uring_sqe_async':
            env[f"QUEUE_{i}"] = str(QUEUE)
            env[f"SQE_ASYNC_FLAG_{i}"] = "yes"
        elif backend == 'thread_pool':
            env[f"UTHREADS_{i}"] = str(UTHREADS)
        else:
            print(f"Error: unknown backend {backend}")
            exit(1)

    return env

def run_cmd(cmd, env):
    # print(f" env: {env} cmd: {cmd}")
    result = subprocess.run(cmd, stdout=subprocess.PIPE, check=True, env=env)
    return result.stdout.decode('utf-8')


def default_configs():
    return [{'use_foreactor': True, 'backend': 'io_uring_default',   'extra_args': []},
            {'use_foreactor': True, 'backend': 'io_uring_sqe_async', 'extra_args': []},
            {'use_foreactor': True, 'backend': 'thread_pool',        'extra_args': []}]

def args_str(original, args, config=None):
    if original:
        args_str = ' '.join(args)
        if len(args_str) > 0:
            args_str = ' ' + args_str
        return args_str
    else:
        args_str = f" use_foreactor={'yes' if config['use_foreactor'] else 'no'}"
        backend = 'none' if config['backend'] is None else config['backend']
        args_str += f" backend={backend}"
        args_str += ' ' + ' '.join(args+config['extra_args'])
        return args_str.rstrip()


def run_dump(name, dbdir, libforeactor, req_size, args=[], extra_configs=[]):
    cmd = ['./demo', '--exper', name, '--dbdir', dbdir, '--iters', '1', '--dump_result',
           '--req_size', str(req_size)]
    cmd += args

    print(f" dumping {name}-original{args_str(True, args)}...")
    env = gen_env(libforeactor, False, None)
    output_original = run_cmd(cmd, env)
    
    configs = default_configs()
    configs += extra_configs
    for config in configs:
        print(f" dumping {name}-config{args_str(False, args, config)}...", end='')
        this_cmd = cmd + config['extra_args']
        env = gen_env(libforeactor, config['use_foreactor'], config['backend'])

        all_correct = True
        for i in range(10):
            output_async = run_cmd(this_cmd, env)
            if output_original != output_async:
                print(" FAILED")
                print("  original:")
                print(output_original)
                print("  config:")
                print(output_async)
                all_correct = False
                break
        if all_correct:
            print(" CORRECT")

def run_stat(name, dbdir, libforeactor, timing_iters, req_size, args=[], extra_configs=[]):
    cmd = ['./demo', '--exper', name, '--dbdir', dbdir, '--iters', str(timing_iters),
           '--req_size', str(req_size)]
    cmd += args

    print(f" running {name}-original{args_str(True, args)}...")
    env = gen_env(libforeactor, False, None)
    output_original = run_cmd(cmd, env)
    if len(output_original) > 0:
        print(output_original)

    configs = default_configs()
    configs += extra_configs
    for config in configs:
        print(f" running {name}-config{args_str(False, args, config)}...")
        this_cmd = cmd + config['extra_args']
        env = gen_env(libforeactor, config['use_foreactor'], config['backend'])
        output_foreactor = run_cmd(this_cmd, env)
        if len(output_foreactor) > 0:
            print(output_foreactor)


def run_all(dbdir, libforeactor, timing_iters, req_size, skip_dump, skip_stat):
    if not skip_dump:
        print("Checking correctness ---")
        run_dump("simple", dbdir, libforeactor, req_size)
        run_dump("simple2", dbdir, libforeactor, req_size)
        run_dump("branching", dbdir, libforeactor, req_size)
        run_dump("looping", dbdir, libforeactor, req_size)
        run_dump("weak_edge", dbdir, libforeactor, req_size)
        run_dump("crossing", dbdir, libforeactor, req_size)
        run_dump("read_seq", dbdir, libforeactor, req_size,
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_dump("read_seq", dbdir, libforeactor, req_size,
                 args=['--same_buffer'])
        run_dump("read_seq", dbdir, libforeactor, req_size,
                 args=['--multi_file'],
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_dump("write_seq", dbdir, libforeactor, req_size,
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_dump("write_seq", dbdir, libforeactor, req_size,
                 args=['--multi_file'],
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_dump("streaming", dbdir, libforeactor, req_size)
        run_dump("streaming", dbdir, libforeactor, req_size,
                 args=['--same_buffer'])
        run_dump("ldb_get", dbdir, libforeactor, req_size)
        run_dump("ldb_get", dbdir, libforeactor, req_size,
                 args=['--same_buffer'])
        run_dump("ldb_get", dbdir, libforeactor, req_size,
                 args=['--key_match_at', '7'])
        run_dump("ldb_get", dbdir, libforeactor, req_size,
                 args=['--open_barrier'])
        run_dump("ldb_get", dbdir, libforeactor, req_size,
                 args=['--open_barrier', '--key_match_at', '7'])
        print()

    if not skip_stat:
        print("Running timed experiments ---")
        run_stat("read_seq", dbdir, libforeactor, timing_iters, req_size,
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_stat("read_seq", dbdir, libforeactor, timing_iters, req_size,
                 args=['--same_buffer'],
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_stat("read_seq", dbdir, libforeactor, timing_iters, req_size,
                 args=['--o_direct'],
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_stat("read_seq", dbdir, libforeactor, timing_iters, req_size,
                 args=['--same_buffer', '--o_direct'],
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_stat("write_seq", dbdir, libforeactor, timing_iters, req_size,
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_stat("write_seq", dbdir, libforeactor, timing_iters, req_size,
                 args=['--o_direct'],
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_stat("write_seq", dbdir, libforeactor, timing_iters, req_size,
                 args=['--multi_file'],
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_stat("write_seq", dbdir, libforeactor, timing_iters, req_size,
                 args=['--multi_file', '--o_direct'],
                 extra_configs=[{'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_ring']},
                                {'use_foreactor': False, 'backend': None, 'extra_args': ['--manual_pool']}])
        run_stat("streaming", dbdir, libforeactor, timing_iters, req_size)
        run_stat("streaming", dbdir, libforeactor, timing_iters, req_size,
                 args=['--same_buffer'])
        run_stat("streaming", dbdir, libforeactor, timing_iters, req_size,
                 args=['--o_direct'])
        run_stat("streaming", dbdir, libforeactor, timing_iters, req_size,
                 args=['--same_buffer', '--o_direct'])
        run_stat("ldb_get", dbdir, libforeactor, timing_iters, req_size)
        run_stat("ldb_get", dbdir, libforeactor, timing_iters, req_size,
                 args=['--same_buffer'])
        run_stat("ldb_get", dbdir, libforeactor, timing_iters, req_size,
                 args=['--o_direct'])
        run_stat("ldb_get", dbdir, libforeactor, timing_iters, req_size,
                 args=['--o_direct', '--open_barrier'])
        run_stat("ldb_get", dbdir, libforeactor, timing_iters, req_size,
                 args=['--o_direct', '--key_match_at', '7'])
        run_stat("ldb_get", dbdir, libforeactor, timing_iters, req_size,
                 args=['--o_direct', '--open_barrier', '--key_match_at', '7'])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run all demo workloads")
    parser.add_argument('--dbdir', type=str, required=True)
    parser.add_argument('--timing_iters', type=int, default=5)
    parser.add_argument('--req_size', type=int, default=65536)
    parser.add_argument('--skip_dump', action='store_true', default=False)
    parser.add_argument('--skip_stat', action='store_true', default=False)
    args = parser.parse_args()

    mydir = os.path.dirname(os.path.realpath(__file__))
    dbdir = os.path.realpath(args.dbdir)
    libforeactor = os.path.realpath(os.path.join(
        mydir, "../../libforeactor/libforeactor.so"))

    if not args.skip_stat and args.timing_iters <= 3:
        print(f"Error: num of timing iters {args.timing_iters} too small")
        exit(1)

    if not os.path.isfile(libforeactor):
        print(f"Error: {libforeactor} not found")
        exit(1)

    run_all(dbdir, libforeactor, args.timing_iters, args.req_size,
            args.skip_dump, args.skip_stat)
