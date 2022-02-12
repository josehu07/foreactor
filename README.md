# Foreactor: Guided Async I/O for Any Application

TODO


## Build Instructions

First, build and install `liburing`, a collection of helpers over io_uring:

```bash
git clone https://github.com/axboe/liburing.git
cd liburing
make
#sudo make install  # FIXME
cd ..
```

Next, build the `foreactor` library:

```bash
cd foreactor
make
cd ..
```

Finally, for each application of interest, go into the corresponding folder (which contains its foreactor-patched source code), then follow the README down there. Example:

```bash
cd demo
make
./makedb
./ldbget
```


## TODO List

- [ ] internal buffer cleanup
- [ ] pre/post-syscall inject logic
- [ ] io_uring fixed buffers
- [ ] io_uring SQ polling
- [ ] config & options on/off
- [ ] show io_wq concurrency


## References

- ALICE
- io_uring, ScyllaDB
- eBPF, AnyCall, BPF for Storage
