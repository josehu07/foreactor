#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <liburing.h>

#include "scg_nodes.hpp"
#include "value_pool.hpp"


#pragma once


namespace foreactor {


// Concrete syscall types of SyscallNode.
// Note that these are not strictly syscalls but rather POSIX glibc functions.
typedef enum SyscallType : unsigned {
    SC_BASE,
    SC_OPEN,    // open
    SC_OPENAT,  // openat
    SC_CLOSE,   // close
    SC_PREAD,   // pread
    SC_PWRITE,  // pwrite
    SC_FSTAT,   // fstat
    SC_FSTATAT, // fstatat
} SyscallType;


struct ThreadPoolSQEntry;   // forward declaration


// open
// TODO: could have merged with openat into a single type
class SyscallOpen final : public SyscallNode {
    typedef std::function<bool(const int *,
                          const char **,
                          int *,
                          mode_t *)> ArggenFunc;
    typedef std::function<void(const int *, int)> RcsaveFunc;

    private:
        ValuePool<const char *> pathname;
        ValuePool<int> flags;
        ValuePool<mode_t> mode;

        // User-provided argument generator function. Returns true if all
        // arguments for that epoch ends up being ready; returns false
        // otherwise.
        ArggenFunc arggen_func;

        // User-defined return code saver function.
        RcsaveFunc rcsave_func;        

        long SyscallSync(const EpochList& epoch);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe);
        void ReflectResult(const EpochList& epoch);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallOpen() = delete;
        // Arguments could be not ready at the point of constructing the
        // SCGraph, in which case those they must be set before calling
        // Issue on this syscall node.
        SyscallOpen(unsigned node_id, std::string name, SCGraph *scgraph,
                    const std::unordered_set<int>& assoc_dims,
                    ArggenFunc arggen_func, RcsaveFunc rcsave_func);
        ~SyscallOpen() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallOpen& n);

        // For validating argument values at the time of syscall hijacking,
        // to verify that the arguments recorded (and probably used in async
        // submissions) have the same value as fed by the invocation.
        // For syscalls that have non-ready arguments until the timepoint of
        // invocation, this function will install their values.
        void CheckArgs(const EpochList& epoch,
                       const char *pathname_, int flags_, mode_t mode_);
};


// openat
class SyscallOpenat final : public SyscallNode {
    typedef std::function<bool(const int *,
                          int *,
                          const char **,
                          int *,
                          mode_t *)> ArggenFunc;
    typedef std::function<void(const int *, int)> RcsaveFunc;

    private:
        ValuePool<int> dirfd;
        ValuePool<const char *> pathname;
        ValuePool<int> flags;
        ValuePool<mode_t> mode;

        ArggenFunc arggen_func;
        RcsaveFunc rcsave_func;

        long SyscallSync(const EpochList& epoch);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe);
        void ReflectResult(const EpochList& epoch);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallOpenat() = delete;
        // Arguments could be not ready at the point of constructing the
        // SCGraph, in which case those they must be set before calling
        // Issue on this syscall node.
        SyscallOpenat(unsigned node_id, std::string name, SCGraph *scgraph,
                      const std::unordered_set<int>& assoc_dims,
                      ArggenFunc arggen_func, RcsaveFunc rcsave_func);
        ~SyscallOpenat() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallOpenat& n);

        // For validating argument values at the time of syscall hijacking,
        // to verify that the arguments recorded (and probably used in async
        // submissions) have the same value as fed by the invocation.
        // For syscalls that have non-ready arguments until the timepoint of
        // invocation, this function will install their values.
        void CheckArgs(const EpochList& epoch,
                       int dirfd_,
                       const char *pathname_,
                       int flags_,
                       mode_t mode_);
};


// close
class SyscallClose final : public SyscallNode {
    typedef std::function<bool(const int *, int *)> ArggenFunc;
    typedef std::function<void(const int *, int)> RcsaveFunc;

    private:
        ValuePool<int> fd;

        ArggenFunc arggen_func;
        RcsaveFunc rcsave_func;

        long SyscallSync(const EpochList& epoch);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe);
        void ReflectResult(const EpochList& epoch);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallClose() = delete;
        SyscallClose(unsigned node_id, std::string name, SCGraph *scgraph,
                     const std::unordered_set<int>& assoc_dims,
                     ArggenFunc arggen_func, RcsaveFunc rcsave_func);
        ~SyscallClose() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallClose& n);

        void CheckArgs(const EpochList& epoch, int fd_);
};


// pread
class SyscallPread final : public SyscallNode {
    typedef std::function<bool(const int *,
                          int *,
                          char **,
                          size_t *,
                          off_t *,
                          bool *)> ArggenFunc;
    typedef std::function<void(const int *, ssize_t)> RcsaveFunc;

    private:
        ValuePool<int> fd;
        ValuePool<char *> buf;
        ValuePool<size_t> count;
        ValuePool<off_t> offset;

        // Used when issued async.
        ValuePool<char *> internal_buf;
        std::unordered_set<char *> pre_alloced_bufs;

        ArggenFunc arggen_func;
        RcsaveFunc rcsave_func;

        long SyscallSync(const EpochList& epoch);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe);
        void ReflectResult(const EpochList& epoch);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallPread() = delete;
        SyscallPread(unsigned node_id, std::string name, SCGraph *scgraph,
                     const std::unordered_set<int>& assoc_dims,
                     ArggenFunc arggen_func, RcsaveFunc rcsave_func,
                     size_t pre_alloc_buf_size);
        ~SyscallPread();

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallPread& n);

        void CheckArgs(const EpochList& epoch,
                       int fd_,
                       void *buf_,
                       size_t count_,
                       off_t offset_);
};


// pwrite
class SyscallPwrite final : public SyscallNode {
    typedef std::function<bool(const int *,
                          int *,
                          const char **,
                          size_t *,
                          off_t *)> ArggenFunc;
    typedef std::function<void(const int *, ssize_t)> RcsaveFunc;

    private:
        ValuePool<int> fd;
        ValuePool<const char *> buf;
        ValuePool<size_t> count;
        ValuePool<off_t> offset;

        ArggenFunc arggen_func;
        RcsaveFunc rcsave_func;

        long SyscallSync(const EpochList& epoch);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe);
        void ReflectResult(const EpochList& epoch);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallPwrite() = delete;
        SyscallPwrite(unsigned node_id, std::string name, SCGraph *scgraph,
                      const std::unordered_set<int>& assoc_dims,
                      ArggenFunc arggen_func, RcsaveFunc rcsave_func);
        ~SyscallPwrite() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallPwrite& n);

        void CheckArgs(const EpochList& epoch,
                       int fd_,
                       const void *buf_,
                       size_t count_,
                       off_t offset_);
};


// fstat
// TODO: could have merged with fstatat into a single type
class SyscallFstat final : public SyscallNode {
    typedef std::function<bool(const int *, int *, struct stat **)> ArggenFunc;
    typedef std::function<void(const int *, int)> RcsaveFunc;

    private:
        ValuePool<int> fd;
        ValuePool<struct stat *> buf;

        ValuePool<struct statx *> statx_buf;
        std::unordered_set<struct statx *> empty_statx_bufs;

        ArggenFunc arggen_func;
        RcsaveFunc rcsave_func;

        long SyscallSync(const EpochList& epoch);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe);
        void ReflectResult(const EpochList& epoch);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallFstat() = delete;
        SyscallFstat(unsigned node_id, std::string name, SCGraph *scgraph,
                     const std::unordered_set<int>& assoc_dims,
                     ArggenFunc arggen_func, RcsaveFunc rcsave_func);
        ~SyscallFstat();

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallFstat& n);

        void CheckArgs(const EpochList& epoch, int fd_, struct stat *buf_);
};


// fstatat
class SyscallFstatat final : public SyscallNode {
    typedef std::function<bool(const int *,
                               int *,
                               const char **,
                               struct stat **,
                               int *)> ArggenFunc;
    typedef std::function<void(const int *, int)> RcsaveFunc;

    private:
        ValuePool<int> dirfd;
        ValuePool<const char *> pathname;
        ValuePool<struct stat *> buf;
        ValuePool<int> flags;

        ValuePool<struct statx *> statx_buf;
        std::unordered_set<struct statx *> empty_statx_bufs;

        ArggenFunc arggen_func;
        RcsaveFunc rcsave_func;

        long SyscallSync(const EpochList& epoch);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void PrepUpoolSqe(int epoch_sum, ThreadPoolSQEntry *sqe);
        void ReflectResult(const EpochList& epoch);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallFstatat() = delete;
        SyscallFstatat(unsigned node_id, std::string name, SCGraph *scgraph,
                       const std::unordered_set<int>& assoc_dims,
                       ArggenFunc arggen_func, RcsaveFunc rcsave_func);
        ~SyscallFstatat();

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallFstatat& n);

        void CheckArgs(const EpochList& epoch,
                       int dirfd_,
                       const char *pathname_,
                       struct stat *buf_,
                       int flags_);
};


}
