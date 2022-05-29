#!/usr/bin/bash


if [[ $# -ne 1 ]]; then
    echo "Usage: $0 DBDIR_PATH"
    exit 1
fi
DBDIR_PATH=$(realpath $1)

SCRIPT_PATH=$(dirname $(realpath $0))
cd ${SCRIPT_PATH}

LIBFOREACTOR=$(SCRIPT_PATH)/../../libforeactor


echo
echo "Making dummy DB image under $1 --"
./demoapp make ${DBDIR_PATH}
echo "Done."


echo
echo "Correctness check --"

echo " demoapp dump original..."
./demoapp dump ${DBDIR_PATH} > dump-original.txt

echo " demoapp dump w/ foreactor..."
LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=8 ./demoapp dump ${DBDIR_PATH} > dump-foreactor.txt

echo "  should see no diff:"
diff dump-original.txt dump-foreactor.txt
rm dump-*.txt


echo
echo "Performance bench (all files open; page cache on) --"

echo " demoapp run original..."
./demoapp run ${DBDIR_PATH}

for DEPTH in 0 1 2 4 8 16; do
    echo " demoapp run w/ foreactor, pre_issue_depth = ${DEPTH}..."
    LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=${DEPTH} ./demoapp run ${DBDIR_PATH}
done


echo
echo "Performance bench (all files open; do drop_caches) --"

echo " demoapp run original..."
./demoapp run ${DBDIR_PATH} --drop_caches

for DEPTH in 0 1 2 4 8 16; do
    echo " demoapp run w/ foreactor, pre_issue_depth = ${DEPTH}..."
    LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=${DEPTH} ./demoapp run ${DBDIR_PATH} --drop_caches
done


echo
echo "Performance bench (selective open; page cache on) --"

echo " demoapp run original..."
./demoapp run ${DBDIR_PATH} --selective_open

for DEPTH in 0 1 2 4 8 16; do
    echo " demoapp run w/ foreactor, pre_issue_depth = ${DEPTH}..."
    LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=${DEPTH} ./demoapp run ${DBDIR_PATH} --selective_open
done


echo
echo "Performance bench (selective open; do drop_caches) --"

echo " demoapp run original..."
./demoapp run ${DBDIR_PATH} --selective_open --drop_caches

for DEPTH in 0 1 2 4 8 16; do
    echo " demoapp run w/ foreactor, pre_issue_depth = ${DEPTH}..."
    LD_PRELOAD=${ROOT_PATH}/foreactor/libforeactor.so USE_FOREACTOR=yes QUEUE_0=32 DEPTH_0=${DEPTH} ./demoapp run ${DBDIR_PATH} --selective_open --drop_caches
done
