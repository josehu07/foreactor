#include <foreactor.h>
#include "hijackees.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 9;


// Some global state for arggen and rcsave functions.
static ExperLdbGetArgs *curr_args = nullptr;
static std::vector<int> curr_fds;
static std::vector<bool> curr_index_block_ready;

static bool branch_table_open_arggen(const int *epoch, bool catching_up, int *decision) {
    *decision = (curr_args->fds[epoch[0]] <= 0) ? 0 : 1;
    return true;
}

static bool open_arggen(const int *epoch, bool *link, const char **pathname, int *flags, mode_t *mode) {
    *pathname = curr_args->filenames[epoch[0]].c_str();
    *flags = curr_args->open_flags;
    *mode = S_IRUSR | S_IWUSR;
    return true;
}

static void open_rcsave(const int *epoch, int fd) {
    curr_fds[epoch[0]] = fd;
}

static bool pread_index_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                               bool *buf_ready, bool *skip_memcpy) {
    if (curr_fds[epoch[0]] <= 0)
        return false;
    *fd = curr_fds[epoch[0]];
    *buf = curr_args->index_blocks[epoch[0]];
    *count = 4096;
    *offset = curr_args->file_size - 4096;
    *buf_ready = true;
    return true;
}

static void pread_index_rcsave(const int *epoch, ssize_t res) {
    assert(!curr_index_block_ready[epoch[0]]);
    curr_index_block_ready[epoch[0]] = true;
}

static bool pread_data_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                              bool *buf_ready, bool *skip_memcpy) {
    if (curr_fds[epoch[0]] <= 0 || !curr_index_block_ready[epoch[0]])
        return false;
    *fd = curr_fds[epoch[0]];
    *buf = curr_args->single_buf ? curr_args->bufs[0] : curr_args->bufs[epoch[0]];
    *count = curr_args->value_size;
    off_t data_offset = ldb_get_calculate_offset(curr_args->index_blocks[epoch[0]], curr_args->key,
                                                 curr_args->file_size, curr_args->value_size);
    *offset = data_offset;
    *buf_ready = curr_args->single_buf ? false : true;
    return true;
}

static bool branch_has_more_arggen(const int *epoch, bool catching_up, int *decision) {
    *decision = (epoch[0] < static_cast<int>(curr_args->key_match_at)) ? 0 : 1;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int common_assoc_dims[1] = {0};

    foreactor_AddBranchNode(graph_id, 0, "table_open", common_assoc_dims, 1, branch_table_open_arggen, 2, /*is_start*/ true);
    foreactor_AddSyscallOpen(graph_id, 1, "open", common_assoc_dims, 1, open_arggen, open_rcsave, false);
    foreactor_AddSyscallPread(graph_id, 2, "pread_index", common_assoc_dims, 1, pread_index_arggen, pread_index_rcsave, 4096, false);
    foreactor_AddSyscallPread(graph_id, 3, "pread_data", common_assoc_dims, 1, pread_data_arggen, nullptr, (1 << 20), false);
    foreactor_AddBranchNode(graph_id, 4, "has_more", common_assoc_dims, 1, branch_has_more_arggen, 2, false);

    foreactor_BranchAppendChild(graph_id, 0, 1, /*epoch_dim*/ -1);
    foreactor_BranchAppendChild(graph_id, 0, 3, -1);
    foreactor_SyscallSetNext(graph_id, 1, 2, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 2, 3, false);
    foreactor_SyscallSetNext(graph_id, 3, 4, true);
    foreactor_BranchAppendChild(graph_id, 4, 0, /*epoch_dim*/ 0);
    foreactor_BranchAppendEndNode(graph_id, 4);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "ldb_get");
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
void __real__Z13exper_ldb_getPv(void *args);

extern "C"
void __wrap__Z13exper_ldb_getPv(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real__Z13exper_ldb_getPv(args);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_args = reinterpret_cast<ExperLdbGetArgs *>(args);
        curr_fds = curr_args->fds;
        curr_index_block_ready.reserve(curr_fds.size());
        for (int fd : curr_fds)
            curr_index_block_ready.push_back(fd <= 0 ? false : true);

        // Call the original function with corresponding SCGraph activated.
        __real__Z13exper_ldb_getPv(args);

        foreactor_LeaveSCGraph(graph_id);
        curr_args = nullptr;
        curr_fds.clear();
        curr_index_block_ready.clear();
    }
}
