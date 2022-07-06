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
//   USE_FOREACTOR=yes               (any other string means no)
//
//     if USE_FOREACTOR is not present in env, or if its value is not the
//     string yes, then foreactor will not be active; in this case, the
//     following two sets of env variables will have no effect
//   
//   DEPTH_{SCGRAPH_ID}=num          (>= 0)
//   
//     the pre-issuing depth of corresponding SCGraph; setting 0 means no
//     pre-issuing for this graph (i.e., wrapped app function); otherwise,
//     one of the following async I/O backend must be active
//   
//   QUEUE_{SCGRAPH_ID}=num          (> 0, >= DEPTH, <= 1024)
//   SQE_ASYNC_FLAG_{SCGRAPH_ID}=yes (any other string means no)
//   
//     if QUEUE_xxx env var is given for an SCGraph, it means to use the
//     io_uring backend for pre-issuing syscalls in this graph, and the
//     value specifies the queue length of this io_uring instance; the
//     SQE_ASYNC_FLAG env var controls whether to force IOSQE_ASYNC flag
//     in submissions
//   
//   UTHREADS_{SCGRAPH_ID}=num       (> 0, < # CPU cores)
//   
//     alternatively, one can use a user-level thread pool backend engine
//     for pre-issuing syscalls instead of io_uring, and the value specifies
//     the number of threads in the thread pool for this SCGraph; giving
//     this env var shadows the QUEUE_xxx env var for this graph
//


#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


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
// 
// Note that there's no SyscallRead and SyscallWrite node. Use SyscallPread
// and SyscallPwrite explicitly, and it's the plugin's job to track their
// offset ordering dependencies.
void foreactor_AddSyscallOpen(unsigned graph_id,
                              unsigned node_id,
                              const char *name,
                              const int *assoc_dims,
                              size_t assoc_dims_len,
                              bool (*arggen_func)(const int *,   // epoch array
                                                  const char **, // pathname
                                                  int *,         // flags
                                                  mode_t *),     // mode
                              void (*rcsave_func)(const int *, int),
                              bool is_start);

void foreactor_AddSyscallOpenat(unsigned graph_id,
                                unsigned node_id,
                                const char *name,
                                const int *assoc_dims,
                                size_t assoc_dims_len,
                                bool (*arggen_func)(const int *, // similar...
                                                    int *,
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
                                                   bool *),  // buf_ready?
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

void foreactor_AddSyscallLseek(unsigned graph_id,
                               unsigned node_id,
                               const char *name,
                               const int *assoc_dims,
                               size_t assoc_dims_len,
                               // not used, lseek is never offloaded async
                               bool (*arggen_func)(const int *,
                                                   int *,
                                                   off_t *,
                                                   int *),
                               void (*rcsave_func)(const int *, off_t),
                               bool is_start);

void foreactor_AddSyscallFstat(unsigned graph_id,
                               unsigned node_id,
                               const char *name,
                               const int *assoc_dims,
                               size_t assoc_dims_len,
                               bool (*arggen_func)(const int *,
                                                   int *,
                                                   struct stat **,
                                                   bool *),
                               void (*rcsave_func)(const int *, int),
                               bool is_start);

void foreactor_AddSyscallFstatat(unsigned graph_id,
                                 unsigned node_id,
                                 const char *name,
                                 const int *assoc_dims,
                                 size_t assoc_dims_len,
                                 bool (*arggen_func)(const int *,
                                                     int *,
                                                     const char **,
                                                     struct stat **,
                                                     int *,
                                                     bool *),
                                 void (*rcsave_func)(const int *, int),
                                 bool is_start);


// Set outgoing edge of a SyscallNode. For a SyscallNode, not setting
// its next node means its next is the end of SCGraph.
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


// Append an outgoing edge to a BranchNode (could be a back-pointing edge,
// in which case the `epoch_dim` arg should be the index of its corresponding
// epoch number; otherwise `epoch_dim` should be -1).
void foreactor_BranchAppendChild(unsigned graph_id, unsigned node_id,
                                 unsigned child_id, int epoch_dim);
void foreactor_BranchAppendEndNode(unsigned graph_id, unsigned node_id);


// Special syscall-type-specific helper functions.
struct stat *foreactor_FstatGetResultBuf(unsigned graph_id, unsigned node_id,
                                         const int *epoch_);
struct stat *foreactor_FstatatGetResultBuf(unsigned graph_id, unsigned node_id,
                                           const int *epoch_);


// Called upon entering/leaving a hijacked app function.
void foreactor_EnterSCGraph(unsigned graph_id);
void foreactor_LeaveSCGraph(unsigned graph_id);


// Visualization helper.
void foreactor_DumpDotImg(unsigned graph_id, const char *filestem);


#ifdef __cplusplus
}
#endif


#endif
