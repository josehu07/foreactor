#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 2;


// Some global state for arggen and rcsave functions.
static ExperBranchingArgs *curr_args = nullptr;
static int curr_fd = -1;
static ssize_t curr_ret = 0;
static bool curr_branch0_done = false;
static bool curr_pwrite2_done = false;
static bool curr_branch1_done = false;
static bool curr_pread1_done = false;
static bool curr_pread2_done = false;
static bool curr_pread3_done = false;
static bool curr_pread4_done = false;
static bool curr_branch3_done = false;

static bool branch0_arggen(const int *epoch, int *decision) {
    *decision = (curr_args->fd < 0) ? 0 : 1;
    return true;
}

static bool open_arggen(const int *epoch, bool *link, const char **pathname, int *flags, mode_t *mode) {
    *pathname = curr_args->filename.c_str();
    *flags = O_CREAT | O_RDWR;
    *mode = S_IRUSR | S_IWUSR;
    return true;
}

static void open_rcsave(const int *epoch, int fd) {
    curr_fd = fd;
}

static bool pwrite0_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    if (curr_fd < 0)
        return false;
    *fd = curr_fd;
    *buf = curr_args->wcontent0.c_str();
    *count = curr_args->wlen;
    *offset = 0;
    return true;
}

static void pwrite0_rcsave(const int *epoch, ssize_t res) {
    curr_branch0_done = true;
}

static bool pwrite1_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    *fd = curr_args->fd;
    *buf = curr_args->wcontent1.c_str();
    *count = curr_args->wlen;
    *offset = 0;
    return true;
}

static void pwrite1_rcsave(const int *epoch, ssize_t res) {
    curr_branch0_done = true;
}

static bool pwrite2_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    if (!curr_branch0_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->wcontent2.c_str();
    *count = curr_args->wlen;
    *offset = 0;
    return true;
}

static void pwrite2_rcsave(const int *epoch, ssize_t res) {
    curr_pwrite2_done = true;
}

static bool branch1_arggen(const int *epoch, int *decision) {
    *decision = (curr_args->read && !curr_args->readtwice) ? 0 :
                (curr_args->read && curr_args->readtwice)  ? 1 : 2;
    if (*decision == 2)
        curr_branch1_done = true;
    return true;
}

static bool pread0_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                          bool *buf_ready, bool *skip_memcpy) {
    if (!curr_pwrite2_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->rbuf0;
    *count = curr_args->rlen;
    *offset = 0;
    *buf_ready = true;
    return true;
}

static void pread0_rcsave(const int *epoch, ssize_t res) {
    curr_branch1_done = true;
}

static bool branch2_arggen(const int *epoch, int *decision) {
    *decision = (!curr_args->moveoff) ? 0 : 1;
    return true;
}

static bool pread1_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                          bool *buf_ready, bool *skip_memcpy) {
    if (!curr_pwrite2_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->rbuf0;
    *count = curr_args->rlen;
    *offset = 0;
    *buf_ready = true;
    return true;
}

static void pread1_rcsave(const int *epoch, ssize_t res) {
    curr_pread1_done = true;
    if (curr_pread2_done)
        curr_branch1_done = true;
}

static bool pread2_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                          bool *buf_ready, bool *skip_memcpy) {
    if (!curr_pwrite2_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->rbuf1;
    *count = curr_args->rlen;
    *offset = 0;
    *buf_ready = true;
    return true;
}

static void pread2_rcsave(const int *epoch, ssize_t res) {
    curr_pread2_done = true;
    if (curr_pread1_done)
        curr_branch1_done = true;
}

static bool pread3_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                          bool *buf_ready, bool *skip_memcpy) {
    if (!curr_pwrite2_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->rbuf0;
    *count = curr_args->rlen;
    *offset = 0;
    *buf_ready = true;
    return true;
}

static void pread3_rcsave(const int *epoch, ssize_t res) {
    curr_pread3_done = true;
    if (curr_pread4_done)
        curr_branch1_done = true;
}

static bool pread4_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                          bool *buf_ready, bool *skip_memcpy) {
    if (!curr_pwrite2_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->rbuf1;
    *count = curr_args->rlen;
    *offset = curr_args->rlen;
    *buf_ready = true;
    return true;
}

static void pread4_rcsave(const int *epoch, ssize_t res) {
    curr_pread4_done = true;
    if (curr_pread3_done)
        curr_branch1_done = true;
    curr_ret = res;
}

static bool branch3_arggen(const int *epoch, int *decision) {
    if (curr_args->read && curr_args->readtwice && !curr_pread4_done)
        return false;
    *decision = (curr_ret != static_cast<ssize_t>(curr_args->rlen)) ? 0 : 1;
    if (*decision == 1)
        curr_branch3_done = true;
    return true;
}

static bool pwrite3_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    if (!curr_branch1_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->wcontent1.c_str();
    *count = curr_args->wlen;
    *offset = 0;
    return true;
}

static void pwrite3_rcsave(const int *epoch, ssize_t res) {
    curr_branch3_done = true;
}

static bool close_arggen(const int *epoch, bool *link, int *fd) {
    if (!curr_branch3_done)
        return false;
    *fd = curr_fd;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 0);

    foreactor_AddBranchNode(graph_id, 0, "branch0", nullptr, 0, branch0_arggen, 2, /*is_start*/ true);
    foreactor_AddSyscallOpen(graph_id, 1, "open", nullptr, 0, open_arggen, open_rcsave, false);
    foreactor_AddSyscallPwrite(graph_id, 2, "pwrite0", nullptr, 0, pwrite0_arggen, pwrite0_rcsave, false);
    foreactor_AddSyscallPwrite(graph_id, 3, "pwrite1", nullptr, 0, pwrite1_arggen, pwrite1_rcsave, false);
    foreactor_AddSyscallPwrite(graph_id, 4, "pwrite2", nullptr, 0, pwrite2_arggen, pwrite2_rcsave, false);
    foreactor_AddBranchNode(graph_id, 5, "branch1", nullptr, 0, branch1_arggen, 3, false);
    foreactor_AddSyscallPread(graph_id, 6, "pread0", nullptr, 0, pread0_arggen, pread0_rcsave, 4096, false);
    foreactor_AddBranchNode(graph_id, 7, "branch2", nullptr, 0, branch2_arggen, 2, false);
    foreactor_AddSyscallPread(graph_id, 8, "pread1", nullptr, 0, pread1_arggen, pread1_rcsave, 4096, false);
    foreactor_AddSyscallPread(graph_id, 9, "pread2", nullptr, 0, pread2_arggen, pread2_rcsave, 4096, false);
    foreactor_AddSyscallPread(graph_id, 10, "pread3", nullptr, 0, pread3_arggen, pread3_rcsave, 4096, false);
    foreactor_AddSyscallPread(graph_id, 11, "pread4", nullptr, 0, pread4_arggen, pread4_rcsave, 4096, false);
    foreactor_AddBranchNode(graph_id, 12, "branch3", nullptr, 0, branch3_arggen, 2, false);
    foreactor_AddSyscallPwrite(graph_id, 13, "pwrite3", nullptr, 0, pwrite3_arggen, pwrite3_rcsave, false);
    foreactor_AddSyscallClose(graph_id, 14, "close", nullptr, 0, close_arggen, nullptr, false);

    foreactor_BranchAppendChild(graph_id, 0, 1, /*epoch_dim*/ -1);
    foreactor_BranchAppendChild(graph_id, 0, 3, -1);
    foreactor_SyscallSetNext(graph_id, 1, 2, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 2, 4, false);
    foreactor_SyscallSetNext(graph_id, 3, 4, false);
    foreactor_SyscallSetNext(graph_id, 4, 5, false);
    foreactor_BranchAppendChild(graph_id, 5, 6, -1);
    foreactor_BranchAppendChild(graph_id, 5, 7, -1);
    foreactor_BranchAppendChild(graph_id, 5, 12, -1);
    foreactor_SyscallSetNext(graph_id, 6, 12, false);
    foreactor_BranchAppendChild(graph_id, 7, 8, -1);
    foreactor_BranchAppendChild(graph_id, 7, 10, -1);
    foreactor_SyscallSetNext(graph_id, 8, 9, false);
    foreactor_SyscallSetNext(graph_id, 9, 12, false);
    foreactor_SyscallSetNext(graph_id, 10, 11, false);
    foreactor_SyscallSetNext(graph_id, 11, 12, false);
    foreactor_BranchAppendChild(graph_id, 12, 13, -1);
    foreactor_BranchAppendChild(graph_id, 12, 14, -1);
    foreactor_SyscallSetNext(graph_id, 13, 14, false);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "branching");
}


//////////////////
// Wrapper stub //
//////////////////

// Mock the function to be made asynchronous with this wrapper function.
// Builds the SCGraph, registers it, so that if foreactor library is
// LD_PRELOADed, asynchroncy happens.
// 
// __real_funcname() will resolve to the original application function,
// while the application's (undefined-at-compile-time) references into
// funcname() will resolve to __wrap_funcname().
// 
// Must use mangled function name for C++. Use `objdump -t filename.o`
// to check the mangled symbol name.
extern "C"
void __real__Z15exper_branchingPv(void *args);

extern "C"
void __wrap__Z15exper_branchingPv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z15exper_branchingPv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_args = reinterpret_cast<ExperBranchingArgs *>(args);
        curr_fd = curr_args->fd;

        // Call the original function with corresponding SCGraph activated.
        __real__Z15exper_branchingPv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
        curr_fd = -1;
        curr_ret = 0;
        curr_branch0_done = false;
        curr_pwrite2_done = false;
        curr_branch1_done = false;
        curr_pread1_done = false;
        curr_pread2_done = false;
        curr_pread3_done = false;
        curr_pread4_done = false;
        curr_branch3_done = false;
    }
}
