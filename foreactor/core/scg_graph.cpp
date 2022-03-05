#include <stdexcept>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"


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

SCGraph::SCGraph(unsigned graph_id)
        : graph_id(graph_id) {
    graph_built = false;
    ring_associated = false;
}

SCGraph::SCGraph(unsigned graph_id, IOUring *ring, int pre_issue_depth)
        : graph_id(graph_id), ring(ring), pre_issue_depth(pre_issue_depth) {
    assert(ring != nullptr);
    assert(ring->ring_initialized);

    assert(pre_issue_depth >= 0);
    assert(pre_issue_depth <= ring->sq_length);

    graph_built = false;
    ring_associated = true;
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


void SCGraph::SetBuilt() {
    graph_built = true;
    TIMER_PAUSE(TimerNameStr("build"));
    DEBUG("built SCGraph %u\n", graph_id);
}

bool SCGraph::IsBuilt() const {
    return graph_built;
}

void SCGraph::CleanNodes() {
    TIMER_START(TimerNameStr("clean"));
    for (auto& node : nodes) {
        if ((node->node_type == NODE_SC_PURE ||
             node->node_type == NODE_SC_SEFF)) {
            SyscallNode *syscall_node
                = static_cast<SyscallNode *>(node);
            // harvest in-progress syscall from uring
            if (syscall_node->stage == STAGE_PROGRESS)
                syscall_node->CmplAsync();
        }
        delete node;
    }

    nodes.clear();
    graph_built = false;
    TIMER_PAUSE(TimerNameStr("clean"));
    DEBUG("cleaned SCGraph %u\n", graph_id);
}


// GetFrontier implementation inside header due to genericity.


}
