#include <config.h>
#include <system.h>

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <foreactor.h>

#include "common.h"


/////////////////////
// SCGrpah builder //
/////////////////////

static const unsigned graph_id = 0;


// Some global state for arggen and rcsave functions.
// Information about the current archive command is stored in the global
// variable `GLOBAL struct tar_stat_info current_stat_info` in `common.h`.


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 0);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "dump_file");
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
void __real_dump_file_my(
        struct tar_stat_info *parent, char const *name,
        char const *fullname);

void __wrap_dump_file_my(
        struct tar_stat_info *parent, char const *name,
        char const *fullname) {
    if (!foreactor_UsingForeactor()) {
        uid_t uid = getuid();
        // Call the original function.
        __real_dump_file_my(parent, name, fullname);
        uid = getuid();
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);

        // Call the original function with corresponding SCGraph activated.
        __real_dump_file_my(parent, name, fullname);

        foreactor_LeaveSCGraph(graph_id);
    }
}
