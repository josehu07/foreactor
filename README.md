# Foreactor

![languages](https://img.shields.io/github/languages/count/josehu07/foreactor)
![top-lang](https://img.shields.io/github/languages/top/josehu07/foreactor)
![license](https://img.shields.io/github/license/josehu07/foreactor)

Foreactor is a library that enables *fine-grained speculative I/O* (or more generally, *speculative syscalls*) in kernel-intensive applications. Foreactor allows the integration of an asynchronous syscall execution backend, e.g., the recent Linux kernel `io_uring` or a user-level thread pool, into an application with little modification to its source code.

This is done by describing the application's critical functions (e.g., cp's `sparse_copy` and LevelDB's `Version::Get`) as *foreaction graphs*, a formal abstraction we propose. Such graph abstraction captures the execution order of syscalls to be issued by the function and their mutual dependencies. If the library gets `LD_PRELOAD`ed when running the application, it automatically intercepts those wrapped functions as well as POSIX glibc calls, and pre-issues proper syscalls ahead of time if the graph says it is safe and beneficial to do so.

TODO paper cite info? =)


## Prerequisites

The following kernel version, compiler, and libraries are required:

- Linux kernel >= 5.10 (we tested on Ubuntu 20.04 with kernel v5.10.60)
- gcc/g++ >= 10.2
- liburing >= 2.1

**Instructions**:

<details>
<summary>Update to latest mainline Linux kernel for Ubuntu 20.04...</summary>

```bash
wget https://raw.githubusercontent.com/pimlie/ubuntu-mainline-kernel.sh/master/ubuntu-mainline-kernel.sh
sudo chmod +x ubuntu-mainline-kernel.sh
./ubuntu-mainline-kernel.sh -r v5.10     # search for 5.10 versions available
sudo ./ubuntu-mainline-kernel.sh -i v5.10.60
sudo reboot
sudo apt --fix-broken install
```
</details>

<details>
<summary>Install gcc/g++ version >= 10.2 for full support of c++20 standard...</summary>

```bash
sudo apt update
sudo apt upgrade
sudo apt install build-essential gcc-10 g++-10 cpp-10 cmake
```
</details>

<details>
<summary>Make gcc-10/g++-10 the default version...</summary>

```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100
sudo update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-10 100
```
</details>

<details>
<summary>Build and install liburing, the official helper library over io_uring...</summary>

```bash
git clone https://github.com/axboe/liburing.git
cd liburing
make -j$(nproc)
sudo make install
cd ..
```
</details>


## Basic Usage

To enable foreactor for an application function, we need the following components:

- The core foreactor library `libforeactor.so`
- A code file (termed a plugin) describing the chosen function's foreaction graph, to be put alongside application source code
- Minor modifications to the application's build system to include compilation of the plugin and some linker wrapper options
- Build, then run the application with proper environment variables to turn on foreactor pre-issuing

**Instructions**:

<details>
<summary>Build the core foreactor library...</summary>

```bash
cd libforeactor
make -j$(nproc)
cd ..
```
</details>

<details>
<summary>Build and try the demo application...</summary>

```bash
cd demoapps/demo-cpp
make -j$(nproc)
mkdir /tmp/demo_dbdir
```

Run the `simple2` function without foreactor:

```bash
./demo --exper simple2 --dbdir /tmp/demo_dbdir --dump_result
```

Run the `simple2` function (whose graph ID is `1`) with foreactor with io_uring backend sqe_async mode, with syscall pre-issuing depth of 2:

```bash
LD_PRELOAD=/path/to/libforeactor/libforeactor.so USE_FOREACTOR=yes \
DEPTH_1=2 QUEUE_1=32 SQE_ASYNC_FLAG_1=yes \
./demo --exper simple2 --dbdir /tmp/demo_dbdir --dump_result
```

See `demo-cpp-src/hijackees.cpp` and `demo-cpp-plg/` for all the intercepted functions and their corresponding plugins.
</details>

<details>
<summary>Explanation of environment variables...</summary>

| Env variable | Note | Explanation |
| :-: | :-: | :- |
| `LD_PRELOAD` | Required | Absolute path to `libforeactor.so` |
| `USE_FOREACTOR` | Required | String `yes` means using foreactor, otherwise not |
| `DEPTH_{GRAPH_ID}` | Required | Non-negative number specifying how many syscalls should foreactor try to peek ahead of time; each graph has its separate depth configuration |
| `QUEUE_{GRAPH_ID}` | io_uring | If set, indicates using `io_uring` backend; io_uring queue-pair capacity (must be greater than depth) |
| `SQE_ASYNC_FLAG_{GRAPH_ID}` | io_uring | String `yes` means forcing multiple kernel io_wq threads (i.e., setting the `IOSQE_ASYNC` flag for each syscall handed off to io_uring), otherwise not |
| `UTHREADS_{GRAPH_ID}` | thread_pool | If set, indicates using user thread pool backend; number of worker threads of the thread pool |

An application can have multiple intercepted functions, each corresponding to a separate plugin file with its unique graph ID.
</details>


## Patched Applications

This repository contains a collection of applications that involve functions suitable to be wrapped by foreactor and benefit from asynchrony. We have written plugins for some of them. The plugins code can be found under `realapps/appname/appname-plg/`.

<details>
<summary>GNU Coreutils v9.1</summary>

| Function | Note |
| :-: | :- |
| `du du_files` | Regular loop of `fstatat`s on files in a directory |
| `cp sparse_copy` | Regular loop of `read`-`write`s of 128KiB chunks |

Build:

```bash
cd realapps/coreutils
sudo apt install automake texinfo
make reconf
make
```

If built successfully, there will be `coreutils-src/src/cp` and `coreutils-src/src/du` binaries produced.

Prepare filesystem workload image (make sure that the parent workspace directory has > 40GB available space and the backing filesystem has sufficient amount of free inodes; may take ~5m):

```bash
python3 eval.py -m du-prepare -d /path/to/workspace/dir
python3 eval.py -m cp-prepare -d /path/to/workspace/dir
```

If finished successfully, produces the workload files at `/path/to/workspace/dir/du_*/` and `/path/to/workspace/dir/cp_*/`.

Benchmark all workloads (may take ~2h):

```bash
python3 eval.py -m du-bencher \
                -d /path/to/workspace/dir \
                -l /path/to/libforeactor.so \
                -r results
python3 eval.py -m cp-bencher \
                -d /path/to/workspace/dir \
                -l /path/to/libforeactor.so \
                -r results
```

If finished successfully, stores all benchmarking result logs under current path's `results/`.

Plot result figures:

```bash
pip3 install matplotlib
python3 eval.py -m du-plotter -r results
python3 eval.py -m cp-plotter -r results
```

If finished successfully, produces all plots under current path's `results/`.
</details>

<details>
<summary>LevelDB v1.23</summary>

| Function | Note |
| :-: | :- |
| `Version::Get` | Chained `pread`s with possible `open`s and early exits |

Build:

```bash
cd realapps/leveldb
make
```

If built successfully, there will be a `ycsbcli` binary produced at current path, which is the client driving LevelDB.

Clone YCSB source code (to any path):

```bash
sudo apt install sysstat default-jre maven
git clone https://github.com/brianfrankcooper/YCSB.git ycsb
# the building of YCSB will be taken care of in the preparation scripts
```

Prepare database image directories and workload traces (make sure that the parent workspace directory has > 20GB available space; may take ~1h30m; give `--skip_load` option to skip loading database images and just re-generating workload traces for easier debugging):

```bash
python3 eval.py -m prepare \
                -y /path/to/ycsb/dir \
                -d /path/to/workspace/dir \
                -t /path/to/workspace/dir/leveldb_tmpdir \
                -w workloads [--skip_load]
```

If finished successfully, produces the database images at `/path/to/workspace/dir/leveldb_*/` and the workload traces under current path's `workloads/`.

Benchmark all workloads (may take ~30h; give `--tiny_bench` option to run only the first few requests of each workload for easier debugging):

```bash
python3 eval.py -m bencher \
                -d /path/to/workspace/dir \
                -l /path/to/libforeactor.so \
                -w workloads \
                -r results [--tiny_bench]
```

If finished successfully, stores all benchmarking result logs under current path's `results/`.

Plot selected figures:

```bash
pip3 install matplotlib
python3 eval.py -m plotter -r results
```

If finished successfully, produces all plots under current path's `results/`.

The breakdown plot requires libforeactor to be compiled with timers on. To produce it, do the following after the above steps are done:

```bash
cd ../../libforeactor
make clean
make -j$(nproc) timer
cd ../realapps/leveldb
python3 eval.py -m breakdown \
                -d /path/to/workspace/dir \
                -l /path/to/libforeactor.so \
                -w workloads \
                -r results
```
</details>


## Making a New Plugin

This section summarizes the steps to make a plugin for an application function, given that the foreaction graph for that function is conceptually clear.

<details>
<summary>Make a plugin for an application function...</summary>

1. Depending on the language of the application, if function names are mangled during linking (e.g. C++), we need to identify the mangled function name:
    ```bash
    objdump -t path/to/original/app/file.o | grep funcname_keyword
    ```
    For example, the mangled name may look like `_Z13exper_simple2Pv` for a C++ function named `exper_simple`. C functions usually follow ther original names.
2. Write a plugin file (using the application's source language), mimicking e.g. `demoapps/demo-cpp/demo-cpp-plg/simple2_wrap.cpp`.
    - Include foreactor library interfaces by `#include <foreactor.h>`.
    - Write the function interception wrapper `__wrap_funcname` and use `__real_funcname` to call the original function.
    - Build the foreaction graph using `foreactor_*` APIs.
    - Keep track of all the necessary states and write the correct `_arggen` and `_rcsave` stubs for each syscall node in the graph (this is probably the most app-specific and error-prone step).
    - Put the plugin file alongside application source files.
3. Modify the application's build system to:
    - Add compilation of the plugin file;
    - Add flags to link with the foreactor library `-I/path/to/libforeactor/include -L/path/to/libforeactor -lforeactor -Wl,-rpath=/path/to/libforeactor`;
    - Use linker wrap trick in the final step of linking to intercept the chosen function `-Wl,--wrap=funcname`.
    - Note: this trick works only for function calls across source files and does not work for e.g. static functions. Workarounds to be added...
4. Build the application. If goes successfully, calls to the chosen function will be intercepted by our wrapper function after linking. Run the application with proper environment variables to enable foreactor pre-issuing.
</details>


## TODO List

- [ ] speculative I/O analysis
- [ ] serious related work study
- [ ] readme & website doc
- [ ] smarter pre_issue_depth
- [ ] internal buffer GC
- [ ] compiler CFG mapping
- [ ] support other static langs
