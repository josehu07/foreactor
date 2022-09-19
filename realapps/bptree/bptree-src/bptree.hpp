// BPTree -- simple B+ tree class.


#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <tuple>
#include <cassert>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.hpp"
#include "pager.hpp"

#pragma once


namespace bptree {


/**
 * Single-file backed simple B+ tree. Non thread-safe. Supporting only
 * integral key and value types within 64-bit width.
 */
template <typename K, typename V>
class BPTree {
    friend BPTreeException;
    friend BPTreeStats;
    friend Pager;

    private:
        std::string filename;
        int fd = -1;

        Pager *pager = nullptr;

        /**
         * Search in page for the closest key that is <= given key.
         * Returns the index to the key in content array, or 0 if all existing
         * keys are greater than given key.
         */
        size_t PageSearchKey(const Page& page, K key);

        /**
         * Insert a key-value pair into leaf page, shifting array content if
         * necessary. serach_idx should be calculated through PageSearchKey.
         */
        void LeafPageInject(Page& page, size_t search_idx, K key, V value);

        /**
         * Insert a key into internal node (carrying its left and right child
         * pageids), shifting array content if necessary. serach_idx should
         * be calculated through PageSearchKey.
         */
        void ItnlPageInject(Page& page, size_t search_idx, K key,
                            uint64_t lpageid, uint64_t rpageid);

        /**
         * Do B+ tree search to traverse through internal nodes and find the
         * correct leaf node.
         * Returns a tuple of (leaf_pageid, last_itnl_pageid, last_itnl_idx).
         */
        std::tuple<uint64_t, uint64_t, size_t> TraverseToLeaf(K key);

        /**
         * Split the given page into two siblings, and propagate one new key
         * up to the parent node. May trigger cascading splits.
         */
        void UpdateParentRefs(uint64_t parentid, const Page& parent);
        void SplitPage(uint64_t pageid, Page& page);

    public:
        BPTree(std::string filename);
        ~BPTree();

        /**
         * Insert a key-value pair into B+ tree.
         * Exceptions might be thrown.
         */
        void Put(K key, V value);

        /**
         * Search for a key, fill given reference with value.
         * Returns false if search failed or key not found.
         * Exceptions might be thrown.
         */
        bool Get(K key, V& value);

        /**
         * Do a range scan over an inclusive key range [lkey, rkey], and
         * append found records to the given vector.
         * Returns the number of records found within range.
         */
        size_t Scan(K lkey, K rkey, std::vector<std::tuple<K, V>>& results);

        /**
         * Scan the whole backing file and print statistics.
         * If print_pages is true, also prints content of all pages.
         */
        void PrintStats(bool print_pages);
};


}


// Include template implementation in-place.
#include "bptree.tpl.hpp"
