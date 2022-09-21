// This plugin is included directly by bptree source code for convenience.


#include <iostream>
#include <vector>
#include <cassert>
#include <foreactor.h>


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 0;


static constexpr size_t BLKSIZE = 8192;


// Some global state for arggen and rcsave functions.
thread_local int curr_scan_fd = -1;
thread_local std::vector<uint64_t> *curr_scan_leaves = nullptr;


static bool pread_leaf_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                              bool *buf_ready, bool *skip_memcpy) {
    assert(curr_scan_fd > 0);
    assert(curr_scan_leaves != nullptr);
    assert(static_cast<size_t>(epoch[0]) < curr_scan_leaves->size());

    *fd = curr_scan_fd;
    *buf = nullptr;
    *count = BLKSIZE;
    *offset = curr_scan_leaves->at(epoch[0]) * BLKSIZE;
    *buf_ready = false;
    return true;
}

static bool branch_next_arggen(const int *epoch, bool catching_up, int *decision) {
    assert(curr_scan_leaves != nullptr);
    if (static_cast<size_t>(epoch[0]) + 1 < curr_scan_leaves->size())
        *decision = 0;
    else
        *decision = 1;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int common_assoc_dims[1] = {0};

    foreactor_AddSyscallPread(graph_id, 0, "pread_leaf", common_assoc_dims, 1, pread_leaf_arggen, nullptr, BLKSIZE, true);
    foreactor_AddBranchNode(graph_id, 1, "branch_next", common_assoc_dims, 1, branch_next_arggen, 2, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_BranchAppendChild(graph_id, 1, 0, /*epoch_dim*/ 0);
    foreactor_BranchAppendEndNode(graph_id, 1);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "bptree_scan");
}


///////////////////
// Wrapper stubs //
///////////////////

// This app includes plugins in-place and calls these stubs directly around
// the code snippet to be wrapped over.
void PluginScanPrologue(int fd, std::vector<uint64_t> *leaves) {
    if (foreactor_UsingForeactor()) {
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        curr_scan_fd = fd;
        curr_scan_leaves = leaves;

        foreactor_EnterSCGraph(graph_id);
    }
}

void PluginScanEpilogue() {
    if (foreactor_UsingForeactor()) {
        foreactor_LeaveSCGraph(graph_id);

        curr_scan_fd = -1;
        curr_scan_leaves = nullptr;
    }
}
