#!/usr/bin/bash


unbuffer python3 microbench.py -d /mnt/ssd/josehu/microbench_dir -o result-backup/read-different-unlimited \
    2>&1 | tee result-backup/read-different-unlimited.log

unbuffer python3 microbench.py -d /mnt/ssd/josehu/microbench_dir -o result-backup/write-different-unlimited \
    --rdwr write 2>&1 | tee result-backup/write-different-unlimited.log

unbuffer python3 microbench.py -d /mnt/ssd/josehu/microbench_dir -o result-backup/read-single-unlimited \
    --file single 2>&1 | tee result-backup/read-single-unlimited.log

unbuffer python3 microbench.py -d /mnt/ssd/josehu/microbench_dir -o result-backup/read-different-direct \
    --mem direct 2>&1 | tee result-backup/read-different-direct.log

# unbuffer python3 microbench.py -d /mnt/ssd/josehu/microbench_dir -o result-backup/read-different-mem50 \
#     --mem 50 2>&1 | tee result-backup/read-different-mem50.log
