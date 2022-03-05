#include <iostream>
#include <string>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"


namespace foreactor {


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


////////////////////////////////
// SCGraphNode implementation //
////////////////////////////////

std::ostream& operator<<(std::ostream& s, const SCGraphNode& n) {
    switch (n.node_type) {
        case NODE_SC_PURE:
        case NODE_SC_SEFF: s << *static_cast<const SyscallNode *>(&n); break;
        case NODE_BRANCH:  s << *static_cast<const BranchNode *>(&n);  break;
        default:           s << "SCGraphNode{}";
    }
    return s;
}


////////////////////////////////
// SyscallNode implementation //
////////////////////////////////

SyscallNode::SyscallNode(SyscallType sc_type, bool pure_sc,
                         ValuePool<SyscallStage> *stage, ValuePool<long> *rc)
        : SCGraphNode(pure_sc ? NODE_SC_PURE : NODE_SC_SEFF),
          next_node(nullptr), sc_type(sc_type), stage(stage), rc(-1) {
    assert(node_type == NODE_SC_PURE || node_type == NODE_SC_SEFF);
    assert(stage != nullptr);
    assert(rc != nullptr);
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
    s << "stage=" << stage << ","
      << "rc=" << rc << ","
      << "next=" << next_node << ","
      << "edge=" << EdgeTypeStr(edge_type);
}


// The io_uring entry's data field should store both the SyscallNode pointer
// and the issuing epoch.
typedef struct NodeAndEpoch {
    SyscallNode *node;
    Epoch
}

void SyscallNode::PrepAsync(EpochList *epoch) {
    assert(stage->GetValue(epoch) == STAGE_UNISSUED);
    assert(scgraph != nullptr);
    struct io_uring_sqe *sqe = io_uring_get_sqe(scgraph->Ring());
    assert(sqe != nullptr);

    // SQE data is the pointer to the SyscallNode instance
    io_uring_sqe_set_data(sqe, this);
    
    PrepUring(epoch, sqe);
}

void SyscallNode::CmplAsync(EpochList *epoch) {
    assert(scgraph != nullptr);
    struct io_uring_cqe *cqe;
    while (true) {
        int ret = io_uring_wait_cqe(scgraph->Ring(), &cqe);
        if (ret != 0) {
            DEBUG("wait CQE failed %d\n", ret);
            assert(false);
        }

        SyscallNode *node = reinterpret_cast<SyscallNode *>(
            io_uring_cqe_get_data(cqe));
        
        node->rc->SetValue(epoch,  cqe->res);
        node->stage->SetValue(epoch,  STAGE_FINISHED);
        io_uring_cqe_seen(scgraph->Ring(), cqe);
        
        if (node == this)
            break;
    }
}


static bool IsForeactable(EdgeType edge, SyscallNode *next, bool instable) {
    if (next->stage == STAGE_NOTREADY)
        return false;
    if (instable)
        return next->node_type == NODE_SC_PURE;
    return !(edge == EDGE_WEAK && next->node_type == NODE_SC_SEFF);
}

// The Issue() method contains the core logic of foreactor's syscall
// pre-issuing algorithm.
long SyscallNode::Issue(void *output_buf) {
    // can only call issue on frontier node
    assert(scgraph != nullptr);
    assert(this == scgraph->frontier);
    DEBUG("issue %s<%p>\n", StreamStr<SyscallNode>(this).c_str(), this);

    // pre-issue the next few syscalls asynchronously
    SCGraphNode *next = next_node;
    EdgeType edge = edge_type;
    int depth = scgraph->pre_issue_depth;
    int num_prepared = 0;
    // FIXME: finish instability logic
    bool instable = true;

    while (depth-- > 0 && next != nullptr) {
        // while next node is a branching node, calculate the user-defined
        // condition function to pick a branch to consider
        while (next != nullptr && next->node_type == NODE_BRANCH) {
            BranchNode *branch_node = static_cast<BranchNode *>(next);
            DEBUG("branch %s<%p>\n",
                  StreamStr<BranchNode>(branch_node).c_str(), branch_node);
            next = branch_node->PickBranch();
            DEBUG("picked branch %p\n", next);
        }
        if (next == nullptr)
            break;

        // decide if the next node is pre-issuable, and if so, prepare
        // for submission to uring
        SyscallNode *syscall_node = static_cast<SyscallNode *>(next);
        if (IsForeactable(edge, syscall_node, instable)) {
            if (syscall_node->stage == STAGE_UNISSUED) {
                // the syscall might have already been submitted to
                // io_uring by some previous foreactions
                syscall_node->PrepAsync();
                syscall_node->stage = STAGE_PROGRESS;
                num_prepared++;
                DEBUG("prepared %s<%p>\n",
                      StreamStr<SyscallNode>(syscall_node).c_str(),
                      syscall_node);
            }
            next = syscall_node->next_node;
            edge = syscall_node->edge_type;
        } else
            break;
    }

    if (num_prepared > 0) {
        TIMER_START("g"+std::to_string(scgraph->graph_id)+"-ring-submit");
        int num_submitted __attribute__((unused)) =
            io_uring_submit(scgraph->Ring());
        TIMER_PAUSE("g"+std::to_string(scgraph->graph_id)+"-ring-submit");
        DEBUG("submitted %d / %d entries to SQ\n",
              num_submitted, num_prepared);
        assert(num_submitted == num_prepared);
    }

    // handle myself
    assert(stage != STAGE_NOTREADY);
    if (stage == STAGE_UNISSUED) {
        // if not pre-issued
        DEBUG("sync-call %s<%p>\n",
              StreamStr<SyscallNode>(this).c_str(), this);
        TIMER_START("g"+std::to_string(scgraph->graph_id)+"-sync-call");
        rc = SyscallSync(output_buf);
        stage = STAGE_FINISHED;
        TIMER_PAUSE("g"+std::to_string(scgraph->graph_id)+"-sync-call");
        DEBUG("sync-call finished rc %ld\n", rc);
    } else {
        // if has been pre-issued, process CQEs from io_uring until mine
        // completion is seen
        DEBUG("ring-cmpl %s<%p>\n",
              StreamStr<SyscallNode>(this).c_str(), this);
        TIMER_START("g"+std::to_string(scgraph->graph_id)+"-ring-cmpl");
        if (stage == STAGE_PROGRESS)
            CmplAsync();
        assert(stage == STAGE_FINISHED);
        TIMER_PAUSE("g"+std::to_string(scgraph->graph_id)+"-ring-cmpl");
        DEBUG("ring-cmpl finished rc %ld\n", rc);
        ReflectResult(output_buf);
        DEBUG("ring-cmpl result reflected\n");
    }

    // push frontier forward
    scgraph->frontier = next_node;
    DEBUG("pushed frontier -> %p\n", scgraph->frontier);

    return rc;
}


///////////////////////////////
// BranchNode implementation //
///////////////////////////////

BranchNode::BranchNode(int decision)
        : SCGraphNode(NODE_BRANCH), decision(decision) {
    assert(decision >= -1);
}

void BranchNode::SetChildren(std::vector<SCGraphNode *> children_list) {
    assert(children_list.size() > 0);
    children = children_list;
}

void BranchNode::AppendChild(SCGraphNode *child) {
    assert(child != nullptr);
    children.push_back(child);
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

void BranchNode::SetDecision(int decision_) {
    assert(decision == -1);
    assert(decision_ >= 0 && decision_ < (int) children.size());
    decision = decision_;
}


}
