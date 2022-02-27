// Application/plugin only needs to include <foreactor.hpp>.


#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"
#include "posix_itf.hpp"


#ifndef __FOREACTOR_FOREACTOR_H__
#define __FOREACTOR_FOREACTOR_H__


namespace foreactor {


// Parsed from environment variables when the library is loaded.
//   USE_FOREACTOR=yes      (any other string means no)
//   QUEUE_{SCGRAPH_ID}=32  (> 0, >= PRE_ISSUE_DEPTH, <= 1024)
//   DEPTH_{SCGRAPH_ID}=16  (>= 0, <= URING_QUEUE_LEN)
extern bool UseForeactor;
int EnvUringQueueLen(unsigned graph_id);
int EnvPreIssueDepth(unsigned graph_id);

// Should be called by a plugin upon its first attempt to use foreactor.
extern bool EnvParsed;
void ParseEnvValues();


}


#endif
