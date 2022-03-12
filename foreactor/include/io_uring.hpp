#include <unordered_set>
#include <assert.h>
#include <liburing.h>

#include "value_pool.hpp"


#ifndef __FOREACTOR_IO_URING_H__
#define __FOREACTOR_IO_URING_H__


namespace foreactor {


class SCGraph;  // forward declarations
class SyscallNode;


// The NodeAndEpoch structure is a handle that uniquely identifies any
// request submitted.
struct NodeAndEpoch {
    SyscallNode *node;
    EpochListBase *epoch;

    NodeAndEpoch() = delete;
    NodeAndEpoch(SyscallNode *node_, EpochListBase *epoch_);
    ~NodeAndEpoch();
};


// Each IOUring instance is a pair of io_uring SQ/CQ queues.
// There should be one IOUring instance per wrapped function per thread.
class IOUring {
    friend class SCGraph;

    private:
        struct io_uring ring;
        bool ring_initialized = false;
        int sq_length = 0;

        std::unordered_set<NodeAndEpoch *> prepared;
        std::unordered_set<NodeAndEpoch *> in_progress;

        struct io_uring *Ring() {
            return &ring;
        }

    public:
        IOUring();
        ~IOUring();

        void Initialize(int sq_length);
        bool IsInitialized() const;

        // Put or remove prepared/in-progress requests.
        void PutToPrepared(NodeAndEpoch *nae);
        void MakeAllInProgress();   // io_uring_prep_xxx() happens here
        void RemoveInProgress(NodeAndEpoch *nae);

        // Clear and complete unused but prepared/submitted requests.
        void DeleteAllPrepared();
        void ClearAllInProgress();
};


}


#endif
