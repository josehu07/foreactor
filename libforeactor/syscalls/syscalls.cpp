#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <liburing.h>

#include "debug.hpp"
#include "posix_itf.hpp"
#include "scg_nodes.hpp"
#include "syscalls.hpp"
#include "value_pool.hpp"


namespace foreactor {


//////////
// open //
//////////

SyscallOpen::SyscallOpen(unsigned node_id, std::string name,
                         SCGraph *scgraph,
                         const std::unordered_set<int>& assoc_dims,
                         std::function<bool(const int *,
                                            const char **,
                                            int *,
                                            mode_t *)> arggen_func,
                         std::function<void(const int *, int)> rcsave_func)
        : SyscallNode(node_id, name, SC_OPEN, /*pure_sc*/ false,
                      scgraph, assoc_dims),
          pathname(assoc_dims), flags(assoc_dims), mode(assoc_dims),
          arggen_func(arggen_func), rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
}


std::ostream& operator<<(std::ostream& s, const SyscallOpen& n) {
    s << "SyscallOpen{";
    n.PrintCommonInfo(s);
    s << ",pathname=" << StreamStr(n.pathname) << ","
      << "flags=" << StreamStr(n.flags) << ","
      << "mode=" << StreamStr(n.mode) << "}";
    return s;
}


long SyscallOpen::SyscallSync(const EpochList& epoch,
                              [[maybe_unused]] void *output_buf) {
    return posix::open(pathname.Get(epoch),
                       flags.Get(epoch),
                       mode.Get(epoch));
}

void SyscallOpen::PrepUringSqe(int epoch_sum,
                               struct io_uring_sqe *sqe) {
    io_uring_prep_openat(sqe, AT_FDCWD,
                         pathname.Get(epoch_sum),
                         flags.Get(epoch_sum),
                         mode.Get(epoch_sum));
}

void SyscallOpen::ReflectResult([[maybe_unused]] const EpochList& epoch,
                                [[maybe_unused]] void *output_buf) {
    return;
}


bool SyscallOpen::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    const char *pathname_;
    int flags_;
    mode_t mode_;
    if (!arggen_func(epoch.RawArray(), &pathname_, &flags_, &mode_))
        return false;

    assert(pathname_ != nullptr);
    if (!pathname.Has(epoch))
        pathname.Set(epoch, pathname_);
    if (!flags.Has(epoch))
        flags.Set(epoch, flags_);
    if (!mode.Has(epoch))
        mode.Set(epoch, mode_);
    assert(strcmp(pathname_, pathname.Get(epoch)) == 0);
    assert(flags_ == flags.Get(epoch));
    assert(mode_ == mode.Get(epoch));

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallOpen::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<int>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    pathname.Remove(epoch);
    flags.Remove(epoch);
    mode.Remove(epoch);
}

void SyscallOpen::ResetValuePools() {
    ResetCommonPools();
    pathname.Reset();
    flags.Reset();
    mode.Reset();
}


void SyscallOpen::CheckArgs(const EpochList& epoch,
                            const char *pathname_, int flags_, mode_t mode_) {
    assert(pathname_ != nullptr);
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!pathname.Has(epoch))
            pathname.Set(epoch, pathname_);
        if (!flags.Has(epoch))
            flags.Set(epoch, flags_);
        if (!mode.Has(epoch))
            mode.Set(epoch, mode_);
        stage.Set(epoch, STAGE_ARGREADY);
    }
    assert(strcmp(pathname_, pathname.Get(epoch)) == 0);
    assert(flags_ == flags.Get(epoch));
    assert(mode_ == mode.Get(epoch));
}


///////////
// close //
///////////

SyscallClose::SyscallClose(unsigned node_id, std::string name,
                           SCGraph *scgraph,
                           const std::unordered_set<int>& assoc_dims,
                           std::function<bool(const int *, int *)> arggen_func,
                           std::function<void(const int *, int)> rcsave_func)
        : SyscallNode(node_id, name, SC_CLOSE, /*pure_sc*/ false,
                      scgraph, assoc_dims),
          fd(assoc_dims), arggen_func(arggen_func), rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
}


std::ostream& operator<<(std::ostream& s, const SyscallClose& n) {
    s << "SyscallClose{";
    n.PrintCommonInfo(s);
    s << ",fd=" << StreamStr(n.fd) << "}";
    return s;
}


long SyscallClose::SyscallSync(const EpochList& epoch,
                               [[maybe_unused]] void *output_buf) {
    return posix::close(fd.Get(epoch));
}

void SyscallClose::PrepUringSqe(int epoch_sum,
                                struct io_uring_sqe *sqe) {
    io_uring_prep_close(sqe, fd.Get(epoch_sum));
}

void SyscallClose::ReflectResult([[maybe_unused]] const EpochList& epoch,
                                 [[maybe_unused]] void *output_buf) {
    return;
}


bool SyscallClose::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    int fd_;
    if (!arggen_func(epoch.RawArray(), &fd_))
        return false;

    if (!fd.Has(epoch))
        fd.Set(epoch, fd_);
    assert(fd_ == fd.Get(epoch));

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallClose::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<int>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    fd.Remove(epoch);
}

void SyscallClose::ResetValuePools() {
    ResetCommonPools();
    fd.Reset();
}


void SyscallClose::CheckArgs(const EpochList& epoch, int fd_) {
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        stage.Set(epoch, STAGE_ARGREADY);
    }
    assert(fd_ == fd.Get(epoch));
}


///////////
// pread //
///////////

SyscallPread::SyscallPread(unsigned node_id, std::string name,
                           SCGraph *scgraph,
                           const std::unordered_set<int>& assoc_dims,
                           std::function<bool(const int *,
                                              int *,
                                              size_t *,
                                              off_t *)> arggen_func,
                           std::function<void(const int *, ssize_t)> rcsave_func)
        : SyscallNode(node_id, name, SC_PREAD, /*pure_sc*/ true,
                      scgraph, assoc_dims),
          fd(assoc_dims), count(assoc_dims), offset(assoc_dims),
          internal_buf(assoc_dims), arggen_func(arggen_func),
          rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
}


std::ostream& operator<<(std::ostream& s, const SyscallPread& n) {
    s << "SyscallPread{";
    n.PrintCommonInfo(s);
    s << ",fd=" << StreamStr(n.fd) << ","
      << "count=" << StreamStr(n.count) << ","
      << "offset=" << StreamStr(n.offset) << "}";
    return s;
}


long SyscallPread::SyscallSync(const EpochList& epoch, void *output_buf) {
    assert(output_buf != nullptr);
    return posix::pread(fd.Get(epoch),
                        output_buf,
                        count.Get(epoch),
                        offset.Get(epoch));
}

void SyscallPread::PrepUringSqe(int epoch_sum,
                                struct io_uring_sqe *sqe) {
    if ((!internal_buf.Has(epoch_sum)) ||
        internal_buf.Get(epoch_sum) == nullptr) {
        internal_buf.Set(epoch_sum, new char[count.Get(epoch_sum)]);
    }
    io_uring_prep_read(sqe,
                       fd.Get(epoch_sum),
                       internal_buf.Get(epoch_sum),
                       count.Get(epoch_sum),
                       offset.Get(epoch_sum));
}

void SyscallPread::ReflectResult(const EpochList& epoch, void *output_buf) {
    assert(output_buf != nullptr);
    memcpy(output_buf, internal_buf.Get(epoch), count.Get(epoch));
}


bool SyscallPread::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    int fd_;
    size_t count_;
    off_t offset_;
    if (!arggen_func(epoch.RawArray(), &fd_, &count_, &offset_))
        return false;

    if (!fd.Has(epoch))
        fd.Set(epoch, fd_);
    if (!count.Has(epoch))
        count.Set(epoch, count_);
    if (!offset.Has(epoch))
        offset.Set(epoch, offset_);
    assert(fd_ == fd.Get(epoch));
    assert(count_ == count.Get(epoch));
    assert(offset_ == offset.Get(epoch));

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallPread::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<ssize_t>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    fd.Remove(epoch);
    count.Remove(epoch);
    offset.Remove(epoch);
    if (internal_buf.Has(epoch))
        internal_buf.Remove(epoch, /*do_delete*/ true);
}

void SyscallPread::ResetValuePools() {
    ResetCommonPools();
    fd.Reset();
    count.Reset();
    offset.Reset();
    internal_buf.Reset(/*do_delete*/ true);
}


void SyscallPread::CheckArgs(const EpochList& epoch,
                             int fd_, size_t count_, off_t offset_) {
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        if (!count.Has(epoch))
            count.Set(epoch, count_);
        if (!offset.Has(epoch))
            offset.Set(epoch, offset_);
        stage.Set(epoch, STAGE_ARGREADY);
    }
    assert(fd_ == fd.Get(epoch));
    assert(count_ == count.Get(epoch));
    assert(offset_ == offset.Get(epoch));
}


////////////
// pwrite //
////////////

SyscallPwrite::SyscallPwrite(unsigned node_id, std::string name,
                             SCGraph *scgraph,
                             const std::unordered_set<int>& assoc_dims,
                             std::function<bool(const int *,
                                                int *,
                                                const char **,
                                                size_t *,
                                                off_t *)> arggen_func,
                             std::function<void(const int *, ssize_t)> rcsave_func)
        : SyscallNode(node_id, name, SC_PWRITE, /*pure_sc*/ false,
                      scgraph, assoc_dims),
          fd(assoc_dims), buf(assoc_dims), count(assoc_dims),
          offset(assoc_dims), arggen_func(arggen_func),
          rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
}


std::ostream& operator<<(std::ostream& s, const SyscallPwrite& n) {
    s << "SyscallPwrite{";
    n.PrintCommonInfo(s);
    s << ",fd=" << StreamStr(n.fd) << ","
      << "buf=" << StreamStr(n.buf) << ","
      << "count=" << StreamStr(n.count) << ","
      << "offset=" << StreamStr(n.offset) << "}";
    return s;
}


long SyscallPwrite::SyscallSync(const EpochList& epoch,
                                [[maybe_unused]] void *output_buf) {
    return posix::pwrite(fd.Get(epoch),
                         buf.Get(epoch),
                         count.Get(epoch),
                         offset.Get(epoch));
}

void SyscallPwrite::PrepUringSqe(int epoch_sum,
                                 struct io_uring_sqe *sqe) {
    io_uring_prep_write(sqe,
                        fd.Get(epoch_sum),
                        buf.Get(epoch_sum),
                        count.Get(epoch_sum),
                        offset.Get(epoch_sum));
}

void SyscallPwrite::ReflectResult([[maybe_unused]] const EpochList& epoch,
                                  [[maybe_unused]] void *output_buf) {
    return;
}


bool SyscallPwrite::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    int fd_;
    const char *buf_;
    size_t count_;
    off_t offset_;
    if (!arggen_func(epoch.RawArray(), &fd_, &buf_, &count_, &offset_))
        return false;

    if (!fd.Has(epoch))
        fd.Set(epoch, fd_);
    if (!buf.Has(epoch))
        buf.Set(epoch, buf_);
    if (!count.Has(epoch))
        count.Set(epoch, count_);
    if (!offset.Has(epoch))
        offset.Set(epoch, offset_);
    assert(fd_ == fd.Get(epoch));
    assert(memcmp(buf_, buf.Get(epoch), count_) == 0);
    assert(count_ == count.Get(epoch));
    assert(offset_ == offset.Get(epoch));

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallPwrite::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<ssize_t>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    fd.Remove(epoch);
    buf.Remove(epoch);
    count.Remove(epoch);
    offset.Remove(epoch);
}

void SyscallPwrite::ResetValuePools() {
    ResetCommonPools();
    fd.Reset();
    buf.Reset();
    count.Reset();
    offset.Reset();
}


void SyscallPwrite::CheckArgs(const EpochList& epoch,
                              int fd_,
                              const void *buf_,
                              size_t count_,
                              off_t offset_) {
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        if (!buf.Has(epoch))
            buf.Set(epoch, reinterpret_cast<const char *>(buf_));
        if (!count.Has(epoch))
            count.Set(epoch, count_);
        if (!offset.Has(epoch))
            offset.Set(epoch, offset_);
        stage.Set(epoch, STAGE_ARGREADY);
    }
    assert(fd_ == fd.Get(epoch));
    assert(memcmp(buf_, buf.Get(epoch), count_) == 0);
    assert(count_ == count.Get(epoch));
    assert(offset_ == offset.Get(epoch));
}


}
