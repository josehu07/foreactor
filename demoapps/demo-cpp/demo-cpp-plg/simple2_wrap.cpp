#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 1;


// Some global state for arggen and rcsave functions.
static ExperSimple2Args *curr_args = nullptr;
static int curr_filefd = -1;
static bool curr_fstatat_done = false;

static bool openat_arggen(const int *epoch, int *dirfd, const char **pathname, int *flags, mode_t *mode) {
    *dirfd = curr_args->dirfd;
    *pathname = curr_args->filename.c_str();
    *flags = O_CREAT | O_RDWR;
    *mode = S_IRUSR | S_IWUSR;
    return true;
}

static void openat_rcsave(const int *epoch, int fd) {
    curr_filefd = fd;
}

static bool fstat_arggen(const int *epoch, int *fd, struct stat **buf) {
    *fd = curr_args->dirfd;
    *buf = &curr_args->sbuf0;
    return true;
}

static bool fstatat_arggen(const int *epoch, int *dirfd, const char **pathname, struct stat **buf, int *flags) {
    *dirfd = curr_args->dirfd;
    *pathname = curr_args->filename.c_str();
    *buf = &curr_args->sbuf1;
    *flags = AT_SYMLINK_NOFOLLOW;
    return true;
}

static void fstatat_rcsave(const int *epoch, int res) {
    curr_fstatat_done = true;
}

static bool close_arggen(const int *epoch, int *fd) {
    if (!curr_fstatat_done)
        return false;
    *fd = curr_filefd;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 0);

    foreactor_AddSyscallOpenat(graph_id, 0, "openat", nullptr, 0, openat_arggen, openat_rcsave, /*is_start*/ true);
    foreactor_AddSyscallFstat(graph_id, 1, "fstat", nullptr, 0, fstat_arggen, nullptr, false);
    foreactor_AddSyscallFstatat(graph_id, 2, "fstatat", nullptr, 0, fstatat_arggen, fstatat_rcsave, false);
    foreactor_AddSyscallClose(graph_id, 3, "close", nullptr, 0, close_arggen, nullptr, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 1, 2, false);
    foreactor_SyscallSetNext(graph_id, 2, 3, false);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "simple2");
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
void __real__Z13exper_simple2Pv(void *args);

extern "C"
void __wrap__Z13exper_simple2Pv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z13exper_simple2Pv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_args = reinterpret_cast<ExperSimple2Args *>(args);

        // Call the original function with corresponding SCGraph activated.
        __real__Z13exper_simple2Pv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
        curr_filefd = -1;
        curr_fstatat_done = false;
    }
}
