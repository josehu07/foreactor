// Template implementation included in-place by the ".hpp".


#pragma once


namespace bptree {


template <typename K, typename V>
BPTree<K, V>::BPTree(std::string filename)
        : filename(filename) {
    // key and value type must be integral within 64-bit width
    static_assert(std::is_unsigned<K>::value,
                  "key must be unsigned integral");
    static_assert(std::is_unsigned<V>::value,
                  "value must be unsigned integral");
    static_assert(sizeof(K) <= sizeof(uint64_t),
                  "key must be within 64-bit width");
    static_assert(sizeof(V) <= sizeof(uint64_t),
                  "value must be within 64-bit width");

    // open the backing file
    int fd = open(filename.c_str(), O_CREAT | O_RDWR,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd <= 0)
        throw BPTreeException("failed to open backing file");

    // initialize pager
    pager = new Pager(fd);
}

template <typename K, typename V>
BPTree<K, V>::~BPTree() {
    if (fd > 0)
        close(fd);

    delete pager;
}


template <typename K, typename V>
size_t BPTree<K, V>::PageSearchKey(const Page& page, K key) {
    size_t nkeys = page.header.nkeys;
    if (nkeys == 0)
        return 0;

    // binary search for key in content array
    size_t spos = 0, epos = nkeys;
    while (true) {
        size_t pos = (spos + epos) / 2;
        size_t idx = 1 + pos * 2;

        if (page.content[idx] == key) {
            // found equality
            return idx;
        } else {
            // shrink range
            if (page.content[idx] < key)
                spos = pos + 1;
            else
                epos = pos;
        }

        if (spos >= epos)
            break;
    }

    // no equality, return idx of last spos
    assert(spos == epos);
    if (spos == 0)
        return 0;
    return 1 + (spos - 1) * 2;
}

template <typename K, typename V>
void BPTree<K, V>::LeafPageInject(Page& page, size_t search_idx, K key,
                                  V value) {
    assert(page.header.nkeys < MAXNKEYS);
    assert(search_idx == 0 || search_idx % 2 == 1);

    // if key has exact match with the one on idx, simply update its value
    if (search_idx > 0) {
        size_t search_pos = (search_idx - 1) / 2;
        if (search_pos < page.header.nkeys && page.content[search_idx] == key) {
            page.content[search_idx + 1] = value;
            return;
        }
    }

    // shift any array content with larger key to the right
    size_t inject_pos, inject_idx;
    if (search_idx == 0) {
        // if key is smaller than all existing keys
        inject_pos = 0;
        inject_idx = 1;
    } else {
        size_t search_pos = (search_idx - 1) / 2;
        inject_pos = search_pos + 1;
        inject_idx = 1 + inject_pos * 2;
    }
    if (inject_pos < page.header.nkeys) {
        size_t shift_src = 1 + inject_pos * 2;
        size_t shift_dst = shift_src + 2;
        size_t shift_len = (page.header.nkeys - inject_pos) * 2
                           * sizeof(uint64_t);
        memmove(&page.content[shift_dst], &page.content[shift_src], shift_len);
    }

    // inject key and value to its slot
    page.content[inject_idx] = key;
    page.content[inject_idx + 1] = value;

    // update number of keys
    page.header.nkeys++;
}

template <typename K, typename V>
void BPTree<K, V>::ItnlPageInject(Page& page, size_t search_idx, K key,
                                  uint64_t lpageid, uint64_t rpageid) {
    assert(page.header.nkeys < MAXNKEYS);
    assert(search_idx == 0 || search_idx % 2 == 1);

    // must not have duplicate internal node keys
    if (search_idx > 0) {
        size_t search_pos = (search_idx - 1) / 2;
        if (search_pos < page.header.nkeys && page.content[search_idx] == key)
            throw BPTreeException("duplicate internal node keys detected");
    }

    // shift any array content with larger key to the right
    size_t inject_pos, inject_idx;
    if (search_idx == 0) {
        // if key is smaller than all existing keys
        inject_pos = 0;
        inject_idx = 1;
    } else {
        size_t search_pos = (search_idx - 1) / 2;
        inject_pos = search_pos + 1;
        inject_idx = 1 + inject_pos * 2;
    }
    if (inject_pos < page.header.nkeys) {
        size_t shift_src = 1 + inject_pos * 2;
        size_t shift_dst = shift_src + 2;
        size_t shift_len = (page.header.nkeys - inject_pos) * 2
                           * sizeof(uint64_t);
        memmove(&page.content[shift_dst], &page.content[shift_src], shift_len);
    }

    // the pageid to the left of inject slot must be equal to left child
    if (page.content[inject_idx - 1] != lpageid)
        throw BPTreeException("left child pageid does not match");

    // inject key and pageid to its slot
    page.content[inject_idx] = key;
    page.content[inject_idx + 1] = rpageid;

    // update number of keys
    page.header.nkeys++;
}


template <typename K, typename V>
std::tuple<uint64_t, uint64_t, size_t> BPTree<K, V>::TraverseToLeaf(K key) {
    Page page;

    // search through internal pages, start from root
    uint64_t pageid = 0;
    unsigned level = 0, depth;
    while (true) {
        if (!pager->ReadPage(pageid, &page))
            throw BPTreeException("failed to read internal page");

        // if at root page, read out depth of tree
        if (level == 0) {
            depth = page.header.depth;
            if (depth == 1) {
                // root is the leaf, return
                return std::make_tuple(0, 0, 0);
            }
        }
        
        // search of nearest key that is <= given key in node
        size_t idx = PageSearchKey(page, key);
        idx = (idx == 0) ? 0 : (idx + 1);

        // fetch the correct child node pageid
        uint64_t childid = page.content[idx];
        if (childid == 0)
            throw BPTreeException("got 0 as child node pageid");
        
        level++;
        if (level == depth - 1)
            return std::make_tuple(childid, pageid, idx);

        pageid = childid;
    }
}


template <typename K, typename V>
void BPTree<K, V>::UpdateParentRefs(uint64_t parentid, const Page& parent) {
    // not the best implementation for a B+ tree, but suffices for us
    for (size_t idx = 0; idx <= 2 * parent.header.nkeys; idx += 2) {
        uint64_t childid = parent.content[idx];
        uint32_t parentid_short = static_cast<uint32_t>(parentid);
        if (!pager->WritePage(childid, 8, 4, (void *) &parentid_short))
            throw BPTreeException("failed to update parent pageid");
    }
}

template <typename K, typename V>
void BPTree<K, V>::SplitPage(uint64_t pageid, Page& page) {
    // if spliting root page, need to allocate two pages
    if (page.header.type == PAGE_ROOT) {
        assert(pageid == 0);
        size_t mpos = page.header.nkeys / 2;
        uint64_t lpageid, rpageid;
        
        // special case of the very first split of root leaf
        if (page.header.depth == 1) {
            Page lpage(PAGE_LEAF, 0), rpage(PAGE_LEAF, 0);
            lpageid = pager->AllocPage(), rpageid = pager->AllocPage();

            // populate left child
            size_t lsize = mpos * 2 * sizeof(uint64_t);
            memcpy(&lpage.content[1], &page.content[1], lsize);
            lpage.header.nkeys = mpos;
            lpage.header.next = rpageid;
            if (!pager->WritePage(lpageid, &lpage))
                throw BPTreeException("failed to write left child page");

            // populate right child
            size_t rsize = (page.header.nkeys - mpos) * 2 * sizeof(uint64_t);
            memcpy(&rpage.content[1], &page.content[1 + mpos * 2], rsize);
            rpage.header.nkeys = page.header.nkeys - mpos;
            if (!pager->WritePage(rpageid, &rpage))
                throw BPTreeException("failed to write right child page");

            // populate split node with first key of right child
            memset(page.content, 0, CONTENTLEN * sizeof(uint64_t));
            page.content[1] = rpage.content[1];

        // splitting into two internal nodes
        } else {
            Page lpage(PAGE_ITNL, 0), rpage(PAGE_ITNL, 0);
            lpageid = pager->AllocPage(), rpageid = pager->AllocPage();

            // populate left child
            size_t lsize = (1 + mpos * 2) * sizeof(uint64_t);
            memcpy(&lpage.content[0], &page.content[0], lsize);
            lpage.header.nkeys = mpos;
            lpage.header.next = rpageid;
            if (!pager->WritePage(lpageid, &lpage))
                throw BPTreeException("failed to write left child page");

            // update the parent pageid in all child pages of the new left
            // sibling page
            UpdateParentRefs(lpageid, lpage);
            
            // populate right child
            size_t rsize = ((page.header.nkeys - mpos) * 2 - 1)
                           * sizeof(uint64_t);
            memcpy(&rpage.content[0], &page.content[2 + mpos * 2], rsize);
            rpage.header.nkeys = page.header.nkeys - mpos - 1;
            if (!pager->WritePage(rpageid, &rpage))
                throw BPTreeException("failed to write right child page");

            // update the parent pageid in all child pages of the new right
            // sibling page
            UpdateParentRefs(rpageid, rpage);
            
            // populate split node with the middle key
            uint64_t mkey = page.content[1 + mpos * 2];
            memset(page.content, 0, CONTENTLEN * sizeof(uint64_t));
            page.content[1] = mkey;
        }

        // prepare new root info, increment tree depth
        page.content[0] = lpageid;
        page.content[2] = rpageid;
        page.header.nkeys = 1;
        page.header.depth++;
        if (!pager->WritePage(pageid, &page))
            throw BPTreeException("failed to write root page after split");

    // if splitting a non-root node
    } else {
        assert(pageid != 0);
        size_t mpos = page.header.nkeys / 2;
        uint64_t rpageid, mkey;

        // if splitting a leaf node
        if (page.header.type == PAGE_LEAF) {
            Page rpage(PAGE_LEAF, page.header.parent);
            rpageid = pager->AllocPage();

            // populate right child
            size_t rsize = (page.header.nkeys - mpos) * 2 * sizeof(uint64_t);
            memcpy(&rpage.content[1], &page.content[1 + mpos * 2], rsize);
            rpage.header.nkeys = page.header.nkeys - mpos;
            rpage.header.next = page.header.next;
            if (!pager->WritePage(rpageid, &rpage))
                throw BPTreeException("failed to write right child page");

            // trim current node
            mkey = rpage.content[1];
            memset(&page.content[1 + mpos * 2], 0, rsize);
            page.header.nkeys = mpos;

        // if splitting an internal node
        } else if (page.header.type == PAGE_ITNL) {
            Page rpage(PAGE_ITNL, page.header.parent);
            rpageid = pager->AllocPage();

            // populate right child
            size_t rsize = ((page.header.nkeys - mpos) * 2 - 1)
                           * sizeof(uint64_t);
            memcpy(&rpage.content[0], &page.content[2 + mpos * 2], rsize);
            rpage.header.nkeys = page.header.nkeys - mpos - 1;
            rpage.header.next = page.header.next;
            if (!pager->WritePage(rpageid, &rpage))
                throw BPTreeException("failed to write right child page");

            // update the parent pageid in all child pages of the new right
            // sibling page
            UpdateParentRefs(rpageid, rpage);

            // trim current node
            mkey = page.content[1 + mpos * 2];
            memset(&page.content[1 + mpos * 2], 0, rsize + sizeof(uint64_t));
            page.header.nkeys = mpos;
            
        } else
            throw BPTreeException("unknown page type encountered");

        // make current node link to new right node
        page.header.next = rpageid;
        if (!pager->WritePage(pageid, &page))
            throw BPTreeException("failed to write split page");

        // insert the uplifted key into parent node
        uint64_t parentid = page.header.parent;
        Page parent;
        if (!pager->ReadPage(parentid, &parent))
            throw BPTreeException("failed to read parent page");
        assert(parent.header.nkeys < MAXNKEYS);
        size_t idx = PageSearchKey(parent, mkey);
        ItnlPageInject(parent, idx, mkey, pageid, rpageid);
        
        // if parent internal node becomes full, do split recursively
        if (parent.header.nkeys == MAXNKEYS)
            SplitPage(parentid, parent);
        else if (!pager->WritePage(parentid, &parent))
            throw BPTreeException("failed to update parent page");
    }
}


template <typename K, typename V>
void BPTree<K, V>::Put(K key, V value) {
    // traverse to the correct leaf node and read
    uint64_t leafid;
    std::tie(leafid, std::ignore, std::ignore) = TraverseToLeaf(key);
    Page page;
    if (!pager->ReadPage(leafid, &page))
        throw BPTreeException("failed to read leaf page");

    // inject key-value pair into the leaf node
    assert(page.header.nkeys < MAXNKEYS);
    size_t idx = PageSearchKey(page, key);
    LeafPageInject(page, idx, key, value);

    // if this leaf node becomes full, do split
    if (page.header.nkeys == MAXNKEYS)
        SplitPage(leafid, page);
    else if (!pager->WritePage(leafid, &page))
        throw BPTreeException("failed to update leaf page");
}

template <typename K, typename V>
bool BPTree<K, V>::Get(K key, V& value) {
    // traverse to the correct leaf node and read
    uint64_t leafid;
    std::tie(leafid, std::ignore, std::ignore) = TraverseToLeaf(key);
    Page page;
    if (!pager->ReadPage(leafid, &page))
        throw BPTreeException("failed to read leaf page");

    // search in leaf node for key
    size_t idx = PageSearchKey(page, key);
    if (idx == 0 || page.content[idx] != key) {
        // not found
        return false;
    }
    
    // found match key, fetch value
    value = page.content[idx + 1];
    return true;
}

template <typename K, typename V>
size_t BPTree<K, V>::Scan(K lkey, K rkey,
                          std::vector<std::tuple<K, V>>& results) {
    uint64_t lleafid, litnlid, rleafid, ritnlid;
    size_t litnlidx, ritnlidx;
    std::vector<uint64_t> leaves;   // leaf pages that should be read out

    // traverse up to leaf node for both bounds of range, gather all the
    // leaf nodes that should be read out
    std::tie(lleafid, litnlid, litnlidx) = TraverseToLeaf(lkey);
    std::tie(rleafid, ritnlid, ritnlidx) = TraverseToLeaf(rkey);
    Page page;
    uint64_t itnlid = litnlid;
    while (true) {
        if (!pager->ReadPage(itnlid, &page))
            throw BPTreeException("failed to read out internal page in scan");

        // gather leaf pageids in sub-range
        size_t lidx = 0, ridx = page.header.nkeys * 2;
        if (itnlid == litnlid) {
            lidx = litnlidx;
            assert(page.content[lidx] == lleafid);
        }
        if (itnlid == ritnlid) {
            ridx = ritnlidx;
            assert(page.content[ridx] == rleafid);
        }
        for (size_t idx = lidx; idx <= ridx; idx += 2)
            leaves.push_back(page.content[idx]);

        // stop after processing the right-most internal page, else move on
        // to next internal page
        if (itnlid == ritnlid)
            break;

        itnlid = page.header.next;
    }

    // read out the leaf pages in a loop, gathering records in range
    size_t nrecords = 0;
    for (uint64_t leafid : leaves) {
        if (!pager->ReadPage(leafid, &page))
            throw BPTreeException("failed to read out leaf page in scan");

        // need a search if in boundary leaf page
        size_t lidx = 1, ridx = page.header.nkeys * 2 - 1;
        if (leafid == lleafid) {
            size_t idx = PageSearchKey(page, lkey);
            if (idx != 0 && page.content[idx] == lkey)
                lidx = idx;
            else if (idx == 0)
                lidx = 1;
            else
                lidx = idx + 2;
        }
        if (leafid == rleafid) {
            size_t idx = PageSearchKey(page, rkey);
            if (idx != 0)
                ridx = idx;
            else
                ridx = 0;
        }

        // gather records within range
        for (size_t idx = lidx; idx <= ridx; idx += 2) {
            K key = page.content[idx];
            V value = page.content[idx + 1];
            results.push_back(std::make_tuple(key, value));
            nrecords++;
        }
    }

    return nrecords;
}


std::ostream& operator<<(std::ostream& s, const BPTreeStats& stats) {
    return s << "BPTreeStats{npages=" << stats.npages
             << ",npages_itnl=" << stats.npages_itnl
             << ",npages_leaf=" << stats.npages_leaf
             << ",npages_empty=" << stats.npages_empty
             << ",nkeys_itnl=" << stats.nkeys_itnl
             << ",nkeys_leaf=" << stats.nkeys_leaf << "}";
}

static inline std::string PageTypeStr(enum PageType type) {
    switch (type) {
        case PAGE_EMPTY: return "empty";
        case PAGE_ROOT:  return "root";
        case PAGE_ITNL:  return "itnl";
        case PAGE_LEAF:  return "leaf";
        default:         return "unknown";
    }
}

std::ostream& operator<<(std::ostream& s, const Page& page) {
    s << "Page{type=" << PageTypeStr(page.header.type)
      << ",nkeys=" << page.header.nkeys
      << ",parent=" << page.header.parent
      << ",depth/next=" << page.header.next << ",content=[";
    for (size_t idx = 0; idx < 1 + page.header.nkeys * 2; ++idx)
        s << page.content[idx] << ",";
    return s << "]}";
}

template <typename K, typename V>
void BPTree<K, V>::PrintStats(bool print_pages) {
    BPTreeStats stats;
    pager->CheckStats(stats);
    std::cout << stats << std::endl;

    if (print_pages) {
        Page page;
        for (uint64_t pageid = 0; pageid < stats.npages; ++pageid) {
            if (!pager->ReadPage(pageid, &page))
                throw BPTreeException("failed to read page for stats");
            std::cout << " " << pageid << " - " << page << std::endl;
        }
    }
}


}
