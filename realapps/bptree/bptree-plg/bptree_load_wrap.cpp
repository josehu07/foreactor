// This plugin is included directly by bptree source code for convenience.


#include <iostream>
#include <vector>
#include <cassert>
#include <foreactor.h>

#include "common.hpp"


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned graph_id = 1;


static constexpr size_t BLKSIZE = 8192;


// Some global state for arggen and rcsave functions.
thread_local int curr_load_fd = -1;
thread_local std::vector<uint64_t> *curr_load_leaves = nullptr;
thread_local std::vector<bptree::Page *> *curr_load_leaves_pages = nullptr;


static bool pwrite_leaf_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    assert(curr_load_fd > 0);
    assert(curr_load_leaves != nullptr);
    assert(curr_load_leaves_pages != nullptr);
    assert(static_cast<size_t>(epoch[0]) < curr_load_leaves->size());

    *fd = curr_load_fd;
    *buf = reinterpret_cast<const char *>(curr_load_leaves_pages->at(epoch[0]));
    *count = BLKSIZE;
    *offset = curr_load_leaves->at(epoch[0]) * BLKSIZE;
    return true;
}

static bool branch_next_arggen(const int *epoch, bool catching_up, int *decision) {
    assert(curr_load_leaves != nullptr);
    if (static_cast<size_t>(epoch[0]) + 1 < curr_load_leaves->size())
        *decision = 0;
    else
        *decision = 1;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int common_assoc_dims[1] = {0};

    foreactor_AddSyscallPwrite(graph_id, 0, "pwrite_leaf", common_assoc_dims, 1, pwrite_leaf_arggen, nullptr, true);
    foreactor_AddBranchNode(graph_id, 1, "branch_next", common_assoc_dims, 1, branch_next_arggen, 2, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_BranchAppendChild(graph_id, 1, 0, /*epoch_dim*/ 0);
    foreactor_BranchAppendEndNode(graph_id, 1);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "bptree_load");
}


///////////////////
// Wrapper stubs //
///////////////////

// This app includes plugins in-place and calls these stubs directly around
// the code snippet to be wrapped over.
void PluginLoadPrologue(int fd, std::vector<uint64_t> *leaves,
                        std::vector<bptree::Page *> *leaves_pages) {
    if (foreactor_UsingForeactor()) {
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        curr_load_fd = fd;
        curr_load_leaves = leaves;
        curr_load_leaves_pages = leaves_pages;
        assert(curr_load_leaves->size() == curr_load_leaves_pages->size());

        foreactor_EnterSCGraph(graph_id);
    }
}

void PluginLoadEpilogue() {
    if (foreactor_UsingForeactor()) {
        foreactor_LeaveSCGraph(graph_id);

        curr_load_fd = -1;
        curr_load_leaves = nullptr;
        curr_load_leaves_pages = nullptr;
    }
}
