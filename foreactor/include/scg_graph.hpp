#include <unordered_map>
#include <assert.h>
#include <liburing.h>

#include "io_uring.hpp"
#include "scg_nodes.hpp"


#ifndef __FOREACTOR_SCG_GRAPH_H__
#define __FOREACTOR_SCG_GRAPH_H__


namespace foreactor {


class SCGraph {
    friend class SCGraphNode;
    friend class SyscallNode;
    friend class BranchNode;

    private:
        IOUring *ring = nullptr;
        std::unordered_map<uint64_t, SCGraphNode *> nodes;

    protected:
        // How many syscalls we try to issue ahead of time.
        // Must be no larger than the length of SQ of uring.
        int pre_issue_depth = 0;

        struct io_uring *Ring() {
            return ring->Ring();
        }

    public:
        SCGraph() = delete;
        SCGraph(IOUring *ring, int pre_issue_depth);
        ~SCGraph();

        // Add a new node into graph, effectively assigning the io_uring
        // instance to it.
        bool AddNode(uint64_t id, SCGraphNode *node);

        // Find a syscall node using given identifier.
        SyscallNode *GetSyscallNode(uint64_t id) const;
};


}


#endif
