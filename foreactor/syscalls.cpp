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


namespace foreactor {


template <typename T>
static void SetArg(SyscallStage& stage, std::vector<bool>& arg_ready,
                   int arg_index, T& arg_field, T arg_value) {
    assert(arg_index >= 0 && arg_index < arg_ready.size());
    assert(!arg_ready[arg_index] && stage == STAGE_NOTREADY);
    arg_field = arg_value;
    arg_ready[arg_index] = true;
    if (SyscallNode::AllArgsReady(arg_ready))
        stage = STAGE_UNISSUED;
}


//////////
// open //
//////////

SyscallOpen::SyscallOpen(std::string pathname, int flags, mode_t mode,
                         std::vector<bool> arg_ready)
        : SyscallNode(SC_OPEN, /*pure_sc*/ false, arg_ready),
          pathname(pathname), flags(flags), mode(mode) {
    assert(arg_ready.size() == 3);
}

std::ostream& operator<<(std::ostream& s, const SyscallOpen& n) {
    s << "SyscallOpen{"
      << "pathname=\"" << n.pathname << "\","
      << "flags=" << n.flags << ","
      << "mode" << n.mode << ",";
    n.PrintCommonInfo(s);
    s << "}";
    return s;
}

long SyscallOpen::SyscallSync(void *output_buf) {
    (void) output_buf;      // unused
    return posix::open(pathname.c_str(), flags, mode);
}

void SyscallOpen::PrepUring(struct io_uring_sqe *sqe) {
    io_uring_prep_openat(sqe, AT_FDCWD, pathname.c_str(), flags, mode);
}

void SyscallOpen::ReflectResult(void *output_buf) {
    (void) output_buf;      // unused
    return;
}

void SyscallOpen::SetArgPathname(std::string pathname_) {
    SetArg(stage, arg_ready, 0, pathname, pathname_);
}

void SyscallOpen::SetArgFlags(int flags_) {
    SetArg(stage, arg_ready, 1, flags, flags_);
}

void SyscallOpen::SetArgMode(mode_t mode_) {
    SetArg(stage, arg_ready, 2, mode, mode_);
}

void SyscallOpen::CheckArgs(const char *pathname_, int flags_, mode_t mode_) {
    assert(pathname_ != nullptr);
    if (stage == STAGE_NOTREADY) {
        assert(arg_ready.size() == 3);
        if (!arg_ready[0]) SetArgPathname(std::string(pathname_));
        if (!arg_ready[1]) SetArgFlags(flags_);
        if (!arg_ready[2]) SetArgMode(mode_);
    }
    assert(strcmp(pathname_, pathname.c_str()) == 0);
    assert(flags_ == flags);
    assert(mode_ == mode);
}


///////////
// pread //
///////////

SyscallPread::SyscallPread(int fd, size_t count, off_t offset,
                           std::vector<bool> arg_ready)
        : SyscallNode(SC_PREAD, /*pure_sc*/ true, arg_ready),
          fd(fd), count(count), offset(offset),
          internal_buf(nullptr) {
    assert(arg_ready.size() == 3);
}

SyscallPread::~SyscallPread() {
    // syscall node cannot be destructed when in the stage of
    // in io_uring progress
    assert(stage != STAGE_PROGRESS);
    if (internal_buf != nullptr)
        delete[] internal_buf;
}

std::ostream& operator<<(std::ostream& s, const SyscallPread& n) {
    s << "SyscallPread{"
      << "fd=" << n.fd << ","
      << "count=" << n.count << ","
      << "offset=" << n.offset << ",";
    n.PrintCommonInfo(s);
    s << "}";
    return s;
}

long SyscallPread::SyscallSync(void *output_buf) {
    assert(output_buf != nullptr);
    char *buf = reinterpret_cast<char *>(output_buf);
    return posix::pread(fd, buf, count, offset);
}

void SyscallPread::PrepUring(struct io_uring_sqe *sqe) {
    if (internal_buf == nullptr)
        internal_buf = new char[count];
    io_uring_prep_read(sqe, fd, internal_buf, count, offset);
}

void SyscallPread::ReflectResult(void *output_buf) {
    assert(output_buf != nullptr);
    char *buf = reinterpret_cast<char *>(output_buf);
    memcpy(buf, internal_buf, count);
}

void SyscallPread::SetArgFd(int fd_) {
    SetArg(stage, arg_ready, 0, fd, fd_);
}

void SyscallPread::SetArgCount(size_t count_) {
    SetArg(stage, arg_ready, 1, count, count_);
}

void SyscallPread::SetArgOffset(off_t offset_) {
    SetArg(stage, arg_ready, 2, offset, offset_);
}

void SyscallPread::CheckArgs(int fd_, size_t count_, off_t offset_) {
    if (stage == STAGE_NOTREADY) {
        assert(arg_ready.size() == 3);
        if (!arg_ready[0]) SetArgFd(fd_);
        if (!arg_ready[1]) SetArgCount(count_);
        if (!arg_ready[2]) SetArgOffset(offset_);
    }
    assert(fd_ == fd);
    assert(count_ == count);
    assert(offset_ == offset);
}


}
