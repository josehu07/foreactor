#include <tuple>
#include <vector>
#include <unordered_set>
#include <assert.h>

#include "scg_nodes.hpp"


#pragma once


namespace foreactor {


// General interface of the async syscall engine backend.
class IOEngine {
    public:
        // uint64_t identifier of each queue entry.
        typedef uint64_t EntryId;

    protected:
        // Encode/Decode the identifier from/to (node, epoch_sum) tuple.
        static EntryId EncodeEntryId(SyscallNode* node, int epoch_sum);
        static std::tuple<SyscallNode *, int> DecodeEntryId(EntryId entry_id);

        // Use vector instead of unordered_set for the prepared set so that
        // we still preserve the order of prepared syscalls when submitting,
        // in case that matters for better perfomrance.
        std::vector<EntryId> prepared;
        std::unordered_set<EntryId> onthefly;

    public:
        IOEngine() {}
        ~IOEngine() {}

        // Insert one request into the prepared set; requests in this set
        // are in PREPARED stage, but the io_uring_prep_xxx() is not done
        // yet to avoid polluting the SQ if it eventually isn't submitted.
        virtual void Prepare(SyscallNode *node, int epoch_sum) = 0;

        // Submit all requests in the prepared set and move them to the
        // onthefly set; io_uring_prep_xxx()s happen here, and the nodes
        // stage will be set to ONTHEFLY.
        // Returns the number of requests successfully submitted.
        virtual int SubmitAll() = 0;

        // Wait and harvest one CQE, then remove it from the onthefly set.
        // Returns a tuple of the completed entry's node, epoch_sum, and
        // return code.
        virtual std::tuple<SyscallNode *, int, long> CompleteOne() = 0;

        // Clear the two sets, properly treating the still on-they-fly
        // requests so they won't end up leaving CQE entries unharvested.
        // TODO: maybe introduce garbage collection here
        virtual void CleanUp() = 0;
};


}
