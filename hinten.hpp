#include <vector>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "liburing.h"


// TODO: argument dependency & state-changing calls
// TODO: more complex branching graph
// TODO: request linking
// TODO: fixed buffer
// TODO: SQPOLL option


class Intention;
class SyscallNode;


typedef enum SyscallStage {
    STAGE_UNISSUED,
    STAGE_ISSUED,
    STAGE_FINISHED
} SyscallStage;

class SyscallNode {
  friend Intention;

  protected:
    SyscallNode *pred = nullptr;
    SyscallNode *succ = nullptr;

    struct io_uring * ring;
    int pre_issue_depth;

    SyscallStage stage = STAGE_UNISSUED;
    long rc = -1;

    virtual long SyscallSync() = 0;
    virtual void PrepUring(struct io_uring_sqe *sqe) = 0;
    virtual void ReflectResult() = 0;

    long CallSync() {
        return SyscallSync();
    }

    void PrepAsync() {
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
        assert(sqe != nullptr);
        // SQE data is the pointer to the SyscallNode instance
        io_uring_sqe_set_data(sqe, this);
        // io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
        PrepUring(sqe);
    }

  public:
    long Issue() {
        // pre-issue the next few syscalls asynchronously
        SyscallNode *node = succ;
        int depth = pre_issue_depth;
        int num_prepared = 0;
        while (depth-- > 0 && node != nullptr) {
            if (node->stage == STAGE_UNISSUED) {
                node->PrepAsync();
                node->stage = STAGE_ISSUED;
                num_prepared++;
            }
            node = node->succ;
        }
        if (num_prepared > 0) {
            int num_submitted = io_uring_submit(ring);
            assert(num_submitted == num_prepared);
        }

        // handle myself
        if (stage == STAGE_UNISSUED) {
            // if not pre-issued
            rc = CallSync();
            stage = STAGE_FINISHED;
        } else {
            // if has been pre-issued
            if (stage == STAGE_ISSUED) {
                // if result not harvested yet, process CQEs until mine
                // is seen
                struct io_uring_cqe *cqe;
                while (true) {
                    int ret = io_uring_wait_cqe(ring, &cqe);
                    assert(ret == 0);
                    SyscallNode *node = reinterpret_cast<SyscallNode *>(
                        io_uring_cqe_get_data(cqe));
                    node->rc = cqe->res;
                    node->stage = STAGE_FINISHED;
                    io_uring_cqe_seen(ring, cqe);
                    if (node == this)
                        break;
                }
            }
            ReflectResult();
        }

        assert(stage == STAGE_FINISHED);
        return rc;
    }
};

class SyscallPread : public SyscallNode {
  private:
    const int fd;
    char * const buf;
    const size_t count;
    const off_t offset;

    // used when issued async
    char * internal_buf = nullptr;

    long SyscallSync() {
        return pread(fd, buf, count, offset);
    }

    void PrepUring(struct io_uring_sqe *sqe) {
        io_uring_prep_read(sqe, fd, buf, count, offset);
    }

    void ReflectResult() {
        memcpy(buf, internal_buf, count);
    }

  public:
    SyscallPread(int fd, char *buf, size_t count, off_t offset)
            : fd(fd), buf(buf), count(count), offset(offset) {
        internal_buf = new char[count];
    }

    ~SyscallPread() {
        delete[] internal_buf;
    }
};


class Intention {
  friend SyscallNode;

  protected:
    std::vector<SyscallNode *>& syscalls;
    struct io_uring ring;

  public:
    Intention(std::vector<SyscallNode *>& syscalls, int pre_issue_depth)
            : syscalls(syscalls) {
        int ret = io_uring_queue_init(pre_issue_depth, &ring, /*flags*/ 0);
        assert(ret == 0);

        for (size_t i = 0; i < syscalls.size(); ++i) {
            syscalls[i]->ring = &ring;
            syscalls[i]->pre_issue_depth = pre_issue_depth;
            syscalls[i]->stage = STAGE_UNISSUED;
            if (i > 0)
                syscalls[i]->pred = syscalls[i - 1];
            if (i < syscalls.size() - 1)
                syscalls[i]->succ = syscalls[i + 1];
        }
    }

    ~Intention() {
        io_uring_queue_exit(&ring);
    }
};
