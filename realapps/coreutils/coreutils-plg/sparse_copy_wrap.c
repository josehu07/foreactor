#include <config.h>

#include <stdio.h>
#include <stdint.h>
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
static int curr_src_fd = -1;
static int curr_dst_fd = -1;
static ssize_t curr_buf_size = -1;
static ssize_t curr_src_file_size = -1;

static bool read_src_arggen(const int *epoch, bool *link, int *fd, char **buf, size_t *count, off_t *offset,
                            bool *buf_ready, bool *skip_memcpy) {
    off_t aligned_offset = ((off_t) curr_buf_size) * epoch[0];
    *link = (aligned_offset < curr_src_file_size) ? true : false;   // don't LINK the last read of size 0
    *fd = curr_src_fd;
    *buf = NULL;
    // Note that this is a slightly different logic than the original app
    // copy loop. cp used a while loop and stops until a read returns 0 size,
    // while this plugin fetches the file size from fstat_src and explicitly
    // controls each pread.
    *count = curr_buf_size;
    *offset = aligned_offset;
    *buf_ready = false;
    *skip_memcpy = true;
    return true;
}

static void read_src_rcsave(const int *epoch, ssize_t res) {
    assert(res >= 0);
}

static bool branch_stop_loop_arggen(const int *epoch, bool catching_up, int *decision) {
    off_t aligned_offset = ((off_t) curr_buf_size) * epoch[0];
    if (aligned_offset >= curr_src_file_size)
        *decision = 0;
    else
        *decision = 1;
    return true;
}

static bool write_dst_arggen(const int *epoch, bool *link, int *fd, const char **buf, size_t *count, off_t *offset) {
    char *ib = foreactor_PreadRefInternalBuf(graph_id, 0, epoch);
    if (ib == NULL)
        return false;
    *fd = curr_dst_fd;
    *buf = ib;
    off_t aligned_offset = ((off_t) curr_buf_size) * epoch[0];
    *count = curr_buf_size;
    *offset = aligned_offset;
    return true;
}

static void write_dst_rcsave(const int *epoch, ssize_t res) {
    assert(res > 0);
    foreactor_PreadPutInternalBuf(graph_id, 0, epoch);
}

static bool branch_continue_arggen(const int *epoch, bool catching_up, int *decision) {
    *decision = 0;
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int rw_assoc_dims[1] = {0};

    foreactor_AddSyscallPread(graph_id, 0, "read_src", rw_assoc_dims, 1, read_src_arggen, read_src_rcsave, IO_BUFSIZE, /*is_start*/ true);
    foreactor_AddBranchNode(graph_id, 1, "stop_loop", rw_assoc_dims, 1, branch_stop_loop_arggen, 2, false);
    foreactor_AddSyscallPwrite(graph_id, 2, "write_dst", rw_assoc_dims, 1, write_dst_arggen, write_dst_rcsave, false);
    foreactor_AddBranchNode(graph_id, 3, "continue", rw_assoc_dims, 1, branch_continue_arggen, 1, false);

    foreactor_SyscallSetNext(graph_id, 0, 1, /*weak_edge*/ false);
    foreactor_BranchAppendEndNode(graph_id, 1);
    foreactor_BranchAppendChild(graph_id, 1, 2, -1);
    foreactor_SyscallSetNext(graph_id, 2, 3, false);
    foreactor_BranchAppendChild(graph_id, 3, 0, 0);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "sparse_copy");
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
bool __real_sparse_copy_my(
        int src_fd, int dest_fd, char **abuf, size_t buf_size,
        size_t hole_size, bool punch_holes, bool allow_reflink,
        char const *src_name, char const *dst_name,
        uintmax_t max_n_read, off_t *total_n_read,
        bool *last_write_made_hole);
void __real_inform_src_file_size(size_t src_file_size);

bool __wrap_sparse_copy_my(
        int src_fd, int dest_fd, char **abuf, size_t buf_size,
        size_t hole_size, bool punch_holes, bool allow_reflink,
        char const *src_name, char const *dst_name,
        uintmax_t max_n_read, off_t *total_n_read,
        bool *last_write_made_hole) {
    if (!foreactor_UsingForeactor()) {
        // Call the original function.
        return __real_sparse_copy_my(src_fd, dest_fd, abuf, buf_size,
                                     hole_size, punch_holes, allow_reflink,
                                     src_name, dst_name, max_n_read,
                                     total_n_read, last_write_made_hole);
    } else {
        // Build SCGraph once if haven't done yet.
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        // Currently only supports the dense file read-write loop branch.
        if (hole_size != 0 || punch_holes || allow_reflink) {
            fprintf(stderr, "Error: plugin currently only supports the dense"
                            " file rw loop branch\n");
            exit(1);
        }
        if (curr_src_file_size < 0) {
            fprintf(stderr, "Error: src_file_size not informed\n");
            exit(1);
        }

        foreactor_EnterSCGraph(graph_id);
        curr_src_fd = src_fd;
        curr_dst_fd = dest_fd;
        curr_buf_size = (ssize_t) buf_size;

        // Call the original function with corresponding SCGraph activated.
        bool ret = __real_sparse_copy_my(src_fd, dest_fd, abuf, buf_size,
                                         hole_size, punch_holes, allow_reflink,
                                         src_name, dst_name, max_n_read,
                                         total_n_read, last_write_made_hole);

        foreactor_LeaveSCGraph(graph_id);
        curr_src_fd = -1;
        curr_dst_fd = -1;
        curr_buf_size = -1;
        curr_src_file_size = -1;
        
        return ret;
    }
}

void __wrap_inform_src_file_size(size_t src_file_size) {
    curr_src_file_size = src_file_size;
}
