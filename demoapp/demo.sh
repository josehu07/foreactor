#!/usr/bin/bash


ROOT_PATH=$(dirname $(dirname $(realpath $0)))
cd ${ROOT_PATH}/demoapp/


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
echo "Performance bench (page cache on) --"

echo " demoapp run original..."
./demoapp run

for DEPTH in 0 1 2 4 8; do
    echo " demoapp run w/ foreactor, pre_issue_depth = ${DEPTH}..."
    LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=${DEPTH} ./demoapp run
done


echo
echo "Performance bench (drop_caches) --"

echo " demoapp run original..."
./demoapp run --drop_caches

for DEPTH in 0 1 2 4 8; do
    echo " demoapp run w/ foreactor, pre_issue_depth = ${DEPTH}..."
    LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=${DEPTH} ./demoapp run --drop_caches
done
