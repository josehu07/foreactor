#include <vector>
#include <liburing.h>

#include "depgraph.h"
#include "io_uring.h"


namespace foreactor {


long SyscallNode::CallSync() {
    return SyscallSync();
}

void SyscallNode::PrepAsync() {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    assert(sqe != nullptr);
    // SQE data is the pointer to the SyscallNode instance
    io_uring_sqe_set_data(sqe, this);
    // io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
    PrepUring(sqe);
}


long SyscallNode::Issue() {
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


void DepGraphEnter(std::vector<SyscallNode *>& syscalls, int pre_issue_depth,
                   IOUring& ring) {
    for (size_t i = 0; i < syscalls.size(); ++i) {
        syscalls[i]->ring = ring.Ring();
        syscalls[i]->pre_issue_depth = pre_issue_depth;
        syscalls[i]->stage = STAGE_UNISSUED;
        if (i > 0)
            syscalls[i]->pred = syscalls[i - 1];
        if (i < syscalls.size() - 1)
            syscalls[i]->succ = syscalls[i + 1];
    }
}

void DepGraphLeave() {
    return;
}


}
