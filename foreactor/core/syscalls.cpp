#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>

#include "debug.hpp"
#include "scg_nodes.hpp"
#include "posix_itf.hpp"
#include "syscalls.hpp"
#include "value_pool.hpp"


namespace foreactor {


//////////
// open //
//////////

SyscallOpen::SyscallOpen(ValuePoolBase<SyscallStage> *stage,
                         ValuePoolBase<long> *rc,
                         ValuePoolBase<std::string> *pathname,
                         ValuePoolBase<int> *flags,
                         ValuePoolBase<mode_t> *mode)
        : SyscallNode(SC_OPEN, /*pure_sc*/ false, stage, rc),
          pathname(pathname), flags(flags), mode(mode) {
    assert(pathname != nullptr);
    assert(flags != nullptr);
    assert(mode != nullptr);
}


std::ostream& operator<<(std::ostream& s, const SyscallOpen& n) {
    s << "SyscallOpen{"
      << "pathname=" << StreamStr<ValuePoolBase<std::string>>(n.pathname) << ","
      << "flags=" << StreamStr<ValuePoolBase<int>>(n.flags) << ","
      << "mode" << StreamStr<ValuePoolBase<mode_t>>(n.mode) << ",";
    n.PrintCommonInfo(s);
    s << "}";
    return s;
}


bool SyscallOpen::RefreshStage(EpochListBase *epoch) {
    if (pathname->GetReady(epoch) &&
        flags->GetReady(epoch) &&
        mode->GetReady(epoch)) {
        stage->SetValue(epoch, STAGE_UNISSUED);
        return true;
    }
    return false;
}

long SyscallOpen::SyscallSync(EpochListBase *epoch, void *output_buf) {
    (void) output_buf;      // unused
    return posix::open(pathname->GetValue(epoch).c_str(),
                       flags->GetValue(epoch),
                       mode->GetValue(epoch));
}

void SyscallOpen::PrepUring(EpochListBase *epoch, struct io_uring_sqe *sqe) {
    io_uring_prep_openat(sqe, AT_FDCWD,
                         pathname->GetValue(epoch).c_str(),
                         flags->GetValue(epoch),
                         mode->GetValue(epoch));
}

void SyscallOpen::ReflectResult(EpochListBase *epoch, void *output_buf) {
    // nothing to do
    return;
}


void SyscallOpen::CheckArgs(EpochListBase *epoch,
                            const char *pathname_, int flags_, mode_t mode_) {
    assert(pathname_ != nullptr);
    if (stage->GetValue(epoch) == STAGE_NOTREADY) {
        if (!pathname->GetReady(epoch))
            pathname->SetValue(epoch, std::string(pathname_));
        if (!flags->GetReady(epoch))
            flags->SetValue(epoch, flags_);
        if (!mode->GetReady(epoch))
            mode->SetValue(epoch, mode_);
    }
    assert(strcmp(pathname_, pathname->GetValue(epoch).c_str()) == 0);
    assert(flags_ == flags->GetValue(epoch));
    assert(mode_ == mode->GetValue(epoch));
}


///////////
// pread //
///////////

SyscallPread::SyscallPread(ValuePoolBase<SyscallStage> *stage,
                           ValuePoolBase<long> *rc,
                           ValuePoolBase<int> *fd,
                           ValuePoolBase<size_t> *count,
                           ValuePoolBase<off_t> *offset,
                           ValuePoolBase<char *> *internal_buf)
        : SyscallNode(SC_PREAD, /*pure_sc*/ true, stage, rc),
          fd(fd), count(count), offset(offset), internal_buf(internal_buf) {
    assert(fd != nullptr);
    assert(count != nullptr);
    assert(offset != nullptr);
    assert(internal_buf != nullptr);
}


std::ostream& operator<<(std::ostream& s, const SyscallPread& n) {
    s << "SyscallPread{"
      << "fd=" << StreamStr<ValuePoolBase<int>>(n.fd) << ","
      << "count=" << StreamStr<ValuePoolBase<size_t>>(n.count) << ","
      << "offset=" << StreamStr<ValuePoolBase<off_t>>(n.offset) << ",";
    n.PrintCommonInfo(s);
    s << "}";
    return s;
}


bool SyscallPread::RefreshStage(EpochListBase *epoch) {
    if (fd->GetReady(epoch) &&
        count->GetReady(epoch) &&
        offset->GetReady(epoch)) {
        stage->SetValue(epoch, STAGE_UNISSUED);
        return true;
    }
    return false;
}

long SyscallPread::SyscallSync(EpochListBase *epoch, void *output_buf) {
    assert(output_buf != nullptr);
    char *buf = reinterpret_cast<char *>(output_buf);
    return posix::pread(fd->GetValue(epoch),
                        buf,
                        count->GetValue(epoch),
                        offset->GetValue(epoch));
}

void SyscallPread::PrepUring(EpochListBase *epoch, struct io_uring_sqe *sqe) {
    if ((!internal_buf->GetReady(epoch)) ||
        internal_buf->GetValue(epoch) == nullptr)
        internal_buf->SetValue(epoch, new char[count->GetValue(epoch)]);
    io_uring_prep_read(sqe,
                       fd->GetValue(epoch),
                       internal_buf->GetValue(epoch),
                       count->GetValue(epoch),
                       offset->GetValue(epoch));
}

void SyscallPread::ReflectResult(EpochListBase *epoch, void *output_buf) {
    assert(output_buf != nullptr);
    char *buf = reinterpret_cast<char *>(output_buf);
    memcpy(buf, internal_buf->GetValue(epoch), count->GetValue(epoch));
}


void SyscallPread::CheckArgs(EpochListBase *epoch,
                             int fd_, size_t count_, off_t offset_) {
    if (stage->GetValue(epoch) == STAGE_NOTREADY) {
        if (!fd->GetReady(epoch))
            fd->SetValue(epoch, fd_);
        if (!count->GetReady(epoch))
            count->SetValue(epoch, count_);
        if (!offset->GetReady(epoch))
            offset->SetValue(epoch, offset_);
    }
    assert(fd_ == fd->GetValue(epoch));
    assert(count_ == count->GetValue(epoch));
    assert(offset_ == offset->GetValue(epoch));
}


}
