#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 4;


// Some global state for arggen and rcsave functions.
static ExperWeakEdgeArgs *curr_args = nullptr;
static int curr_pwrites_done = 0;

static bool pwrite_arggen(const int *epoch, int *fd, const char **buf, size_t *count, off_t *offset) {
    *fd = curr_args->fd;
    *buf = curr_args->wcontents[epoch[0]].c_str();
    *count = curr_args->len;
    *offset = 0;
    return true;
}

static void pwrite_rcsave(const int *epoch, ssize_t res) {
    curr_pwrites_done++;
}

static bool pread0_arggen(const int *epoch, int *fd, char **buf, size_t *count, off_t *offset, bool *buf_ready) {
    if (curr_pwrites_done < epoch[0] + 1)
        return false;
    *fd = curr_args->fd;
    *buf = curr_args->rbuf0;
    *count = curr_args->len;
    *offset = 0;
    *buf_ready = true;
    return true;
}

static bool pread1_arggen(const int *epoch, int *fd, char **buf, size_t *count, off_t *offset, bool *buf_ready) {
    if (curr_pwrites_done < epoch[0] + 1)
        return false;
    *fd = curr_args->fd;
    *buf = curr_args->rbuf1;
    *count = curr_args->len;
    *offset = 0;
    *buf_ready = false;     // use internal buffer for this pread
    return true;
}

static bool branch_arggen(const int *epoch, int *decision) {
    *decision = (epoch[0] + 1) < static_cast<int>(curr_args->nrepeats) ? 0 : 1;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int common_assoc_dims[1] = {0};

    foreactor_AddSyscallPwrite(graph_id, 0, "pwrite", common_assoc_dims, 1, pwrite_arggen, pwrite_rcsave, /*is_start*/ true);
    foreactor_AddSyscallPread(graph_id, 1, "pread0", common_assoc_dims, 1, pread0_arggen, nullptr, 512, false);
    foreactor_AddSyscallPread(graph_id, 2, "pread1", common_assoc_dims, 1, pread1_arggen, nullptr, 512, false);
    foreactor_AddBranchNode(graph_id, 3, "branch", common_assoc_dims, 1, branch_arggen, 2, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 1, 2, true);
    foreactor_SyscallSetNext(graph_id, 2, 3, false);
    foreactor_BranchAppendChild(graph_id, 3, 0, 0);
    foreactor_BranchAppendEndNode(graph_id, 3);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "weak_edge");
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
void __real__Z15exper_weak_edgePv(void *args);

extern "C"
void __wrap__Z15exper_weak_edgePv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z15exper_weak_edgePv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_args = reinterpret_cast<ExperWeakEdgeArgs *>(args);

        // Call the original function with corresponding SCGraph activated.
        __real__Z15exper_weak_edgePv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
        curr_pwrites_done = 0;
    }
}
