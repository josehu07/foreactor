// Template implementation should be included in-place with the ".hpp".


#pragma once


namespace foreactor {


template <typename NodeT>
std::tuple<NodeT *, const EpochList *> SCGraph::GetFrontier() {
    static_assert(std::is_base_of<SyscallNode, NodeT>::value,
                  "NodeT must be derived from SyscallNode");
    static_assert(!std::is_same<SyscallNode, NodeT>::value,
                  "NodeT cannot be the base SyscallNode");

    // if current frontier is a BranchNode, it must have been decided
    // at this time point -- check and fetch the correct child
    TIMER_START(timer_get_frontier);
    while (frontier != nullptr &&
           frontier->node_type == NODE_BRANCH) {
        BranchNode *branch_node = static_cast<BranchNode *>(frontier);
        DEBUG("branch %s<%p>@%s in frontier\n",
              StreamStr(*branch_node).c_str(), branch_node,
              StreamStr(frontier_epoch).c_str());
        // may need to call .GenerateDecision() since BranchNodes do
        // not have hijacked control points like SyscallNodes, so the
        // decision values are not in our ValuePool yet -- but the
        // user-given generator function must be able to generate them now
        if (!branch_node->decision.Has(frontier_epoch)) {
            [[maybe_unused]] bool ready =
                branch_node->GenerateDecision(frontier_epoch);
            assert(ready);
        }
        // pick a branch and progress frontier_epoch
        frontier = branch_node->PickBranch(frontier_epoch, /*do_remove*/ true);
        DEBUG("picked branch '%s' in frontier\n",
              frontier == nullptr ? "end" : frontier->name.c_str());
    }
    TIMER_PAUSE(timer_get_frontier);

    assert(frontier != nullptr);
    assert(frontier->node_type == NODE_SC_PURE ||
           frontier->node_type == NODE_SC_SEFF);
    return std::make_tuple(static_cast<NodeT *>(frontier), &frontier_epoch);
}


}
