#include <tuple>
#include <unordered_set>
#include <assert.h>

#include "debug.hpp"
#include "io_engine.hpp"
#include "scg_nodes.hpp"


namespace foreactor {


IOEngine::EntryId IOEngine::EncodeEntryId(SyscallNode *node, int epoch_sum) {
    // relies on the fact that Linux on x86_64 only uses 48-bit virtual
    // addresses currently. We encode the top 48 bits of the uint64_t
    // identifier as the node pointer, and the lower 16 bits as the epoch_sum
    // number.
    assert(epoch_sum >= 0 && epoch_sum < (1 << 16));
    uint64_t node_bits = reinterpret_cast<uint64_t>(node);
    uint64_t epoch_bits = static_cast<uint64_t>(epoch_sum);
    uint64_t entry_id = (node_bits << 16) | (epoch_bits & ((1 << 16) - 1));
    return entry_id;
}

std::tuple<SyscallNode *, int> IOEngine::DecodeEntryId(EntryId entry_id) {
    uint64_t node_bits = entry_id >> 16;
    uint64_t epoch_bits = entry_id & ((1 << 16) - 1);
    return std::make_tuple(reinterpret_cast<SyscallNode *>(node_bits),
                           static_cast<int>(epoch_bits));
}


}
