// Template implementation should be included in-place with the ".hpp".


namespace foreactor {


template <typename NodeT>
std::tuple<NodeT *, const EpochList&> SCGraph::GetFrontier() {
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
    return std::make_tuple(static_cast<NodeT *>(frontier), frontier_epoch);
}


}
