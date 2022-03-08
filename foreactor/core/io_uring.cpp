#include <liburing.h>

#include "debug.hpp"
#include "timer.hpp"
#include "io_uring.hpp"
#include "scg_nodes.hpp"
#include "value_pool.hpp"


namespace foreactor {


///////////////////////////////////////
// NodeAndEpoch handle implementaion //
///////////////////////////////////////

NodeAndEpoch::NodeAndEpoch(SyscallNode *node_, EpochListBase *epoch_)
        : node(node_), epoch(EpochListBase::Copy(epoch_)) {
    // stores a copy of epoch to remember the epoch at submission
    assert(node_ != nullptr);
    assert(epoch_ != nullptr);
}

NodeAndEpoch::~NodeAndEpoch() {
    EpochListBase::Delete(epoch);
}


////////////////////////////
// IOUring implementation //
////////////////////////////

IOUring::IOUring()
        : sq_length(0) {
    ring_initialized = false;
}

IOUring::~IOUring() {
    TIMER_START("uring-exit");
    if (ring_initialized) {
        io_uring_queue_exit(&ring);
        DEBUG("destroyed IOUring @ %p\n", &ring);
    }
    TIMER_PAUSE("uring-exit");
    TIMER_PRINT("uring-exit", TIME_MICRO);
    TIMER_RESET("uring-exit");

    
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
    TIMER_PRINT("uring-init", TIME_MICRO);
    TIMER_RESET("uring-init");
}

bool IOUring::IsInitialized() const {
    return ring_initialized;
}


void IOUring::PutInProgress(NodeAndEpoch *nae) {
    assert(in_progress.find(nae) == in_progress.end());
    in_progress.insert(nae);
}

void IOUring::RemoveInProgress(NodeAndEpoch *nae) {
    assert(in_progress.find(nae) != in_progress.end());
    in_progress.erase(nae);
}


void IOUring::ClearAllInProgress() {
    for (NodeAndEpoch *nae : in_progress) {
        assert(nae->node->stage->GetValue(nae->epoch) == STAGE_PROGRESS);
        nae->node->CmplAsync(nae->epoch);   // nae will be deleted here
    }
    in_progress.clear();
}


}
