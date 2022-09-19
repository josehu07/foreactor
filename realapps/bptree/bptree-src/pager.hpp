// Pager -- backing file allocation and I/O manager.


#include <iostream>
#include <stdexcept>
#include <string>
#include <set>
#include <cassert>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.hpp"

#pragma once


namespace bptree {


/**
 * Manages free pages of a BPTree backing file. Uses a simple freelist
 * mechanism. The file can grow but can never shrink.
 */
class Pager {
    private:
        int fd = -1;

        std::set<uint64_t> freelist;

        /**
         * Scan file and gather statistics.
         * If init is true, will initialize the freelist set.
         */
        void CheckStatsInternal(BPTreeStats& stats, bool init);

    public:
        Pager(int fd);
        ~Pager();

        /** Data I/O on page. */
        bool ReadPage(uint64_t pageid, void *buf);
        bool ReadPage(uint64_t pageid, size_t off, size_t len, void *buf);
        bool WritePage(uint64_t pageid, void *buf);
        bool WritePage(uint64_t pageid, size_t off, size_t len, void *buf);

        /** Allocate one new page.*/
        uint64_t AllocPage();

        /** Scan file and gather statistics. */
        void CheckStats(BPTreeStats& stats);
};


}
