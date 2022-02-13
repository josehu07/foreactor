#include <liburing.h>

#include "scg_nodes.hpp"
#include "scg_graph.hpp"


namespace foreactor {


SCGraph::SCGraph(int sq_length, int pre_issue_depth)
        : pre_issue_depth(pre_issue_depth) {
    assert(pre_issue_depth >= 0);
    assert(pre_issue_depth <= sq_length);

    if (sq_length > 0) {
        int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
        assert(ret == 0);
        ring_initialized = true;
    }
}

SCGraph::~SCGraph() {
    if (ring_initialized)
        io_uring_queue_exit(&ring);
}


bool SCGraph::AddNode(uint64_t id, SCGraphNode *node) {
    if (nodes.find(id) != nodes.end())
        return false;
    assert(node != nullptr);
    nodes.insert(std::make_pair(id, node));
    node->scgraph = this;
    return true;
}

// Find a syscall node using the identifier.
SyscallNode *SCGraph::GetSyscallNode(uint64_t id) const {
    SCGraphNode *node = nodes.at(id);
    assert(node != nullptr);
    assert(node->node_type == NODE_SC_PURE ||
           node->node_type == NODE_SC_SEFF);
    return static_cast<SyscallNode *>(node);
}

// Deconstruct all nodes added to graph.
void SCGraph::DeleteAllNodes() {
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
    nodes.clear();
}


}
