#include <foreactor.h>


/////////////////////
// SCGrpah builder //
/////////////////////

static const unsigned graph_id = 0;


// Some global state for arggen and rcsave functions.
// TODO


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 0);

    // TODO
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
void __real_copy(void *args);

void __wrap_copy(void *args) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        __real_copy(args);
    } else {
        // Build SCGraph once if haven't done yet.
        // if (!foreactor_HasSCGraph(graph_id))
        //     BuildSCGraph();

        // foreactor_EnterSCGraph(graph_id);    
        // curr_args = (ExperSimpleArgs *) args;

        // Call the original function with corresponding SCGraph activated.
        __real_copy(args);

        // foreactor_LeaveSCGraph(graph_id);
        // curr_args = NULL;
        // curr_fd = -1;
        // curr_pwrite_done = false;
        // curr_pread0_done = false;
        // curr_pread1_done = false;
    }
}
