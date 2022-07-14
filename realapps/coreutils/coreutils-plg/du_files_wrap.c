#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <foreactor.h>

#include "xfts.h"
#include "du.h"


/////////////////////
// SCGrpah builder //
/////////////////////

static const unsigned graph_id = 1;


// Some global state for arggen and rcsave functions.


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 0);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "du_files");
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
// Use `objdump -t filename.o` to check the desired symbol name.
bool __real_du_files_my(char **files, int bit_flags);
bool __real_process_file_my (FTS *fts, FTSENT *ent);

bool __wrap_du_files_my(char **files, int bit_flags) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        uid_t uid = getuid();
        bool ret = __real_du_files_my(files, bit_flags);
        uid = getuid();
        return ret;
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);

        // Call the original function with corresponding SCGraph activated.
        bool ret = __real_du_files_my(files, bit_flags);

        foreactor_LeaveSCGraph(graph_id);
        
        return ret;
    }
}

bool __wrap_process_file_my (FTS *fts, FTSENT *ent) {
    return __real_process_file_my(fts, ent);
}
