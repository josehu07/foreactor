#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <assert.h>
#include <liburing.h>

#include "debug.hpp"
#include "posix_itf.hpp"
#include "scg_graph.hpp"
#include "scg_nodes.hpp"
#include "syscalls.hpp"
#include "thread_pool.hpp"
#include "value_pool.hpp"


namespace foreactor {


//////////
// open //
//////////

SyscallOpen::SyscallOpen(unsigned node_id, std::string name,
                         SCGraph *scgraph,
                         const std::unordered_set<int>& assoc_dims,
                         ArggenFunc arggen_func, RcsaveFunc rcsave_func)
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

long SyscallOpen::SyscallSync(const EpochList& epoch) {
    return posix::open(pathname.Get(epoch),
                       flags.Get(epoch),
                       mode.Get(epoch));
}

void SyscallOpen::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    io_uring_prep_openat(sqe, AT_FDCWD,
                         pathname.Get(epoch_sum),
                         flags.Get(epoch_sum),
                         mode.Get(epoch_sum));
}

void SyscallOpen::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    sqe->sc_type = SC_OPEN;
    sqe->buf = reinterpret_cast<uint64_t>(pathname.Get(epoch_sum));
    sqe->open_flags = flags.Get(epoch_sum);
    sqe->open_mode = mode.Get(epoch_sum);
}

void SyscallOpen::ReflectResult([[maybe_unused]] const EpochList& epoch) {
    return;
}

bool SyscallOpen::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    bool link_ = false;
    const char *pathname_;
    int flags_;
    mode_t mode_;
    if (!arggen_func(epoch.RawArray(), &link_, &pathname_, &flags_, &mode_))
        return false;
    link.Set(epoch, link_);

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
    TIMER_START(scgraph->timer_check_args);
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
    TIMER_PAUSE(scgraph->timer_check_args);
}


////////////
// openat //
////////////

SyscallOpenat::SyscallOpenat(unsigned node_id, std::string name,
                             SCGraph *scgraph,
                             const std::unordered_set<int>& assoc_dims,
                             ArggenFunc arggen_func, RcsaveFunc rcsave_func)
        : SyscallNode(node_id, name, SC_OPENAT, /*pure_sc*/ false,
                      scgraph, assoc_dims),
          dirfd(assoc_dims), pathname(assoc_dims), flags(assoc_dims),
          mode(assoc_dims), arggen_func(arggen_func),
          rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
}

std::ostream& operator<<(std::ostream& s, const SyscallOpenat& n) {
    s << "SyscallOpenat{";
    n.PrintCommonInfo(s);
    s << ",dirfd=" << StreamStr(n.dirfd) << ","
      << "pathname=" << StreamStr(n.pathname) << ","
      << "flags=" << StreamStr(n.flags) << ","
      << "mode=" << StreamStr(n.mode) << "}";
    return s;
}

long SyscallOpenat::SyscallSync(const EpochList& epoch) {
    return posix::openat(dirfd.Get(epoch),
                         pathname.Get(epoch),
                         flags.Get(epoch),
                         mode.Get(epoch));
}

void SyscallOpenat::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    io_uring_prep_openat(sqe,
                         dirfd.Get(epoch_sum),
                         pathname.Get(epoch_sum),
                         flags.Get(epoch_sum),
                         mode.Get(epoch_sum));
}

void SyscallOpenat::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    sqe->sc_type = SC_OPENAT;
    sqe->fd = dirfd.Get(epoch_sum);
    sqe->buf = reinterpret_cast<uint64_t>(pathname.Get(epoch_sum));
    sqe->open_flags = flags.Get(epoch_sum);
    sqe->open_mode = mode.Get(epoch_sum);
}

void SyscallOpenat::ReflectResult([[maybe_unused]] const EpochList& epoch) {
    return;
}

bool SyscallOpenat::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    bool link_ = false;
    int dirfd_;
    const char *pathname_;
    int flags_;
    mode_t mode_;
    if (!arggen_func(epoch.RawArray(), &link_, &dirfd_, &pathname_, &flags_, &mode_))
        return false;
    link.Set(epoch, link_);

    assert(pathname_ != nullptr);
    if (!dirfd.Has(epoch))
        dirfd.Set(epoch, dirfd_);
    if (!pathname.Has(epoch))
        pathname.Set(epoch, pathname_);
    if (!flags.Has(epoch))
        flags.Set(epoch, flags_);
    if (!mode.Has(epoch))
        mode.Set(epoch, mode_);
    assert(dirfd_ == dirfd.Get(epoch));
    assert(strcmp(pathname_, pathname.Get(epoch)) == 0);
    assert(flags_ == flags.Get(epoch));
    assert(mode_ == mode.Get(epoch));

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallOpenat::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<int>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    dirfd.Remove(epoch);
    pathname.Remove(epoch);
    flags.Remove(epoch);
    mode.Remove(epoch);
}

void SyscallOpenat::ResetValuePools() {
    ResetCommonPools();
    dirfd.Reset();
    pathname.Reset();
    flags.Reset();
    mode.Reset();
}

void SyscallOpenat::CheckArgs(const EpochList& epoch,
                              int dirfd_,
                              const char *pathname_,
                              int flags_,
                              mode_t mode_) {
    TIMER_START(scgraph->timer_check_args);
    assert(pathname_ != nullptr);
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!dirfd.Has(epoch))
            dirfd.Set(epoch, dirfd_);
        if (!pathname.Has(epoch))
            pathname.Set(epoch, pathname_);
        if (!flags.Has(epoch))
            flags.Set(epoch, flags_);
        if (!mode.Has(epoch))
            mode.Set(epoch, mode_);
        stage.Set(epoch, STAGE_ARGREADY);
    }
    assert(dirfd_ == dirfd.Get(epoch));
    assert(strcmp(pathname_, pathname.Get(epoch)) == 0);
    assert(flags_ == flags.Get(epoch));
    assert(mode_ == mode.Get(epoch));
    TIMER_PAUSE(scgraph->timer_check_args);
}


///////////
// close //
///////////

SyscallClose::SyscallClose(unsigned node_id, std::string name,
                           SCGraph *scgraph,
                           const std::unordered_set<int>& assoc_dims,
                           ArggenFunc arggen_func, RcsaveFunc rcsave_func)
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

long SyscallClose::SyscallSync(const EpochList& epoch) {
    return posix::close(fd.Get(epoch));
}

void SyscallClose::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    io_uring_prep_close(sqe, fd.Get(epoch_sum));
}

void SyscallClose::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    sqe->sc_type = SC_CLOSE;
    sqe->fd = fd.Get(epoch_sum);
}

void SyscallClose::ReflectResult([[maybe_unused]] const EpochList& epoch) {
    return;
}

bool SyscallClose::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    bool link_ = false;
    int fd_;
    if (!arggen_func(epoch.RawArray(), &link_, &fd_))
        return false;
    link.Set(epoch, link_);

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
    TIMER_START(scgraph->timer_check_args);
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        stage.Set(epoch, STAGE_ARGREADY);
    }
    assert(fd_ == fd.Get(epoch));
    TIMER_PAUSE(scgraph->timer_check_args);
}


///////////
// pread //
///////////

SyscallPread::SyscallPread(unsigned node_id, std::string name,
                           SCGraph *scgraph,
                           const std::unordered_set<int>& assoc_dims,
                           ArggenFunc arggen_func, RcsaveFunc rcsave_func,
                           size_t pre_alloc_buf_size)
        : SyscallNode(node_id, name, SC_PREAD, /*pure_sc*/ true,
                      scgraph, assoc_dims),
          fd(assoc_dims), buf(assoc_dims), count(assoc_dims),
          offset(assoc_dims), skip_memcpy(assoc_dims),
          internal_buf(assoc_dims), arggen_func(arggen_func),
          rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
    // at most can have pre_issue_depth + 1 internal buffers simultaneously
    // in use
    for (int i = 0; i < scgraph->pre_issue_depth + 1; ++i) {
        // align allocation to hardware sector size, in case the file is
        // open O_DIRECT
        char *ib = new (std::align_val_t(512)) char[pre_alloc_buf_size];
        pre_alloced_bufs.insert(ib);
        ib_ref_counts[ib] = 0;
    }
}

SyscallPread::~SyscallPread() {
    for (auto internal_buf : pre_alloced_bufs) {
        if (internal_buf != nullptr)
            delete[] internal_buf;
    }
}

std::ostream& operator<<(std::ostream& s, const SyscallPread& n) {
    s << "SyscallPread{";
    n.PrintCommonInfo(s);
    s << ",fd=" << StreamStr(n.fd) << ","
      << "buf=" << StreamStr(n.buf) << ","
      << "count=" << StreamStr(n.count) << ","
      << "offset=" << StreamStr(n.offset) << "}";
    return s;
}

long SyscallPread::SyscallSync(const EpochList& epoch) {
    // as long as an internal_buf is assigned for this epoch, read into
    // it even for a SyscallSync, in case any subsequent writes used this
    // internal_buf through linking
    char *read_buf;
    bool use_internal_buf = internal_buf.Has(epoch) && link.Get(epoch);
    if (use_internal_buf)
        read_buf = internal_buf.Get(epoch);
    else
        read_buf = buf.Get(epoch);

    long res = posix::pread(fd.Get(epoch),
                            read_buf,
                            count.Get(epoch),
                            offset.Get(epoch));

    if (use_internal_buf) {
        assert(skip_memcpy.Has(epoch));
        if (!skip_memcpy.Get(epoch))
            memcpy(buf.Get(epoch), read_buf, res);
    }

    return res;
}

void SyscallPread::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    char *read_buf;
    if (!buf.Has(epoch_sum)) {
        assert(internal_buf.Has(epoch_sum));
        read_buf = internal_buf.Get(epoch_sum);
    } else
        read_buf = buf.Get(epoch_sum);
    
    io_uring_prep_read(sqe,
                       fd.Get(epoch_sum),
                       read_buf,
                       count.Get(epoch_sum),
                       offset.Get(epoch_sum));
}

void SyscallPread::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    char *read_buf;
    if (!buf.Has(epoch_sum)) {
        assert(internal_buf.Has(epoch_sum));
        read_buf = internal_buf.Get(epoch_sum);
    } else
        read_buf = buf.Get(epoch_sum);
    
    sqe->sc_type = SC_PREAD;
    sqe->fd = fd.Get(epoch_sum);
    sqe->buf = reinterpret_cast<uint64_t>(read_buf);
    sqe->rw_len = count.Get(epoch_sum);
    sqe->offset = offset.Get(epoch_sum);
}

void SyscallPread::ReflectResult(const EpochList& epoch) {
    assert(buf.Has(epoch));
    if (internal_buf.Has(epoch)) {
        assert(skip_memcpy.Has(epoch));
        if (!skip_memcpy.Get(epoch))
            memcpy(buf.Get(epoch), internal_buf.Get(epoch), rc.Get(epoch));
    }
}

bool SyscallPread::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    bool link_ = false;
    int fd_;
    char *buf_;
    size_t count_;
    off_t offset_;
    bool buf_ready = false;
    bool skip_memcpy_ = false;
    if (!arggen_func(epoch.RawArray(), &link_, &fd_, &buf_, &count_, &offset_,
                     &buf_ready, &skip_memcpy_)) {
        return false;
    }
    link.Set(epoch, link_);
    skip_memcpy.Set(epoch, skip_memcpy_);

    if (!fd.Has(epoch))
        fd.Set(epoch, fd_);
    if (buf_ready && !buf.Has(epoch))
        buf.Set(epoch, buf_);
    if (!count.Has(epoch))
        count.Set(epoch, count_);
    if (!offset.Has(epoch))
        offset.Set(epoch, offset_);
    assert(fd_ == fd.Get(epoch));
    if (buf_ready)
        assert(buf_ == buf.Get(epoch));
    assert(count_ == count.Get(epoch));
    assert(offset_ == offset.Get(epoch));

    // use internal buffer if true destination buffer not ready yet
    if (!buf_ready) {
        if ((!internal_buf.Has(epoch)) ||
            internal_buf.Get(epoch) == nullptr) {
            assert(pre_alloced_bufs.size() > 0);
            internal_buf.Set(epoch,
                pre_alloced_bufs.extract(pre_alloced_bufs.cbegin()).value());
        }
    }

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallPread::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<ssize_t>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    fd.Remove(epoch);
    buf.Remove(epoch);
    count.Remove(epoch);
    offset.Remove(epoch);
    if (skip_memcpy.Has(epoch))
        skip_memcpy.Remove(epoch);
    if (internal_buf.Has(epoch)) {
        char *ib = internal_buf.Get(epoch);
        if (ib_ref_counts[ib] == 0)
            internal_buf.Remove(epoch, /*move_into*/ &pre_alloced_bufs);
    }
}

void SyscallPread::ResetValuePools() {
    ResetCommonPools();
    fd.Reset();
    buf.Reset();
    count.Reset();
    offset.Reset();
    skip_memcpy.Reset();
    internal_buf.Reset(/*move_into*/ &pre_alloced_bufs);
}

void SyscallPread::CheckArgs(const EpochList& epoch,
                             int fd_,
                             void *buf_,
                             size_t count_,
                             off_t offset_) {
    TIMER_START(scgraph->timer_check_args);
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        if (!buf.Has(epoch))
            buf.Set(epoch, reinterpret_cast<char *>(buf_));
        if (!count.Has(epoch))
            count.Set(epoch, count_);
        if (!offset.Has(epoch))
            offset.Set(epoch, offset_);
        stage.Set(epoch, STAGE_ARGREADY);
    } else if (!buf.Has(epoch)) {
        // previously used internal buffer
        buf.Set(epoch, reinterpret_cast<char *>(buf_));
    }
    assert(fd_ == fd.Get(epoch));
    assert(buf_ == buf.Get(epoch));
    assert(count_ == count.Get(epoch));
    assert(offset_ == offset.Get(epoch));
    TIMER_PAUSE(scgraph->timer_check_args);
}

char *SyscallPread::RefInternalBuf(const EpochList& epoch) {
    if (!internal_buf.Has(epoch))
        return nullptr;
    char *ib = internal_buf.Get(epoch);
    assert(ib_ref_counts[ib] >= 0);
    ib_ref_counts[ib]++;
    return ib;
}

void SyscallPread::PutInternalBuf(const EpochList& epoch) {
    if (internal_buf.Has(epoch)) {
        char *ib = internal_buf.Get(epoch);
        assert(ib_ref_counts[ib] > 0);
        ib_ref_counts[ib]--;
        if (ib_ref_counts[ib] == 0)
            internal_buf.Remove(epoch, /*move_into*/ &pre_alloced_bufs);
    }
}


////////////
// pwrite //
////////////

SyscallPwrite::SyscallPwrite(unsigned node_id, std::string name,
                             SCGraph *scgraph,
                             const std::unordered_set<int>& assoc_dims,
                             ArggenFunc arggen_func, RcsaveFunc rcsave_func)
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

long SyscallPwrite::SyscallSync(const EpochList& epoch) {
    return posix::pwrite(fd.Get(epoch),
                         buf.Get(epoch),
                         count.Get(epoch),
                         offset.Get(epoch));
}

void SyscallPwrite::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    io_uring_prep_write(sqe,
                        fd.Get(epoch_sum),
                        buf.Get(epoch_sum),
                        count.Get(epoch_sum),
                        offset.Get(epoch_sum));
}

void SyscallPwrite::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    sqe->sc_type = SC_PWRITE;
    sqe->fd = fd.Get(epoch_sum);
    sqe->buf = reinterpret_cast<uint64_t>(buf.Get(epoch_sum));
    sqe->rw_len = count.Get(epoch_sum);
    sqe->offset = offset.Get(epoch_sum);
}

void SyscallPwrite::ReflectResult([[maybe_unused]] const EpochList& epoch) {
    return;
}

bool SyscallPwrite::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    bool link_ = false;
    int fd_;
    const char *buf_;
    size_t count_;
    off_t offset_;
    if (!arggen_func(epoch.RawArray(), &link_, &fd_, &buf_, &count_, &offset_))
        return false;
    link.Set(epoch, link_);

    if (!fd.Has(epoch))
        fd.Set(epoch, fd_);
    if (!buf.Has(epoch))
        buf.Set(epoch, buf_);
    if (!count.Has(epoch))
        count.Set(epoch, count_);
    if (!offset.Has(epoch))
        offset.Set(epoch, offset_);
    assert(fd_ == fd.Get(epoch));
    assert(buf_ == buf.Get(epoch));
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
    TIMER_START(scgraph->timer_check_args);
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
    // if using the internal buffer of previous pread, the address value will
    // be different from the user buffer fed in here and the user buffer will
    // contain meaningless bytes
    assert(count_ == count.Get(epoch));
    assert(offset_ == offset.Get(epoch));
    TIMER_PAUSE(scgraph->timer_check_args);
}


///////////
// lseek //
///////////

SyscallLseek::SyscallLseek(unsigned node_id, std::string name,
                           SCGraph *scgraph,
                           const std::unordered_set<int>& assoc_dims,
                           ArggenFunc arggen_func, RcsaveFunc rcsave_func)
        : SyscallNode(node_id, name, SC_LSEEK, /*pure_sc*/ false,
                      scgraph, assoc_dims),
          fd(assoc_dims), offset(assoc_dims), whence(assoc_dims),
          arggen_func(arggen_func), rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
}

std::ostream& operator<<(std::ostream& s, const SyscallLseek& n) {
    s << "SyscallLseek{";
    n.PrintCommonInfo(s);
    s << ",fd=" << StreamStr(n.fd) << ","
      << "offset=" << StreamStr(n.offset) << ","
      << "whence=" << StreamStr(n.whence) << "}";
    return s;
}

long SyscallLseek::SyscallSync(const EpochList& epoch) {
    return posix::lseek(fd.Get(epoch), offset.Get(epoch), whence.Get(epoch));
}

void SyscallLseek::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    // lseek is never offloaded async
    return;
}

void SyscallLseek::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    // lseek is never offloaded async
    return;
}

void SyscallLseek::ReflectResult([[maybe_unused]] const EpochList& epoch) {
    return;
}

bool SyscallLseek::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);
    return false;
}

void SyscallLseek::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<off_t>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    fd.Remove(epoch);
    offset.Remove(epoch);
    whence.Remove(epoch);
}

void SyscallLseek::ResetValuePools() {
    ResetCommonPools();
    fd.Reset();
    offset.Reset();
    whence.Reset();
}

void SyscallLseek::CheckArgs(const EpochList& epoch,
                             int fd_,
                             off_t offset_,
                             int whence_) {
    TIMER_START(scgraph->timer_check_args);
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        if (!offset.Has(epoch))
            offset.Set(epoch, offset_);
        if (!whence.Has(epoch))
            whence.Set(epoch, whence_);
        stage.Set(epoch, STAGE_ARGREADY);
    }
    assert(fd_ == fd.Get(epoch));
    assert(offset_ == offset.Get(epoch));
    assert(whence_ == whence.Get(epoch));
    TIMER_PAUSE(scgraph->timer_check_args);
}


///////////
// fstat //
///////////

SyscallFstat::SyscallFstat(unsigned node_id, std::string name,
                           SCGraph *scgraph,
                           const std::unordered_set<int>& assoc_dims,
                           ArggenFunc arggen_func, RcsaveFunc rcsave_func)
        : SyscallNode(node_id, name, SC_FSTAT, /*pure_sc*/ true,
                      scgraph, assoc_dims),
          fd(assoc_dims), buf(assoc_dims), statx_buf(assoc_dims),
          arggen_func(arggen_func), rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
    // since io_uring only supports statx, we need internal struct statx
    // buffers to hold their results, and reflect them to struct stat later
    for (int i = 0; i < scgraph->pre_issue_depth + 1; ++i)
        empty_statx_bufs.insert(new struct statx);
}

SyscallFstat::~SyscallFstat() {
    for (auto statx_buf : empty_statx_bufs) {
        if (statx_buf != nullptr)
            delete statx_buf;
    }
}

std::ostream& operator<<(std::ostream& s, const SyscallFstat& n) {
    s << "SyscallFstat{";
    n.PrintCommonInfo(s);
    s << ",fd=" << StreamStr(n.fd) << ","
      << "buf=" << StreamStr(n.buf) << "}";
    return s;
}

long SyscallFstat::SyscallSync(const EpochList& epoch) {
    return posix::fstat(fd.Get(epoch), buf.Get(epoch));
}

void SyscallFstat::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    io_uring_prep_statx(sqe,
                        fd.Get(epoch_sum),
                        "",
                        AT_EMPTY_PATH,
                        STATX_BASIC_STATS,
                        statx_buf.Get(epoch_sum));
}

void SyscallFstat::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    sqe->sc_type = SC_FSTAT;
    sqe->fd = fd.Get(epoch_sum);
    sqe->buf = reinterpret_cast<uint64_t>(statx_buf.Get(epoch_sum));
}

void SyscallFstat::ReflectResult(const EpochList& epoch) {
    assert(buf.Has(epoch));
    if (statx_buf.Has(epoch)) {
        struct stat *dst_buf = buf.Get(epoch);
        struct statx *src_buf = statx_buf.Get(epoch);
        dst_buf->st_dev = makedev(src_buf->stx_dev_major,
                                  src_buf->stx_dev_minor);
        dst_buf->st_ino = src_buf->stx_ino;
        dst_buf->st_mode = src_buf->stx_mode;
        dst_buf->st_nlink = src_buf->stx_nlink;
        dst_buf->st_uid = src_buf->stx_uid;
        dst_buf->st_gid = src_buf->stx_gid;
        dst_buf->st_rdev = makedev(src_buf->stx_rdev_major,
                                   src_buf->stx_rdev_minor);
        dst_buf->st_size = src_buf->stx_size;
        dst_buf->st_blksize = src_buf->stx_blksize;
        dst_buf->st_blocks = src_buf->stx_blocks;
        dst_buf->st_atime = src_buf->stx_atime.tv_sec;
        dst_buf->st_mtime = src_buf->stx_mtime.tv_sec;
        dst_buf->st_ctime = src_buf->stx_ctime.tv_sec;
    }
}

bool SyscallFstat::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    bool link_ = false;
    int fd_;
    struct stat *buf_;
    bool buf_ready = false;
    if (!arggen_func(epoch.RawArray(), &link_, &fd_, &buf_, &buf_ready))
        return false;
    link.Set(epoch, link_);

    if (!fd.Has(epoch))
        fd.Set(epoch, fd_);
    if (buf_ready && !buf.Has(epoch))
        buf.Set(epoch, buf_);
    assert(fd_ == fd.Get(epoch));
    if (buf_ready)
        assert(buf_ == buf.Get(epoch));

    // assign internal statx_buf
    if ((!statx_buf.Has(epoch)) || statx_buf.Get(epoch) == nullptr) {
        assert(empty_statx_bufs.size() > 0);
        statx_buf.Set(epoch,
            empty_statx_bufs.extract(empty_statx_bufs.cbegin()).value());
    }

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallFstat::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<int>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    fd.Remove(epoch);
    buf.Remove(epoch);
    if (statx_buf.Has(epoch))
        statx_buf.Remove(epoch, /*move_into*/ &empty_statx_bufs);
}

void SyscallFstat::ResetValuePools() {
    ResetCommonPools();
    fd.Reset();
    buf.Reset();
    statx_buf.Reset(/*move_into*/ &empty_statx_bufs);
}

void SyscallFstat::CheckArgs(const EpochList& epoch,
                             int fd_,
                             struct stat *buf_) {
    TIMER_START(scgraph->timer_check_args);
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        if (!buf.Has(epoch))
            buf.Set(epoch, buf_);
        stage.Set(epoch, STAGE_ARGREADY);
    } else if (!buf.Has(epoch)) {
        // previously used internal buffer
        buf.Set(epoch, buf_);
    }
    assert(fd_ == fd.Get(epoch));
    assert(buf_ == buf.Get(epoch));
    TIMER_PAUSE(scgraph->timer_check_args);
}

struct stat *SyscallFstat::GetStatBuf(const EpochList& epoch) {
    return buf.Get(epoch);
}


/////////////
// fstatat //
/////////////

SyscallFstatat::SyscallFstatat(unsigned node_id, std::string name,
                               SCGraph *scgraph,
                               const std::unordered_set<int>& assoc_dims,
                               ArggenFunc arggen_func, RcsaveFunc rcsave_func)
        : SyscallNode(node_id, name, SC_FSTATAT, /*pure_sc*/ true,
                      scgraph, assoc_dims),
          dirfd(assoc_dims), pathname(assoc_dims), buf(assoc_dims),
          flags(assoc_dims), statx_buf(assoc_dims),
          arggen_func(arggen_func), rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
    // since io_uring only supports statx, we need internal struct statx
    // buffers to hold their results, and reflect them to struct stat later
    for (int i = 0; i < scgraph->pre_issue_depth + 1; ++i)
        empty_statx_bufs.insert(new struct statx);
}

SyscallFstatat::~SyscallFstatat() {
    for (auto statx_buf : empty_statx_bufs) {
        if (statx_buf != nullptr)
            delete statx_buf;
    }
}

std::ostream& operator<<(std::ostream& s, const SyscallFstatat& n) {
    s << "SyscallFstatat{";
    n.PrintCommonInfo(s);
    s << ",dirfd=" << StreamStr(n.dirfd) << ","
      << "pathname=" << StreamStr(n.pathname) << ","
      << "buf=" << StreamStr(n.buf) << ","
      << "flags=" << StreamStr(n.flags) << "}";
    return s;
}

long SyscallFstatat::SyscallSync(const EpochList& epoch) {
    return posix::fstatat(dirfd.Get(epoch),
                             pathname.Get(epoch),
                             buf.Get(epoch),
                             flags.Get(epoch));
}

void SyscallFstatat::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    io_uring_prep_statx(sqe,
                        dirfd.Get(epoch_sum),
                        pathname.Get(epoch_sum),
                        flags.Get(epoch_sum),
                        STATX_BASIC_STATS,
                        statx_buf.Get(epoch_sum));
}

void SyscallFstatat::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    sqe->sc_type = SC_FSTATAT;
    sqe->fd = dirfd.Get(epoch_sum);
    sqe->stat_path = reinterpret_cast<uint64_t>(pathname.Get(epoch_sum));
    sqe->buf = reinterpret_cast<uint64_t>(statx_buf.Get(epoch_sum));
    sqe->stat_flags = flags.Get(epoch_sum);
}

void SyscallFstatat::ReflectResult(const EpochList& epoch) {
    assert(buf.Has(epoch));
    if (statx_buf.Has(epoch)) {
        struct stat *dst_buf = buf.Get(epoch);
        struct statx *src_buf = statx_buf.Get(epoch);
        dst_buf->st_dev = makedev(src_buf->stx_dev_major,
                                  src_buf->stx_dev_minor);
        dst_buf->st_ino = src_buf->stx_ino;
        dst_buf->st_mode = src_buf->stx_mode;
        dst_buf->st_nlink = src_buf->stx_nlink;
        dst_buf->st_uid = src_buf->stx_uid;
        dst_buf->st_gid = src_buf->stx_gid;
        dst_buf->st_rdev = makedev(src_buf->stx_rdev_major,
                                   src_buf->stx_rdev_minor);
        dst_buf->st_size = src_buf->stx_size;
        dst_buf->st_blksize = src_buf->stx_blksize;
        dst_buf->st_blocks = src_buf->stx_blocks;
        dst_buf->st_atime = src_buf->stx_atime.tv_sec;
        dst_buf->st_mtime = src_buf->stx_mtime.tv_sec;
        dst_buf->st_ctime = src_buf->stx_ctime.tv_sec;
    }
}

bool SyscallFstatat::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    bool link_ = false;
    int dirfd_;
    const char *pathname_;
    struct stat *buf_;
    int flags_;
    bool buf_ready = false;
    if (!arggen_func(epoch.RawArray(), &link_, &dirfd_, &pathname_, &buf_,
                     &flags_, &buf_ready)) {
        return false;
    }
    link.Set(epoch, link_);

    if (!dirfd.Has(epoch))
        dirfd.Set(epoch, dirfd_);
    if (!pathname.Has(epoch))
        pathname.Set(epoch, pathname_);
    if (buf_ready && !buf.Has(epoch))
        buf.Set(epoch, buf_);
    if (!flags.Has(epoch))
        flags.Set(epoch, flags_);
    assert(dirfd_ == dirfd.Get(epoch));
    assert(pathname_ == pathname.Get(epoch));
    if (buf_ready)
        assert(buf_ == buf.Get(epoch));
    assert(flags_ == flags.Get(epoch));

    // assign internal statx_buf
    if ((!statx_buf.Has(epoch)) || statx_buf.Get(epoch) == nullptr) {
        assert(empty_statx_bufs.size() > 0);
        statx_buf.Set(epoch,
            empty_statx_bufs.extract(empty_statx_bufs.cbegin()).value());
    }

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallFstatat::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<int>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    dirfd.Remove(epoch);
    pathname.Remove(epoch);
    buf.Remove(epoch);
    flags.Remove(epoch);
    if (statx_buf.Has(epoch))
        statx_buf.Remove(epoch, /*move_into*/ &empty_statx_bufs);
}

void SyscallFstatat::ResetValuePools() {
    ResetCommonPools();
    dirfd.Reset();
    pathname.Reset();
    buf.Reset();
    flags.Reset();
    statx_buf.Reset(/*move_into*/ &empty_statx_bufs);
}

void SyscallFstatat::CheckArgs(const EpochList& epoch,
                               int dirfd_,
                               const char *pathname_,
                               struct stat *buf_,
                               int flags_) {
    TIMER_START(scgraph->timer_check_args);
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!dirfd.Has(epoch))
            dirfd.Set(epoch, dirfd_);
        if (!pathname.Has(epoch))
            pathname.Set(epoch, pathname_);
        if (!buf.Has(epoch))
            buf.Set(epoch, buf_);
        if (!flags.Has(epoch))
            flags.Set(epoch, flags_);
        stage.Set(epoch, STAGE_ARGREADY);
    } else if (!buf.Has(epoch)) {
        // previously used internal buffer
        buf.Set(epoch, buf_);
    }
    assert(dirfd_ == dirfd.Get(epoch));
    assert(pathname_ == pathname.Get(epoch));
    assert(buf_ == buf.Get(epoch));
    assert(flags_ == flags.Get(epoch));
    TIMER_PAUSE(scgraph->timer_check_args);
}

struct stat *SyscallFstatat::GetStatBuf(const EpochList& epoch) {
    return buf.Get(epoch);
}


//////////////
// getdents //
//////////////

SyscallGetdents::SyscallGetdents(unsigned node_id, std::string name,
                                 SCGraph *scgraph,
                                 const std::unordered_set<int>& assoc_dims,
                                 ArggenFunc arggen_func,
                                 RcsaveFunc rcsave_func,
                                 size_t pre_alloc_buf_size)
        : SyscallNode(node_id, name, SC_GETDENTS, /*pure_sc*/ true,
                      scgraph, assoc_dims),
          fd(assoc_dims), dirp(assoc_dims), count(assoc_dims),
          internal_dirp(assoc_dims), arggen_func(arggen_func),
          rcsave_func(rcsave_func) {
    assert(arggen_func != nullptr);
    // at most can have pre_issue_depth + 1 internal buffers simultaneously
    // in use
    for (int i = 0; i < scgraph->pre_issue_depth + 1; ++i) {
        pre_alloced_dirps.insert(reinterpret_cast<struct dirent *>(
            new char[pre_alloc_buf_size]));
    }
}

SyscallGetdents::~SyscallGetdents() {
    for (auto internal_dirp : pre_alloced_dirps) {
        if (internal_dirp != nullptr)
            delete[] reinterpret_cast<char *>(internal_dirp);
    }
}

std::ostream& operator<<(std::ostream& s, const SyscallGetdents& n) {
    s << "SyscallGetdents{";
    n.PrintCommonInfo(s);
    s << ",fd=" << StreamStr(n.fd) << ","
      << "dirp=" << StreamStr(n.dirp) << ","
      << "count=" << StreamStr(n.count) << "}";
    return s;
}

long SyscallGetdents::SyscallSync(const EpochList& epoch) {
    return posix::getdents(fd.Get(epoch),
                           dirp.Get(epoch),
                           count.Get(epoch));
}

void SyscallGetdents::PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe) {
    PANIC_IF(true, "io_uring backend does not support getdents syscall yet");
}

void SyscallGetdents::PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe) {
    sqe->sc_type = SC_GETDENTS;
    sqe->fd = fd.Get(epoch_sum);
    sqe->buf = reinterpret_cast<uint64_t>(dirp.Get(epoch_sum));
    sqe->dirp_count = count.Get(epoch_sum);
}

void SyscallGetdents::ReflectResult(const EpochList& epoch) {
    assert(dirp.Has(epoch));
    if (internal_dirp.Has(epoch))
        memcpy(dirp.Get(epoch), internal_dirp.Get(epoch), rc.Get(epoch));
}

bool SyscallGetdents::GenerateArgs(const EpochList& epoch) {
    assert(!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY);

    bool link_ = false;
    int fd_;
    struct dirent *dirp_;
    size_t count_;
    bool buf_ready = false;
    if (!arggen_func(epoch.RawArray(), &link_, &fd_, &dirp_, &count_,
                     &buf_ready)) {
        return false;
    }
    link.Set(epoch, link_);

    if (!fd.Has(epoch))
        fd.Set(epoch, fd_);
    if (buf_ready && !dirp.Has(epoch))
        dirp.Set(epoch, dirp_);
    if (!count.Has(epoch))
        count.Set(epoch, count_);
    assert(fd_ == fd.Get(epoch));
    if (buf_ready)
        assert(dirp_ == dirp.Get(epoch));
    assert(count_ == count.Get(epoch));

    // use internal buffer if true destination buffer not ready yet
    if (!buf_ready) {
        if ((!internal_dirp.Has(epoch)) ||
            internal_dirp.Get(epoch) == nullptr) {
            assert(pre_alloced_dirps.size() > 0);
            internal_dirp.Set(epoch,
                pre_alloced_dirps.extract(pre_alloced_dirps.cbegin()).value());
        }
    }

    stage.Set(epoch, STAGE_ARGREADY);
    return true;
}

void SyscallGetdents::RemoveOneEpoch(const EpochList& epoch) {
    if (rcsave_func != nullptr)
        rcsave_func(epoch.RawArray(), static_cast<ssize_t>(rc.Get(epoch)));

    RemoveOneFromCommonPools(epoch);
    fd.Remove(epoch);
    dirp.Remove(epoch);
    count.Remove(epoch);
    if (internal_dirp.Has(epoch))
        internal_dirp.Remove(epoch, /*move_into*/ &pre_alloced_dirps);
}

void SyscallGetdents::ResetValuePools() {
    ResetCommonPools();
    fd.Reset();
    dirp.Reset();
    count.Reset();
    internal_dirp.Reset(/*move_into*/ &pre_alloced_dirps);
}

void SyscallGetdents::CheckArgs(const EpochList& epoch,
                                int fd_,
                                void *dirp_,
                                size_t count_) {
    TIMER_START(scgraph->timer_check_args);
    if (!stage.Has(epoch) || stage.Get(epoch) == STAGE_NOTREADY) {
        if (!fd.Has(epoch))
            fd.Set(epoch, fd_);
        if (!dirp.Has(epoch))
            dirp.Set(epoch, reinterpret_cast<struct dirent *>(dirp_));
        if (!count.Has(epoch))
            count.Set(epoch, count_);
        stage.Set(epoch, STAGE_ARGREADY);
    } else if (!dirp.Has(epoch)) {
        // previously used internal buffer
        dirp.Set(epoch, reinterpret_cast<struct dirent *>(dirp_));
    }
    assert(fd_ == fd.Get(epoch));
    assert(dirp_ == dirp.Get(epoch));
    assert(count_ == count.Get(epoch));
    TIMER_PAUSE(scgraph->timer_check_args);
}

struct dirent *SyscallGetdents::GetDirpBuf(const EpochList& epoch) {
    return dirp.Get(epoch);
}


}
