#include <tuple>
#include <unordered_set>
#include <assert.h>
#include <liburing.h>

#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "value_pool.hpp"


#ifndef __FOREACTOR_SCG_GRAPH_H__
#define __FOREACTOR_SCG_GRAPH_H__


namespace foreactor {


// Each thread has at most one local active SCGraph at any time.
// With this thread-wide SCGraph pointer, a hijacked syscall checks the
// current frontier of the SCGraph on its thread to locate its node.
extern thread_local SCGraph *active_scgraph;

// Register this SCGraph as active on my thread. The SCGraph must have
// been initialized and associated with the given IOUring queue pair.
void RegisterSCGraph(SCGraph *scgraph);

// Unregister the currently active scgraph.
void UnregisterSCGraph();


// Syscall graph class, consisting of a collection of well-defined nodes
// and DAG links between them.
class SCGraph {
    friend class SCGraphNode;
    friend class SyscallNode;
    friend class BranchNode;

    public:
        const unsigned graph_id;
        const unsigned max_dims;

    private:
        bool graph_built = false;
        bool ring_associated = false;
        IOUring *ring = nullptr;
        std::unordered_set<SCGraphNode *> nodes;

        // How many syscalls we try to issue ahead of time.
        // Must be no larger than the length of SQ of uring.
        int pre_issue_depth = 0;

        // Number of uring-prepared syscalls not yet submitted, and the
        // distance of the earliest prepared SyscallNode from frontier.
        int num_prepared = 0;
        int prepared_distance = -1;

        // Current frontier node during execution. The frontier_epoch
        // field stores the EpochList of where the actual execution timeline
        // has hit. SyscallNode->Issue() might create temporary EpochLists
        // that are ahead of frontier_epoch for pre-issuing purposes.
        SCGraphNode *initial_frontier = nullptr;
        SCGraphNode *frontier = nullptr;
        EpochListBase *frontier_epoch = nullptr;

        // Current head of peeking. Each time a SyscallLnode->Issue() is
        // called, the peeking procedure starts here. The peekhead_distance
        // field stores how far away is the current peekhead node from the
        // current frontier node.
        SCGraphNode *peekhead = nullptr;
        EpochListBase *peekhead_epoch = nullptr;
        EdgeType peekhead_edge = EDGE_BASE;
        int peekhead_distance = -1;
        bool peekhead_hit_end = false;

        struct io_uring *Ring() const {
            assert(ring_associated);
            return ring->Ring();
        }

    public:
        SCGraph() = delete;
        SCGraph(unsigned graph_id, unsigned max_dims);
        ~SCGraph();

        std::string TimerNameStr(std::string timer) const {
            return "g" + std::to_string(graph_id) + "-" + timer;
        }

        void StartTimer(std::string timer) const;
        void PauseTimer(std::string timer) const;

        // Associate the graph to an IOUring instance.
        void AssociateRing(IOUring *ring, int pre_issue_depth);
        bool IsRingAssociated() const;

        // Set to "built" at the entrance of hijacked function, cleaned at
        // the exit.
        void SetBuilt();
        bool IsBuilt() const;

        // Reset graph state to start, clear epoch numbers in frontier_epoch
        // and reset frontier pointer to start node.
        void ResetToStart();
        void ClearAllInProgress();      // TODO: do GC instead of this

        // Add a new node into graph -- used at graph building.
        void AddNode(SCGraphNode *node, bool is_start = false);

        // Get current frontier node and frontier epoch.
        // NodeT must be one of those listed in syscalls.hpp.
        template <typename NodeT>
        std::tuple<NodeT *, EpochListBase *> GetFrontier() {
            static_assert(std::is_base_of<SyscallNode, NodeT>::value,
                          "NodeT must be derived from SyscallNode");
            static_assert(!std::is_same<SyscallNode, NodeT>::value,
                          "NodeT cannot be the base SyscallNode");

            // if current frontier is a BranchNode, it must have been decided
            // at this time point -- check and fetch the correct child
            while (frontier != nullptr &&
                   frontier->node_type == NODE_BRANCH) {
                BranchNode *branch_node = static_cast<BranchNode *>(frontier);
                frontier = branch_node->PickBranch(frontier_epoch);
            }

            if (frontier == nullptr)
                return std::make_tuple(nullptr, frontier_epoch);

            assert(frontier->node_type == NODE_SC_PURE ||
                   frontier->node_type == NODE_SC_SEFF);
            return std::make_tuple(static_cast<NodeT *>(frontier),
                                   frontier_epoch);
        }

        friend void RegisterSCGraph(SCGraph *scgraph);
        friend void UnregisterSCGraph();
};


}


#endif
