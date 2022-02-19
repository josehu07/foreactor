#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <liburing.h>

#include "scg_nodes.hpp"
#include "syscalls.hpp"


namespace foreactor {


static bool AllArgsReady(std::vector<bool>& arg_ready) {
    return std::all_of(arg_ready.begin(), arg_ready.end(),
                       [](bool a) { return a; });
}

template <typename T>
static void SetArg(SyscallStage& stage, std::vector<bool>& arg_ready,
                   int arg_index, T& arg_field, T arg_value) {
    assert(!arg_ready[arg_index] && stage == STAGE_NOTREADY);
    arg_field = arg_value;
    arg_ready[arg_index] = true;
    if (AllArgsReady(arg_ready))
        stage = STAGE_UNISSUED;
}


//////////
// open //
//////////

SyscallOpen::SyscallOpen(std::string filename, int flags, mode_t mode,
                         std::vector<bool> arg_ready)
        : SyscallNode(SC_OPEN, /*pure_sc*/ false, arg_ready),
          filename(filename), flags(flags), mode(mode) {
    assert(arg_ready.size() == 3);
}

std::ostream& operator<<(std::ostream& s, const SyscallOpen& n) {
    s << "SyscallOpen{"
      << "filename=" << n.filename << ","
      << "flags=" << n.flags << ","
      << "mode" << n.mode << ",";
    n.PrintCommonInfo(s);
    s << "}";
    return s;
}

long SyscallOpen::SyscallSync(void *output_buf) {
    (void) output_buf;      // unused
    return open(filename.c_str(), flags, mode);
}

void SyscallOpen::PrepUring(struct io_uring_sqe *sqe) {
    io_uring_prep_openat(sqe, AT_FDCWD, filename.c_str(), flags, mode);
}

void SyscallOpen::ReflectResult(void *output_buf) {
    (void) output_buf;      // unused
    return;
}

void SyscallOpen::SetArgFilename(std::string filename_) {
    SetArg(stage, arg_ready, 0, filename, filename_);
}

void SyscallOpen::SetArgFlags(int flags_) {
    SetArg(stage, arg_ready, 1, flags, flags_);
}

void SyscallOpen::SetArgMode(mode_t mode_) {
    SetArg(stage, arg_ready, 2, mode, mode_);
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
    return pread(fd, buf, count, offset);
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
    SetArg(stage, arg_ready, 2, count, count_);
}

void SyscallPread::SetArgOffset(off_t offset_) {
    SetArg(stage, arg_ready, 3, offset, offset_);
}


}
