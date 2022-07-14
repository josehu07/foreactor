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
static int curr_fstatat_src_res = -2;
static int curr_fstatat_dst_res = -2;
static int curr_fstatat_dst_extra_res = -2;
static int curr_src_fd = -1;
static bool curr_src_fstat_done = false;
static ssize_t curr_src_file_size = -1;
static int curr_dst_fd = -1;
static bool curr_dst_fstat_done = false;

static bool fstatat_src_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, struct stat **buf, int *flags,
                               bool *buf_ready) {
    return false;
}

static void fstatat_src_rcsave(const int *epoch, int res) {
    curr_fstatat_src_res = res;
}

static bool branch_fstatat_dst_arggen(const int *epoch, int *decision) {
    if (curr_fstatat_src_res < -1)
        return false;
    if (curr_nonexistent_dst == 0)
        *decision = 0;
    else
        *decision = 1;
    return true;
}

static bool fstatat_dst_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, struct stat **buf, int *flags,
                               bool *buf_ready) {
    return false;
}

static void fstatat_dst_rcsave(const int *epoch, int res) {
    curr_fstatat_dst_res = res;
}

static bool branch_fstatat_dst_extra_arggen(const int *epoch, int *decision) {
    if (curr_fstatat_dst_res < -1)
        return false;
    if (curr_x->dest_info != NULL)
        *decision = 0;
    else
        *decision = 1;
    return true;
}

static bool fstatat_dst_extra_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, struct stat **buf, int *flags,
                                     bool *buf_ready) {
    return false;
}

static void fstatat_dst_extra_rcsave(const int *epoch, int res) {
    curr_fstatat_dst_extra_res = res;
}

static bool open_src_arggen(const int *epoch, bool *link, const char **pathname, int *flags, mode_t *mode) {
    return false;
}

static void open_src_rcsave(const int *epoch, int fd) {
    curr_src_fd = fd;
}

static bool fstat_src_arggen(const int *epoch, bool *link, int *fd, struct stat **buf,
                             bool *buf_ready) {
    return false;
}

static void fstat_src_rcsave(const int *epoch, int res) {
    curr_src_fstat_done = true;
    curr_src_file_size = foreactor_FstatGetResultBuf(graph_id, 6, epoch)->st_size;
}

static bool openat_dst_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, int *flags, mode_t *mode) {
    return false;
}

static void openat_dst_rcsave(const int *epoch, int fd) {
    curr_dst_fd = fd;
}

static bool fstat_dst_arggen(const int *epoch, bool *link, int *fd, struct stat **buf, bool *buf_ready) {
    return false;
}

static void fstat_dst_rcsave(const int *epoch, int res) {
    curr_dst_fstat_done = true;
}

static bool read_src_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                            bool *buf_ready, bool *skip_memcpy) {
    if (!curr_dst_fstat_done)
        return false;
    off_t aligned_offset = ((off_t) IO_BUFSIZE) * epoch[0];
    if (aligned_offset >= curr_src_file_size)
        return false;   // don't pre-issue the last read since it will have LINK flag set and thus never get submitted
    *link = true;
    *fd = curr_src_fd;
    *buf = NULL;
    // Note that this is a slightly different logic than the original app
    // copy loop. cp used a while loop and stops until a read returns 0 size,
    // while this plugin fetches the file size from fstat_src and explicitly
    // controls each pread.
    *count = IO_BUFSIZE;
    *offset = aligned_offset;
    *buf_ready = false;
    *skip_memcpy = true;
    return true;
}

static void read_src_rcsave(const int *epoch, ssize_t res) {
    assert(res >= 0);
}

static bool branch_stop_loop_arggen(const int *epoch, int *decision) {
    if (!curr_src_fstat_done)
        return false;
    off_t aligned_offset = ((off_t) IO_BUFSIZE) * epoch[0];
    if (aligned_offset >= curr_src_file_size)
        *decision = 0;
    else
        *decision = 1;
    return true;
}

static bool write_dst_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    char *ib = foreactor_PreadRefInternalBuf(graph_id, 9, epoch);
    if (ib == NULL)
        return false;
    *fd = curr_dst_fd;
    *buf = ib;
    off_t aligned_offset = ((off_t) IO_BUFSIZE) * epoch[0];
    *count = IO_BUFSIZE;
    *offset = aligned_offset;
    return true;
}

static void write_dst_rcsave(const int *epoch, ssize_t res) {
    assert(res > 0);
    foreactor_PreadPutInternalBuf(graph_id, 9, epoch);
}

static bool branch_all_written_arggen(const int *epoch, int *decision) {
    *decision = 0;
    return true;
}

static bool close_dst_arggen(const int *epoch, bool *link, int *fd) {
    return false;
}

static bool close_src_arggen(const int *epoch, bool *link, int *fd) {
    return false;
}

static bool fstatat_dst_end_arggen(const int *epoch, bool *link, int *dirfd, const char **pathname, struct stat **buf, int *flags,
                                   bool *buf_ready) {
    return false;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int rw_assoc_dims[1] = {0};

    foreactor_AddSyscallFstatat(graph_id, 0, "fstatat_src", NULL, 0, fstatat_src_arggen, fstatat_src_rcsave, /*is_start*/ true);
    foreactor_AddBranchNode(graph_id, 1, "do_fstatat_dst", NULL, 0, branch_fstatat_dst_arggen, 2, false);
    foreactor_AddSyscallFstatat(graph_id, 2, "fstatat_dst", NULL, 0, fstatat_dst_arggen, fstatat_dst_rcsave, false);
    foreactor_AddBranchNode(graph_id, 3, "do_fstatat_dst_extra", NULL, 0, branch_fstatat_dst_extra_arggen, 2, false);
    foreactor_AddSyscallFstatat(graph_id, 4, "fstatat_dst_extra", NULL, 0, fstatat_dst_extra_arggen, fstatat_dst_extra_rcsave, false);
    foreactor_AddSyscallOpen(graph_id, 5, "open_src", NULL, 0, open_src_arggen, open_src_rcsave, false);
    foreactor_AddSyscallFstat(graph_id, 6, "fstat_src", NULL, 0, fstat_src_arggen, fstat_src_rcsave, false);
    foreactor_AddSyscallOpenat(graph_id, 7, "openat_dst", NULL, 0, openat_dst_arggen, openat_dst_rcsave, false);
    foreactor_AddSyscallFstat(graph_id, 8, "fstat_dst", NULL, 0, fstat_dst_arggen, fstat_dst_rcsave, false);
    foreactor_AddSyscallPread(graph_id, 9, "read_src", rw_assoc_dims, 1, read_src_arggen, read_src_rcsave, IO_BUFSIZE, false);
    foreactor_AddBranchNode(graph_id, 10, "stop_loop", rw_assoc_dims, 1, branch_stop_loop_arggen, 2, false);
    foreactor_AddSyscallPwrite(graph_id, 11, "write_dst", rw_assoc_dims, 1, write_dst_arggen, write_dst_rcsave, false);
    foreactor_AddBranchNode(graph_id, 12, "continue", rw_assoc_dims, 1, branch_all_written_arggen, 1, false);
    foreactor_AddSyscallClose(graph_id, 13, "close_dst", NULL, 0, close_dst_arggen, NULL, false);
    foreactor_AddSyscallClose(graph_id, 14, "close_src", NULL, 0, close_src_arggen, NULL, false);
    foreactor_AddSyscallFstatat(graph_id, 15, "fstatat_dst_end", NULL, 0, fstatat_dst_end_arggen, NULL, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ true);
    foreactor_BranchAppendChild(graph_id, 1, 2, /*epoch_dim*/ -1);
    foreactor_BranchAppendChild(graph_id, 1, 5, -1);
    foreactor_SyscallSetNext(graph_id, 2, 3, false);
    foreactor_BranchAppendChild(graph_id, 3, 4, -1);
    foreactor_BranchAppendChild(graph_id, 3, 5, -1);
    foreactor_SyscallSetNext(graph_id, 4, 5, false);
    foreactor_SyscallSetNext(graph_id, 5, 6, false);
    foreactor_SyscallSetNext(graph_id, 6, 7, false);
    foreactor_SyscallSetNext(graph_id, 7, 8, false);
    foreactor_SyscallSetNext(graph_id, 8, 9, false);
    foreactor_SyscallSetNext(graph_id, 9, 10, false);
    foreactor_BranchAppendChild(graph_id, 10, 13, -1);
    foreactor_BranchAppendChild(graph_id, 10, 11, -1);
    foreactor_SyscallSetNext(graph_id, 11, 12, false);
    foreactor_BranchAppendChild(graph_id, 12, 9, 0);
    foreactor_SyscallSetNext(graph_id, 13, 14, false);
    foreactor_SyscallSetNext(graph_id, 14, 15, true);

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
        curr_fstatat_src_res = -2;
        curr_fstatat_dst_res = -2;
        curr_fstatat_dst_extra_res = -2;
        curr_src_fd = -1;
        curr_src_fstat_done = false;
        curr_src_file_size = -1;
        curr_dst_fd = -1;
        curr_dst_fstat_done = false;
        
        return ret;
    }
}
