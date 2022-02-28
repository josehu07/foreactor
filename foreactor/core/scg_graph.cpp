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

void RegisterSCGraph(SCGraph *scgraph, const IOUring *ring) {
    assert(active_scgraph == nullptr);
    assert(scgraph != nullptr);
    assert(scgraph->ring == ring);
    assert(scgraph->frontier != nullptr);
    active_scgraph = scgraph;
    DEBUG("registered SCGraph @ %p frontier %p as active\n",
          scgraph, scgraph->frontier);
    TIMER_PAUSE("g"+std::to_string(scgraph->graph_id)+"-build");
}

void UnregisterSCGraph() {
    assert(active_scgraph != nullptr);
    DEBUG("unregister SCGraph @ %p\n", active_scgraph);
    active_scgraph = nullptr;
}


////////////////////////////
// SCGraph implementation //
////////////////////////////

SCGraph::SCGraph(unsigned graph_id, IOUring *ring, int pre_issue_depth)
        : graph_id(graph_id), ring(ring), pre_issue_depth(pre_issue_depth) {
    assert(ring != nullptr);
    assert(ring->ring_initialized);

    assert(pre_issue_depth >= 0);
    assert(pre_issue_depth <= ring->sq_length);

    TIMER_START("g"+std::to_string(graph_id)+"-build");
}

SCGraph::~SCGraph() {
    // delete all nodes and their internal buffers if any
    TIMER_START("g"+std::to_string(graph_id)+"-clean");
    for (auto& [id, node] : nodes) {
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
    TIMER_PAUSE("g"+std::to_string(graph_id)+"-clean");

    // show sync-call, ring-cmpl, ring-submit, etc. timers, then reset them
    TIMER_PRINT("g"+std::to_string(graph_id)+"-build",       TIME_MICRO);
    TIMER_PRINT("g"+std::to_string(graph_id)+"-clean",       TIME_MICRO);
    TIMER_PRINT("g"+std::to_string(graph_id)+"-sync-call",   TIME_MICRO);
    TIMER_PRINT("g"+std::to_string(graph_id)+"-ring-submit", TIME_MICRO);
    TIMER_PRINT("g"+std::to_string(graph_id)+"-ring-cmpl",   TIME_MICRO);
    TIMER_RESET("g"+std::to_string(graph_id)+"-build");
    TIMER_RESET("g"+std::to_string(graph_id)+"-clean");
    TIMER_RESET("g"+std::to_string(graph_id)+"-sync-call");
    TIMER_RESET("g"+std::to_string(graph_id)+"-ring-submit");
    TIMER_RESET("g"+std::to_string(graph_id)+"-ring-cmpl");

    nodes.clear();
}


void SCGraph::AddNode(uint64_t id, SCGraphNode *node, bool is_start) {
    assert(nodes.find(id) == nodes.end());
    assert(node != nullptr);

    nodes.insert(std::make_pair(id, node));
    node->scgraph = this;
    DEBUG("added node %s<%p>\n", StreamStr<SCGraphNode>(node).c_str(), node);

    if (is_start) {
        assert(frontier == nullptr);
        frontier = node;
        DEBUG("inited frontier -> %p\n", frontier);
    }
}


// GetNode implementations inside header due to genericity.


}
