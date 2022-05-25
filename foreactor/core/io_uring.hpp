#include <tuple>
#include <unordered_set>
#include <assert.h>
#include <liburing.h>


#ifndef __FOREACTOR_IO_URING_H__
#define __FOREACTOR_IO_URING_H__


namespace foreactor {


class SCGraph;  // forward declarations
class SyscallNode;


// Each IOUring instance is a pair of io_uring SQ/CQ queues.
// There should be one IOUring instance per wrapped function per thread.
class IOUring {
    friend class SCGraph;

    // uint64_t user_data field of each queue entry.
    typedef uint64_t EntryId;

    public:
        // Encode/Decode the user_data field from/to (node, epoch_sum) tuple.
        static EntryId EncodeEntryId(SyscallNode* node, int epoch_sum);
        static std::tuple<SyscallNode *, int> DecodeEntryId(EntryId entry_id);

    private:
        struct io_uring ring;
        int sq_length = 0;

        std::unordered_set<EntryId> prepared;
        std::unordered_set<EntryId> onthefly;

        // Get pointer to struct io_uring.
        struct io_uring *Ring() const;

    public:
        IOUring() = delete;
        IOUring(int sq_length);
        ~IOUring();

        // Insert one request into the prepared set; requests in this set
        // are in PREPARED stage, but the io_uring_prep_xxx() is not done
        // yet to avoid polluting the SQ if it eventually isn't submitted.
        void Prepare(EntryId entry_id);

        // Submit all requests in the prepared set and move them to the
        // onthefly set; io_uring_prep_xxx()s happen here, and the nodes
        // stage will be set to ONTHEFLY.
        void SubmitAll();

        // Remove one request from the onthefly set after its CQE has been
        // processed.
        void RemoveOne(EntryId entry_id);

        // CQE-related helper functions.
        struct io_uring_cqe *WaitOneCqe();
        void SeenOneCqe(struct io_uring_cqe *cqe);
        EntryId GetCqeEntryId(struct io_uring_cqe *cqe);

        // Clear the two sets, properly treating the still on-they-fly
        // requests so they won't end up leaving CQE entries unharvested.
        // TODO: maybe introduce garbage collection here
        void CleanUp();
};


}


#endif
