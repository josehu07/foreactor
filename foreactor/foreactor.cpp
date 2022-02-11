#include <vector>
#include <liburing.h>

#include "scg_nodes.hpp"
#include "scg_graph.hpp"


namespace foreactor {


long SyscallNode::CallSync() {
    return SyscallSync();
}

void SyscallNode::PrepAsync() {
    assert(scgraph != nullptr);
    struct io_uring_sqe *sqe = io_uring_get_sqe(scgraph->Ring());
    assert(sqe != nullptr);
    // SQE data is the pointer to the SyscallNode instance
    io_uring_sqe_set_data(sqe, this);
    // io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
    PrepUring(sqe);
}


long SyscallNode::Issue() {
    // pre-issue the next few syscalls asynchronously
    if (scgraph != nullptr) {
        SCGraphNode *node = next;
        EdgeType dep = next_dep;
        int depth = scgraph->pre_issue_depth;
        int num_prepared = 0;

        while (depth-- > 0 && node != nullptr) {
            while (node != nullptr && node->node_type == NODE_BRANCH) {
                // next node is a branching node, calculate the user-defined
                // condition function to pick a branch to consider
                BranchNode *branch_node = static_cast<BranchNode *>(node);
                node = branch_node->PickBranch();
            }
            if (node == nullptr)
                break;

            SyscallNode *syscall_node = static_cast<SyscallNode *>(node);
            if ((dep == DEP_NONE) ||
                (dep == DEP_OCCURRENCE &&
                 syscall_node->node_type == NODE_SYSCALL_PURE)) {
                // if the next syscall has no dependency on me, or there is
                // occurrence dependency and the next syscall is non stateful,
                // then it is safe to kick it off asynchronously
                if (syscall_node->stage == STAGE_UNISSUED) {
                    syscall_node->PrepAsync();
                    syscall_node->stage = STAGE_ISSUED;
                    num_prepared++;
                }
                node = syscall_node->next;
                dep = syscall_node->next_dep;
            } else {
                // TODO: should allow more cases here
                break;
            }
        }

        if (num_prepared > 0) {
            int num_submitted = io_uring_submit(scgraph->Ring());
            assert(num_submitted == num_prepared);
        }
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
            assert(scgraph != nullptr);

            struct io_uring_cqe *cqe;
            while (true) {
                int ret = io_uring_wait_cqe(scgraph->Ring(), &cqe);
                assert(ret == 0);
                SyscallNode *node = reinterpret_cast<SyscallNode *>(
                    io_uring_cqe_get_data(cqe));
                node->rc = cqe->res;
                node->stage = STAGE_FINISHED;
                io_uring_cqe_seen(scgraph->Ring(), cqe);
                if (node == this)
                    break;
            }
        }

        ReflectResult();
    }

    assert(stage == STAGE_FINISHED);
    return rc;
}


}
