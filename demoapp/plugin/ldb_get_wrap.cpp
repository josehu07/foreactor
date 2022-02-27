#include <vector>

#include "ldb_get.hpp"

#include <foreactor.hpp>
namespace fa = foreactor;


// There is an IOUring instance per wrapped function per thread.
thread_local fa::IOUring *ring = nullptr;


fa::SCGraph *build_ldb_get_scgraph(unsigned graph_id, int pre_issue_depth,
                                   std::vector<std::vector<int>>& files) {
    fa::SCGraph *scgraph = new fa::SCGraph(graph_id, ring, pre_issue_depth);

    auto GenNodeId = [](int level, int index, int op) -> uint64_t {
        constexpr int fmax = NUM_LEVELS * FILES_PER_LEVEL;
        int fid = level * FILES_PER_LEVEL + index;
        return op * fmax + fid;
    };

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
        scgraph->AddNode(GenNodeId(0, index, 0), node_branch, /*is_start*/ index == FILES_PER_LEVEL - 1);
        scgraph->AddNode(GenNodeId(0, index, 1), node_open);
        scgraph->AddNode(GenNodeId(0, index, 2), node_pread);
        if (last_node != nullptr)
            last_node->SetNext(node_branch, /*weak_edge*/ true);
        node_branch->SetChildren(std::vector<fa::SCGraphNode *>{node_open, node_pread});
        node_open->SetNext(node_pread, /*weak_edge*/ false);
        last_node = node_pread;
    }

    return scgraph;
}


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
extern "C" std::vector<std::string> __real__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(std::vector<std::vector<int>>& files);

extern "C" std::vector<std::string> __wrap__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(std::vector<std::vector<int>>& files) {
    if (!fa::EnvParsed)
        fa::ParseEnvValues();
    if (fa::UseForeactor && ring == nullptr)
        ring = new fa::IOUring(fa::EnvUringQueueLen(0));

    fa::SCGraph *scgraph = nullptr;
    if (fa::UseForeactor) {
        scgraph = build_ldb_get_scgraph(0, fa::EnvPreIssueDepth(0), files);
        fa::RegisterSCGraph(scgraph, ring);
    }

    // Call the original function.
    auto ret = __real__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(files);

    if (fa::UseForeactor) {
        assert(scgraph != nullptr);
        fa::UnregisterSCGraph();
        delete scgraph;
    }

    return ret;
}
