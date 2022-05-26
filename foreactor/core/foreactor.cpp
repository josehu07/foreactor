#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "debug.hpp"
#include "timer.hpp"
#include "env_vars.hpp"
#include "io_uring.hpp"
#include "scg_graph.hpp"
#include "scg_nodes.hpp"
#include "syscalls.hpp"
#include "foreactor.h"


namespace foreactor {


///////////////////////////////////////////
// Shared library constructor/destructor //
///////////////////////////////////////////

// Called when the shared library is first loaded.
void __attribute__((constructor)) foreactor_ctor() {
    DEBUG("foreactor library loaded: timer %s\n",
#ifdef NTIMER
          "OFF"
#else
          "ON"
#endif
          );
}

// Called when the shared library is unloaded.
void __attribute__((destructor)) foreactor_dtor() {
    DEBUG("foreactor library unloaded\n");
}


/////////////////////////////
// Thread-local structures //
/////////////////////////////

// For each hijacked app function, there's one SCGraph instance per thread.
thread_local std::unordered_map<unsigned, SCGraph> scgraphs;

// One IOUring instance per SCGraph instance.
thread_local std::unordered_map<unsigned, IOUring> rings;


//////////////////////////
// Interface to plugins //
//////////////////////////

extern "C" {


void foreactor_CreateSCGraph(unsigned graph_id, unsigned total_dims) {
    // upon first call into library, parse environment config variables
    if (!EnvParsed)
        ParseEnvValues();

    PANIC_IF(scgraphs.contains(graph_id),
             "graph_id %u already exists in scgraphs\n", graph_id);
    PANIC_IF(rings.contains(graph_id),
             "graph_id %u already exists in rings\n", graph_id);

    // create new IOUring instance
    rings.emplace(std::piecewise_construct,
                  std::forward_as_tuple(graph_id),
                  std::forward_as_tuple(EnvUringQueueLen(graph_id)));

    // create new SCGraph instance, add to map
    scgraphs.emplace(std::piecewise_construct,
                     std::forward_as_tuple(graph_id),
                     std::forward_as_tuple(graph_id, total_dims,
                                           &rings.at(graph_id),
                                           EnvPreIssueDepth(graph_id)));
}


void foreactor_AddSyscallNode(unsigned graph_id, unsigned node_id,
                              const char *name, const int *assoc_dims,
                              size_t assoc_dims_len, SyscallType type,
                              bool is_start) {
    PANIC_IF(!scgraphs.contains(graph_id), "graph_id %u not found\n", graph_id);
    SCGraph *scgraph = &scgraphs.at(graph_id);

    std::unordered_set<int> assoc_dims_set;
    assoc_dims_set.reserve(assoc_dims_len);
    for (size_t i = 0; i < assoc_dims_len; ++i)
        assoc_dims_set.insert(assoc_dims[i]);

    PANIC_IF(scgraph->nodes.contains(node_id),
             "node_id %u already exists in scgraph %u\n", node_id, graph_id);
    SyscallNode *node = nullptr;

    switch (type) {
    case SC_OPEN:
        node = new SyscallOpen(node_id, std::string(name), scgraph,
                               assoc_dims_set);
        break;
    case SC_PREAD:
        node = new SyscallPread(node_id, std::string(name), scgraph,
                                assoc_dims_set);
        break;
    default: break;
    }
    PANIC_IF(node == nullptr, "unknown SyscallType %u\n", type);

    scgraph->AddNode(node, is_start);
}

void foreactor_SyscallSetNext(unsigned graph_id, unsigned node_id,
                              unsigned next_id, bool weak_edge) {
    PANIC_IF(!scgraphs.contains(graph_id), "graph_id %u not found\n", graph_id);
    SCGraph *scgraph = &scgraphs.at(graph_id);

    PANIC_IF(!scgraph->nodes.contains(node_id),
             "node_id %u not found in scgraph %u\n", node_id, graph_id);
    SCGraphNode *node = scgraph->nodes[node_id];
    PANIC_IF(node->node_type != NODE_SC_PURE &&
             node->node_type != NODE_SC_SEFF,
             "node_id %u is not a SyscallNode\n", node_id);
    SyscallNode *sc_node = static_cast<SyscallNode *>(node);

    PANIC_IF(!scgraph->nodes.contains(next_id),
             "next_id %u not found in scgraph %u\n", next_id, graph_id);
    SCGraphNode *next_node = scgraph->nodes[next_id];

    sc_node->SetNext(next_node, weak_edge);
}


void foreactor_AddBranchNode(unsigned graph_id, unsigned node_id,
                             const char *name, const int *assoc_dims,
                             size_t assoc_dims_len, size_t num_children,
                             bool is_start) {
    PANIC_IF(!scgraphs.contains(graph_id), "graph_id %u not found\n", graph_id);
    SCGraph *scgraph = &scgraphs.at(graph_id);

    std::unordered_set<int> assoc_dims_set;
    assoc_dims_set.reserve(assoc_dims_len);
    for (size_t i = 0; i < assoc_dims_len; ++i)
        assoc_dims_set.insert(assoc_dims[i]);

    PANIC_IF(scgraph->nodes.contains(node_id),
             "node_id %u already exists in scgraph %u\n", node_id, graph_id);
    BranchNode *node = new BranchNode(node_id, std::string(name), num_children,
                                      scgraph, assoc_dims_set);

    scgraph->AddNode(node, is_start);
}

void foreactor_BranchAppendChild(unsigned graph_id, unsigned node_id,
                                 unsigned child_id, int epoch_dim) {
    PANIC_IF(!scgraphs.contains(graph_id), "graph_id %u not found\n", graph_id);
    SCGraph *scgraph = &scgraphs.at(graph_id);

    PANIC_IF(!scgraph->nodes.contains(node_id),
             "node_id %u not found in scgraph %u\n", node_id, graph_id);
    SCGraphNode *node = scgraph->nodes[node_id];
    PANIC_IF(node->node_type != NODE_SC_PURE &&
             node->node_type != NODE_SC_SEFF,
             "node_id %u is not a BranchNode\n", node_id);
    BranchNode *branch_node = static_cast<BranchNode *>(node);

    PANIC_IF(!scgraph->nodes.contains(child_id),
             "child_id %u not found in scgraph %u\n", child_id, graph_id);
    SCGraphNode *child_node = scgraph->nodes[child_id];

    branch_node->AppendChild(child_node, epoch_dim);
}


void foreactor_EnterSCGraph(unsigned graph_id) {
    // register this SCGraph as active
    if (UseForeactor) {
        PANIC_IF(!scgraphs.contains(graph_id),
                 "graph_id %u not found\n", graph_id);
        SCGraph *scgraph = &scgraphs.at(graph_id);

        assert(scgraph->IsBuilt());
        SCGraph::RegisterSCGraph(scgraph);
    }

}

void foreactor_LeaveSCGraph(unsigned graph_id) {
    if (UseForeactor) {
        PANIC_IF(!scgraphs.contains(graph_id),
                 "graph_id %u not found\n", graph_id);
        SCGraph *scgraph = &scgraphs.at(graph_id);
        assert(scgraph->IsBuilt());

        // at every exit, unregister this SCGraph
        SCGraph::UnregisterSCGraph();

        // TODO: do background garbage collection of unharvested completions,
        // instead of always waiting for all of them at every exit
        scgraph->ClearAllReqs();

        // remember to reset all epoch numbers to 0
        scgraph->ResetToStart();
    }
}


}


}
