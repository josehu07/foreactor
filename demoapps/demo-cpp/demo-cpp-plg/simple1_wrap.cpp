#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 0;


// Some global state for arggen and rcsave functions.
static ExperSimple1Args *curr_args = nullptr;
static int curr_fd = -1;
static bool curr_pwrite_done = false;
static bool curr_pread0_done = false;
static bool curr_pread1_done = false;

static bool open_arggen(const int *epoch, bool *link, const char **pathname, int *flags, mode_t *mode) {
    *pathname = curr_args->filename.c_str();
    *flags = O_CREAT | O_RDWR;
    *mode = S_IRUSR | S_IWUSR;
    return true;
}

static void open_rcsave(const int *epoch, int fd) {
    curr_fd = fd;
}

static bool pwrite_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    if (curr_fd < 0)
        return false;
    *fd = curr_fd;
    *buf = curr_args->wcontent.c_str();
    *count = curr_args->wlen;
    *offset = 0;
    return true;
}

static void pwrite_rcsave(const int *epoch, ssize_t res) {
    curr_pwrite_done = true;
}

static bool pread0_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                          bool *buf_ready, bool *skip_memcpy) {
    if (!curr_pwrite_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->rbuf0;
    *count = curr_args->rlen;
    *offset = 0;
    *buf_ready = true;
    return true;
}

static void pread0_rcsave(const int *epoch, ssize_t res) {
    curr_pread0_done = true;
}

static bool pread1_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                          bool *buf_ready, bool *skip_memcpy) {
    if (!curr_pwrite_done)
        return false;
    *fd = curr_fd;
    *buf = curr_args->rbuf1;
    *count = curr_args->rlen;
    *offset = curr_args->rlen;
    *buf_ready = true;
    return true;
}

static void pread1_rcsave(const int *epoch, ssize_t res) {
    curr_pread1_done = true;
}

static bool close_arggen(const int *epoch, bool *link, int *fd) {
    if (!curr_pread0_done || !curr_pread1_done)
        return false;
    *fd = curr_fd;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 0);

    foreactor_AddSyscallOpen(graph_id, 0, "open", nullptr, 0, open_arggen, open_rcsave, /*is_start*/ true);
    foreactor_AddSyscallPwrite(graph_id, 1, "pwrite", nullptr, 0, pwrite_arggen, pwrite_rcsave, false);
    foreactor_AddSyscallPread(graph_id, 2, "pread0", nullptr, 0, pread0_arggen, pread0_rcsave, 4096, false);
    foreactor_AddSyscallPread(graph_id, 3, "pread1", nullptr, 0, pread1_arggen, pread1_rcsave, 4096, false);
    foreactor_AddSyscallClose(graph_id, 4, "close", nullptr, 0, close_arggen, nullptr, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 1, 2, false);
    foreactor_SyscallSetNext(graph_id, 2, 3, false);
    foreactor_SyscallSetNext(graph_id, 3, 4, false);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "simple");
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
void __real__Z13exper_simple1Pv(void *args);

extern "C"
void __wrap__Z13exper_simple1Pv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z13exper_simple1Pv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_args = reinterpret_cast<ExperSimple1Args *>(args);

        // Call the original function with corresponding SCGraph activated.
        __real__Z13exper_simple1Pv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
        curr_fd = -1;
        curr_pwrite_done = false;
        curr_pread0_done = false;
        curr_pread1_done = false;
    }
}
