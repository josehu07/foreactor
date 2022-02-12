# Helios Traced App - LevelDB 1.23

## Bulid For Tracing

Build LevelDB static library:

```bash
cd leveldb-1.23
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)
```

Build YCSB client:

```bash
cd ../..
make tracing
```

## Build For Release

Build LevelDB static library:

```bash
cd leveldb-1.23
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

Build YCSB client:

```bash
cd ../..
make release
```

## Usage

Generate YCSB text traces of opcode-key pairs from a workload, e.g., `workloada`:

```bash
cd /path/to/ycsb/repo
./bin/ycsb load basic -P workloads/workloada > somefile-load.txt
./bin/ycsb run basic -P workloads/workloada > somefile-run.txt
cd /path/to/this/repo/ycsb-traces
python3 converter.py somefile-load.txt a-load.txt
python3 converter.py somefile-run.txt a-run.txt
cd ..
```

Run the client:

```bash
./ycsbcli -h
```
