#include <tuple>
#include <unordered_set>
#include <stdexcept>
#include <assert.h>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "value_pool.hpp"


namespace foreactor {


///////////////////////////////////////////////////////////////
// For plugins to register/unregister current active SCGraph //
///////////////////////////////////////////////////////////////

thread_local SCGraph *active_scgraph = nullptr;

void RegisterSCGraph(SCGraph *scgraph) {
    assert(active_scgraph == nullptr);
    assert(scgraph != nullptr);
    active_scgraph = scgraph;
    DEBUG("registered SCGraph @ %p as active\n", scgraph);
}

void UnregisterSCGraph() {
    assert(active_scgraph != nullptr);
    DEBUG("unregister SCGraph @ %p\n", active_scgraph);
    active_scgraph = nullptr;
}


////////////////////////////
// SCGraph implementation //
////////////////////////////

SCGraph::SCGraph(unsigned graph_id, unsigned max_dims)
        : graph_id(graph_id), max_dims(max_dims),
          graph_built(false), ring_associated(false),
          num_prepared(0), prepared_distance(-1),
          frontier_epoch(EpochListBase::New(max_dims)),
          peekhead_epoch(EpochListBase::New(max_dims)),
          peekhead_distance(-1), peekhead_hit_end(false) {
}

SCGraph::~SCGraph() {
    TIMER_PRINT(TimerNameStr("build-graph"), TIME_MICRO);
    TIMER_RESET(TimerNameStr("build-graph"));

    EpochListBase::Delete(frontier_epoch);
    EpochListBase::Delete(peekhead_epoch);

    // clean up and delete all nodes added
    for (auto& node : nodes)
        delete node;
}


void SCGraph::StartTimer(std::string timer) const {
    TIMER_START(TimerNameStr(timer));
}

void SCGraph::PauseTimer(std::string timer) const {
    TIMER_PAUSE(TimerNameStr(timer));
}


void SCGraph::AssociateRing(IOUring *ring_, int pre_issue_depth_) {
    assert(ring_ != nullptr);
    assert(ring_->ring_initialized);

    assert(pre_issue_depth_ >= 0);
    assert(pre_issue_depth_ <= ring_->sq_length);

    ring = ring_;
    pre_issue_depth = pre_issue_depth_;
    ring_associated = true;
    DEBUG("associate SCGraph %u to IOUring %p\n", graph_id, ring);
}

bool SCGraph::IsRingAssociated() const {
    return ring_associated;
}


void SCGraph::SetBuilt() {
    graph_built = true;
    DEBUG("built SCGraph %u\n", graph_id);
}

bool SCGraph::IsBuilt() const {
    return graph_built;
}


void SCGraph::ResetToStart() {
    num_prepared = 0;
    prepared_distance = -1;

    frontier = initial_frontier;
    frontier_epoch->ClearEpochs();
    
    peekhead = nullptr;
    peekhead_edge = EDGE_BASE;
    peekhead_epoch->ClearEpochs();
    peekhead_distance = -1;
    peekhead_hit_end = false;
}

void SCGraph::ClearAllInProgress() {
    TIMER_START(TimerNameStr("clear-prog"));
    ring->ClearAllInProgress();
    TIMER_PAUSE(TimerNameStr("clear-prog"));
    DEBUG("cleared SCGraph %u\n", graph_id);

    TIMER_PRINT(TimerNameStr("pool-flush"),  TIME_MICRO);
    TIMER_PRINT(TimerNameStr("pool-clear"),  TIME_MICRO);
    TIMER_PRINT(TimerNameStr("peek-algo"),   TIME_MICRO);
    TIMER_PRINT(TimerNameStr("ring-submit"), TIME_MICRO);
    TIMER_PRINT(TimerNameStr("sync-call"),   TIME_MICRO);
    TIMER_PRINT(TimerNameStr("ring-cmpl"),   TIME_MICRO);
    TIMER_PRINT(TimerNameStr("clear-prog"),  TIME_MICRO);
    TIMER_RESET(TimerNameStr("pool-flush"));
    TIMER_RESET(TimerNameStr("pool-clear"));
    TIMER_RESET(TimerNameStr("peek-algo"));
    TIMER_RESET(TimerNameStr("ring-submit"));
    TIMER_RESET(TimerNameStr("sync-call"));
    TIMER_RESET(TimerNameStr("ring-cmpl"));
    TIMER_RESET(TimerNameStr("clear-prog"));
}


void SCGraph::AddNode(SCGraphNode *node, bool is_start) {
    assert(node != nullptr);
    assert(nodes.find(node) == nodes.end());

    nodes.insert(node);
    node->scgraph = this;
    DEBUG("added node %s<%p>\n", StreamStr<SCGraphNode>(node).c_str(), node);

    if (is_start) {
        assert(frontier == nullptr);
        frontier = node;
        initial_frontier = node;
        DEBUG("inited frontier -> %p\n", frontier);
    }
}


// GetFrontier implementation inside header due to genericity.


}
