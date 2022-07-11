# Foreactor

![languages](https://img.shields.io/github/languages/count/josehu07/foreactor)
![top-lang](https://img.shields.io/github/languages/top/josehu07/foreactor)
![license](https://img.shields.io/github/license/josehu07/foreactor)

Foreactor is a library that enables asynchronous I/O (or more generally, asynchronous syscalls) in C/C++ applications transparently. Foreactor allows the integration of an asynchronous execution backend, e.g. the recent Linux kernel `io_uring` or a user-level thread pool, into an application with no modification to the application's original source code.

This is done by describing the application's critical functions (e.g., LevelDB's `Version::Get`) as **syscall graphs** (SCGraphs), a formal abstraction we propose. Such graph abstraction captures the original execution order of syscalls to be issued by the function and their mutual dependencies. If the library gets `LD_PRELOAD`ed when running the application, it automatically intercepts those wrapped functions as well as POSIX syscalls, and pre-issues proper syscalls ahead of time if the syscall graph says it is safe and beneficial to do so.

TODO paper cite info? =)


## Prerequisites

The following kernel version, compiler, and libraries are required:

- Linux kernel >= 5.10 (we tested with Ubuntu 20.04 on kernel v5.10.60)
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
<summary>Build and install liburing, a set of official helpers over io_uring...</summary>

```bash
git clone https://github.com/axboe/liburing.git
cd liburing
make
sudo make install
cd ..
```
</details>


## Basic Usage

To enable foreactor for an application function, we need the following components:

- The core foreactor library `libforeactor.so`
- A code file (called a plugin) describing the chosen function's syscall graph, to be put alongside application source code
- Minor modifications to the application's build system to include compilation of the plugin and some linker wrapper options
- Build, then run the application with proper environment variables to turn on foreactor pre-issuing

**Instructions**:

<details>
<summary>Build the core foreactor library...</summary>

```bash
cd libforeactor
make
cd ..
```
</details>

<details>
<summary>Build and try the demo application...</summary>

```bash
cd demoapps/demo-cpp
make
mkdir /tmp/demo_dbdir

# Run the `simple` function without foreactor:
./demo --exper simple2 --dbdir /tmp/demo_dbdir --dump_result

# Run it with foreactor with io_uring backend sqe_async mode,
# with syscall pre-issuing depth of 2
LD_PRELOAD=/path/to/libforeactor/libforeactor.so USE_FOREACTOR=yes \
DEPTH_0=2 QUEUE_0=32 SQE_ASYNC_FLAG_0=yes \
./demo --exper simple2 --dbdir /tmp/demo_dbdir --dump_result
```

See `demo-cpp-src/hijackees.cpp` and `demo-cpp-plg/` for all the example functions and their corresponding plugins.
</details>

<details>
<summary>Explanation of environment variables...</summary>

- `LD_PRELOAD`: absolute path to the `libforeactor.so` dynamic library
- `USE_FOREACTOR`: string `yes` means using foreactor, otherwise not
- `DEPTH_{SCGRAPH_ID}`: non-negative number specifying how many syscalls should foreactor try to pre-issue ahead of time; each SCGraph has its separate depth configuration
- Backend configuration variables:
    - To use io_uring, set `QUEUE_{SCGRAPH_ID}` to the io_uring queue-pair capacity (must be greater than depth) and `SQE_ASYNC_FLAG_{SCGRAPH_ID}` to `yes` if forcing multiple kernel io_wq threads (i.e., setting `IOSQE_ASYNC` flag for each syscall handed off to io_uring)
    - To use user-level thread pool, set `UTHREADS_{SCGRAPH_ID}` to the number of worker threads of the thread pool for this SCGraph

An application can have multiple wrapped functions, each corresponding to a separate SCGraph plugin file with its unique SCGraph ID. The ID is set by the plugin file when `foreactor_CreateSCGraph()` is called.
</details>


## Listed Applications

This repository contains a collection of applications that involve functions suitable to be wrapped by foreactor and benefit from asynchrony. We have written plugins for some of them. The plugins code can be found under `realapps/appname/appname-plg/`.

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

Run:

```bash
python3 run-all.py -h   # TODO better instructions
```
</details>

<details>
<summary>GNU Coreutils v9.1</summary>

| Function | Note |
| :-: | :- |
| `cp sparse_copy` | Standard loop of `read`-`write`s of 128KiB chunks |

Build:

```bash
cd realapps/coreutils
sudo apt install automake
make reconf
make
```

Run:

```bash
# TODO
```
</details>

<details>
<summary>GNU Tar v1.34</summary>

| Function | Note |
| :-: | :- |
| `TODO` | TODO |

Build:

```bash
cd realapps/tar
sudo apt install automake
make reconf
make
```

Run:

```bash
# TODO
```
</details>

<details>
<summary>Git v2.36.0-rc0</summary>

| Function | Note |
| :-: | :- |
| `TODO` | TODO |

Build:

```bash
cd realapps/git
sudo apt install libcurl4-openssl-dev
make
```

Run:

```bash
# TODO
```
</details>


## Making a New Plugin

It does not take too much effort to make a new plugin for an application function, as long as the syscall graph for that function is conceptually clear.

<details>
<summary>Make a new plugin for new appliation function...</summary>

```bash
objdump -t path/to/original/app/file.o | grep funcname_keyword
```

TODO describe linker wrapping procedure

TODO complete tutorial
</details>


## TODO List

- [ ] smarter pre_issue_depth
- [ ] internal buffer GC
- [ ] serious related work study
- [ ] current async I/O study
- [ ] readme & website doc
- [ ] compiler CFG mapping
- [ ] support other static langs


## References

- ALICE
- io_uring, ScyllaDB
- eBPF, AnyCall, BPF for Storage
- MultiCall, FlexSC
- MAGE
- ...
