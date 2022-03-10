# Foreactor: Transparent Async Syscalls for Any Application

TODO (kernel version, etc.)


## Prerequisites

<details>
<summary>Requires `gcc`/`g++` version >= 10.2 for full support of `c++20` standard.</summary>

```bash
sudo apt update
sudo apt upgrade
sudo apt install build-essential gcc-10 g++-10 cpp-10 cmake
```
</details>

Make `gcc-10`/`g++-10` the default version:

```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100
sudo update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-10 100
```

Build and install `liburing`, a collection of helpers over the io_uring interface:

```bash
git clone https://github.com/axboe/liburing.git
cd liburing
make
sudo make install
cd ..
```


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
