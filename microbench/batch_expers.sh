#!/usr/bin/bash


MICROBENCH_DIR=/mnt/ssd/josehu/microbench_dir


# sudo python3 microbench.py -d ${MICROBENCH_DIR} --make


function run_exper {
    local exper_name=$1
    local exper_flags=${@:2}

    sudo unbuffer python3 microbench.py -d ${MICROBENCH_DIR} -o result-backup/${exper_name} \
        ${exper_flags} 2>&1 | sudo tee result-backup/${exper_name}.log
}

run_exper "read-different-unlimited"
run_exper "write-different-unlimited" --rdwr write
run_exper "read-single-unlimited" --file single
run_exper "read-different-direct" --mem direct
# run_exper "read-different-mem50" --mem 50
