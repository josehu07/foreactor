#!/usr/bin/env python3

import subprocess
import os
import shutil
import argparse


YCSBCLI_BIN = "./ycsbcli"
GET_GRAPH_ID = 0

URING_QUEUE = 32
CGROUP_NAME = "leveldb_group"

CACHED_ITERS = 5
DROP_CACHES_ITERS = 1


def check_file_exists(path):
    if not os.path.isfile(path):
        print(f"Error: {path} does not exist")
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

def copy_dir(src_dir, dst_dir):
    if not os.path.isdir(src_dir):
        print(f"Error: source path {src_dir} is not a directory")
        exit(1)
    shutil.copytree(src_dir, dst_dir, dirs_exist_ok=True)


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


def set_cgroup_mem_limit(mem_limit):
    cmd = ["sudo", "lscgroup"]
    output = run_subprocess_cmd(cmd, merge=False)
    cgroup_found = False
    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("memory:") and CGROUP_NAME in line:
            cgroup_found = True
            break

    if not cgroup_found:    # create cgroup
        cmd = ["sudo", "cgcreate", "-g", "memory:"+CGROUP_NAME]
        run_subprocess_cmd(cmd, merge=False)

    cmd = ["sudo", "cgset", "-r", "memory.limit_in_bytes="+str(mem_limit),
           CGROUP_NAME]
    run_subprocess_cmd(cmd, merge=False)

def get_iostat_bio_mb_read():
    # FIXME: currently just reads stat of the first device
    cmd = ["sudo", "iostat", "-d", "-m", "1", "1"]
    output = run_subprocess_cmd(cmd, merge=False)
    in_dev_line, mb_read_idx = False, -1
    for line in output.split('\n'):
        line = line.strip()
        if "MB_read" in line:
            in_dev_line = True
            mb_read_idx = line.split().index("MB_read")
        elif in_dev_line:
            assert mb_read_idx > 0
            return float(line.split()[mb_read_idx])
    return None


def run_ycsbcli_single(libforeactor, dbdir, trace, mem_limit, drop_caches,
                       use_foreactor, backend=None, pre_issue_depth=0,
                       num_threads=0, with_writes=False, tiny_bench=False):
    work_dbdir = dbdir
    if with_writes:
        work_dbdir = f"{dbdir}_copy"
        prepare_dir(work_dbdir, True)
        copy_dir(dbdir, work_dbdir)

    os.system("ulimit -n 65536")
    os.system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'")

    envs = os.environ.copy()
    envs["LD_PRELOAD"] = libforeactor
    envs["USE_FOREACTOR"] = "yes" if use_foreactor else "no"
    envs[f"DEPTH_{GET_GRAPH_ID}"] = str(pre_issue_depth)
    if backend == "io_uring_default":
        envs[f"QUEUE_{GET_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{GET_GRAPH_ID}"] = "no"
    elif backend == "io_uring_sqe_async":
        envs[f"QUEUE_{GET_GRAPH_ID}"] = str(URING_QUEUE)
        envs[f"SQE_ASYNC_FLAG_{GET_GRAPH_ID}"] = "yes"
    else:
        num_uthreads = pre_issue_depth
        if num_uthreads <= 0:
            num_uthreads = 1
        elif num_uthreads > 16:
            num_uthreads = 16
        envs[f"UTHREADS_{GET_GRAPH_ID}"] = str(num_uthreads)

    cmd = [YCSBCLI_BIN, "-d", work_dbdir, "-f", trace, "-t", str(num_threads),
           "--no_fill_cache"]
    if not with_writes:
        cmd.append("--bg_compact_off")
    if tiny_bench:
        cmd.append("--tiny_bench")
    if drop_caches:
        cmd.append("--drop_caches")
    if mem_limit != "none":
        set_cgroup_mem_limit(int(mem_limit))
        cmd = ["sudo", "cgexec", "-g", "memory:"+CGROUP_NAME] + cmd

    mb_read_before = get_iostat_bio_mb_read()
    output = run_subprocess_cmd(cmd, merge=True, env=envs)
    mb_read_after = get_iostat_bio_mb_read()
    return output, mb_read_after - mb_read_before


def get_us_result_from_output(output):
    in_timing_section = False
    sum_us, avg_us, p99_us = None, None, None

    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("Removing top-1"):
            in_timing_section = True
        elif in_timing_section and line.startswith("sum"):
            sum_us = float(line.split()[1])
        elif in_timing_section and line.startswith("avg"):
            avg_us = float(line.split()[1])
        elif in_timing_section and line.startswith("p99"):
            p99_us = float(line.split()[1])
            break

    return (sum_us, avg_us, p99_us)

def get_ops_result_from_output(output):
    in_timing_section = False
    sum_ops, avg_ops = None, None

    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("Throughput stats"):
            in_timing_section = True
        elif in_timing_section and line.startswith("sum"):
            sum_ops = float(line.split()[1])
        elif in_timing_section and line.startswith("avg"):
            avg_ops = float(line.split()[1])
            break

    return (sum_ops, avg_ops)

def get_timer_segs_from_output(output):
    timer_segs = dict()

    for line in output.split('\n'):
        line = line.strip()
        if line.startswith("# t"):
            l = line.split()
            seg_name = l[1][l[1].index("g0-")+3:]
            seg_cnt = int(l[4])
            if seg_cnt > 0:
                seg_us = float(l[-2])
                if seg_name not in timer_segs:
                    timer_segs[seg_name] = seg_us
                else:
                    timer_segs[seg_name] += seg_us

    return timer_segs


def run_ycsbcli_iters(num_iters, num_threads, libforeactor, dbdir, trace, mem_limit,
                      drop_caches, use_foreactor, backend=None, pre_issue_depth=0,
                      with_writes=False, with_timer=False, tiny_bench=False):
    result_sum_us, result_avg_us, result_p99_us = 0., 0., 0.
    result_sum_ops, result_avg_ops = 0., 0.
    result_mb_read = 0.
    result_timer_segs = dict()
    for i in range(num_iters):
        output, mb_read = run_ycsbcli_single(libforeactor, dbdir, trace, mem_limit,
                                             drop_caches, use_foreactor, backend=backend,
                                             pre_issue_depth=pre_issue_depth,
                                             num_threads=num_threads, with_writes=with_writes,
                                             tiny_bench=tiny_bench)
        sum_us, avg_us, p99_us = get_us_result_from_output(output)
        sum_ops, avg_ops = get_ops_result_from_output(output)
        if sum_us is not None:
            assert avg_us is not None
            assert p99_us is not None
            result_sum_us += sum_us
            result_avg_us += avg_us
            result_p99_us += p99_us
        if sum_ops is not None:
            assert avg_ops is not None
            result_sum_ops += sum_ops
            result_avg_ops += avg_ops
        result_mb_read += mb_read

        if with_timer:
            timer_segs = get_timer_segs_from_output(output)
            if timer_segs is not None:
                for seg_name, seg_us in timer_segs.items():
                    if seg_name not in result_timer_segs:
                        result_timer_segs[seg_name] = seg_us
                    else:
                        result_timer_segs[seg_name] += seg_us

    result_sum_us /= num_iters
    result_avg_us /= num_iters
    result_p99_us /= num_iters
    result_sum_ops /= num_iters
    result_avg_ops /= num_iters
    result_mb_read /= num_iters
    for seg_name in result_timer_segs:
        result_timer_segs[seg_name] /= num_iters

    return (result_sum_us, result_avg_us, result_p99_us, result_sum_ops, result_avg_ops,
            result_mb_read, result_timer_segs)

def run_exprs(libforeactor, dbdir, trace, mem_limit, drop_caches, output_log, backend,
              pre_issue_depth_list, num_threads, with_writes, with_timer, tiny_bench):
    num_iters = CACHED_ITERS if not drop_caches else DROP_CACHES_ITERS

    with open(output_log, 'w') as fout:
        sum_us, avg_us, p99_us, sum_ops, avg_ops, mb_read, timer_segs = \
            run_ycsbcli_iters(num_iters, num_threads, libforeactor, dbdir, trace,
                              mem_limit, drop_caches, False, with_writes=with_writes,
                              with_timer=with_timer, tiny_bench=tiny_bench)
        result = f" orig: sum_us {sum_us:.3f} avg_us {avg_us:.3f} p99_us {p99_us:.3f}" + \
                 f" sum_ops {sum_ops:.3f} avg_ops {avg_ops:.3f} MB_read {mb_read:.3f}"
        for seg_name, seg_us in timer_segs.items():
            result += f"\n       timer_us {seg_name} {seg_us:.3f}"
        fout.write(result+'\n')
        print(result)

        for pre_issue_depth in pre_issue_depth_list:
            sum_us, avg_us, p99_us, sum_ops, avg_ops, mb_read, timer_segs = \
                run_ycsbcli_iters(num_iters, num_threads, libforeactor, dbdir, trace,
                                  mem_limit, drop_caches, True, backend=backend,
                                  pre_issue_depth=pre_issue_depth, with_writes=with_writes,
                                  with_timer=with_timer, tiny_bench=tiny_bench)
            result = f" {pre_issue_depth:4d}:" + \
                     f" sum_us {sum_us:.3f} avg_us {avg_us:.3f} p99_us {p99_us:.3f}" + \
                     f" sum_ops {sum_ops:.3f} avg_ops {avg_ops:.3f} MB_read {mb_read:.3f}"
            for seg_name, seg_us in timer_segs.items():
                result += f"\n       timer_us {seg_name} {seg_us:.3f}"
            fout.write(result+'\n')
            print(result)


def main():
    parser = argparse.ArgumentParser(description="LevelDB benchmark driver")
    parser.add_argument('-l', dest='libforeactor', required=True,
                        help="absolute path to libforeactor.so")
    parser.add_argument('-d', dest='dbdir', required=True,
                        help="dbdir of LevelDB")
    parser.add_argument('-f', dest='trace', required=True,
                        help="trace file to run ycsbcli")
    parser.add_argument('-o', dest='output_log', required=True,
                        help="output log filename")
    parser.add_argument('-b', dest='backend', required=True,
                        help="io_uring_default|io_uring_sqe_async|thread_pool")
    parser.add_argument('-t', dest='num_threads', type=int, required=False, default=0,
                        help="if > 1, do multithreading on read-only snapshot")
    parser.add_argument('-m', dest='mem_limit', required=False, default="none",
                        help="memory limit to bound page cache size")
    parser.add_argument('--with_writes', dest='with_writes', action='store_true',
                        help="should be given when running a workload with writes")
    parser.add_argument('--with_timer', dest='with_timer', action='store_true',
                        help="expect timer information output")
    parser.add_argument('--tiny_bench', dest='tiny_bench', action='store_true',
                        help="for debugging; run only a few lines of each workload")
    parser.add_argument('--drop_caches', dest='drop_caches', action='store_true',
                        help="do drop_caches per request")
    parser.add_argument('pre_issue_depths', metavar='D', type=int, nargs='+',
                        help="list of pre_issue_depth to try")
    args = parser.parse_args()

    if args.backend != "io_uring_default" and args.backend != "io_uring_sqe_async" \
       and args.backend != "thread_pool":
        print(f"Error: unrecognized backend {args.backend}")
        exit(1)

    if args.mem_limit != "none":
        try:
            int(args.mem_limit)
        except:
            print("Error: mem_limit must be an integer in bytes")
            exit(1)

    if args.num_threads < 0:
        print(f"Error: invalid number of threads {args.num_threads}")
        exit(1)

    if args.with_writes and args.num_threads > 0:
        print(f"Error: with_writes currently incompatible with multithreading")
        exit(1)

    check_file_exists(args.libforeactor)
    check_file_exists(YCSBCLI_BIN)
    check_dir_exists(args.dbdir)

    if args.num_threads == 0 and not args.with_writes:
        check_file_exists(args.trace)
    else:
        if not args.trace.endswith(".txt"):
            check_file_exists(args.trace+"-0.txt")
            check_file_exists(args.trace+f"-{args.num_threads-1}.txt")

    run_exprs(args.libforeactor, args.dbdir, args.trace, args.mem_limit,
              args.drop_caches, args.output_log, args.backend, args.pre_issue_depths,
              args.num_threads, args.with_writes, args.with_timer, args.tiny_bench)

if __name__ == "__main__":
    main()
