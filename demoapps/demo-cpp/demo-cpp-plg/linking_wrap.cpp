#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 10;


// Some global state for arggen and rcsave functions.
static ExperLinkingArgs *curr_args = nullptr;

static bool pread_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                         bool *buf_ready, bool *skip_memcpy) {
    *link = true;
    *fd = curr_args->fd_in;
    *buf = curr_args->buf;
    *count = curr_args->block_size;
    *offset = epoch[0] * curr_args->block_size;
    *buf_ready = false;
    *skip_memcpy = true;
    return true;
}

static bool pwrite_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    char *ib = foreactor_PreadRefInternalBuf(graph_id, 0, epoch);
    if (ib == NULL)
        return false;
    *fd = curr_args->fd_out;
    *buf = ib;
    *count = curr_args->block_size;
    *offset = epoch[0] * curr_args->block_size;
    return true;
}

static void pwrite_rcsave(const int *epoch, ssize_t res) {
    assert(res > 0);
    foreactor_PreadPutInternalBuf(graph_id, 0, epoch);
}

static bool branch_arggen(const int *epoch, int *decision) {
    *decision = (epoch[0] + 1 < static_cast<int>(curr_args->num_blocks)) ? 0 : 1;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int common_assoc_dims[1] = {0};

    foreactor_AddSyscallPread(graph_id, 0, "pread", common_assoc_dims, 1, pread_arggen, nullptr, (1 << 20), /*is_start*/ true);
    foreactor_AddSyscallPwrite(graph_id, 1, "pwrite", common_assoc_dims, 1, pwrite_arggen, pwrite_rcsave, false);
    foreactor_AddBranchNode(graph_id, 2, "branch", common_assoc_dims, 1, branch_arggen, 2, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 1, 2, false);
    foreactor_BranchAppendChild(graph_id, 2, 0, /*epoch_dim*/ 0);
    foreactor_BranchAppendEndNode(graph_id, 2);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "linking");
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
void __real__Z13exper_linkingPv(void *args);

extern "C"
void __wrap__Z13exper_linkingPv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z13exper_linkingPv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_args = reinterpret_cast<ExperLinkingArgs *>(args);

        // Call the original function with corresponding SCGraph activated.
        __real__Z13exper_linkingPv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
    }
}
