#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <liburing.h>

#include "scg_nodes.hpp"
#include "value_pool.hpp"


#ifndef __FOREACTOR_SYSCALLS_H__
#define __FOREACTOR_SYSCALLS_H__


namespace foreactor {


// open
class SyscallOpen final : public SyscallNode {
    private:
        ValuePool<const char *> pathname;
        ValuePool<int> flags;
        ValuePool<mode_t> mode;

        // User-provided argument generator function. Returns true if all
        // arguments for that epoch ends up being ready; returns false
        // otherwise.
        std::function<bool(const int *,
                           const char **,
                           int *,
                           mode_t *)> arggen_func;

        // User-defined return code saver function.
        std::function<void(const int *, int)> rcsave_func;        

        long SyscallSync(const EpochList& epoch, void *output_buf);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void ReflectResult(const EpochList& epoch, void *output_buf);

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
                    std::function<bool(const int *,
                                       const char **,
                                       int *,
                                       mode_t *)> arggen_func,
                    std::function<void(const int *, int)> rcsave_func);
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


// close
class SyscallClose final : public SyscallNode {
    private:
        ValuePool<int> fd;

        std::function<bool(const int *, int *)> arggen_func;
        std::function<void(const int *, int)> rcsave_func;

        long SyscallSync(const EpochList& epoch, void *output_buf);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void ReflectResult(const EpochList& epoch, void *output_buf);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallClose() = delete;
        SyscallClose(unsigned node_id, std::string name, SCGraph *scgraph,
                     const std::unordered_set<int>& assoc_dims,
                     std::function<bool(const int *, int *)> arggen_func,
                     std::function<void(const int *, int)> rcsave_func);
        ~SyscallClose() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallClose& n);

        void CheckArgs(const EpochList& epoch, int fd_);
};


// pread
class SyscallPread final : public SyscallNode {
    private:
        ValuePool<int> fd;
        ValuePool<size_t> count;
        ValuePool<off_t> offset;

        // Used when issued async.
        ValuePool<char *> internal_buf;
        std::unordered_set<char *> pre_alloced_bufs;

        std::function<bool(const int *,
                           int *,
                           size_t *,
                           off_t *)> arggen_func;
        std::function<void(const int *, ssize_t)> rcsave_func;

        long SyscallSync(const EpochList& epoch, void *output_buf);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void ReflectResult(const EpochList& epoch, void *output_buf);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallPread() = delete;
        SyscallPread(unsigned node_id, std::string name, SCGraph *scgraph,
                     const std::unordered_set<int>& assoc_dims,
                     std::function<bool(const int *,
                                        int *,
                                        size_t *,
                                        off_t *)> arggen_func,
                     std::function<void(const int *, ssize_t)> rcsave_func,
                     size_t pre_alloc_buf_size);
        ~SyscallPread();

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallPread& n);

        void CheckArgs(const EpochList& epoch,
                       int fd_, size_t count_, off_t offset_);
};


// pwrite
class SyscallPwrite final : public SyscallNode {
    private:
        ValuePool<int> fd;
        ValuePool<const char *> buf;
        ValuePool<size_t> count;
        ValuePool<off_t> offset;

        std::function<bool(const int *,
                           int *,
                           const char **,
                           size_t *,
                           off_t *)> arggen_func;
        std::function<void(const int *, ssize_t)> rcsave_func;

        long SyscallSync(const EpochList& epoch, void *output_buf);
        void PrepUringSqe(int epoch_sum, struct io_uring_sqe *sqe);
        void ReflectResult(const EpochList& epoch, void *output_buf);

        bool GenerateArgs(const EpochList& epoch);
        void RemoveOneEpoch(const EpochList& epoch);
        void ResetValuePools();

    public:
        SyscallPwrite() = delete;
        SyscallPwrite(unsigned node_id, std::string name, SCGraph *scgraph,
                      const std::unordered_set<int>& assoc_dims,
                      std::function<bool(const int *,
                                         int *,
                                         const char **,
                                         size_t *,
                                         off_t *)> arggen_func,
                      std::function<void(const int *, ssize_t)> rcsave_func);
        ~SyscallPwrite() {}

        friend std::ostream& operator<<(std::ostream& s,
                                        const SyscallPwrite& n);

        void CheckArgs(const EpochList& epoch,
                       int fd_,
                       const void *buf_,
                       size_t count_,
                       off_t offset_);
};


}


#endif
