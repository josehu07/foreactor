#include <tuple>
#include <unordered_set>
#include <assert.h>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_uring.hpp"
#include "scg_nodes.hpp"


namespace foreactor {


////////////////////////////
// IOUring implementation //
////////////////////////////

IOUring::IOUring(int sq_length)
        : sq_length(sq_length), prepared{}, onthefly{} {
    assert(sq_length >= 0);

    TIMER_START("uring-init");
    if (sq_length > 0) {
        int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
        if (ret != 0) {
            DEBUG("initilize IOUring failed %d\n", ret);
            return;
        }
        DEBUG("inited IOUring @ %p sq_length %d\n", &ring, sq_length);
    }
    TIMER_PAUSE("uring-init");
}

IOUring::~IOUring() {
    TIMER_START("uring-exit");
    io_uring_queue_exit(&ring);
    DEBUG("destroyed IOUring @ %p\n", &ring);
    TIMER_PAUSE("uring-exit");

    TIMER_PRINT("uring-init", TIME_MICRO);
    TIMER_PRINT("uring-exit", TIME_MICRO);
    TIMER_RESET("uring-init");
    TIMER_RESET("uring-exit");
    
    // TIMER_PRINT_ALL(TIME_MICRO);
}


struct io_uring *IOUring::Ring() {
    return &ring;
}


IOUring::EntryId IOUring::EncodeEntryId(SyscallNode *node, int epoch_sum) {
    // relies on the fact that Linux on x86_64 only uses 48-bit virtual
    // addresses currently. We encode the top 48 bits of the uint64_t
    // user_data as the node pointer, and the lower 16 bits as the epoch_sum
    // number.
    assert(epoch_sum >= 0 && epoch_sum < (1 << 16));
    uint64_t node_bits = reinterpret_cast<uint64_t>(node);
    uint64_t epoch_bits = static_cast<uint64_t>(epoch_sum);
    uint64_t entry_id = (node_bits << 16) | (epoch_bits & ((1 << 16) - 1));
    return entry_id;
}

std::tuple<SyscallNode *, int> IOUring::DecodeEntryId(EntryId entry_id) {
    uint64_t node_bits = entry_id >> 16;
    uint64_t epoch_bits = entry_id & ((1 << 16) - 1);
    return std::make_tuple(reinterpret_cast<SyscallNode *>(node_bits),
                           static_cast<int>(epoch_bits));
}


void IOUring::Prepare(EntryId entry_id) {
    assert(!prepared.contains(entry_id));
    prepared.insert(entry_id);
}

void IOUring::SubmitAll() {
    for (EntryId entry_id : prepared) {
        auto [node, epoch_sum] = DecodeEntryId(entry_id);
        assert(!onthefly.contains(entry_id));
        assert(node->stage.Get(epoch_sum) == STAGE_PREPARED);

        struct io_uring_sqe *sqe = io_uring_get_sqe(Ring());
        assert(sqe != nullptr);
        io_uring_sqe_set_data(sqe, reinterpret_cast<void *>(entry_id));

        // do syscall-specific io_uring_prep_xxx() here
        node->PrepUringSqe(epoch_sum, sqe);

        // use async flag to force using kernel workers
        // TODO: tune this option on/off
        sqe->flags |= IOSQE_ASYNC;

        onthefly.insert(entry_id);
        node->stage.Set(epoch_sum, STAGE_ONTHEFLY);
    }

    prepared.clear();
    // io_uring_submit() will be done by SyscallNode->Issue()
}

void IOUring::RemoveOne(EntryId entry_id) {
    assert(onthefly.contains(entry_id));
    onthefly.erase(entry_id);
}


struct io_uring_cqe *IOUring::WaitOneCqe() {
    struct io_uring_cqe *cqe;
    int ret = io_uring_wait_cqe(Ring(), &cqe);
    if (ret != 0) {
        DEBUG("wait CQE failed %d\n", ret);
        // TODO: handle this more elegantly
        assert(false);
    }
    return cqe;
}

void IOUring::SeenOneCqe(struct io_uring_cqe *cqe) {
    io_uring_cqe_seen(Ring(), cqe);
}

IOUring::EntryId IOUring::GetCqeEntryId(struct io_uring_cqe *cqe) {
    return reinterpret_cast<EntryId>(io_uring_cqe_get_data(cqe));
}


void IOUring::CleanUp() {
    // clear the prepared set
    prepared.clear();

    // call CmplAsync() on all requests in the onthefly set
    while (onthefly.size() > 0) {
        EntryId entry_id = *(onthefly.begin());
        auto [node, epoch_sum] = DecodeEntryId(entry_id);
        assert(node->stage.Get(epoch_sum) == STAGE_ONTHEFLY);
        node->CmplAsync(epoch_sum);     // entry_id will be removed here
    }
}


}
