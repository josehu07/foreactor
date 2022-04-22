#!/usr/bin/bash


MICROBENCH_DIR=/mnt/ssd/josehu/microbench_dir
FLAMEGRAPH_DIR=/users/josehu/flame-graph


function get_flame_graph {
    local exper_name=$1
    local exper_flags=${@:2}

    sudo perf record -F 99 -a -g -o result-backup/perf-${exper_name}.data -- ./microbench -d ${MICROBENCH_DIR} \
        ${exper_flags} --tr 100000 --wr 0
    sudo perf script -i result-backup/perf-${exper_name}.data | ${FLAMEGRAPH_DIR}/stackcollapse-perf.pl \
        > result-backup/perf-${exper_name}.fold
    ${FLAMEGRAPH_DIR}/flamegraph.pl result-backup/perf-${exper_name}.fold > result-backup/perf-${exper_name}.svg
}

get_flame_graph "read-different-unlimited-sync" -f 64 -s $((128*1024*1024)) -n 64 -r 4096 -a basic
get_flame_graph "read-different-unlimited-io_uring_default" -f 64 -s $((128*1024*1024)) -n 64 -r 4096 -a io_uring
