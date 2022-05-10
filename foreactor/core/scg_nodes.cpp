#include <iostream>
#include <string>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"
#include "ring_buffer.hpp"


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

SyscallNode::SyscallNode(std::string name, SyscallType sc_type, bool pure_sc,
                         size_t rb_capacity)
        : SCGraphNode(name, pure_sc ? NODE_SC_PURE : NODE_SC_SEFF),
          sc_type(sc_type), next_node(nullptr),
          stage(rb_capacity), rc(rb_capacity) {
    assert(node_type == NODE_SC_PURE || node_type == NODE_SC_SEFF);
    assert(rb_capacity > 0);
}


void SyscallNode::PrintCommonInfo(std::ostream& s) const {
    s << "stage=" << stage << ","
      << "rc=" << rc << ","
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


void SyscallNode::PrepAsync(int epoch) {
    assert(stage.Get(epoch) == STAGE_UNISSUED);
    assert(scgraph != nullptr);

    // encode node ptr and epoch number into a uint64_t user_data
    EntryId entry_id = IOUring::EncodeEntryId(this, epoch);

    // put this node to prepared state
    scgraph->ring->Prepare(entry_id);
    stage->Set(epoch, STAGE_PREPARED);
}

void SyscallNode::CmplAsync(int epoch) {
    assert(scgraph != nullptr);

    // loop until the completion for myself is seen
    while (true) {
        struct io_uring_cqe *cqe = scgraph->ring->WaitOneCqe();
        assert(cqe != nullptr);

        // fetch the node ptr and epoch of this request
        EntryId entry_id = scgraph->ring->GetCqeEntryId(cqe);
        auto [sqe_node, sqe_epoch] = IOUring::DecodeEntryId(entry_id);
        assert(sqe_node->stage->Get(sqe_epoch) == STAGE_PROGRESS);
        
        // reflect rc and stage
        sqe_node->rc->Set(sqe_epoch, cqe->res);
        sqe_node->stage->Set(sqe_epoch, STAGE_FINISHED);
        scgraph->ring->SeenOneCqe(cqe);
        scgraph->ring->RemoveOne(entry_id);
        
        // if desired completion found, break
        if ((sqe_node == this) && sqe_epoch == epoch)
            break;
    }
}


void SyscallNode::SetNext(SCGraphNode *node, bool weak_edge) {
    assert(node != nullptr);
    next_node = node;
    edge_type = weak_edge ? EDGE_WEAK : EDGE_MUST;
}


// The condition function that decides if an edge can be pre-issued across.
bool SyscallNode::IsForeactable(EdgeType edge, SyscallNode *next, int epoch) {
    if (next->stage.Get(epoch) == STAGE_NOTREADY) {
        if (!next->RefreshStage(epoch))     // check again
            return false;
    }
    return !(edge == EDGE_WEAK && next->node_type == NODE_SC_SEFF);
}

// The Issue() method contains the core logic of foreactor's syscall
// pre-issuing algorithm. The epoch argument is the current frontier_epoch.
long SyscallNode::Issue(int epoch, void *output_buf) {
    // can only call issue on frontier node
    assert(scgraph != nullptr);
    assert(this == scgraph->frontier);
    DEBUG("issue %s<%p>@%s\n",
          StreamStr<SyscallNode>(this).c_str(), this,
          StreamStr<EpochListBase>(epoch).c_str());

    // if pre_issue_depth > 0, and previous pre-issuing procedure has not hit
    // the end of SCGraph, try to peek and pre-issue
    if (scgraph->pre_issue_depth > 0 && !scgraph->peekhead_hit_end) {
        TIMER_START(scgraph->TimerNameStr("peek-algo"));
        // sometimes, peekhead might get stuck due to some barrier, and thus
        // the current frontier might proceed ahead of the stored peekhead;
        // in this case, update peekhead to be current frontier
        if (scgraph->peekhead_distance < 0) {
            assert(epoch->AheadOf(scgraph->peekhead_epoch));
            scgraph->peekhead = next_node;
            scgraph->peekhead_edge = edge_type;
            scgraph->peekhead_epoch->CopyFrom(epoch);
            scgraph->peekhead_distance = 0;
            DEBUG("peekhead catch up with frontier\n");
        }
        SCGraphNode *next = scgraph->peekhead;
        EpochListBase *peek_epoch = scgraph->peekhead_epoch;
        EdgeType edge = scgraph->peekhead_edge;

        // peek and pre-issue the next few syscalls asynchronously; at most
        // peek as far as pre_issue_depth nodes beyond current frontier
        assert(scgraph->peekhead_distance >= 0);
        assert(scgraph->peekhead_distance <= scgraph->pre_issue_depth);
        int depth = scgraph->pre_issue_depth - scgraph->peekhead_distance;
        DEBUG("peeking %d starts %s<%p>@%s\n",
              depth, StreamStr<SCGraphNode>(next).c_str(), next,
              StreamStr<EpochListBase>(peek_epoch).c_str());
        while (depth-- > 0 && next != nullptr) {
            // while next node is a branching node, pick up the correct
            // branch if the branching has been decided
            while (next != nullptr && next->node_type == NODE_BRANCH) {
                BranchNode *branch_node = static_cast<BranchNode *>(next);
                DEBUG("branch %s<%p>@%s\n",
                      StreamStr<BranchNode>(branch_node).c_str(), branch_node,
                      StreamStr<EpochListBase>(peek_epoch).c_str());
                next = branch_node->PickBranch(peek_epoch);
                DEBUG("picked branch %p\n", next);
            }
            if (next == nullptr) {
                // FIXME: argument install logic, may also return nullptr
                // hit end of graph, no need of peeking for future Issue()s
                scgraph->peekhead_hit_end = true;
                DEBUG("peeking hit end of SCGraph\n");
                break;
            } else
                scgraph->peekhead = next;

            // decide if the next node is pre-issuable, and if so, prepare
            // for submission to uring
            SyscallNode *syscall_node = static_cast<SyscallNode *>(next);
            if (IsForeactable(edge, syscall_node, peek_epoch)) {
                // prepare the IOUring submission entry, set node stage to be
                // in-progress
                assert(syscall_node->stage->GetValue(peek_epoch) ==
                       STAGE_UNISSUED);
                syscall_node->PrepAsync(peek_epoch);
                DEBUG("prepared %d %s<%p>@%s\n",
                      scgraph->num_prepared,
                      StreamStr<SyscallNode>(syscall_node).c_str(),
                      syscall_node,
                      StreamStr<EpochListBase>(peek_epoch).c_str());
                // distance of peekhead from frontier increments by 1
                next = syscall_node->next_node;
                edge = syscall_node->edge_type;
                scgraph->peekhead = next;
                scgraph->peekhead_edge = edge;
                scgraph->peekhead_distance++;
                // update prepared_distance if this is the earliest
                if (scgraph->num_prepared == 0)
                    scgraph->prepared_distance = scgraph->peekhead_distance;
                scgraph->num_prepared++;
            } else
                break;
        }
        TIMER_PAUSE(scgraph->TimerNameStr("peek-algo"));
    }

    // see some number of syscall nodes prepared, may do io_uring_submit
    // if we have a sufficient number of prepared entries or if the
    // earliest prepared entry is close enough to current frontier
    if (scgraph->num_prepared > 0) {
        // TODO: these conditions might be tunable
        if ((scgraph->num_prepared >= scgraph->pre_issue_depth / 2) ||
            (scgraph->prepared_distance <= 1)) {
            // move all prepared requests to in_progress stage, actually
            // call the io_uring_prep_xxx()s
            TIMER_START(scgraph->TimerNameStr("ring-submit"));
            scgraph->ring->MakeAllInProgress();
            // do io_uring_submit()
            int num_submitted __attribute__((unused)) =
                io_uring_submit(scgraph->Ring());
            TIMER_PAUSE(scgraph->TimerNameStr("ring-submit"));
            DEBUG("submitted %d / %d entries to SQ\n",
                  num_submitted, scgraph->num_prepared);
            assert(num_submitted == scgraph->num_prepared);
            // clear num_prepared and prepared_distance
            scgraph->num_prepared = 0;
            scgraph->prepared_distance = -1;
        }
    }

    // handle myself
    if (stage->GetValue(epoch) == STAGE_NOTREADY) {
        bool now_ready __attribute__((unused)) = RefreshStage(epoch);
        assert(now_ready);
    }
    if (stage->GetValue(epoch) == STAGE_UNISSUED) {
        // if not pre-issued, invoke syscall synchronously
        DEBUG("sync-call %s<%p>@%s\n",
              StreamStr<SyscallNode>(this).c_str(), this,
              StreamStr<EpochListBase>(epoch).c_str());
        TIMER_START(scgraph->TimerNameStr("sync-call"));
        rc->SetValue(epoch, SyscallSync(epoch, output_buf));
        stage->SetValue(epoch, STAGE_FINISHED);
        TIMER_PAUSE(scgraph->TimerNameStr("sync-call"));
        DEBUG("sync-call finished rc %ld\n", rc->GetValue(epoch));
    } else {
        // if has been pre-issued, process CQEs from io_uring until mine
        // completion is seen
        assert(stage->GetValue(epoch) != STAGE_PREPARED);
        DEBUG("ring-cmpl %s<%p>@%s\n",
              StreamStr<SyscallNode>(this).c_str(), this,
              StreamStr<EpochListBase>(epoch).c_str());
        TIMER_START(scgraph->TimerNameStr("ring-cmpl"));
        if (stage->GetValue(epoch) == STAGE_PROGRESS)
            CmplAsync(epoch);
        assert(stage->GetValue(epoch) == STAGE_FINISHED);
        TIMER_PAUSE(scgraph->TimerNameStr("ring-cmpl"));
        DEBUG("ring-cmpl finished rc %ld\n", rc->GetValue(epoch));
        ReflectResult(epoch, output_buf);
        DEBUG("ring-cmpl result reflected\n");
    }

    // push frontier forward, reduce distance counters by 1
    scgraph->frontier = next_node;
    scgraph->peekhead_distance--;
    scgraph->prepared_distance--;
    DEBUG("pushed frontier -> %p\n", scgraph->frontier);

    return rc->GetValue(epoch);
}


///////////////////////////////
// BranchNode implementation //
///////////////////////////////

BranchNode::BranchNode(std::string name, size_t num_children,
                       size_t rb_capacity)
        : SCGraphNode(name, NODE_BRANCH), num_children(num_children),
          decision(rb_capacity) {
    assert(rb_capacity > 0);
    assert(num_children > 1);
    children = new SCGraphNode *[num_children];
    enclosed = new std::unordered_set<SCGraphNode *>[num_children];
    assert(children != nullptr && enclosed != nullptr);
}

BranchNode::~BranchNode() {
    if (children != nullptr)
        delete[] children;
    if (enclosed != nullptr)
        delete[] enclosed;
}


std::ostream& operator<<(std::ostream& s, const BranchNode& n) {
    s << "BranchNode{"
      << "decision=" << decision
      << ",children=[";
    for (size_t i = 0; i < n.children.size(); ++i) {
        s << n.children[i];
        if (n.children.size() > 0 && i < n.children.size() - 1)
            s << ",";
    }
    s << "]}";
    return s;
}


SCGraphNode *BranchNode::PickBranch(int epoch) {
    int d = decision->Get(epoch);
    assert(d >= 0 && d < static_cast<int>(children.size()));
    SCGraphNode *child = children[d];

    // if goes through a looping-back edge, increment the corresponding
    // epoch number of that node
    if (enclosed[d] != nullptr) {
        // TODO: looping-back logic
    }

    return child;
}


void BranchNode::SetChild(int child_idx, SCGraphNode *child_node,
                          std::unordered_set<SCGraphNode *> *enclosed) {
    assert(child_idx >= 0 && child_idx < num_children);
    children[child_idx] = child_node;
    enclosed[child_idx] = enclosed;
}


}
