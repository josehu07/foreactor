#include <tuple>
#include <unordered_set>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_uring.hpp"
#include "scg_nodes.hpp"


namespace foreactor {


////////////////////////////
// IOUring implementation //
////////////////////////////

IOUring::IOUring()
        : sq_length(0) {
    ring_initialized = false;
}

IOUring::~IOUring() {
    if (ring_initialized) {
        TIMER_START("uring-exit");
        io_uring_queue_exit(&ring);
        DEBUG("destroyed IOUring @ %p\n", &ring);
        TIMER_PAUSE("uring-exit");
    }

    // TIMER_PRINT("uring-init", TIME_MICRO);
    // TIMER_PRINT("uring-exit", TIME_MICRO);
    // TIMER_RESET("uring-init");
    // TIMER_RESET("uring-exit");
    
    TIMER_PRINT_ALL(TIME_MICRO);
}


EntryId IOUring::EncodeEntryId(SyscallNode *node, int epoch) {
    // relies on the fact that Linux on x86_64 only uses 48-bit virtual
    // addresses currently. We encode the top 48 bits of the uint64_t
    // user_data as the node pointer, and the lower 16 bits as the epoch
    // number.
    assert(epoch > 0);
    PANIC_IF(epoch >= (1 << 16), "epoch %d larger than limit %d\n",
             epoch, (1 << 16));
    uint64_t node_bits = static_cast<uint64_t>(node);
    uint64_t epoch_bits = static_cast<uint64_t>(epoch);
    uint64_t entry_id = (node_bits << 16) | (epoch_bits & ((1 << 16) - 1));
    return entry_id;
}

std::tuple<SyscallNode *, int> IOUring::DecodeEntryId(EntryId entry_id) {
    uint64_t node_bits = entry_id >> 16;
    uint64_t epoch_bits = entry_id & ((1 << 16) - 1);
    return std::make_tuple(reinterpret_cast<SyscallNode *>(node_bits),
                           static_cast<int>(epoch_bits));
}


void IOUring::Initialize(int sq_length_) {
    assert(!ring_initialized);
    sq_length = sq_length_;

    TIMER_START("uring-init");
    if (sq_length > 0) {
        int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
        if (ret != 0) {
            DEBUG("initilize IOUring failed %d\n", ret);
            return;
        }

        ring_initialized = true;
        DEBUG("initialized IOUring @ %p sq_length %d\n", &ring, sq_length);
    }
    TIMER_PAUSE("uring-init");
}

bool IOUring::IsInitialized() const {
    return ring_initialized;
}


void IOUring::Prepare(EntryId entry_id) {
    assert(prepared.find(entry_id) == prepared.end());
    prepared.insert(entry_id);
}

void IOUring::SubmitAll() {
    for (EntryId entry_id : prepared) {
        auto [node, epoch] = DecodeEntryId(entry_id);
        assert(onthefly.find(entry_id) == onthefly.end());
        assert(node->stage.Get(epoch) == STAGE_PREPARED);

        struct io_uring_sqe *sqe = io_uring_get_sqe(Ring());
        assert(sqe != nullptr);
        io_uring_sqe_set_data(sqe, entry_id);

        // do syscall-specific io_uring_prep_xxx() here
        node->PrepUringSqe(epoch, sqe);

        // use async flag to force using kernel workers
        // TODO: tune this option on/off
        sqe->flags |= IOSQE_ASYNC;

        onthefly.insert(entry_id);
        node->stage.Set(epoch, STAGE_PROGRESS);
    }

    prepared.clear();
    // io_uring_submit() will be done by SyscallNode->Issue()
}

void IOUring::RemoveOne(EntryId entry_id) {
    assert(onthefly.find(entry_id) != onthefly.end());
    onthefly.erase(entry_id);
}


struct io_uring_cqe *IOUring::WaitOneCqe() {
    struct io_uring_cqe *cqe;
    int ret = io_uring_wait_cqe(Ring(), &cqe);
    if (ret != 0) {
        DEBUG("wait CQE failed %d\n", ret);
        assert(false);
    }
    return cqe;
}

void IOUring::SeenOneCqe(struct io_uring_cqe *cqe) {
    io_uring_cqe_seen(Ring(), cqe);
}

EntryId IOUring::GetCqeEntryId(struct io_uring_cqe *cqe) {
    return io_uring_cqe_get_data(cqe);
}


void IOUring::CleanUp() {
    // clear the prepared set
    prepared.clear();

    // call CmplAsync() on all requests in the onthefly set
    while (onthefly.size() > 0) {
        EntryId entry_id = *(onthefly.begin());
        auto [node, epoch] = DecodeEntryId(entry_id);
        assert(node->stage.Get(epoch) == STAGE_PROGRESS);
        node->CmplAsync(epoch);     // entry_id will be removed here
    }
}


}
