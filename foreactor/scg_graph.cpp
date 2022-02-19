#include <stdexcept>
#include <liburing.h>

#include "scg_nodes.hpp"
#include "scg_graph.hpp"


namespace foreactor {


SCGraph::SCGraph(IOUring *ring, int pre_issue_depth)
        : ring(ring), pre_issue_depth(pre_issue_depth) {
    assert(ring != nullptr);
    assert(ring->ring_initialized);

    assert(pre_issue_depth >= 0);
    assert(pre_issue_depth <= ring->sq_length);
}

SCGraph::~SCGraph() {
    // delete all nodes and their internal buffers if any
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
    SCGraphNode *node = nullptr;
    try {
        node = nodes.at(id);
    } catch (const std::out_of_range& e) {
        return nullptr;
    }

    assert(node != nullptr);
    assert(node->node_type == NODE_SC_PURE ||
           node->node_type == NODE_SC_SEFF);
    return static_cast<SyscallNode *>(node);
}


}
