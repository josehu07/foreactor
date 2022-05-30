#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 2;


// Some global state for arggen and rcsave functions.
static ExperLoopingArgs *curr_args = nullptr;
static int curr_fd = -1;
static int curr_pwrites_done = 0;
static int curr_preads_done = 0;

static bool open_arggen(const int *epoch, const char **pathname, int *flags, mode_t *mode) {
    *pathname = curr_args->filename.c_str();
    *flags = O_CREAT | O_RDWR;
    *mode = S_IRUSR | S_IWUSR;
    return true;
}

static void open_rcsave(const int *epoch, int fd) {
    curr_fd = fd;
}

static bool pwrite_arggen(const int *epoch, int *fd, const char **buf, size_t *count, off_t *offset) {
    if (curr_fd < 0 || curr_preads_done % (curr_args->nreadsd2 * 2) != 0)
        return false;
    *fd = curr_fd;
    *buf = curr_args->wcontent.c_str();
    *count = curr_args->wlen;
    int j = (epoch[0] + epoch[1]) % curr_args->nwrites;
    *offset = j * curr_args->wlen;
    return true;
}

static void pwrite_rcsave(const int *epoch, ssize_t res) {
    curr_pwrites_done++;
}

static bool branch0_arggen(const int *epoch, int *decision) {
    *decision = ((epoch[0] + epoch[1] + 1) % curr_args->nwrites == 0) ? 0 : 1;
    return true;
}

static bool pread0_arggen(const int *epoch, int *fd, size_t *count, off_t *offset) {
    if (curr_pwrites_done == 0 || curr_pwrites_done % curr_args->nwrites != 0)
        return false;
    *fd = curr_fd;
    *count = curr_args->rlen;
    int j = (epoch[0] + epoch[2]) % curr_args->nreadsd2;
    *offset = (j*2) * curr_args->rlen;
    return true;
}

static void pread0_rcsave(const int *epoch, ssize_t res) {
    curr_preads_done++;
}

static bool pread1_arggen(const int *epoch, int *fd, size_t *count, off_t *offset) {
    if (curr_pwrites_done == 0 || curr_pwrites_done % curr_args->nwrites != 0)
        return false;
    *fd = curr_fd;
    *count = curr_args->rlen;
    int j = (epoch[0] + epoch[2]) % curr_args->nreadsd2;
    *offset = (j*2+1) * curr_args->rlen;
    return true;
}

static void pread1_rcsave(const int *epoch, ssize_t res) {
    curr_preads_done++;
}

static bool branch1_arggen(const int *epoch, int *decision) {
    *decision = ((epoch[0] + epoch[2] + 1) % curr_args->nreadsd2 == 0) ? 0 : 1;
    return true;
}

static bool branch2_arggen(const int *epoch, int *decision) {
    *decision = (epoch[0] + 1 == static_cast<int>(curr_args->nrepeats)) ? 0 : 1;
    return true;
}

static bool close_arggen(const int *epoch, int *fd) {
    if (curr_preads_done < static_cast<int>(curr_args->nreadsd2) * 2)
        return false;
    *fd = curr_fd;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 3);

    int pwrite_assoc_dims[2] = {0, 1};
    int branch0_assoc_dims[2] = {0, 1};
    int pread_assoc_dims[2] = {0, 2};
    int branch1_assoc_dims[2] = {0, 2};
    int branch2_assoc_dims[1] = {0};

    foreactor_AddSyscallOpen(graph_id, 0, "open", nullptr, 0, open_arggen, open_rcsave, /*is_start*/ true);
    foreactor_AddSyscallPwrite(graph_id, 1, "pwrite", pwrite_assoc_dims, 2, pwrite_arggen, pwrite_rcsave, false);
    foreactor_AddBranchNode(graph_id, 2, "branch0", branch0_assoc_dims, 2, branch0_arggen, 2, false);
    foreactor_AddSyscallPread(graph_id, 3, "pread0", pread_assoc_dims, 2, pread0_arggen, pread0_rcsave, false);
    foreactor_AddSyscallPread(graph_id, 4, "pread1", pread_assoc_dims, 2, pread1_arggen, pread1_rcsave, false);
    foreactor_AddBranchNode(graph_id, 5, "branch1", branch1_assoc_dims, 2, branch1_arggen, 2, false);
    foreactor_AddBranchNode(graph_id, 6, "branch2", branch2_assoc_dims, 1, branch2_arggen, 2, false);
    foreactor_AddSyscallClose(graph_id, 7, "close", nullptr, 0, close_arggen, nullptr, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 1, 2, false);
    foreactor_BranchAppendChild(graph_id, 2, 3, /*epoch_dim*/ -1);
    foreactor_BranchAppendChild(graph_id, 2, 1, 1);
    foreactor_SyscallSetNext(graph_id, 3, 4, false);
    foreactor_SyscallSetNext(graph_id, 4, 5, false);
    foreactor_BranchAppendChild(graph_id, 5, 6, -1);
    foreactor_BranchAppendChild(graph_id, 5, 3, 2);
    foreactor_BranchAppendChild(graph_id, 6, 7, -1);
    foreactor_BranchAppendChild(graph_id, 6, 1, 0);

    foreactor_SetSCGraphBuilt(graph_id);
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
void __real__Z13exper_loopingPv(void *args);

extern "C"
void __wrap__Z13exper_loopingPv(void *args) {
    if (!foreactor_HasSCGraph(graph_id))
        BuildSCGraph();

    foreactor_EnterSCGraph(graph_id);    
    curr_args = reinterpret_cast<ExperLoopingArgs *>(args);

    // Call the original function with corresponding SCGraph activated.
    __real__Z13exper_loopingPv(args);

    foreactor_LeaveSCGraph(graph_id);
    curr_args = nullptr;
    curr_fd = -1;
    curr_pwrites_done = 0;
    curr_preads_done = 0;
}
