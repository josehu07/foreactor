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
    
    TIMER_START(scgraph->TimerNameStr("build"));
}

void UnregisterSCGraph() {
    assert(active_scgraph != nullptr);
    DEBUG("unregister SCGraph @ %p\n", active_scgraph);

    active_scgraph = nullptr;
}


////////////////////////////
// SCGraph implementation //
////////////////////////////

SCGraph::SCGraph(unsigned graph_id, EpochListBase *frontier_epoch)
        : graph_id(graph_id), frontier_epoch(frontier_epoch) {
    graph_built = false;
    ring_associated = false;
}

SCGraph::~SCGraph() {
    // show sync-call, ring-cmpl, ring-submit, etc. timers, then reset them
    TIMER_PRINT(TimerNameStr("build"),       TIME_MICRO);
    TIMER_PRINT(TimerNameStr("clean"),       TIME_MICRO);
    TIMER_PRINT(TimerNameStr("sync-call"),   TIME_MICRO);
    TIMER_PRINT(TimerNameStr("ring-submit"), TIME_MICRO);
    TIMER_PRINT(TimerNameStr("ring-cmpl"),   TIME_MICRO);
    TIMER_RESET(TimerNameStr("build"));
    TIMER_RESET(TimerNameStr("clean"));
    TIMER_RESET(TimerNameStr("sync-call"));
    TIMER_RESET(TimerNameStr("ring-submit"));
    TIMER_RESET(TimerNameStr("ring-cmpl"));
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
    TIMER_PAUSE(TimerNameStr("build"));
    DEBUG("built SCGraph %u\n", graph_id);
}

bool SCGraph::IsBuilt() const {
    return graph_built;
}


void SCGraph::ClearAllInProgress() {
    TIMER_START(TimerNameStr("clean"));
    ring->ClearAllInProgress();
    graph_built = false;
    TIMER_PAUSE(TimerNameStr("clean"));
    DEBUG("cleaned SCGraph %u\n", graph_id);
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
        DEBUG("inited frontier -> %p\n", frontier);
    }
}


// GetFrontier implementation inside header due to genericity.


}
