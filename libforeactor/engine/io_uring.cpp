#include <tuple>
#include <unordered_set>
#include <assert.h>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_engine.hpp"
#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "value_pool.hpp"


namespace foreactor {


IOUring::IOUring(int sq_length)
        : sq_length(sq_length) {
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


void IOUring::Prepare(SyscallNode *node, int epoch_sum) {
    IOUring::EntryId entry_id = IOUring::EncodeEntryId(node, epoch_sum);
    
    assert(!prepared.contains(entry_id));
    prepared.insert(entry_id);
}

int IOUring::SubmitAll() {
    // move everything from prepared set to onthefly set
    for (EntryId entry_id : prepared) {
        auto [node, epoch_sum] = DecodeEntryId(entry_id);
        assert(!onthefly.contains(entry_id));
        assert(node->stage.Get(epoch_sum) == STAGE_PREPARED);

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
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

    return io_uring_submit(&ring);
}

std::tuple<SyscallNode *, int, long> IOUring::CompleteOne() {
    struct io_uring_cqe *cqe;
    int ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret != 0) {
        DEBUG("wait CQE failed %d\n", ret);
        // TODO: handle this more elegantly
        assert(false);
    }

    EntryId entry_id = reinterpret_cast<EntryId>(io_uring_cqe_get_data(cqe));
    auto [node, epoch_sum] = DecodeEntryId(entry_id);
    assert(node->stage.Get(epoch_sum) == STAGE_ONTHEFLY);

    io_uring_cqe_seen(&ring, cqe);

    assert(onthefly.contains(entry_id));
    onthefly.erase(entry_id);

    return std::make_tuple(node, epoch_sum, cqe->res);
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