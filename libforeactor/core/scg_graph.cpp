#include <iostream>
#include <fstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <stdexcept>
#include <assert.h>
#include <unistd.h>

#include "debug.hpp"
#include "timer.hpp"
#include "scg_nodes.hpp"
#include "scg_graph.hpp"
#include "io_engine.hpp"
#include "value_pool.hpp"


namespace foreactor {


////////////////////////////////////////////////
// Register/unregister current active SCGraph //
////////////////////////////////////////////////

thread_local SCGraph *active_scgraph = nullptr;
thread_local SCGraph *paused_scgraph = nullptr;

void SCGraph::RegisterSCGraph(SCGraph *scgraph) {
    assert(active_scgraph == nullptr);
    assert(scgraph != nullptr);
    active_scgraph = scgraph;
    DEBUG("registered SCGraph @ %p as active\n", scgraph);
}

void SCGraph::UnregisterSCGraph() {
    assert(active_scgraph != nullptr);
    DEBUG("unregister SCGraph @ %p\n", active_scgraph);
    active_scgraph = nullptr;
}


////////////////////////////
// SCGraph implementation //
////////////////////////////

SCGraph::SCGraph(unsigned graph_id, unsigned total_dims, IOEngine *engine,
                 int pre_issue_depth)
        : graph_id(graph_id), total_dims(total_dims), nodes{},
          pre_issue_depth(pre_issue_depth),
          graph_built(false), engine(engine),
          num_prepared(0), prepared_distance(-1),
          initial_frontier(nullptr), frontier(nullptr),
          frontier_epoch(total_dims),
          peekhead(nullptr), peekhead_edge(EDGE_BASE),
          peekhead_epoch(total_dims), peekhead_distance(-1),
          peekhead_hit_end(false),
          weakedge_distance(-1), firstskip_epoch(total_dims),
          timer_get_frontier(TimerIdStr("get-frontier")),
          timer_check_args(TimerIdStr("check-args")),
          timer_peek_algo(TimerIdStr("peek-algo")),
          timer_sync_call(TimerIdStr("sync-call")),
          timer_engine_submit(TimerIdStr("engine-submit")),
          timer_engine_cmpl(TimerIdStr("engine-cmpl")),
          timer_reflect_res(TimerIdStr("reflect-res")),
          timer_push_forward(TimerIdStr("push-forward")),
          timer_clear_prog(TimerIdStr("clear-prog")),
          timer_reset_graph(TimerIdStr("reset-graph")) {
    assert(engine != nullptr);
    assert(pre_issue_depth >= 0);
}

SCGraph::~SCGraph() {
    // clean up and delete all nodes added
    for (auto&& [_, node] : nodes)
        delete node;
}


void SCGraph::SetBuilt() {
    assert(!graph_built);
    graph_built = true;
}

bool SCGraph::IsBuilt() const {
    return graph_built;
}


void SCGraph::ClearAllReqs() {
    TIMER_START(timer_clear_prog);
    engine->CleanUp();
    TIMER_PAUSE(timer_clear_prog);
    DEBUG("cleared SCGraph %u\n", graph_id);
}

void SCGraph::ResetToStart() {
    TIMER_START(timer_reset_graph);
    num_prepared = 0;
    prepared_distance = -1;

    frontier = initial_frontier;
    frontier_epoch.Reset();

    peekhead = nullptr;
    peekhead_edge = EDGE_BASE;
    peekhead_epoch.Reset();
    peekhead_distance = -1;
    peekhead_hit_end = false;

    weakedge_distance = -1;
    firstskip_epoch.Reset();
    
    for (auto&& [_, node] : nodes)
        node->ResetValuePools();
    TIMER_PAUSE(timer_reset_graph);

    TIMER_PRINT(timer_get_frontier,  TIME_MICRO);
    TIMER_PRINT(timer_check_args,    TIME_MICRO);
    TIMER_PRINT(timer_peek_algo,     TIME_MICRO);
    TIMER_PRINT(timer_sync_call,     TIME_MICRO);
    TIMER_PRINT(timer_engine_submit, TIME_MICRO);
    TIMER_PRINT(timer_engine_cmpl,   TIME_MICRO);
    TIMER_PRINT(timer_reflect_res,   TIME_MICRO);
    TIMER_PRINT(timer_push_forward,  TIME_MICRO);
    TIMER_PRINT(timer_clear_prog,    TIME_MICRO);
    TIMER_PRINT(timer_reset_graph,   TIME_MICRO);
    TIMER_RESET(timer_get_frontier);
    TIMER_RESET(timer_check_args);
    TIMER_RESET(timer_peek_algo);
    TIMER_RESET(timer_sync_call);
    TIMER_RESET(timer_engine_submit);
    TIMER_RESET(timer_engine_cmpl);
    TIMER_RESET(timer_reflect_res);
    TIMER_RESET(timer_push_forward);
    TIMER_RESET(timer_clear_prog);
    TIMER_RESET(timer_reset_graph);
}


void SCGraph::AddNode(SCGraphNode *node, bool is_start) {
    assert(node != nullptr);
    assert(!nodes.contains(node->node_id));
    assert(node->scgraph == this);

    nodes[node->node_id] = node;
    DEBUG("added node %s<%p>\n", StreamStr(*node).c_str(), node);

    if (is_start) {
        assert(frontier == nullptr);
        assert(initial_frontier == nullptr);
        frontier = node;
        initial_frontier = node;
        DEBUG("inited frontier -> %p\n", frontier);
    }
}


// GetFrontier implementation inside header due to templating.


///////////////////////////
// Visualization helpers //
///////////////////////////

static inline std::string NodeName(const SCGraphNode *node) {
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

        if ((!plotted.contains(node->next_node)) &&
            (!pending.contains(node->next_node)))
            pending.insert(node->next_node);
    };

    auto DumpBranchNode = [&](const BranchNode *node) {
        fdot << "  " << NodeName(node) << " [shape=diamond,label=\""
             << NodeName(node) << "?\"];" << std::endl;

        for (size_t i = 0; i < node->children.size(); ++i) {
            SCGraphNode *child = node->children[i];
            int dim_idx = node->epoch_dims[i];

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

            if ((!plotted.contains(child)) &&
                (!pending.contains(child)))
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

        if (!plotted.contains(node))
            plotted.insert(node);
    }

    fdot << "}" << std::endl;
    fdot.close();

    std::string dot_cmd = "dot -T svg -o " + filestem + ".svg "
                          + filestem + ".dot";
    [[maybe_unused]] int rc = system(dot_cmd.c_str());
    assert(rc == 0);
}


}
