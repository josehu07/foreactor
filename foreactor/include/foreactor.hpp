//
// Application/plugin only needs to include this header file, <foreactor.hpp>.
//


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


// Syntax sugars to make plugin code cleaner.
void WrapperFuncEnter(SCGraph *scgraph, IOUring *ring, unsigned graph_id);
void WrapperFuncLeave(SCGraph *scgraph);


}


#endif
