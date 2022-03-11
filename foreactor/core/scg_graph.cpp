#include <iostream>
#include <fstream>
#include <tuple>
#include <string>
#include <unordered_set>
#include <stdexcept>
#include <assert.h>
#include <unistd.h>
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

    // DumpDotImg("version_get");
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


///////////////////////////
// Visualization helpers //
///////////////////////////

static std::string NodeName(const SCGraphNode *node) {
    if (node == nullptr)
        return "end";
    return node->name;
}

void SCGraph::DumpDotImg(std::string filestem) const {
    std::ofstream fdot;
    fdot.open(filestem + ".dot");
    fdot << "digraph SCGraph {" << std::endl;
    fdot << "  graph [fontname=\"helvetica\"];" << std::endl;
    fdot << "  node  [fontname=\"helvetica\"];" << std::endl;
    fdot << "  edge  [fontname=\"helvetica\"];" << std::endl;

    std::unordered_set<SCGraphNode *> plotted;
    std::unordered_set<SCGraphNode *> pending;

    auto DumpSyscallNode = [&](const SyscallNode *node) {
        bool node_pure = node->node_type == NODE_SC_PURE;
        bool edge_weak = node->edge_type == EDGE_WEAK;
        
        if (!node_pure) {
            fdot << "  " << NodeName(node) << " [shape=box,style=bold];"
                 << std::endl;
        }

        fdot << "  " << NodeName(node) << " -> " << NodeName(node->next_node);
        if (edge_weak)
            fdot << " [style=dashed]";
        fdot << ";" << std::endl;

        if (plotted.find(node->next_node) == plotted.end() &&
            pending.find(node->next_node) == pending.end())
            pending.insert(node->next_node);
    };

    auto DumpBranchNode = [&](const BranchNode *node) {
        fdot << "  " << NodeName(node) << " [shape=diamond,label=\""
             << NodeName(node) << "?\"];" << std::endl;

        for (size_t i = 0; i < node->children.size(); ++i) {
            SCGraphNode *child = node->children[i];
            int dim_idx = node->epoch_dim_idx[i];

            fdot << "  " << NodeName(node);
            if (dim_idx >= 0)
                fdot << ":e";
            fdot << " -> " << NodeName(child);
            if (dim_idx >= 0)
                fdot << ":e";

            fdot << " [arrowhead=empty";
            if (dim_idx >= 0)
                fdot << ",dir=both,arrowtail=odot";
            fdot << "];" << std::endl;

            if (plotted.find(child) == plotted.end() &&
                pending.find(child) == pending.end())
                pending.insert(child);
        }
    };

    // special start node
    fdot << "  " << "start" << " [shape=plaintext];" << std::endl;
    fdot << "  " << "start" << " -> " << NodeName(initial_frontier) << ";"
         << std::endl;
    // special end node
    fdot << "  " << "end" << " [shape=plaintext];" << std::endl;

    pending.insert(initial_frontier);
    while (plotted.size() != nodes.size() && pending.size() > 0) {
        auto node = *(pending.begin());
        pending.erase(pending.begin());

        if (node == nullptr)
            continue;

        switch (node->node_type) {
        case NODE_SC_PURE:
        case NODE_SC_SEFF:
            DumpSyscallNode(static_cast<const SyscallNode *>(node));
            break;
        case NODE_BRANCH:
            DumpBranchNode(static_cast<const BranchNode *>(node));
            break;
        default:
            throw std::runtime_error("unknown node type");
        }

        if (plotted.find(node) == plotted.end())
            plotted.insert(node);
    }

    fdot << "}" << std::endl;
    fdot.close();

    std::string dot_cmd = "dot -T svg -o " + filestem + ".svg "
                          + filestem + ".dot";
    int rc __attribute__((unused)) = system(dot_cmd.c_str());
    assert(rc == 0);
}


}
