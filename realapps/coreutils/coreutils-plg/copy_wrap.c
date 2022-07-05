#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <foreactor.h>

#include "backupfile.h"
#include "ioblksize.h"
#include "copy.h"


/////////////////////
// SCGrpah builder //
/////////////////////

static const unsigned graph_id = 0;


// Some global state for arggen and rcsave functions.
static char const *curr_src_name = NULL;
static char const *curr_dst_name = NULL;
static int curr_dst_dirfd = -1;
static char const *curr_dst_relname = NULL;
static int curr_nonexistent_dst = 0;
static const struct cp_options *curr_x = NULL;


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 0);

    // foreactor_AddSyscallOpen(graph_id, 0, "open", NULL, 0, open_arggen, open_rcsave, /*is_start*/ true);
    // foreactor_AddSyscallPwrite(graph_id, 1, "pwrite", NULL, 0, pwrite_arggen, pwrite_rcsave, false);
    // foreactor_AddSyscallPread(graph_id, 2, "pread0", NULL, 0, pread0_arggen, pread0_rcsave, 4096, false);
    // foreactor_AddSyscallPread(graph_id, 3, "pread1", NULL, 0, pread1_arggen, pread1_rcsave, 4096, false);
    // foreactor_AddSyscallClose(graph_id, 4, "close", NULL, 0, close_arggen, NULL, false);

    // foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    // foreactor_SyscallSetNext(graph_id, 1, 2, false);
    // foreactor_SyscallSetNext(graph_id, 2, 3, false);
    // foreactor_SyscallSetNext(graph_id, 3, 4, false);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "copy");
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
bool __real_copy(
        char const *src_name, char const *dst_name,
        int dst_dirfd, char const *dst_relname,
        int nonexistent_dst, const struct cp_options *options,
        bool *copy_into_self, bool *rename_succeeded);

bool __wrap_copy(
        char const *src_name, char const *dst_name,
        int dst_dirfd, char const *dst_relname,
        int nonexistent_dst, const struct cp_options *options,
        bool *copy_into_self, bool *rename_succeeded) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        return __real_copy(src_name, dst_name, dst_dirfd, dst_relname,
                           nonexistent_dst, options, copy_into_self,
                           rename_succeeded);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        foreactor_EnterSCGraph(graph_id);    
        curr_src_name = src_name;
        curr_dst_name = dst_name;
        curr_dst_dirfd = dst_dirfd;
        curr_dst_relname = dst_relname;
        curr_nonexistent_dst = nonexistent_dst;
        curr_x = options;

        // Call the original function with corresponding SCGraph activated.
        bool ret = __real_copy(src_name, dst_name, dst_dirfd, dst_relname,
                               nonexistent_dst, options, copy_into_self,
                               rename_succeeded);

        foreactor_LeaveSCGraph(graph_id);
        curr_src_name = NULL;
        curr_dst_name = NULL;
        curr_dst_dirfd = -1;
        curr_dst_relname = NULL;
        curr_nonexistent_dst = 0;
        curr_x = NULL;
        
        return ret;
    }
}
