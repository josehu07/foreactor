#include <tuple>
#include <unordered_map>
#include <assert.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_engine.hpp"
#include "scg_nodes.hpp"
#include "value_pool.hpp"


#pragma once


namespace foreactor {


// Each thread has at most one local active SCGraph at any time.
// With this thread-wide SCGraph pointer, a hijacked syscall checks the
// current frontier of the SCGraph on its thread to locate its node.
extern thread_local SCGraph *active_scgraph;


// Syscall graph class, consisting of a collection of well-defined nodes
// and DAG links between them.
class SCGraph {
    friend class SCGraphNode;
    friend class SyscallNode;
    friend class BranchNode;

    public:
        const unsigned graph_id;
        const unsigned total_dims;

        std::unordered_map<unsigned, SCGraphNode *> nodes;

        // How many syscalls we try to issue ahead of time.
        // Must be no larger than the length of SQ of uring.
        const int pre_issue_depth = 0;

        // Register this SCGraph as active on my thread. The SCGraph must have
        // been initialized and associated with the given IOUring queue pair.
        static void RegisterSCGraph(SCGraph *scgraph);

        // Unregister the currently active scgraph.
        static void UnregisterSCGraph();

    private:
        bool graph_built = false;
        IOEngine *engine = nullptr;

        // Number of uring-prepared syscalls not yet submitted, and the
        // distance of the earliest prepared SyscallNode from frontier.
        int num_prepared = 0;
        int prepared_distance = -1;

        // Current frontier node during execution. The frontier_epoch
        // field stores the EpochList of where the actual execution timeline
        // has reached.
        SCGraphNode *initial_frontier = nullptr;
        SCGraphNode *frontier = nullptr;
        EpochList frontier_epoch;

        // Current head of peeking. Each time a SyscallLnode->Issue() is
        // called, the peeking procedure starts here. The peekhead_distance
        // field stores how far away is the current peekhead node from the
        // current frontier node.
        SCGraphNode *peekhead = nullptr;
        EdgeType peekhead_edge = EDGE_BASE;
        EpochList peekhead_epoch;
        int peekhead_distance = -1;
        bool peekhead_hit_end = false;

        // Other states for correct bookkeeping.
        int weakedge_distance = -1;
        EpochList firstskip_epoch;

    public:
        Timer timer_get_frontier;
        Timer timer_check_args;
        Timer timer_peek_algo;
        Timer timer_sync_call;
        Timer timer_engine_submit;
        Timer timer_engine_cmpl;
        Timer timer_reflect_res;
        Timer timer_push_forward;
        Timer timer_clear_prog;
        Timer timer_reset_graph;

        inline std::string TimerIdStr(std::string name) {
            return "t" + tid_str + "-g" + std::to_string(graph_id) + "-" + name;
        }

        SCGraph() = delete;
        SCGraph(unsigned graph_id, unsigned total_dims, IOEngine *backend,
                int pre_issue_depth);
        ~SCGraph();

        // Checked at entrance of hijacked app function.
        void SetBuilt();
        [[nodiscard]] bool IsBuilt() const;

        // Reset graph state to initial state.
        void ResetToStart();
        void ClearAllReqs();

        // Add a new node into graph -- used at graph building.
        void AddNode(SCGraphNode *node, bool is_start = false);

        // Get current frontier node and frontier epoch.
        // NodeT must be one of those listed in syscalls.hpp.
        template <typename NodeT>
        std::tuple<NodeT *, const EpochList *> GetFrontier();

        // Visualization helper.
        [[maybe_unused]] void DumpDotImg(std::string filestem) const;
};


}


// Include template implementation in-place.
#include "scg_graph.tpl.hpp"
