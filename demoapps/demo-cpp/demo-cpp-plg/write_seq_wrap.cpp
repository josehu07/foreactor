#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 6;


// Some global state for arggen and rcsave functions.
static ExperWriteSeqArgs *curr_args = nullptr;

static bool pwrite_arggen(const int *epoch, int *fd, const char **buf, size_t *count, off_t *offset) {
    *fd = curr_args->multi_file ? curr_args->fds[epoch[0]] : curr_args->fds[0];
    *buf = curr_args->wbufs[epoch[0]];
    *count = curr_args->wlen;
    *offset = curr_args->multi_file ? 0 : epoch[0] * curr_args->wlen;
    return true;
}

static bool branch_arggen(const int *epoch, int *decision) {
    *decision = (epoch[0] + 1 < static_cast<int>(curr_args->nwrites)) ? 0 : 1;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int pwrite_assoc_dims[1] = {0};
    int branch_assoc_dims[1] = {0};

    foreactor_AddSyscallPwrite(graph_id, 0, "pwrite", pwrite_assoc_dims, 1, pwrite_arggen, nullptr, /*is_start*/ true);
    foreactor_AddBranchNode(graph_id, 1, "branch", branch_assoc_dims, 1, branch_arggen, 2, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_BranchAppendChild(graph_id, 1, 0, /*epoch_dim*/ 0);
    foreactor_BranchAppendEndNode(graph_id, 1);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "write_seq");
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
void __real__Z15exper_write_seqPv(void *args);

extern "C"
void __wrap__Z15exper_write_seqPv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z15exper_write_seqPv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_args = reinterpret_cast<ExperWriteSeqArgs *>(args);

        // Call the original function with corresponding SCGraph activated.
        __real__Z15exper_write_seqPv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
    }
}
