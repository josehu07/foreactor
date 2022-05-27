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
                             const char **, int *, mode_t *)> arggen_func)
        : SyscallNode(node_id, name, SC_OPEN, /*pure_sc*/ false,
                      scgraph, assoc_dims),
          pathname(assoc_dims), flags(assoc_dims), mode(assoc_dims),
          arggen_func(arggen_func) {
}


std::ostream& operator<<(std::ostream& s, const SyscallOpen& n) {
    s << "SyscallOpen{"
      << "pathname=" << StreamStr(n.pathname) << ","
      << "flags=" << StreamStr(n.flags) << ","
      << "mode" << StreamStr(n.mode) << ",";
    n.PrintCommonInfo(s);
    s << "}";
    return s;
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
        pathname.Set(epoch, std::string(pathname_));
    if (!flags.Has(epoch))
        flags.Set(epoch, flags_);
    if (!mode.Has(epoch))
        mode.Set(epoch, mode_);
    assert(strcmp(pathname_, pathname.Get(epoch).c_str()) == 0);
    assert(flags_ == flags.Get(epoch));
    assert(mode_ == mode.Get(epoch));

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}


long SyscallOpen::SyscallSync(const EpochList& epoch, void *output_buf) {
    return posix::open(pathname.Get(epoch).c_str(),
                       flags.Get(epoch),
                       mode.Get(epoch));
}

void SyscallOpen::PrepUringSqe(const EpochList& epoch,
                               struct io_uring_sqe *sqe) {
    io_uring_prep_openat(sqe, AT_FDCWD,
                         pathname.Get(epoch).c_str(),
                         flags.Get(epoch),
                         mode.Get(epoch));
}

void SyscallOpen::ReflectResult(const EpochList& epoch, void *output_buf) {
    // nothing to do
    return;
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
    if (stage.Get(epoch) == STAGE_NOTREADY) {
        if (!pathname.Has(epoch))
            pathname.Set(epoch, std::string(pathname_));
        if (!flags.Has(epoch))
            flags.Set(epoch, flags_);
        if (!mode.Has(epoch))
            mode.Set(epoch, mode_);
    }
    assert(strcmp(pathname_, pathname.Get(epoch).c_str()) == 0);
    assert(flags_ == flags.Get(epoch));
    assert(mode_ == mode.Get(epoch));
}


///////////
// pread //
///////////

SyscallPread::SyscallPread(unsigned node_id, std::string name,
                           SCGraph *scgraph,
                           const std::unordered_set<int>& assoc_dims,
                           std::function<bool(const int *,
                               int *, size_t *, off_t *)> arggen_func)
        : SyscallNode(node_id, name, SC_PREAD, /*pure_sc*/ true,
                      scgraph, assoc_dims),
          fd(assoc_dims), count(assoc_dims), offset(assoc_dims),
          internal_buf(assoc_dims), arggen_func(arggen_func) {
}


std::ostream& operator<<(std::ostream& s, const SyscallPread& n) {
    s << "SyscallPread{"
      << "fd=" << StreamStr(n.fd) << ","
      << "count=" << StreamStr(n.count) << ","
      << "offset=" << StreamStr(n.offset) << ",";
    n.PrintCommonInfo(s);
    s << "}";
    return s;
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


long SyscallPread::SyscallSync(const EpochList& epoch, void *output_buf) {
    assert(output_buf != nullptr);
    return posix::pread(fd.Get(epoch),
                        reinterpret_cast<char *>(output_buf),
                        count.Get(epoch),
                        offset.Get(epoch));
}

void SyscallPread::PrepUringSqe(const EpochList& epoch,
                                struct io_uring_sqe *sqe) {
    if ((!internal_buf.Has(epoch)) || internal_buf.Get(epoch) == nullptr)
        internal_buf.Set(epoch, new char[count.Get(epoch)]);
    io_uring_prep_read(sqe,
                       fd.Get(epoch),
                       internal_buf.Get(epoch),
                       count.Get(epoch),
                       offset.Get(epoch));
}

void SyscallPread::ReflectResult(const EpochList& epoch, void *output_buf) {
    assert(output_buf != nullptr);
    memcpy(reinterpret_cast<char *>(output_buf), internal_buf.Get(epoch),
           count.Get(epoch));
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
    if (stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        if (!count.Has(epoch))
            count.Set(epoch, count_);
        if (!offset.Has(epoch))
            offset.Set(epoch, offset_);
    }
    assert(fd_ == fd.Get(epoch));
    assert(count_ == count.Get(epoch));
    assert(offset_ == offset.Get(epoch));
}


}
