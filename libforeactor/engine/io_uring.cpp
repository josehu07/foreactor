#include <tuple>
#include <vector>
#include <unordered_set>
#include <assert.h>
#include <signal.h>
#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_engine.hpp"
#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "value_pool.hpp"


namespace foreactor {


IOUring::IOUring(int sq_length, bool sqe_async_flag)
        : sq_length(sq_length), sqe_async_flag(sqe_async_flag) {
    assert(sq_length >= 0);

    if (sq_length > 0) {
        int ret = io_uring_queue_init(sq_length, &ring, /*flags*/ 0);
        if (ret != 0) {
            DEBUG("initilize IOUring failed %d\n", ret);
            return;
        }
        DEBUG("inited IOUring @ %p sq_length %d\n", &ring, sq_length);
    }
}

IOUring::~IOUring() {
    io_uring_queue_exit(&ring);
    DEBUG("destroyed IOUring @ %p\n", &ring);
}


void IOUring::Prepare(SyscallNode *node, int epoch_sum) {
    EntryId entry_id = EncodeEntryId(node, epoch_sum);
    prepared.push_back(entry_id);
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
        if (sqe_async_flag)
            sqe->flags |= IOSQE_ASYNC;

        // if this request is to be linked with the next one in queue to
        // form an ordered chain
        // somehow using IOSQE_IO_LINK leads to unexpected chain breaks
        // even when the previous syscall returned perfect results, hence,
        // we use the stronger IOSQE_IO_HARDLINK here
        if (node->link.Get(epoch_sum))
            sqe->flags |= IOSQE_IO_HARDLINK;

        onthefly.insert(entry_id);
        node->stage.Set(epoch_sum, STAGE_ONTHEFLY);
    }

    size_t num_prepared = prepared.size(), num_submitted = 0;
    prepared.clear();

    // call io_uring_submit() for this bulk
    while (num_submitted < num_prepared) {
        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            DEBUG("submit SQEs failed %d\n", ret);
            // TODO: handle this more elegantly
            assert(false);
        } else
            num_submitted += ret;
    }
    return num_submitted;
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
    // discard the prepared set
    prepared.clear();

    // harvest completion for all on-the-fly requests
    while (onthefly.size() > 0)
        [[maybe_unused]] auto [node, epoch_sum, rc] = CompleteOne();
}


}
