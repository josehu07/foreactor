#!/usr/bin/bash


MICROBENCH_DIR=/mnt/ssd/josehu/microbench_dir
FLAMEGRAPH_DIR=/users/josehu/flame-graph


# sudo apt install linux-tools-common linux-tools-generic
# sudo apt install linux-tools-5.10.0-1057-oem    # closest version in apt repo
# sudo mv /usr/bin/perf /usr/bin/perf-script
# sudo ln -s /usr/lib/linux-tools/5.10.0-1057-oem/perf /usr/bin/perf


function get_flame_graph {
    local exper_name=$1
    local exper_flags=${@:2}

    sudo perf record -F 99 -a -g -o result-backup/perf-${exper_name}.data -- ./microbench -d ${MICROBENCH_DIR} ${exper_flags}
    sudo perf script -F comm,pid,tid,cpu,time,event,ip,sym,dso,trace -i result-backup/perf-${exper_name}.data \
        | ${FLAMEGRAPH_DIR}/stackcollapse-perf.pl --tid > result-backup/perf-${exper_name}.fold
    ${FLAMEGRAPH_DIR}/flamegraph.pl result-backup/perf-${exper_name}.fold > result-backup/perf-${exper_name}.svg
}

get_flame_graph "read-different-unlimited-4096-sync" --tr 100000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 4096 -a basic
get_flame_graph "read-different-unlimited-4096-io_uring_default" --tr 100000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 4096 -a io_uring
get_flame_graph "read-different-unlimited-4096-io_uring_sqeasync" --tr 100000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 4096 -a io_uring --iosqe_async
get_flame_graph "read-different-unlimited-4096-thread_pool_nproc" --tr 100000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 4096 -a thread_pool -t $(nproc)
get_flame_graph "read-different-unlimited-4096-thread_pool_4xsocks" --tr 100000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 4096 -a thread_pool -t 8

get_flame_graph "read-different-unlimited-1048576-sync" --tr 1000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 1048576 -a basic
get_flame_graph "read-different-unlimited-1048576-io_uring_default" --tr 1000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 1048576 -a io_uring
get_flame_graph "read-different-unlimited-1048576-io_uring_sqeasync" --tr 1000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 1048576 -a io_uring --iosqe_async
get_flame_graph "read-different-unlimited-1048576-thread_pool_nproc" --tr 1000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 1048576 -a thread_pool -t $(nproc)
get_flame_graph "read-different-unlimited-1048576-thread_pool_4xsocks" --tr 1000 --wr 0 -f 32 -s $((128*1024*1024)) -n 32 -r 1048576 -a thread_pool -t 8
