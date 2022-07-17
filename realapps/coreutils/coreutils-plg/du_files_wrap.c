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
static FTS *curr_fts = NULL;
static FTSENT *curr_peek_ftsent = NULL;
static bool curr_in_peek_seg = false;
static int curr_ftsent_cnt = 0;

static bool fstatat_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, struct stat **buf, int *flags,
                           bool *buf_ready) {
    if (!curr_in_peek_seg)
        return false;
    assert(curr_peek_ftsent != NULL);
    // pre-issuing of fstatat's can happen for a segment of contiguous file entries
    *dirfd = curr_fts->fts_cwd_fd;
    *pathname = curr_peek_ftsent->fts_name;
    *buf = NULL;
    *flags = AT_SYMLINK_NOFOLLOW;
    *buf_ready = false;
    return true;
}

// static bool branch_is_dir_arggen(const int *epoch, int *decision) {
//     if (curr_peek_ftsent == NULL)
//         return false;
//     assert(curr_peek_entidx == epoch[0]);
//     if (curr_peek_ftsent->fts_info == FTS_F)
//         *decision = 1;
//     else
//         *decision = 0;
//     return true;
// }

// static bool openat_dir_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, int *flags, mode_t *mode) {
//     return false;
// }

// static bool fstatat_dir_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, struct stat **buf, int *flags,
//                                bool *buf_ready) {
//     return false;
// }

// static bool getdents_arggen(const int *epoch, bool *link, int *fd, struct dirent64 **dirp, size_t *count,
//                             bool *buf_ready) {
//     return false;
// }

// static void getdents_rcsave(const int *epoch, ssize_t res) {
//     if (res == 0)
//         curr_getdents_done = true;
// }

// static bool branch_more_dents_arggen(const int *epoch, int *decision) {
//     if (curr_getdents_done) {
//         curr_getdents_done = false;
//         *decision = 1;
//         return true;
//     } else
//         return false;
// }

static bool branch_next_file_arggen(const int *epoch, int *decision) {
    // if catching up to frontier...
    if (curr_ftsent_cnt > epoch[0]) {
        *decision = 0;
        return true;
    }

    // can proceed to the next file ahead of time only if we are now in a
    // contiguous segment of regular file entries within a directory
    if (curr_peek_ftsent != NULL) {
        FTSENT *next_ftsent = curr_peek_ftsent->fts_link;
        if (curr_in_peek_seg) {
            if (next_ftsent == NULL || next_ftsent->fts_info != FTS_NSOK)
                curr_in_peek_seg = false;
            else
                curr_peek_ftsent = next_ftsent;
        } else {
            if (curr_peek_ftsent->fts_info == FTS_NSOK && next_ftsent != NULL
                && next_ftsent->fts_info == FTS_NSOK) {
                curr_in_peek_seg = true;
                curr_peek_ftsent = next_ftsent;
            }
        }
    }

    if (curr_in_peek_seg) {
        *decision = 0;
        return true;
    } else
        return false;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int outer_assoc_dims[1] = {0};
    // int dir_assoc_dims[2] = {0, 1};

    foreactor_AddSyscallFstatat(graph_id, 0, "fstatat", outer_assoc_dims, 1, fstatat_arggen, NULL, /*is_start*/ true);
    // foreactor_AddBranchNode(graph_id, 1, "is_dir", outer_assoc_dims, 1, branch_is_dir_arggen, 2, false);
    // foreactor_AddSyscallOpenat(graph_id, 2, "openat_dir", outer_assoc_dims, 1, openat_dir_arggen, NULL, false);
    // foreactor_AddSyscallFstatat(graph_id, 6, "fstatat_dir", outer_assoc_dims, 1, fstatat_dir_arggen, NULL, false);
    // foreactor_AddSyscallGetdents(graph_id, 3, "getdents", dir_assoc_dims, 2, getdents_arggen, getdents_rcsave, 0, false);
    // foreactor_AddBranchNode(graph_id, 4, "more_dents", dir_assoc_dims, 2, branch_more_dents_arggen, 2, false);
    foreactor_AddBranchNode(graph_id, 5, "next_file", outer_assoc_dims, 1, branch_next_file_arggen, 2, false);

    foreactor_SyscallSetNext(graph_id, 0, 5, /*weak_edge*/ false);
    // foreactor_BranchAppendChild(graph_id, 1, 2, /*epoch_dim*/ -1);
    // foreactor_BranchAppendChild(graph_id, 1, 5, -1);
    // foreactor_SyscallSetNext(graph_id, 2, 6, false);
    // foreactor_SyscallSetNext(graph_id, 6, 3, false);
    // foreactor_SyscallSetNext(graph_id, 3, 4, false);
    // foreactor_BranchAppendChild(graph_id, 4, 3, 1);
    // foreactor_BranchAppendChild(graph_id, 4, 5, -1);
    foreactor_BranchAppendChild(graph_id, 5, 0, 0);
    foreactor_BranchAppendEndNode(graph_id, 5);

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
        return __real_du_files_my(files, bit_flags);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        // Must have FTS_NOSTAT set by du invocation.
        if ((bit_flags & FTS_NOSTAT) == 0) {
            fprintf(stderr, "Error: must have FTS_NOSTAT option set\n");
            exit(1);
        }

        foreactor_EnterSCGraph(graph_id);

        // Call the original function with corresponding SCGraph activated.
        bool ret = __real_du_files_my(files, bit_flags);

        foreactor_LeaveSCGraph(graph_id);
        curr_fts = NULL;
        curr_peek_ftsent = NULL;
        curr_in_peek_seg = false;
        curr_ftsent_cnt = 0;
        
        return ret;
    }
}

bool __wrap_process_file_my(FTS *fts, FTSENT *ent) {
    bool ret = __real_process_file_my(fts, ent);

    curr_ftsent_cnt++;
    if (curr_fts == NULL)
        curr_fts = fts;
    if (!curr_in_peek_seg)
        curr_peek_ftsent = ent->fts_link;

    return ret;
}
