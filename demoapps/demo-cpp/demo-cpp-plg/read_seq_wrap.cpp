#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 6;


// Some global state for arggen and rcsave functions.
static ExperReadSeqArgs *curr_args = nullptr;
static int curr_preads_done = 0;

static bool pread_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                         bool *buf_ready, bool *skip_memcpy) {
    *fd = curr_args->multi_file ? curr_args->fds[epoch[0]] : curr_args->fds[0];
    *buf = curr_args->same_buffer ? curr_args->rbufs[0] : curr_args->rbufs[epoch[0]];
    *count = curr_args->rlen;
    *offset = curr_args->multi_file ? 0 : epoch[0] * curr_args->rlen;
    if (curr_args->same_buffer && curr_preads_done < epoch[0])
        *buf_ready = false;
    else
        *buf_ready = true;
    return true;
}

static void pread_rcsave(const int *epoch, ssize_t res) {
    curr_preads_done++;
}

static bool branch_arggen(const int *epoch, bool catching_up, int *decision) {
    *decision = (epoch[0] + 1 < static_cast<int>(curr_args->nreads)) ? 0 : 1;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int pread_assoc_dims[1] = {0};
    int branch_assoc_dims[1] = {0};

    foreactor_AddSyscallPread(graph_id, 0, "pread", pread_assoc_dims, 1, pread_arggen, pread_rcsave, (1 << 20), /*is_start*/ true);
    foreactor_AddBranchNode(graph_id, 1, "branch", branch_assoc_dims, 1, branch_arggen, 2, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_BranchAppendChild(graph_id, 1, 0, /*epoch_dim*/ 0);
    foreactor_BranchAppendEndNode(graph_id, 1);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "read_seq");
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
void __real__Z14exper_read_seqPv(void *args);

extern "C"
void __wrap__Z14exper_read_seqPv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z14exper_read_seqPv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);
        curr_args = reinterpret_cast<ExperReadSeqArgs *>(args);

        // Call the original function with corresponding SCGraph activated.
        __real__Z14exper_read_seqPv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
        curr_preads_done = 0;
    }
}
