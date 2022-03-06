#include <iostream>
#include <string>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"
#include "value_pool.hpp"


namespace foreactor {


std::ostream& operator<<(std::ostream& s, const EdgeType& t) {
    switch (t) {
    case EDGE_MUST: s << "MUST";    break;
    case EDGE_WEAK: s << "WEAK";    break;
    default:        s << "UNKNOWN"; break;
    }
    return s;
}

std::ostream& operator<<(std::ostream& s, const SyscallStage& t) {
    switch (t) {
    case STAGE_NOTREADY: s << "NOTREADY"; break;
    case STAGE_UNISSUED: s << "UNISSUED"; break;
    case STAGE_PROGRESS: s << "PROGRESS"; break;
    case STAGE_FINISHED: s << "FINISHED"; break;
    default:             s << "UNKNOWN";  break;
    }
    return s;
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
                         ValuePoolBase<SyscallStage> *stage,
                         ValuePoolBase<long> *rc)
        : SCGraphNode(pure_sc ? NODE_SC_PURE : NODE_SC_SEFF),
          sc_type(sc_type), next_node(nullptr), stage(stage), rc(rc) {
    assert(node_type == NODE_SC_PURE || node_type == NODE_SC_SEFF);
    assert(stage != nullptr);
    assert(rc != nullptr);
}


void SyscallNode::PrintCommonInfo(std::ostream& s) const {
    s << "stage=" << StreamStr<ValuePoolBase<SyscallStage>>(stage) << ","
      << "rc=" << StreamStr<ValuePoolBase<long>>(rc) << ","
      << "next=" << next_node << ","
      << "edge=" << edge_type;
}

std::ostream& operator<<(std::ostream& s, const SyscallNode& n) {
    switch (n.sc_type) {
    case SC_OPEN:  s << *static_cast<const SyscallOpen *>(&n);  break;
    case SC_PREAD: s << *static_cast<const SyscallPread *>(&n); break;
    default:       s << "SyscallNode{}";
    }
    return s;
}


void SyscallNode::PrepAsync(EpochListBase *epoch) {
    assert(stage->GetValue(epoch) == STAGE_UNISSUED);
    assert(scgraph != nullptr);
    assert(epoch != nullptr);
    struct io_uring_sqe *sqe = io_uring_get_sqe(scgraph->Ring());
    assert(sqe != nullptr);

    // SQE data is the pointer to a NodeAndEpoch structure
    NodeAndEpoch *nae = new NodeAndEpoch(this, epoch);
    io_uring_sqe_set_data(sqe, nae);
    
    // call syscall-specific preparation function
    PrepUring(epoch, sqe);
    scgraph->ring->PutInProgress(nae);
}

void SyscallNode::CmplAsync(EpochListBase *epoch) {
    assert(scgraph != nullptr);
    assert(epoch != nullptr);
    struct io_uring_cqe *cqe;
    while (true) {
        int ret = io_uring_wait_cqe(scgraph->Ring(), &cqe);
        if (ret != 0) {
            DEBUG("wait CQE failed %d\n", ret);
            assert(false);
        }

        // fetch the NodeAndEpoch structure
        NodeAndEpoch *nae = reinterpret_cast<NodeAndEpoch *>(
            io_uring_cqe_get_data(cqe));
        
        // reflect rc and stage
        nae->node->rc->SetValue(nae->epoch,  cqe->res);
        nae->node->stage->SetValue(nae->epoch,  STAGE_FINISHED);
        io_uring_cqe_seen(scgraph->Ring(), cqe);
        scgraph->ring->RemoveInProgress(nae);
        
        // if desired completion found, break
        if (nae->node == this && nae->epoch->IsSame(epoch)) {
            delete nae;
            break;
        }
        delete nae;
    }
}


void SyscallNode::SetNext(SCGraphNode *node, bool weak_edge) {
    assert(node != nullptr);
    next_node = node;
    edge_type = weak_edge ? EDGE_WEAK : EDGE_MUST;
}


// The condition function that decides if an edge can be pre-issued across.
bool SyscallNode::IsForeactable(EdgeType edge, SyscallNode *next,
                                EpochListBase *epoch) {
    if (next->stage->GetValue(epoch) == STAGE_NOTREADY) {
        if (!next->RefreshStage(epoch))     // check again
            return false;
    }
    return !(edge == EDGE_WEAK && next->node_type == NODE_SC_SEFF);
}

// The Issue() method contains the core logic of foreactor's syscall
// pre-issuing algorithm.
long SyscallNode::Issue(EpochListBase *epoch, void *output_buf) {
    // can only call issue on frontier node
    assert(scgraph != nullptr);
    assert(this == scgraph->frontier);
    DEBUG("issue %s<%p>@%s\n",
          StreamStr<SyscallNode>(this).c_str(), this,
          StreamStr<EpochListBase>(epoch).c_str());

    // pre-issue the next few syscalls asynchronously
    SCGraphNode *next = next_node;
    EdgeType edge = edge_type;
    int depth = scgraph->pre_issue_depth;
    int num_prepared = 0;

    EpochListBase *peek_epoch = EpochListBase::Copy(epoch);
    while (depth-- > 0 && next != nullptr) {
        // while next node is a branching node, calculate the user-defined
        // condition function to pick a branch to consider
        while (next != nullptr && next->node_type == NODE_BRANCH) {
            BranchNode *branch_node = static_cast<BranchNode *>(next);
            DEBUG("branch %s<%p>@%s\n",
                  StreamStr<BranchNode>(branch_node).c_str(), branch_node,
                  StreamStr<EpochListBase>(peek_epoch).c_str());
            next = branch_node->PickBranch(peek_epoch);
            DEBUG("picked branch %p\n", next);
        }
        if (next == nullptr)
            break;

        // decide if the next node is pre-issuable, and if so, prepare
        // for submission to uring
        SyscallNode *syscall_node = static_cast<SyscallNode *>(next);
        if (IsForeactable(edge, syscall_node, peek_epoch)) {
            if (syscall_node->stage->GetValue(peek_epoch) == STAGE_UNISSUED) {
                // the syscall might have already been submitted to
                // io_uring by some previous foreactions
                syscall_node->PrepAsync(peek_epoch);
                syscall_node->stage->SetValue(peek_epoch, STAGE_PROGRESS);
                num_prepared++;
                DEBUG("prepared %s<%p>@%s\n",
                      StreamStr<SyscallNode>(syscall_node).c_str(),
                      syscall_node, StreamStr<EpochListBase>(peek_epoch).c_str());
            }
            next = syscall_node->next_node;
            edge = syscall_node->edge_type;
        } else
            break;
    }
    EpochListBase::Delete(peek_epoch);

    // see some number of syscall nodes prepared, do io_uring_submit
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
    if (stage->GetValue(epoch) == STAGE_NOTREADY) {
        bool now_ready __attribute__((unused)) = RefreshStage(epoch);
        assert(now_ready);
    }
    if (stage->GetValue(epoch) == STAGE_UNISSUED) {
        // if not pre-issued
        DEBUG("sync-call %s<%p>@%s\n",
              StreamStr<SyscallNode>(this).c_str(), this,
              StreamStr<EpochListBase>(epoch).c_str());
        TIMER_START("g"+std::to_string(scgraph->graph_id)+"-sync-call");
        rc->SetValue(epoch, SyscallSync(epoch, output_buf));
        stage->SetValue(epoch, STAGE_FINISHED);
        TIMER_PAUSE("g"+std::to_string(scgraph->graph_id)+"-sync-call");
        DEBUG("sync-call finished rc %ld\n", rc->GetValue(epoch));
    } else {
        // if has been pre-issued, process CQEs from io_uring until mine
        // completion is seen
        DEBUG("ring-cmpl %s<%p>@%s\n",
              StreamStr<SyscallNode>(this).c_str(), this,
              StreamStr<EpochListBase>(epoch).c_str());
        TIMER_START("g"+std::to_string(scgraph->graph_id)+"-ring-cmpl");
        if (stage->GetValue(epoch) == STAGE_PROGRESS)
            CmplAsync(epoch);
        assert(stage->GetValue(epoch) == STAGE_FINISHED);
        TIMER_PAUSE("g"+std::to_string(scgraph->graph_id)+"-ring-cmpl");
        DEBUG("ring-cmpl finished rc %ld\n", rc->GetValue(epoch));
        ReflectResult(epoch, output_buf);
        DEBUG("ring-cmpl result reflected\n");
    }

    // push frontier forward
    scgraph->frontier = next_node;
    DEBUG("pushed frontier -> %p\n", scgraph->frontier);

    return rc->GetValue(epoch);
}


///////////////////////////////
// BranchNode implementation //
///////////////////////////////

BranchNode::BranchNode(ValuePoolBase<int> *decision)
        : SCGraphNode(NODE_BRANCH), decision(decision) {
    assert(decision != nullptr);
}


std::ostream& operator<<(std::ostream& s, const BranchNode& n) {
    s << "BranchNode{"
      << "decision=" << StreamStr<ValuePoolBase<int>>(n.decision)
      << ",children=[";
    for (size_t i = 0; i < n.children.size(); ++i) {
        s << n.children[i];
        if (n.children.size() > 0 && i < n.children.size() - 1)
            s << ",";
    }
    s << "]}";
    return s;
}


SCGraphNode *BranchNode::PickBranch(EpochListBase *epoch) {
    if (!decision->GetReady(epoch))
        return nullptr;

    int d = decision->GetValue(epoch);
    assert(d >= 0 && d < static_cast<int>(children.size()));
    SCGraphNode *child = children[d];

    // if goes through a looping-back edge, increment the corresponding
    // epoch number
    if (epoch_dim_idx[d] >= 0) {
        DEBUG("epoch currently @ -> %s\n",
              StreamStr<EpochListBase>(epoch).c_str());
        epoch->IncrementEpoch(epoch_dim_idx[d]);
        DEBUG("incremented epoch -> %s\n",
              StreamStr<EpochListBase>(epoch).c_str());
    }
    return child;
}


void BranchNode::AppendChild(SCGraphNode *child, int dim_idx) {
    // child could be nullptr, which means end of scgraph
    children.push_back(child);
    if (dim_idx >= 0) {
        assert(dim_idx < static_cast<int>(scgraph->frontier_epoch->max_dims));
        epoch_dim_idx.push_back(dim_idx);
    } else
        epoch_dim_idx.push_back(-1);
}


}
