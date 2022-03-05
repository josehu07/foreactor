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

    private:
        const unsigned graph_id;
        bool graph_built = false;
        bool ring_associated = false;
        IOUring *ring = nullptr;
        std::unordered_set<SCGraphNode *> nodes;

        // How many syscalls we try to issue ahead of time.
        // Must be no larger than the length of SQ of uring.
        int pre_issue_depth = 0;

        // Current frontier node during execution. The frontier_epoch
        // field stores the EpochList of where the actual execution timeline
        // has hit. SyscallNode->Issue() might create temporary EpochLists
        // that are ahead of frontier_epoch for pre-issuing purposes.
        SCGraphNode *frontier = nullptr;
        EpochList *frontier_epoch;

        struct io_uring *Ring() const {
            assert(ring_associated);
            return ring->Ring();
        }

        std::string TimerNameStr(std::string timer) const {
            return "g" + std::to_string(graph_id) + "-" + timer;
        }

    public:
        SCGraph() = delete;
        SCGraph(unsigned graph_id, EpochList *frontier_epoch);
        ~SCGraph();

        // Associate the graph to an IOUring instance.
        void AssociateRing(IOUring *ring, int pre_issue_depth);
        bool IsRingAssociated() const;

        // Add a new node into graph -- used at graph building.
        void AddNode(SCGraphNode *node, bool is_start = false);

        // Set to "built" at the entrance of hijacked function, cleaned at
        // the exit.
        void SetBuilt();
        bool IsBuilt() const;
        void CleanNodes();

        // Get current frontier node and frontier epoch.
        // NodeT must be one of those listed in syscalls.hpp.
        template <typename NodeT>
        std::tuple<NodeT *, EpochList *> GetFrontier() {
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
