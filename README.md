# Foreactor: Transparent Asynchronous Syscalls for Any Serial Application

Foreactor is a library + plugin framework that enables asynchronous I/O (or more generally, asynchronous syscalls) with Linux `io_uring` in any application. Foreactor is transparent -- it allows the integration of `io_uring` into any originally serial application with no modification to application code.

This is done by describing the application's critical functions (e.g., LevelDB's `Version::Get`) as syscall graphs -- a formal abstraction we propose. Such graph abstraction captures the original execution order of syscalls to be issued by the function and their mutual dependencies. If the `foreactor` library gets `LD_PRELOAD`ed when running the application, it automatically hijacks those wrapped functions as well as POSIX syscalls, and pre-issues a certain number of syscalls ahead of time if the syscall graph says it is safe to do so.


## Prerequisites

The following kernel version, compiler, and libraries are required:

* Linux kernel >= 5.10 (we tested with Ubuntu 20.04)
* gcc/g++ >= 10.2
* liburing >= 2.1

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


## Build Instructions

Build the `foreactor` library:

```bash
cd foreactor
make clean && make
cd ..
```

Finally, for each application of interest, go into the corresponding folder (which contains its foreactor-patched source code), then follow the README down there.


## Making a New Plugin

TODO

```bash
objdump -t path/to/original/app/file.o | grep funcname_keyword
```


## TODO List

- [x] pool flush optimization
- [x] cleverer pre-issuing algo
- [ ] longer chain for demoapp
- [ ] readme & website doc
- [ ] control point inject logic
- [ ] syscall graph visualize
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
