//
// Application/plugin only needs to include this header file, <foreactor.h>.
// 
// This header hides all C++ class-specific details, so that it is safe to
// be included by C applications.
//

//
// The foreactor library expects the following environment variables:
// 
//   LD_PRELOAD=/abs/path/to/libforeactor.so
//   
//     required for foreactor to hijack POSIX library calls
// 
//   USE_FOREACTOR=yes          (any other string means no)
//
//     if USE_FOREACTOR is not present in env, or if its value is not the
//     string yes, then foreactor will not be active; in this case, the
//     following two sets of env variables will have no effect
//   
//   DEPTH_{SCGRAPH_ID}=num     (>= 0)
//   
//     the pre-issuing depth of corresponding SCGraph; setting 0 means no
//     pre-issuing for this graph (i.e., wrapped app function); otherwise,
//     one of the following async I/O backend must be active
//   
//   QUEUE_{SCGRAPH_ID}=num     (> 0, >= DEPTH, <= 1024)
//   SQE_ASYNC_FLAG=yes         (any other string means no)
//   
//     if QUEUE_xxx env var is given for an SCGraph, it means to use the
//     io_uring backend for pre-issuing syscalls in this graph, and the
//     value specifies the queue length of this io_uring instance; the
//     SQE_ASYNC_FLAG env var controls whether to force IOSQE_ASYNC flag
//     in submissions
//   
//   UTHREADS_{SCGRAPH_ID}=num  (> 0, < # CPU cores)
//   
//     alternatively, one can use a user-level thread pool backend engine
//     for pre-issuing syscalls instead of io_uring, and the value specifies
//     the number of threads in the thread pool for this SCGraph; giving
//     this env var shadows the QUEUE_xxx env var for this graph
//


#include <fcntl.h>
#include <unistd.h>


#ifndef __FOREACTOR_H__
#define __FOREACTOR_H__


#ifdef __cplusplus
extern "C" {
#endif


//////////////////////////
// Interface to plugins //
//////////////////////////

// Check env var to see if using foreactor. The first call by a plugin into
// the library must be this function.
bool foreactor_UsingForeactor();


// Create a new SCGraph representing a hijacked app function.
void foreactor_CreateSCGraph(unsigned grpah_id, unsigned total_dims);
void foreactor_SetSCGraphBuilt(unsigned graph_id);
bool foreactor_HasSCGraph(unsigned graph_id);


// Add a SyscallNode of certain type to the SCGraph. Exactly one node in
// graph (SyscallNode or BranchNode) sets is_start = true.
void foreactor_AddSyscallOpen(unsigned graph_id,
                              unsigned node_id,
                              const char *name,
                              const int *assoc_dims,
                              size_t assoc_dims_len,
                              bool (*arggen_func)(const int *,
                                                  const char **,
                                                  int *,
                                                  mode_t *),
                              void (*rcsave_func)(const int *, int),
                              bool is_start);

void foreactor_AddSyscallClose(unsigned graph_id,
                               unsigned node_id,
                               const char *name,
                               const int *assoc_dims,
                               size_t assoc_dims_len,
                               bool (*arggen_func)(const int *, int *),
                               void (*rcsave_func)(const int *, int),
                               bool is_start);

void foreactor_AddSyscallPread(unsigned graph_id,
                               unsigned node_id,
                               const char *name,
                               const int *assoc_dims,
                               size_t assoc_dims_len,
                               bool (*arggen_func)(const int *,
                                                   int *,
                                                   char **,
                                                   size_t *,
                                                   off_t *,
                                                   bool *),
                               void (*rcsave_func)(const int *, ssize_t),
                               size_t pre_alloc_buf_size,
                               bool is_start);

void foreactor_AddSyscallPwrite(unsigned graph_id,
                                unsigned node_id,
                                const char *name,
                                const int *assoc_dims,
                                size_t assoc_dims_len,
                                bool (*arggen_func)(const int *,
                                                    int *,
                                                    const char **,
                                                    size_t *,
                                                    off_t *),
                                void (*rcsave_func)(const int *, ssize_t),
                                bool is_start);

// Set outgoing edge of a SyscallNode.
void foreactor_SyscallSetNext(unsigned graph_id, unsigned node_id,
                              unsigned next_id, bool weak_edge);


// Add a BranchNode to the SCGraph.
void foreactor_AddBranchNode(unsigned graph_id,
                             unsigned node_id,
                             const char *name,
                             const int *assoc_dims,
                             size_t assoc_dims_len,
                             bool (*arggen_func)(const int *, int *),
                             size_t num_children,
                             bool is_start);

// Append a outgoing edge to a BranchNode (could be a back-pointing edge).
void foreactor_BranchAppendChild(unsigned graph_id, unsigned node_id,
                                 unsigned child_id, int epoch_dim);
void foreactor_BranchAppendEndNode(unsigned graph_id, unsigned node_id);


// Called upon entering/leaving a hijacked app function.
void foreactor_EnterSCGraph(unsigned graph_id);
void foreactor_LeaveSCGraph(unsigned graph_id);


// Visualization helper.
void foreactor_DumpDotImg(unsigned graph_id, const char *filestem);


/////////////////////////////
// POSIX library hijacking //
/////////////////////////////

int open(const char *pathname, int flags, ...);
int close(int fd);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);


#ifdef __cplusplus
}
#endif


#endif
