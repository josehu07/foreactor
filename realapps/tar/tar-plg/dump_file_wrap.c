#include <config.h>
#include <system.h>

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <foreactor.h>

#include "common.h"


/////////////////////
// SCGrpah builder //
/////////////////////

static const unsigned graph_id = 0;


// Some global state for arggen and rcsave functions.

static bool fstatat_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, struct stat **buf, int *flags,
                           bool *buf_ready) {
    return false;
}

static bool openat_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, int *flags, mode_t *mode) {
    return false;
}

static void openat_rcsave(const int *epoch, int fd) {

}

static bool fstat_before_arggen(const int *epoch, bool *link, int *fd, struct stat **buf,
                                bool *buf_ready) {
    return false;
}

static bool branch_is_dir_arggen(const int *epoch, int *decision) {
    return false;
}

static bool fstat_after_arggen(const int *epoch, bool *link, int *fd, struct stat **buf,
                               bool *buf_ready) {
    return false;
}

static bool close_arggen(const int *epoch, bool *link, int *fd) {
    return false;
}

static bool branch_next_file_arggen(const int *epoch, int *decision) {
    return false;
}

static bool fstat_opendir_arggen(const int *epoch, bool *link, int *fd, struct stat **buf,
                                 bool *buf_ready) {
    return false;
}

static bool getdents_arggen(const int *epoch, bool *link, int *fd, struct dirent64 **dirp, size_t *count,
                            bool *buf_ready) {
    return false;
}

static void getdents_rcsave(const int *epoch, ssize_t res) {
    
}

static bool branch_dir_end_arggen(const int *epoch, int *decision) {
    return false;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 3);

    int outer_assoc_dims[1] = {0};
    int dir_assoc_dims[2] = {0, 1};
    int close_assoc_dims[2] = {0, 2};

    foreactor_AddSyscallFstatat(graph_id, 0, "fstatat", outer_assoc_dims, 1, fstatat_arggen, NULL, /*is_start*/ true);
    foreactor_AddSyscallOpenat(graph_id, 1, "openat", outer_assoc_dims, 1, openat_arggen, openat_rcsave, false);
    foreactor_AddSyscallFstat(graph_id, 2, "fstat_before", outer_assoc_dims, 1, fstat_before_arggen, NULL, false);
    foreactor_AddBranchNode(graph_id, 3, "is_dir", outer_assoc_dims, 1, branch_is_dir_arggen, 2, false);
    foreactor_AddSyscallFstat(graph_id, 4, "fstat_after", close_assoc_dims, 2, fstat_after_arggen, NULL, false);
    foreactor_AddSyscallClose(graph_id, 5, "close", close_assoc_dims, 2, close_arggen, NULL, false);
    foreactor_AddBranchNode(graph_id, 6, "next_file", close_assoc_dims, 2, branch_next_file_arggen, 2, false);
    foreactor_AddSyscallFstat(graph_id, 7, "fstat_opendir", outer_assoc_dims, 1, fstat_opendir_arggen, NULL, false);
    foreactor_AddSyscallGetdents(graph_id, 8, "getdents", dir_assoc_dims, 2, getdents_arggen, getdents_rcsave, 0, false);
    foreactor_AddBranchNode(graph_id, 9, "dir_end", dir_assoc_dims, 2, branch_dir_end_arggen, 3, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 1, 2, false);
    foreactor_SyscallSetNext(graph_id, 2, 3, false);
    foreactor_BranchAppendChild(graph_id, 3, 7, /*epoch_dim*/ -1);
    foreactor_BranchAppendChild(graph_id, 3, 4, -1);
    foreactor_SyscallSetNext(graph_id, 7, 8, false);
    foreactor_SyscallSetNext(graph_id, 8, 9, false);
    foreactor_BranchAppendChild(graph_id, 9, 8, 1);
    foreactor_BranchAppendChild(graph_id, 9, 6, -1);
    foreactor_SyscallSetNext(graph_id, 4, 5, false);
    foreactor_SyscallSetNext(graph_id, 5, 6, false);
    foreactor_BranchAppendChild(graph_id, 6, 0, 0);
    foreactor_BranchAppendChild(graph_id, 6, 4, 2);
    foreactor_BranchAppendEndNode(graph_id, 6);

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
        // Call the original function.
        __real_dump_file_my(parent, name, fullname);
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
