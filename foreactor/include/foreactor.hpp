//
// Application/plugin only needs to include this header file, <foreactor.hpp>.
//


#include <utility>

#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"
#include "posix_itf.hpp"
#include "value_pool.hpp"


#ifndef __FOREACTOR_FOREACTOR_H__
#define __FOREACTOR_FOREACTOR_H__


namespace foreactor {


// The foreactor library expects the following environment variables:
// 
//   USE_FOREACTOR=yes       (any other string means no)
//
//   if USE_FOREACTOR is not present in env, or if its value is not the
//   string yes, then foreactor will not be active; in this case, the
//   following two sets of env variables will have no effect
//   
//   QUEUE_{SCGRAPH_ID}=num  (> 0, >= PRE_ISSUE_DEPTH, <= 1024)
//   DEPTH_{SCGRAPH_ID}=num  (>= 0, <= URING_QUEUE_LEN)
//   
//   if USE_FOREACTOR is present and has the string value yes, then these
//   two groups of env variables must be present for each wrapped function
//   (i.e., SCGraph type) involved in the application; for example, giving
//   QUEUE_0=32 and DEPTH_0=8 says that the IOUring queue length for SCGraph
//   type 0 is 32 and the pre-issuing depth is set to be 8
extern bool EnvParsed;
extern bool UseForeactor;


void ParseEnvValues();
int EnvUringQueueLen(unsigned graph_id);
int EnvPreIssueDepth(unsigned graph_id);


// Syntax sugars to make plugin code cleaner.
// Template impelmentation must be put in-place.
template <typename BuilderFunc, typename PoolFlushFunc, typename ... Args>
void WrapperFuncEnter(SCGraph *scgraph, IOUring *ring, unsigned graph_id,
                      BuilderFunc builder_func, PoolFlushFunc pool_flush_func,
                      Args&&... pool_flush_args) {
    assert(scgraph != nullptr);
    assert(ring != nullptr);

    // upon first call into library, parse environment config variables
    if (!EnvParsed)
        ParseEnvValues();

    if (UseForeactor) {
        // upon first call into library, initialize IOUring, also link this
        // SCGraph to this IOUring instance
        if (!ring->IsInitialized())
            ring->Initialize(EnvUringQueueLen(graph_id));
        if (!scgraph->IsRingAssociated())
            scgraph->AssociateRing(ring, EnvPreIssueDepth(graph_id));

        // upon first call into library of this SCGraph, build the static graph
        if (!scgraph->IsBuilt()) {
            scgraph->StartTimer("build-graph");
            builder_func();
            scgraph->PauseTimer("build-graph");
            scgraph->SetBuilt();
        }

        // at every entrance, flush value pools to reflect the necessary syscall
        // argument values
        assert(scgraph->IsBuilt());
        scgraph->StartTimer("pool-flush");
        pool_flush_func(std::forward<Args>(pool_flush_args)...);
        scgraph->PauseTimer("pool-flush");
        
        // register this SCGraph as active
        RegisterSCGraph(scgraph);
    }

}

template <typename PoolClearFunc>
void WrapperFuncLeave(SCGraph *scgraph, PoolClearFunc pool_clear_func) {
    if (UseForeactor) {
        assert(scgraph != nullptr);
        assert(scgraph->IsBuilt());

        // at every exit, unregister this SCGraph
        UnregisterSCGraph();

        // TODO: do background garbage collection of unharvested completions,
        // instead of always waiting for all of them at every exit
        scgraph->ClearAllInProgress();

        // remember to reset all epoch numbers to 0
        scgraph->ResetToStart();

        // at every exit, clear all value pools
        scgraph->StartTimer("pool-clear");
        pool_clear_func();
        scgraph->PauseTimer("pool-clear");
    } else {
        assert(scgraph == nullptr);
    }
}


}


#endif
