#include <unordered_map>
#include <assert.h>
#include <liburing.h>

#include "io_uring.hpp"
#include "scg_nodes.hpp"


#ifndef __FOREACTOR_SCG_GRAPH_H__
#define __FOREACTOR_SCG_GRAPH_H__


namespace foreactor {


// Each thread has at most one local active SCGraph at any time.
// With this thread-wide SCGraph pointer, a hijacked syscall checks the
// current frontier of the SCGraph on its thread to locate its node.
extern thread_local SCGraph *active_scgraph;


// Register this SCGraph as active on my thread. The SCGraph must have
// been initialized and associated with the given IOUring queue pair.
void RegisterSCGraph(SCGraph *scgraph, const IOUring *ring);

// Unregister the currently active scgraph.
void UnregisterSCGraph();


// Syscall graph class, consisting of a collection of well-defined nodes
// and DAG links between them.
class SCGraph {
    friend class SCGraphNode;
    friend class SyscallNode;
    friend class BranchNode;

    private:
        unsigned graph_id;
        IOUring * const ring = nullptr;
        std::unordered_map<uint64_t, SCGraphNode *> nodes;

        // How many syscalls we try to issue ahead of time.
        // Must be no larger than the length of SQ of uring.
        const int pre_issue_depth = 0;

        // Current frontier node during execution.
        SCGraphNode *frontier = nullptr;

        struct io_uring *Ring() const {
            return ring->Ring();
        }

    public:
        SCGraph() = delete;
        SCGraph(unsigned graph_id, IOUring *ring, int pre_issue_depth);
        ~SCGraph();

        // Add a new node into graph.
        void AddNode(uint64_t id, SCGraphNode *node, bool is_start = false);

        // Get current frontier node.
        // Return type must be one of those listed in syscalls.hpp.
        template <typename NodeT>
        NodeT *GetFrontier() {
            static_assert(std::is_base_of<SyscallNode, NodeT>::value,
                          "NodeT must be derived from SyscallNode");
            static_assert(!std::is_same<SyscallNode, NodeT>::value,
                          "NodeT cannot be the base SyscallNode");

            if (frontier == nullptr)
                return nullptr;

            // if current frontier is a BranchNode, it must have been decided
            // at this time point -- check and fetch the correct child
            while (frontier->node_type == NODE_BRANCH) {
                BranchNode *branch_node = static_cast<BranchNode *>(frontier);
                frontier = branch_node->PickBranch();
                assert(frontier != nullptr);
            }

            assert(frontier->node_type == NODE_SC_PURE ||
                   frontier->node_type == NODE_SC_SEFF);
            return static_cast<NodeT *>(frontier);
        }

        // Find a syscall node using given identifier. Typically, this method
        // is not needed -- syscall invocations will be hijacked by the
        // library and its corresponding SyscallNode is located at current
        // frontier of the SCGraph.
        // Return type must be one of those listed in syscalls.hpp.
        template <typename NodeT>
        NodeT *GetNodeById(uint64_t id) const {
            static_assert(std::is_base_of<SyscallNode, NodeT>::value,
                          "NodeT must be derived from SyscallNode");
            static_assert(!std::is_same<SyscallNode, NodeT>::value,
                          "NodeT cannot be the base SyscallNode");

            SCGraphNode *node = nullptr;
            try {
                node = nodes.at(id);
            } catch (const std::out_of_range& e) {
                return nullptr;
            }

            assert(node != nullptr);
            assert(node->node_type == NODE_SC_PURE ||
                   node->node_type == NODE_SC_SEFF);
            return static_cast<NodeT *>(node);
        }

        friend void RegisterSCGraph(SCGraph *scgraph, const IOUring *ring);
        friend void UnregisterSCGraph();
};


}


#endif
