#include <vector>

#include "ldb_get.hpp"

#include <foreactor.hpp>
namespace fa = foreactor;


//////////////////////
// IOUring instance //
//////////////////////

// There is one IOUring instance per wrapped function per thread.
thread_local fa::IOUring ring;


/////////////////////
// SCGraph builder //
/////////////////////

static constexpr unsigned ldb_get_graph_id = 0;
thread_local fa::SCGraph scgraph(ldb_get_graph_id);

void build_ldb_get_scgraph(fa::SCGraph *scgraph,
                           std::vector<std::vector<int>>& files) {
    assert(scgraph != nullptr);

    fa::SyscallNode *last_node = nullptr;
    // level-0 tables from latest to oldest
    for (int index = FILES_PER_LEVEL - 1; index >= 0; --index) {
        int fd = files[0][index];
        auto node_branch = new fa::BranchNode(fd < 0    // decision actually known at the start
                                              ? 0       // branch with open
                                              : 1);     // branch without open
        auto node_open = new fa::SyscallOpen(table_name(0, index), 0, O_RDONLY);
        auto node_pread = new fa::SyscallPread(fd, FILE_SIZE, 0,
                                               fd < 0
                                               ? std::vector{false, true, true}
                                               : std::vector{true,  true, true});
        scgraph->AddNode(node_branch, /*is_start*/ index == FILES_PER_LEVEL - 1);
        scgraph->AddNode(node_open);
        scgraph->AddNode(node_pread);
        if (last_node != nullptr)
            last_node->SetNext(node_branch, /*weak_edge*/ true);
        node_branch->SetChildren(std::vector<fa::SCGraphNode *>{node_open, node_pread});
        node_open->SetNext(node_pread, /*weak_edge*/ false);
        last_node = node_pread;
    }

    scgraph->SetBuilt();
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
extern "C" std::vector<std::string> __real__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(
        std::vector<std::vector<int>>& files);

extern "C" std::vector<std::string> __wrap__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(
        std::vector<std::vector<int>>& files) {
    fa::WrapperFuncEnter(&scgraph, &ring, ldb_get_graph_id);

    // Build SCGraph if using foreactor.
    if (fa::UseForeactor)
        build_ldb_get_scgraph(&scgraph, files);

    // Call the original function.
    auto ret = __real__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(files);

    fa::WrapperFuncLeave(&scgraph);
    return ret;
}
