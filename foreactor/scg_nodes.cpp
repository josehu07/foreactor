#include <iostream>
#include <liburing.h>

#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"


namespace foreactor {


static bool AllArgsReady(std::vector<bool>& arg_ready) {
    return std::all_of(arg_ready.begin(), arg_ready.end(),
                       [](bool a) { return a; });
}


static std::string EdgeTypeStr(EdgeType edge_type) {
    switch (edge_type) {
        case EDGE_MUST: return "MUST";
        case EDGE_WEAK: return "WEAK";
        default:        return "UNKNOWN";
    }
}

static std::string SyscallStageStr(SyscallStage stage) {
    switch (stage) {
        case STAGE_NOTREADY: return "NOTREADY";
        case STAGE_UNISSUED: return "UNISSUED";
        case STAGE_PROGRESS: return "PROGRESS";
        case STAGE_FINISHED: return "FINISHED";
        default:             return "UNKNOWN";
    }
}


/////////////////
// SCGraphNode //
/////////////////

std::ostream& operator<<(std::ostream& s, const SCGraphNode& n) {
    switch (n.node_type) {
        case NODE_SC_PURE:
        case NODE_SC_SEFF: s << *static_cast<const SyscallNode *>(&n); break;
        case NODE_BRANCH:  s << *static_cast<const BranchNode *>(&n);  break;
        default:           s << "SCGraphNode{}";
    }
    return s;
}


/////////////////
// SyscallNode //
/////////////////

SyscallNode::SyscallNode(SyscallType sc_type, bool pure_sc,
                         std::vector<bool> arg_ready)
        : SCGraphNode(pure_sc ? NODE_SC_PURE : NODE_SC_SEFF),
          sc_type(sc_type), arg_ready(arg_ready),
          stage(AllArgsReady(arg_ready) ? STAGE_UNISSUED : STAGE_NOTREADY),
          next_node(nullptr), rc(-1) {
    assert(node_type == NODE_SC_PURE || node_type == NODE_SC_SEFF);
}

void SyscallNode::SetNext(SCGraphNode *node, bool weak_edge) {
    assert(node != nullptr);
    next_node = node;
    edge_type = weak_edge ? EDGE_WEAK : EDGE_MUST;
}

std::ostream& operator<<(std::ostream& s, const SyscallNode& n) {
    switch (n.sc_type) {
        case SC_OPEN:  s << *static_cast<const SyscallOpen *>(&n);  break;
        case SC_PREAD: s << *static_cast<const SyscallPread *>(&n); break;
        default:       s << "SyscallNode{}";
    }
    return s;
}

void SyscallNode::PrintCommonInfo(std::ostream& s) const {
    s << "arg_ready=";
    for (bool ready : arg_ready)
        s << (ready ? "T" : "F");
    s << ","
      << "stage=" << SyscallStageStr(stage) << ","
      << "rc=" << rc << ","
      << "next=" << next_node << ","
      << "edge=" << EdgeTypeStr(edge_type);
}


void SyscallNode::PrepAsync() {
    assert(stage == STAGE_UNISSUED);
    assert(scgraph != nullptr);
    struct io_uring_sqe *sqe = io_uring_get_sqe(scgraph->Ring());
    assert(sqe != nullptr);
    // SQE data is the pointer to the SyscallNode instance
    io_uring_sqe_set_data(sqe, this);
    // io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
    PrepUring(sqe);
}

void SyscallNode::CmplAsync() {
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


static bool EdgeForeactable(EdgeType edge, SyscallNode *next) {
    return (edge == EDGE_MUST) ||
           (edge == EDGE_WEAK && next->node_type == NODE_SC_PURE);
}

long SyscallNode::Issue(void *output_buf) {
    // pre-issue the next few syscalls asynchronously
    if (scgraph != nullptr) {
        SCGraphNode *next = next_node;
        EdgeType edge = edge_type;
        int depth = scgraph->pre_issue_depth;
        int num_prepared = 0;

        while (depth-- > 0 && next != nullptr) {
            // while next node is a branching node, calculate the user-defined
            // condition function to pick a branch to consider
            while (next != nullptr && next->node_type == NODE_BRANCH) {
                BranchNode *branch_node = static_cast<BranchNode *>(next);
                next = branch_node->PickBranch();
            }
            if (next == nullptr)
                break;

            // decide if the next node is pre-issuable, and if so, prepare
            // for submission to uring
            SyscallNode *syscall_node = static_cast<SyscallNode *>(next);
            if (syscall_node->stage != STAGE_NOTREADY &&
                EdgeForeactable(edge, syscall_node)) {
                if (syscall_node->stage == STAGE_UNISSUED) {
                    // the syscall might have already been submitted to
                    // io_uring by some previous foreactions
                    syscall_node->PrepAsync();
                    syscall_node->stage = STAGE_PROGRESS;
                    num_prepared++;
                }
                next = syscall_node->next_node;
                edge = syscall_node->edge_type;
            } else
                break;
        }

        if (num_prepared > 0) {
            int num_submitted = io_uring_submit(scgraph->Ring());
            assert(num_submitted == num_prepared);
        }
    }

    // handle myself
    if (stage == STAGE_UNISSUED) {
        // if not pre-issued
        rc = SyscallSync(output_buf);
        stage = STAGE_FINISHED;
    } else {
        // if has been pre-issued, process CQEs from io_uring until mine
        // completion is seen
        if (stage == STAGE_PROGRESS)
            CmplAsync();
        assert(stage == STAGE_FINISHED);
        ReflectResult(output_buf);
    }

    return rc;
}


////////////////
// BranchNode //
////////////////

BranchNode::BranchNode()
        : SCGraphNode(NODE_BRANCH), decision(-1) {
}

void BranchNode::SetChildren(std::vector<SCGraphNode *> children_list) {
    assert(children_list.size() > 1);
    children = children_list;
}

std::ostream& operator<<(std::ostream& s, const BranchNode& n) {
    s << "BranchNode{"
      << "decision=" << n.decision << ",children=[";
    for (size_t i = 0; i < n.children.size(); ++i) {
        s << n.children[i];
        if (i < n.children.size() - 1)
            s << ",";
    }
    s << "]}";
    return s;
}

SCGraphNode *BranchNode::PickBranch() {
    if (decision >= 0 && decision < (int) children.size())
        return children[decision];
    return nullptr;
}


}
