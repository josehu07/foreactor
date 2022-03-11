# Foreactor

![languages](https://img.shields.io/github/languages/count/josehu07/foreactor)
![top-lang](https://img.shields.io/github/languages/top/josehu07/foreactor)
![license](https://img.shields.io/github/license/josehu07/foreactor)

Foreactor is a library + plugins framework that enables asynchronous I/O (or more generally, asynchronous syscalls) with Linux `io_uring` in any application. Foreactor is transparent -- it allows the integration of `io_uring` into any originally serial application with no modification to application code.

This is done by describing the application's critical functions (e.g., LevelDB's `Version::Get`) as **syscall graphs**, a formal abstraction we propose, in plugins. Such graph abstraction captures the original execution order of syscalls to be issued by the function and their mutual dependencies. If the `foreactor` library gets `LD_PRELOAD`ed when running the application, it automatically hijacks those wrapped functions specified in plugins as well as certain POSIX syscalls, and pre-issues some syscalls ahead of time if the syscall graph says it is safe to do so.


## Prerequisites

The following kernel version, compiler, and libraries are required:

- Linux kernel >= 5.10 (we tested with Ubuntu 20.04)
- gcc/g++ >= 10.2
- liburing >= 2.1

Detailed instructions:

<details>
<summary>Update to latest mainline Linux kernel for Ubuntu 20.04...</summary>

```bash
wget https://raw.githubusercontent.com/pimlie/ubuntu-mainline-kernel.sh/master/ubuntu-mainline-kernel.sh
sudo chmod +x ubuntu-mainline-kernel.sh
./ubuntu-mainline-kernel.sh -c
sudo ./ubuntu-mainline-kernel.sh -i
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

- The core foreactor library under `foreactor/`
- An application plugin describing the function's syscall graph, to be put alongside application code
- Some minor modifications to the applications build system

Detailed instructions:

<details>
<summary>Build the core foreactor library...</summary>

```bash
cd foreactor
make clean && make
cd ..
```
</details>

<details>
<summary>Build and try the demo application...</summary>

```bash
cd demoapp
./demo.sh
cd ..
```
</details>


## Listed Applications

This repository contains a collection of applications that involve functions suitable to be wrapped by foreactor and benefit from asynchrony. We have written plugins for some of them.

<details>
<summary>LevelDB v1.23</summary>

Build:

```bash
cd leveldb
make clean && make
```

Run with foreactor:

```bash
./ycsbcli TODO
```
</details>


## Making a New Plugin

It does not take too much effort to make a new plugin for an application function, as long as the syscall graph for that function is conceptually clear.

<details>
<summary>Make a new plugin for new appliation function...</summary>

TODO

```bash
objdump -t path/to/original/app/file.o | grep funcname_keyword
```
</details>


## TODO List

- [x] pool flush optimization
- [x] cleverer pre-issuing algo
- [ ] longer chain for demoapp
- [ ] control point inject logic
- [ ] syscall graph visualize
- [ ] readme & website doc
- [ ] unstable arguments
- [ ] internal buffer GC
- [ ] io_uring fixed buffers
- [ ] io_uring SQ polling
- [ ] config & options on/off
- [ ] show io_wq concurrency
- [ ] serious related work study
- [ ] compiler CFG mapping


## References

- ALICE
- io_uring, ScyllaDB
- eBPF, AnyCall, BPF for Storage
- TBC...
