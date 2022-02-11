#include <iostream>
#include <unordered_map>
#include <assert.h>
#include <liburing.h>

#include "scg_nodes.hpp"


#ifndef __FOREACTOR_SCG_GRAPH_H__
#define __FOREACTOR_SCG_GRAPH_H__


namespace foreactor {


class SCGraph {
    friend class SCGraphNode;
    friend class SyscallNode;
    friend class BranchNode;

    private:
        struct io_uring ring;
        bool ring_initialized = false;

        std::unordered_map<uint64_t, SCGraphNode *> nodes;

    protected:
        // How many syscalls we try to issue ahead of time.
        // Must be no larger than the length of SQ of uring.
        int pre_issue_depth = 0;

        struct io_uring *Ring() {
            return &ring;
        }

    public:
        SCGraph() = delete;
        SCGraph(int sq_length, int pre_issue_depth)
                : pre_issue_depth(pre_issue_depth) {
            assert(pre_issue_depth >= 0);
            assert(pre_issue_depth <= sq_length);

            if (sq_length > 0) {
                int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
                assert(ret == 0);
                ring_initialized = true;
            }
        }

        ~SCGraph() {
            if (ring_initialized)
                io_uring_queue_exit(&ring);
        }

        // Add a new node into graph, effectively assigning the io_uring
        // instance to it.
        bool AddNode(uint64_t id, SCGraphNode *node) {
            if (nodes.find(id) != nodes.end())
                return false;
            assert(node != nullptr);
            nodes.insert(std::make_pair(id, node));
            node->scgraph = this;
            return true;
        }

        // Find a node using the identifier and issue the syscall. Must give
        // an identifier of added syscall node.
        long IssueSyscall(uint64_t id) const {
            SCGraphNode *node = nodes.at(id);
            assert(node != nullptr);
            assert(node->node_type == NODE_SYSCALL_PURE ||
                   node->node_type == NODE_SYSCALL_SIDE);
            return static_cast<SyscallNode *>(node)->Issue();
        }

        // Deconstruct all nodes added to graph.
        void DeleteAllNodes() {
            for (auto& [id, node] : nodes)
                delete node;
            nodes.clear();
        }
};


}


#endif
