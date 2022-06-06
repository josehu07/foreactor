#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 4;


// Some global state for arggen and rcsave functions.
static ExperCrossingArgs *curr_args = nullptr;
static int curr_preads_done = 0;

static bool pread_arggen(const int *epoch, int *fd, char **buf, size_t *count, off_t *offset, bool *buf_ready) {
    *fd = curr_args->fd;
    *buf = curr_args->rbuf;
    *count = curr_args->len;
    *offset = epoch[0] * curr_args->len;
    *buf_ready = false;
    return true;
}

static void pread_rcsave(const int *epoch, ssize_t res) {
    curr_preads_done++;
}

static bool pwrite_arggen(const int *epoch, int *fd, const char **buf, size_t *count, off_t *offset) {
    if (curr_preads_done != epoch[0] + 1)
        return false;
    *fd = curr_args->fd;
    *buf = curr_args->rbuf;
    *count = curr_args->len;
    *offset = epoch[0] * curr_args->len;
    return true;
}

static bool branch_arggen(const int *epoch, int *decision) {
    *decision = (epoch[0] + 1) < static_cast<int>(curr_args->nblocks) ? 0 : 1;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int common_assoc_dims[1] = {0};

    foreactor_AddSyscallPread(graph_id, 0, "pread", common_assoc_dims, 1, pread_arggen, pread_rcsave, 4096, /*is_start*/ true);
    foreactor_AddSyscallPwrite(graph_id, 1, "pwrite", common_assoc_dims, 1, pwrite_arggen, nullptr, false);
    foreactor_AddBranchNode(graph_id, 2, "branch", common_assoc_dims, 1, branch_arggen, 2, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 1, 2, false);
    foreactor_BranchAppendChild(graph_id, 2, 0, 0);
    foreactor_BranchAppendEndNode(graph_id, 2);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "crossing");
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
void __real__Z14exper_crossingPv(void *args);

extern "C"
void __wrap__Z14exper_crossingPv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z14exper_crossingPv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_args = reinterpret_cast<ExperCrossingArgs *>(args);

        // Call the original function with corresponding SCGraph activated.
        __real__Z14exper_crossingPv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
        curr_preads_done = 0;
    }
}
