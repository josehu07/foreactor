#!/usr/bin/bash


ROOT_PATH=$(realpath ..)


make clean && make


echo
echo "Correctness check --"

echo " demoapp dump original..."
./demoapp dump > dump-original.txt

echo " demoapp dump w/ foreactor..."
LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=8 ./demoapp dump > dump-foreactor.txt

echo "  should see no diff:"
diff dump-original.txt dump-foreactor.txt
rm dump-*.txt


echo
echo "Performance bench --"

echo " demoapp run original..."
./demoapp run

for DEPTH in 0 1 2 4 8; do
    echo " demoapp run w/ foreactor, pre_issue_depth = ${DEPTH}..."
    LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=${DEPTH} ./demoapp run
done
