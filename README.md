# Foreactor: Transparent Async Syscalls for Any Application

TODO (kernel version, etc.)


## Build Instructions

Build and install `liburing`, a collection of helpers over io_uring:

```bash
git clone https://github.com/axboe/liburing.git
cd liburing
make
sudo make install
cd ..
```

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

- [ ] internal buffer GC
- [ ] control point inject logic
- [ ] io_uring fixed buffers
- [ ] io_uring SQ polling
- [ ] config & options on/off
- [ ] show io_wq concurrency


## References

- ALICE
- io_uring, ScyllaDB
- eBPF, AnyCall, BPF for Storage
