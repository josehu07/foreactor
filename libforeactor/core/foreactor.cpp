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


static SCGraph *GetSCGraphFromId(unsigned graph_id) {
    PANIC_IF(!scgraphs.contains(graph_id),
             "graph_id %u not found\n", graph_id);
    return &scgraphs.at(graph_id);
}

static std::unordered_set<int> MakeAssocDimsSet(const int *assoc_dims,
                                                size_t assoc_dims_len) {
    std::unordered_set<int> assoc_dims_set;
    if (assoc_dims == nullptr) {
        assert(assoc_dims_len == 0);
        return assoc_dims_set;
    }

    assoc_dims_set.reserve(assoc_dims_len);
    for (size_t i = 0; i < assoc_dims_len; ++i)
        assoc_dims_set.insert(assoc_dims[i]);
    return assoc_dims_set;
}

static void PanicIfNodeExists(SCGraph *scgraph, unsigned graph_id,
                              unsigned node_id) {
    PANIC_IF(scgraph->nodes.contains(node_id),
             "node_id %u already exists in scgraph %u\n", node_id, graph_id);
}

static SCGraphNode *GetNodeFromId(SCGraph *scgraph, unsigned graph_id,
                          unsigned node_id) {
    PANIC_IF(!scgraph->nodes.contains(node_id),
             "node_id %u not found in scgraph %u\n", node_id, graph_id);
    return scgraph->nodes[node_id];
}


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

void foreactor_SetSCGraphBuilt(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    PANIC_IF(scgraph->IsBuilt(),
             "scgraph %u has already been set to built\n", graph_id);
    scgraph->SetBuilt();
    assert(scgraph->IsBuilt());
}

bool foreactor_HasSCGraph(unsigned graph_id) {
    // upon first call into library, parse environment config variables
    if (!EnvParsed)
        ParseEnvValues();
    return scgraphs.contains(graph_id);
}


void foreactor_AddSyscallOpen(unsigned graph_id,
                              unsigned node_id,
                              const char *name,
                              const int *assoc_dims,
                              size_t assoc_dims_len,
                              bool (*arggen_func)(const int *,
                                                  const char **,
                                                  int *,
                                                  mode_t *),
                              void (*rcsave_func)(const int *, int),
                              bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallOpen *node = new SyscallOpen(node_id, std::string(name), scgraph,
                                        assoc_dims_set, arggen_func,
                                        rcsave_func);
    scgraph->AddNode(node, is_start);
}

void foreactor_AddSyscallClose(unsigned graph_id,
                               unsigned node_id,
                               const char *name,
                               const int *assoc_dims,
                               size_t assoc_dims_len,
                               bool (*arggen_func)(const int *, int *),
                               void (*rcsave_func)(const int *, int),
                               bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallClose *node = new SyscallClose(node_id, std::string(name), scgraph,
                                          assoc_dims_set, arggen_func,
                                          rcsave_func);
    scgraph->AddNode(node, is_start);
}

void foreactor_AddSyscallPread(unsigned graph_id,
                              unsigned node_id,
                              const char *name,
                              const int *assoc_dims,
                              size_t assoc_dims_len,
                              bool (*arggen_func)(const int *,
                                                  int *,
                                                  size_t *,
                                                  off_t *),
                              void (*rcsave_func)(const int *, ssize_t),
                              bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallPread *node = new SyscallPread(node_id, std::string(name), scgraph,
                                          assoc_dims_set, arggen_func,
                                          rcsave_func);
    scgraph->AddNode(node, is_start);
}

void foreactor_AddSyscallPwrite(unsigned graph_id,
                                unsigned node_id,
                                const char *name,
                                const int *assoc_dims,
                                size_t assoc_dims_len,
                                bool (*arggen_func)(const int *,
                                                    int *,
                                                    const char **,
                                                    size_t *,
                                                    off_t *),
                                void (*rcsave_func)(const int *, ssize_t),
                                bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallPwrite *node = new SyscallPwrite(node_id, std::string(name), scgraph,
                                            assoc_dims_set, arggen_func,
                                            rcsave_func);
    scgraph->AddNode(node, is_start);
}


void foreactor_SyscallSetNext(unsigned graph_id, unsigned node_id,
                              unsigned next_id, bool weak_edge) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);

    SCGraphNode *node = GetNodeFromId(scgraph, graph_id, node_id);
    PANIC_IF(node->node_type != NODE_SC_PURE &&
             node->node_type != NODE_SC_SEFF,
             "node_id %u is not a SyscallNode\n", node_id);
    SyscallNode *sc_node = static_cast<SyscallNode *>(node);
    
    SCGraphNode *next_node = GetNodeFromId(scgraph, graph_id, next_id);
    sc_node->SetNext(next_node, weak_edge);
}


void foreactor_AddBranchNode(unsigned graph_id,
                             unsigned node_id,
                             const char *name,
                             const int *assoc_dims,
                             size_t assoc_dims_len,
                             bool (*arggen_func)(const int *, int *),
                             size_t num_children,
                             bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    BranchNode *node = new BranchNode(node_id, std::string(name), num_children,
                                      scgraph, assoc_dims_set, arggen_func);
    scgraph->AddNode(node, is_start);
}


void foreactor_BranchAppendChild(unsigned graph_id, unsigned node_id,
                                 unsigned child_id, int epoch_dim) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);

    SCGraphNode *node = GetNodeFromId(scgraph, graph_id, node_id);
    PANIC_IF(node->node_type != NODE_BRANCH,
             "node_id %u is not a BranchNode\n", node_id);
    BranchNode *branch_node = static_cast<BranchNode *>(node);

    SCGraphNode *child_node = GetNodeFromId(scgraph, graph_id, child_id);
    branch_node->AppendChild(child_node, epoch_dim);
}

void foreactor_BranchAppendEndNode(unsigned graph_id, unsigned node_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);

    SCGraphNode *node = GetNodeFromId(scgraph, graph_id, node_id);
    PANIC_IF(node->node_type != NODE_BRANCH,
             "node_id %u is not a BranchNode\n", node_id);
    BranchNode *branch_node = static_cast<BranchNode *>(node);

    branch_node->AppendChild(nullptr, -1);      // nullptr for end
}


void foreactor_EnterSCGraph(unsigned graph_id) {
    if (UseForeactor) {
        // register this SCGraph as active
        SCGraph *scgraph = GetSCGraphFromId(graph_id);
        assert(scgraph->IsBuilt());
        SCGraph::RegisterSCGraph(scgraph);
    }

}

void foreactor_LeaveSCGraph(unsigned graph_id) {
    if (UseForeactor) {
        // unregister this SCGraph
        SCGraph::UnregisterSCGraph();
        // clear everything prepared/on-the-fly, reset all epoch numbers to 0
        SCGraph *scgraph = GetSCGraphFromId(graph_id);
        assert(scgraph->IsBuilt());
        scgraph->ClearAllReqs();
        scgraph->ResetToStart();
    }
}


void foreactor_DumpDotImg(unsigned graph_id, const char *filestem) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->DumpDotImg(std::string(filestem));
}


}


}
