#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <assert.h>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "syscalls.hpp"
#include "value_pool.hpp"
#include "foreactor.h"


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
    case STAGE_ARGREADY: s << "ARGREADY"; break;
    case STAGE_ONTHEFLY: s << "ONTHEFLY"; break;
    case STAGE_FINISHED: s << "FINISHED"; break;
    default:             s << "UNKNOWN";  break;
    }
    return s;
}


////////////////////////////////
// SCGraphNode implementation //
////////////////////////////////

SCGraphNode::SCGraphNode(unsigned node_id, std::string name,
                         NodeType node_type, SCGraph *scgraph,
                         const std::unordered_set<int>& assoc_dims)
        : node_id(node_id), name(name), node_type(node_type),
          scgraph(scgraph), assoc_dims(assoc_dims) {
    assert(node_type != NODE_BASE);
    assert(scgraph != nullptr);
    assert(assoc_dims.size() >= 0 && assoc_dims.size() <= scgraph->total_dims);
    for ([[maybe_unused]] int assoc_dim : assoc_dims) {
        assert(assoc_dim >= 0 &&
               assoc_dim < static_cast<int>(scgraph->total_dims));
    }
}


std::ostream& operator<<(std::ostream& s, const SCGraphNode& n) {
    switch (n.node_type) {
    case NODE_SC_PURE: [[fallthrough]];
    case NODE_SC_SEFF:
        s << StreamStr(static_cast<const SyscallNode&>(n));
        break;
    case NODE_BRANCH:
        s << StreamStr(static_cast<const BranchNode&>(n));
        break;
    default:
        s << "SCGraphNode{" << n.node_id << "}";
    }
    return s;
}


int SCGraphNode::EpochSum(const EpochList& epoch) {
    return epoch.Sum(assoc_dims);
}


////////////////////////////////
// SyscallNode implementation //
////////////////////////////////

SyscallNode::SyscallNode(unsigned node_id, std::string name,
                         SyscallType sc_type, bool pure_sc, SCGraph* scgraph,
                         const std::unordered_set<int>& assoc_dims)
        : SCGraphNode(node_id, name, pure_sc ? NODE_SC_PURE : NODE_SC_SEFF,
                      scgraph, assoc_dims),
          sc_type(sc_type), next_node(nullptr), edge_type(EDGE_BASE),
          stage(assoc_dims), rc(assoc_dims) {
    assert(node_type == NODE_SC_PURE || node_type == NODE_SC_SEFF);
}


void SyscallNode::PrintCommonInfo(std::ostream& s) const {
    s << node_id << ",next=" << next_node << ",edge=" << edge_type
      << ",stage=" << stage << ",rc=" << rc;
}

std::ostream& operator<<(std::ostream& s, const SyscallNode& n) {
    switch (n.sc_type) {
    case SC_OPEN:
        s << StreamStr(static_cast<const SyscallOpen&>(n));
        break;
    case SC_CLOSE:
        s << StreamStr(static_cast<const SyscallClose&>(n));
        break;
    case SC_PREAD:
        s << StreamStr(static_cast<const SyscallPread&>(n));
        break;
    case SC_PWRITE:
        s << StreamStr(static_cast<const SyscallPwrite&>(n));
        break;
    default:       s << "SyscallNode{" << n.node_id << "}";
    }
    return s;
}


void SyscallNode::PrepAsync(const EpochList& epoch) {
    assert(stage.Get(epoch) == STAGE_ARGREADY);
    assert(scgraph != nullptr);

    // encode node ptr and epoch_sum number into a uint64_t user_data
    IOUring::EntryId entry_id = IOUring::EncodeEntryId(this, EpochSum(epoch));

    // put this node to prepared state
    scgraph->ring->Prepare(entry_id);
    stage.Set(epoch, STAGE_PREPARED);
}

void SyscallNode::CmplAsync(const EpochList& epoch) {
    assert(scgraph != nullptr);
    const int epoch_sum = EpochSum(epoch);

    // loop until the completion for myself is seen
    while (true) {
        struct io_uring_cqe *cqe = scgraph->ring->WaitOneCqe();
        assert(cqe != nullptr);

        // fetch the node ptr and epoch of this request
        IOUring::EntryId entry_id = scgraph->ring->GetCqeEntryId(cqe);
        auto [sqe_node, sqe_epoch_sum] = IOUring::DecodeEntryId(entry_id);
        assert(sqe_node->stage.Get(sqe_epoch_sum) == STAGE_ONTHEFLY);
        
        // reflect rc and stage
        sqe_node->rc.Set(sqe_epoch_sum, cqe->res);
        sqe_node->stage.Set(sqe_epoch_sum, STAGE_FINISHED);
        scgraph->ring->SeenOneCqe(cqe);
        scgraph->ring->RemoveOne(entry_id);
        
        // if desired completion found, break
        if (sqe_node == this && sqe_epoch_sum == epoch_sum) {

            break;
        }
    }
}


void SyscallNode::RemoveOneFromCommonPools(const EpochList& epoch) {
    stage.Remove(epoch);
    rc.Remove(epoch);
}

void SyscallNode::ResetCommonPools() {
    stage.Reset();
    rc.Reset();
}


void SyscallNode::SetNext(SCGraphNode *node, bool weak_edge) {
    assert(node != nullptr);
    next_node = node;
    edge_type = weak_edge ? EDGE_WEAK : EDGE_MUST;
}


// The condition function that decides if an edge can be pre-issued across.
bool SyscallNode::IsForeactable(bool weak_state, const SyscallNode *next) {
    return !(weak_state && next->node_type == NODE_SC_SEFF);
}

// The Issue() method contains the core logic of foreactor's syscall
// pre-issuing algorithm. The epoch argument is the current frontier_epoch.
long SyscallNode::Issue(const EpochList& epoch, void *output_buf) {
    // can only call issue on frontier node
    assert(scgraph != nullptr);
    assert(this == scgraph->frontier);
    assert(epoch.SameAs(scgraph->frontier_epoch));
    DEBUG("issue %s<%p>@%s\n",
          StreamStr(*this).c_str(), this, StreamStr(epoch).c_str());

    // if pre_issue_depth > 0, and previous pre-issuing procedure has not hit
    // the end of SCGraph, try to peek and pre-issue
    if (scgraph->pre_issue_depth > 0 && !scgraph->peekhead_hit_end) {
        TIMER_START(scgraph->TimerNameStr("peek-algo"));
        // sometimes, peekhead might get stuck due to some barrier, and thus
        // the current frontier might proceed ahead of the stored peekhead;
        // in this case, update peekhead to be current frontier
        if (scgraph->peekhead_distance < 0) {
            assert(epoch.AheadOf(scgraph->peekhead_epoch));
            scgraph->peekhead = next_node;
            scgraph->peekhead_edge = edge_type;
            scgraph->peekhead_epoch.CopyFrom(epoch);
            scgraph->peekhead_distance = 0;
            DEBUG("peekhead catch up with frontier\n");
        }
        SCGraphNode *next = scgraph->peekhead;
        EdgeType edge = scgraph->peekhead_edge;
        bool weak_state = false;

        // peek and pre-issue the next few syscalls asynchronously; at most
        // peek as far as pre_issue_depth nodes beyond current frontier
        assert(scgraph->peekhead_distance >= 0);
        assert(scgraph->peekhead_distance <= scgraph->pre_issue_depth);
        int depth = scgraph->pre_issue_depth - scgraph->peekhead_distance;
        EpochList& peek_epoch = scgraph->peekhead_epoch;
        if (next != nullptr) {
            DEBUG("peeking %d starts %s<%p>@%s\n",
                  depth, StreamStr(*next).c_str(), next,
                  StreamStr(peek_epoch).c_str());
        } else {
            scgraph->peekhead_hit_end = true;
            DEBUG("peeking hit end of SCGraph\n");
        }
        while (depth-- > 0 && next != nullptr) {
            // while next node is a branching node, pick up the correct
            // branch if the branching has been decided
            bool decision_barrier = false;
            while (next != nullptr && next->node_type == NODE_BRANCH) {
                BranchNode *branch_node = static_cast<BranchNode *>(next);
                DEBUG("branch %s<%p>@%s in peeking\n",
                      StreamStr(*branch_node).c_str(), branch_node,
                      StreamStr(epoch).c_str());
                // if decision not ready, see if it can be generated now
                if (!branch_node->decision.Has(peek_epoch)) {
                    if (!branch_node->GenerateDecision(peek_epoch)) {
                        decision_barrier = true;
                        break;
                    }
                }
                next = branch_node->PickBranch(peek_epoch);
                DEBUG("picked branch %p in peeking\n", next);
            }
            if (next == nullptr && !decision_barrier) {
                // hit end of graph, no need of peeking for future Issue()s
                scgraph->peekhead_hit_end = true;
                DEBUG("peeking hit end of SCGraph\n");
                break;
            } else if (decision_barrier) {
                DEBUG("peeking hit a decision barrier\n");
                break;
            } else
                scgraph->peekhead = next;

            // calculate arguments for this node from generator func if they
            // haven't been ready yet
            SyscallNode *syscall_node = static_cast<SyscallNode *>(next);
            if (!syscall_node->stage.Has(peek_epoch) ||
                syscall_node->stage.Get(peek_epoch) != STAGE_ARGREADY) {
                if (!syscall_node->GenerateArgs(peek_epoch)) {
                    DEBUG("arggen %s<%p>@%s not ready\n",
                          StreamStr(*syscall_node).c_str(), syscall_node,
                          StreamStr(peek_epoch).c_str());
                    break;
                } else {
                    DEBUG("arggen %s<%p>@%s successful\n",
                          StreamStr(*syscall_node).c_str(), syscall_node,
                          StreamStr(peek_epoch).c_str());
                }
            }
            assert(syscall_node->stage.Get(peek_epoch) == STAGE_ARGREADY);

            // decide if the next node is pre-issuable, and if so, prepare
            // for submission to uring
            if (edge == EDGE_WEAK)
                weak_state = true;
            if (IsForeactable(weak_state, syscall_node)) {
                // prepare the IOUring submission entry, set node stage to be
                // in-progress
                syscall_node->PrepAsync(peek_epoch);
                DEBUG("prepared %d %s<%p>@%s\n",
                      scgraph->num_prepared, StreamStr(*syscall_node).c_str(),
                      syscall_node, StreamStr(peek_epoch).c_str());
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
            scgraph->ring->SubmitAll();
            // do io_uring_submit()
            [[maybe_unused]] int num_submitted =
                io_uring_submit(scgraph->ring->Ring());
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
    assert(stage.Get(epoch) != STAGE_NOTREADY);  // must have done CheckArgs
    if (stage.Get(epoch) == STAGE_ARGREADY) {
        // if not pre-issued, invoke syscall synchronously
        DEBUG("sync-call %s<%p>@%s\n",
              StreamStr(*this).c_str(), this, StreamStr(epoch).c_str());
        TIMER_START(scgraph->TimerNameStr("sync-call"));
        long syscall_rc = SyscallSync(epoch, output_buf);
        rc.Set(epoch, syscall_rc);
        stage.Set(epoch, STAGE_FINISHED);
        TIMER_PAUSE(scgraph->TimerNameStr("sync-call"));
        DEBUG("sync-call finished rc %ld\n", syscall_rc);
    } else {
        // if has been pre-issued, process CQEs from io_uring until mine
        // completion is seen
        assert(stage.Get(epoch) != STAGE_PREPARED);
        DEBUG("ring-cmpl %s<%p>@%s\n",
              StreamStr(*this).c_str(), this, StreamStr(epoch).c_str());
        TIMER_START(scgraph->TimerNameStr("ring-cmpl"));
        if (stage.Get(epoch) == STAGE_ONTHEFLY)
            CmplAsync(epoch);
        assert(stage.Get(epoch) == STAGE_FINISHED);
        TIMER_PAUSE(scgraph->TimerNameStr("ring-cmpl"));
        DEBUG("ring-cmpl finished rc %ld\n", rc.Get(epoch));
        ReflectResult(epoch, output_buf);
        DEBUG("ring-cmpl result reflected\n");
    }

    // push frontier forward, reduce distance counters by 1
    scgraph->frontier = next_node;
    scgraph->peekhead_distance--;
    scgraph->prepared_distance--;
    DEBUG("pushed frontier -> %p\n", scgraph->frontier);

    assert(rc.Has(epoch));
    long rc_this_epoch = rc.Get(epoch);
    RemoveOneEpoch(epoch);
    return rc_this_epoch;
}


///////////////////////////////
// BranchNode implementation //
///////////////////////////////

BranchNode::BranchNode(unsigned node_id, std::string name, size_t num_children,
                       SCGraph *scgraph,
                       const std::unordered_set<int>& assoc_dims,
                       std::function<bool(const int *, int *)> arggen_func)
        : SCGraphNode(node_id, name, NODE_BRANCH, scgraph, assoc_dims),
          num_children(num_children), children{}, epoch_dims{},
          decision(assoc_dims), arggen_func(arggen_func) {
    assert(num_children > 1);
}


std::ostream& operator<<(std::ostream& s, const BranchNode& n) {
    s << "BranchNode{" << n.node_id
      << ",decision=" << n.decision
      << ",children=[";
    for (size_t i = 0; i < n.children.size(); ++i) {
        s << n.children[i] << "(" << n.epoch_dims[i] << ")";
        if (i < n.children.size() - 1)
            s << ",";
    }
    s << "]}";
    return s;
}


bool BranchNode::GenerateDecision(const EpochList& epoch) {
    assert(!decision.Has(epoch));

    int d;
    if (!arggen_func(epoch.RawArray(), &d))
        return false;
    
    assert(d >= 0 && d < static_cast<int>(num_children));
    decision.Set(epoch, d);
    return true;
}


SCGraphNode *BranchNode::PickBranch(EpochList& epoch, bool do_remove) {
    int d = decision.Get(epoch);
    assert(d >= 0 && d < static_cast<int>(num_children));
    SCGraphNode *child = children[d];

    // if removing decision value for this passing epoch
    if (do_remove)
        RemoveOneEpoch(epoch);

    // if is a back-pointing edge, increment corresponding epoch dimension
    if (epoch_dims[d] >= 0)
        epoch.Increment(epoch_dims[d]);

    return child;
}


void BranchNode::RemoveOneEpoch(const EpochList& epoch) {
    decision.Remove(epoch);
}

void BranchNode::ResetValuePools() {
    decision.Reset();
}


void BranchNode::AppendChild(SCGraphNode *child_node, int epoch_dim) {
    assert(children.size() < num_children);
    children.push_back(child_node);

    if (epoch_dim >= 0) {
        assert(epoch_dim < static_cast<int>(scgraph->total_dims));
        epoch_dims.push_back(epoch_dim);
    } else
        epoch_dims.push_back(-1);
}


}
