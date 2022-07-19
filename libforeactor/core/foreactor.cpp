#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "debug.hpp"
#include "timer.hpp"
#include "env_vars.hpp"
#include "io_engine.hpp"
#include "io_uring.hpp"
#include "thread_pool.hpp"
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
thread_local std::unordered_map<unsigned, int> scgraph_nest_cnts;

// If on, one IOUring instance per SCGraph instance per thread.
thread_local std::unordered_map<unsigned, IOUring> io_urings;

// If on, one ThreadPool instance per SCGraph instance per thread.
thread_local std::unordered_map<unsigned, ThreadPool> thread_pools;


//////////////////////////
// Interface to plugins //
//////////////////////////

extern "C" {


bool foreactor_UsingForeactor(void) {
    if (!EnvParsed)
        ParseEnvValues();
    return UseForeactor;
}


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
    PANIC_IF(scgraphs.contains(graph_id),
             "graph_id %u already exists in scgraphs\n", graph_id);
    PANIC_IF(io_urings.contains(graph_id),
             "graph_id %u already exists in io_urings\n", graph_id);

    // create new backend async syscall engine instance
    IOEngine *engine = nullptr;
    int num_uthreads = EnvThreadPoolSize(graph_id);
    if (num_uthreads <= 0) {
        DEBUG("using IOUring backend engine\n");
        io_urings.emplace(std::piecewise_construct,
                          std::forward_as_tuple(graph_id),
                          std::forward_as_tuple(EnvUringQueueLen(graph_id),
                                                EnvUringAsyncFlag(graph_id)));
        engine = &io_urings.at(graph_id);
    } else {
        DEBUG("using ThreadPool backend engine\n");
        thread_pools.emplace(std::piecewise_construct,
                             std::forward_as_tuple(graph_id),
                             std::forward_as_tuple(num_uthreads));
        engine = &thread_pools.at(graph_id);
    }

    // create new SCGraph instance, add to map
    scgraphs.emplace(std::piecewise_construct,
                     std::forward_as_tuple(graph_id),
                     std::forward_as_tuple(graph_id, total_dims, engine,
                                           EnvPreIssueDepth(graph_id)));

    // initialize nest_cnt to 0
    scgraph_nest_cnts[graph_id] = 0;
}

void foreactor_SetSCGraphBuilt(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    PANIC_IF(scgraph->IsBuilt(),
             "scgraph %u has already been set to built\n", graph_id);
    scgraph->SetBuilt();
    assert(scgraph->IsBuilt());
}

bool foreactor_HasSCGraph(unsigned graph_id) {
    return scgraphs.contains(graph_id);
}


void foreactor_AddSyscallOpen(unsigned graph_id,
                              unsigned node_id,
                              const char *name,
                              const int *assoc_dims,
                              size_t assoc_dims_len,
                              bool (*arggen_func)(const int *,
                                                  bool *,
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

void foreactor_AddSyscallOpenat(unsigned graph_id,
                                unsigned node_id,
                                const char *name,
                                const int *assoc_dims,
                                size_t assoc_dims_len,
                                bool (*arggen_func)(const int *,
                                                    bool *,
                                                    int *,
                                                    const char **,
                                                    int *,
                                                    mode_t *),
                                void (*rcsave_func)(const int *, int),
                                bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallOpenat *node = new SyscallOpenat(node_id, std::string(name),
                                            scgraph, assoc_dims_set,
                                            arggen_func, rcsave_func);
    scgraph->AddNode(node, is_start);
}

void foreactor_AddSyscallClose(unsigned graph_id,
                               unsigned node_id,
                               const char *name,
                               const int *assoc_dims,
                               size_t assoc_dims_len,
                               bool (*arggen_func)(const int *, bool *, int *),
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
                                                  bool *,
                                                  int *,
                                                  char **,
                                                  size_t *,
                                                  off_t *,
                                                  bool *,
                                                  bool *),
                              void (*rcsave_func)(const int *, ssize_t),
                              size_t pre_alloc_buf_size,
                              bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallPread *node = new SyscallPread(node_id, std::string(name), scgraph,
                                          assoc_dims_set, arggen_func,
                                          rcsave_func, pre_alloc_buf_size);
    scgraph->AddNode(node, is_start);
}

void foreactor_AddSyscallPwrite(unsigned graph_id,
                                unsigned node_id,
                                const char *name,
                                const int *assoc_dims,
                                size_t assoc_dims_len,
                                bool (*arggen_func)(const int *,
                                                    bool *,
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

void foreactor_AddSyscallLseek(unsigned graph_id,
                               unsigned node_id,
                               const char *name,
                               const int *assoc_dims,
                               size_t assoc_dims_len,
                               bool (*arggen_func)(const int *,  // not used
                                                   bool *,
                                                   int *,
                                                   off_t *,
                                                   int *),
                               void (*rcsave_func)(const int *, off_t),
                               bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallLseek *node = new SyscallLseek(node_id, std::string(name), scgraph,
                                          assoc_dims_set, arggen_func,
                                          rcsave_func);
    scgraph->AddNode(node, is_start);
}

void foreactor_AddSyscallFstat(unsigned graph_id,
                               unsigned node_id,
                               const char *name,
                               const int *assoc_dims,
                               size_t assoc_dims_len,
                               bool (*arggen_func)(const int *,
                                                   bool *,
                                                   int *,
                                                   struct stat **,
                                                   bool *),
                               void (*rcsave_func)(const int *, int),
                               bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallFstat *node = new SyscallFstat(node_id, std::string(name), scgraph,
                                          assoc_dims_set, arggen_func,
                                          rcsave_func);
    scgraph->AddNode(node, is_start);
}

void foreactor_AddSyscallFstatat(unsigned graph_id,
                                 unsigned node_id,
                                 const char *name,
                                 const int *assoc_dims,
                                 size_t assoc_dims_len,
                                 bool (*arggen_func)(const int *,
                                                     bool *,
                                                     int *,
                                                     const char **,
                                                     struct stat **,
                                                     int *,
                                                     bool *),
                                 void (*rcsave_func)(const int *, int),
                                 bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallFstatat *node = new SyscallFstatat(node_id, std::string(name),
                                              scgraph, assoc_dims_set,
                                              arggen_func, rcsave_func);
    scgraph->AddNode(node, is_start);
}

void foreactor_AddSyscallGetdents(unsigned graph_id,
                                  unsigned node_id,
                                  const char *name,
                                  const int *assoc_dims,
                                  size_t assoc_dims_len,
                                  bool (*arggen_func)(const int *,
                                                      bool *,
                                                      int *,
                                                      struct dirent64 **,
                                                      size_t *,
                                                      bool *),  // buf_ready?
                                  void (*rcsave_func)(const int *, ssize_t),
                                  size_t pre_alloc_buf_size,
                                  bool is_start) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    std::unordered_set<int> assoc_dims_set =
        MakeAssocDimsSet(assoc_dims, assoc_dims_len);

    PanicIfNodeExists(scgraph, graph_id, node_id);
    SyscallGetdents *node = new SyscallGetdents(node_id, std::string(name),
                                                scgraph, assoc_dims_set,
                                                arggen_func, rcsave_func,
                                                pre_alloc_buf_size);
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


struct stat *foreactor_FstatGetResultBuf(unsigned graph_id, unsigned node_id,
                                         const int *epoch_) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    
    SCGraphNode *node = GetNodeFromId(scgraph, graph_id, node_id);
    PANIC_IF(node->node_type != NODE_SC_PURE,
             "node_id %u is not a pure SyscallNode\n", node_id);
    SyscallFstat *fstat_node = static_cast<SyscallFstat *>(node);
    PANIC_IF(fstat_node->sc_type != SC_FSTAT,
             "node_id %u is not a fstat node\n", node_id);

    const EpochList epoch(scgraph->total_dims, epoch_);
    return fstat_node->GetStatBuf(epoch);
}

struct stat *foreactor_FstatatGetResultBuf(unsigned graph_id, unsigned node_id,
                                           const int *epoch_) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    
    SCGraphNode *node = GetNodeFromId(scgraph, graph_id, node_id);
    PANIC_IF(node->node_type != NODE_SC_PURE,
             "node_id %u is not a pure SyscallNode\n", node_id);
    SyscallFstatat *fstatat_node = static_cast<SyscallFstatat *>(node);
    PANIC_IF(fstatat_node->sc_type != SC_FSTATAT,
             "node_id %u is not a fstatat node\n", node_id);

    const EpochList epoch(scgraph->total_dims, epoch_);
    return fstatat_node->GetStatBuf(epoch);
}

struct dirent64 *foreactor_GetdentsGetResultBuf(unsigned graph_id,
                                                unsigned node_id,
                                                const int *epoch_) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    
    SCGraphNode *node = GetNodeFromId(scgraph, graph_id, node_id);
    PANIC_IF(node->node_type != NODE_SC_PURE,
             "node_id %u is not a pure SyscallNode\n", node_id);
    SyscallGetdents *getdents_node = static_cast<SyscallGetdents *>(node);
    PANIC_IF(getdents_node->sc_type != SC_GETDENTS,
             "node_id %u is not a getdents node\n", node_id);

    const EpochList epoch(scgraph->total_dims, epoch_);
    return getdents_node->GetDirpBuf(epoch);
}

char *foreactor_PreadRefInternalBuf(unsigned graph_id, unsigned node_id,
                                    const int *epoch_) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    
    SCGraphNode *node = GetNodeFromId(scgraph, graph_id, node_id);
    PANIC_IF(node->node_type != NODE_SC_PURE,
             "node_id %u is not a pure SyscallNode\n", node_id);
    SyscallPread *pread_node = static_cast<SyscallPread *>(node);
    PANIC_IF(pread_node->sc_type != SC_PREAD,
             "node_id %u is not a pread node\n", node_id);

    const EpochList epoch(scgraph->total_dims, epoch_);
    return pread_node->RefInternalBuf(epoch);
}

void foreactor_PreadPutInternalBuf(unsigned graph_id, unsigned node_id,
                                   const int *epoch_) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    
    SCGraphNode *node = GetNodeFromId(scgraph, graph_id, node_id);
    PANIC_IF(node->node_type != NODE_SC_PURE,
             "node_id %u is not a pure SyscallNode\n", node_id);
    SyscallPread *pread_node = static_cast<SyscallPread *>(node);
    PANIC_IF(pread_node->sc_type != SC_PREAD,
             "node_id %u is not a pread node\n", node_id);

    const EpochList epoch(scgraph->total_dims, epoch_);
    pread_node->PutInternalBuf(epoch);
}


void foreactor_IgnoreSyscallOpen(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_OPEN);
}

void foreactor_IgnoreSyscallOpenat(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_OPENAT);
}

void foreactor_IgnoreSyscallClose(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_CLOSE);
}

void foreactor_IgnoreSyscallPread(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_PREAD);
}

void foreactor_IgnoreSyscallPwrite(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_PWRITE);
}

void foreactor_IgnoreSyscallLseek(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_LSEEK);
}

void foreactor_IgnoreSyscallFstat(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_FSTAT);
}

void foreactor_IgnoreSyscallFstatat(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_FSTATAT);
}

void foreactor_IgnoreSyscallGetdents(unsigned graph_id) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->AddNonInterceptionType(SC_GETDENTS);
}


void foreactor_EnterSCGraph(unsigned graph_id) {
    if (UseForeactor) {
        if (active_scgraph == nullptr) {
            // this is the first time entering this wrapped function;
            // future recursive invocations will stay in the same SCGraph
            assert(scgraph_nest_cnts[graph_id] == 0);
            // register this SCGraph as active
            SCGraph *scgraph = GetSCGraphFromId(graph_id);
            assert(scgraph->IsBuilt());
            SCGraph::RegisterSCGraph(scgraph);
        }
        scgraph_nest_cnts[graph_id]++;
    }
}

void foreactor_LeaveSCGraph(unsigned graph_id) {
    if (UseForeactor) {
        assert(scgraph_nest_cnts[graph_id] > 0);
        scgraph_nest_cnts[graph_id]--;
        if (scgraph_nest_cnts[graph_id] == 0) {
            // clear everything prepared/on-the-fly, reset all epoch numbers to 0
            SCGraph *scgraph = GetSCGraphFromId(graph_id);
            assert(scgraph->IsBuilt());
            scgraph->ClearAllReqs();
            scgraph->ResetToStart();
            // unregister this SCGraph
            SCGraph::UnregisterSCGraph();
        }
    }
}


void foreactor_PauseCurrentSCGraph(void) {
    if (UseForeactor) {
        assert(active_scgraph != nullptr);
        assert(paused_scgraph == nullptr);
        paused_scgraph = active_scgraph;
        active_scgraph = nullptr;
    }
}

void foreactor_ResumeCurrentSCGraph(void) {
    if (UseForeactor) {
        assert(active_scgraph == nullptr);
        assert(paused_scgraph != nullptr);
        active_scgraph = paused_scgraph;
        paused_scgraph = nullptr;
    }
}


void foreactor_DumpDotImg(unsigned graph_id, const char *filestem) {
    SCGraph *scgraph = GetSCGraphFromId(graph_id);
    scgraph->DumpDotImg(std::string(filestem));
}


}


}
