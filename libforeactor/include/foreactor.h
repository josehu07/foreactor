//
// Application/plugin only needs to include this header file, <foreactor.h>.
// 
// This header hides all C++ class-specific details, so that it is safe to
// be included by C applications.
//
//
// The foreactor library expects the following environment variables:
// 
//   USE_FOREACTOR=yes       (any other string means no)
//
//   if USE_FOREACTOR is not present in env, or if its value is not the
//   string yes, then foreactor will not be active; in this case, the
//   following two sets of env variables will have no effect
//   
//   QUEUE_{SCGRAPH_ID}=num  (> 0, >= PRE_ISSUE_DEPTH, <= 1024)
//   DEPTH_{SCGRAPH_ID}=num  (>= 0, <= URING_QUEUE_LEN)
//   
//   if USE_FOREACTOR is present and has the string value yes, then these
//   two groups of env variables must be present for each wrapped function
//   (i.e., SCGraph type) involved in the application; for example, giving
//   QUEUE_0=32 and DEPTH_0=8 says that the IOUring queue length for SCGraph
//   type 0 is 32 and the pre-issuing depth is set to be 8
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
                                                   size_t *,
                                                   off_t *),
                               void (*rcsave_func)(const int *, ssize_t),
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
