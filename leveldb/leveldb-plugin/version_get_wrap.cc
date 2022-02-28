#include <vector>

// Dirty trick to allow access to private members because we need them
// when building the SCGraph. This trick is used only because we want to
// leave original application code 100% unmodified -- otherwise, the
// correct thing to do would be to simple insert a few friend declarations.
//
// There's also debate on whether enforcing privateness is really a
// good thing for c++ classes.
#define private public
#define protected public

#include "db/version_set.h"
#include "db/filename.h"
#include "db/table_cache.h"
#include "leveldb/status.h"
#include "leveldb/env.h"
#include "table/format.h"
#include "table/block.h"
#include "table/filter_block.h"

#undef private
#undef protected

#include <foreactor.hpp>
namespace fa = foreactor;


//////////////////////
// IOUring instance //
//////////////////////

// There is one IOUring instance per wrapped function per thread.
thread_local fa::IOUring ring;


namespace leveldb {


//////////////////////
// Helper functions //
//////////////////////

struct TableAndFile {
    RandomAccessFile *file;
    Table *table;
};

struct Table::Rep {
    ~Rep() {
        delete filter;
        delete[] filter_data;
        delete index_block;
    }

    Options options;
    Status status;
    RandomAccessFile *file;
    uint64_t cache_id;
    FilterBlockReader *filter;
    const char *filter_data;

    BlockHandle metaindex_handle;
    Block *index_block;
};

class PosixRandomAccessFile final : public RandomAccessFile {
 public:
  PosixRandomAccessFile(std::string filename, int fd, void* fd_limiter);
  ~PosixRandomAccessFile() override;

  Status Read(uint64_t offset, size_t n, Slice* result,
              char* scratch) const override;

  const bool has_permanent_fd_;
  const int fd_;
  void* const fd_limiter_;
  const std::string filename_;
};

static Cache::Handle *TryFindTable(TableCache *tc, uint64_t file_number) {
    char buf[sizeof(file_number)];
    EncodeFixed64(buf, file_number);
    Slice key(buf, sizeof(buf));
    Cache::Handle *handle = tc->cache_->Lookup(key);
    return handle;
}

static Table *CacheHandleToTable(TableCache *tc, Cache::Handle *handle) {
    if (handle == nullptr)
        return nullptr;
    return reinterpret_cast<TableAndFile *>(tc->cache_->Value(handle))->table;
}

static void ReleaseCacheHandle(TableCache *tc, Cache::Handle *handle) {
    tc->cache_->Release(handle);
}

static bool SearchBlockHandle(Table *t, const Slice& k, BlockHandle& handle) {
  Iterator* iiter = t->rep_->index_block->NewIterator(t->rep_->options.comparator);
  iiter->Seek(k);
  if (!iiter->Valid())
    return false;
  Slice index_value = iiter->value();
  Status s = handle.DecodeFrom(&index_value);
  return s.ok();
}

static int GetFileFd(Table *t) {
  return static_cast<PosixRandomAccessFile *>(t->rep_->file)->fd_;
}

static size_t BlockHandleToSize(const BlockHandle& handle) {
  return static_cast<size_t>(handle.size()) + kBlockTrailerSize;
}

static off_t BlockHandleToOffset(const BlockHandle& handle) {
  return handle.offset();
}

static bool NewestFirst(FileMetaData* a, FileMetaData* b) {
  return a->number > b->number;
}


/////////////////////
// SCGraph builder //
/////////////////////

void BuildVersionGetSCGraph(fa::SCGraph *scgraph, Version *me,
                            const ReadOptions& options, const LookupKey& k,
                            std::string *value, Version::GetStats *stats) {
    assert(scgraph != nullptr);

    auto GenNodeId = [](FileMetaData *f, int op) -> uint64_t {
        // TODO: this is now arbitrary
        return reinterpret_cast<uint64_t>(f) + static_cast<uint64_t>(op);
    };

    // Gather level-0 tables in order from newest to oldest.
    const Comparator *ucmp = me->vset_->icmp_.user_comparator();
    std::vector<FileMetaData *> tables;
    tables.reserve(me->files_[0].size());
    for (uint32_t i = 0; i < me->files_[0].size(); i++) {
        FileMetaData *f = me->files_[0][i];
        if (ucmp->Compare(k.user_key(), f->smallest.user_key()) >= 0 &&
            ucmp->Compare(k.user_key(), f->largest.user_key()) <= 0) {
            tables.push_back(f);
        }
    }
    if (!tables.empty())
        std::sort(tables.begin(), tables.end(), NewestFirst);
    // Gather deeper levels, at most one table per level.
    for (int level = 1; level < config::kNumLevels; level++) {
        size_t num_files = me->files_[level].size();
        if (num_files == 0) continue;
        // Binary search to find earliest index whose largest key >= internal_key.
        uint32_t index = FindFile(me->vset_->icmp_, me->files_[level], k.internal_key());
        if (index < num_files) {
            FileMetaData *f = me->files_[level][index];
            if (ucmp->Compare(k.user_key(), f->smallest.user_key()) < 0) {
                // All of "f" is past any data for user_key
            } else {
                tables.push_back(f);
            }
        }
    }

    fa::SyscallPread *last_pread_data = nullptr;
    for (size_t i = 0; i < tables.size(); i++) {
        Cache::Handle *handle = TryFindTable(me->vset_->table_cache_, tables[i]->number);
        Table* table = CacheHandleToTable(me->vset_->table_cache_, handle);

        auto node_branch_open = new fa::BranchNode(table == nullptr
                                                   ? 0     // need open
                                                   : 1);   // already open
        auto node_open = new fa::SyscallOpen(TableFileName(me->vset_->dbname_, tables[i]->number),
                                             O_CLOEXEC,
                                             O_RDONLY);
        auto node_pread_footer = new fa::SyscallPread(-1,
                                                      Footer::kEncodedLength,
                                                      tables[i]->file_size - Footer::kEncodedLength,
                                                      std::vector{false, true, true});
        auto node_pread_index = new fa::SyscallPread(-1,
                                                     0,
                                                     0,
                                                     std::vector{false, false, false});
        fa::SyscallPread *node_pread_data = nullptr;
        if (table == nullptr) {
            node_pread_data = new fa::SyscallPread(-1,
                                                   0,
                                                   0,
                                                   std::vector{false, false, false});
        } else {
            BlockHandle block_handle;
            bool searched = SearchBlockHandle(table, k.internal_key(), block_handle);
            assert(searched);
            node_pread_data = new fa::SyscallPread(GetFileFd(table),
                                                   BlockHandleToSize(block_handle),
                                                   BlockHandleToOffset(block_handle));
        }
        scgraph->AddNode(GenNodeId(tables[i], 0), node_branch_open, i == 0);
        scgraph->AddNode(GenNodeId(tables[i], 1), node_open);
        scgraph->AddNode(GenNodeId(tables[i], 2), node_pread_footer);
        scgraph->AddNode(GenNodeId(tables[i], 3), node_pread_index);
        scgraph->AddNode(GenNodeId(tables[i], 5), node_pread_data);
        if (last_pread_data != nullptr)
            last_pread_data->SetNext(node_branch_open, /*weak_edge*/ true);
        node_branch_open->SetChildren(std::vector<fa::SCGraphNode *>{node_open, node_pread_data});
        node_open->SetNext(node_pread_footer, /*weak_edge*/ false);
        node_pread_footer->SetNext(node_pread_index, /*weak_edge*/ false);
        node_pread_index->SetNext(node_pread_data, /*weak_edge*/ false);
        last_pread_data = node_pread_data;

        if (handle != nullptr)
            ReleaseCacheHandle(me->vset_->table_cache_, handle);
    }
}


//////////////////
// Wrapper stub //
//////////////////

// Mock the function to be made asynchronous with this wrapper function.
// Builds the SCGraph, registers it, so that if foreactor library is
// LD_PRELOADed, asynchroncy happens.
// 
// __real_funcname() will resolve to the original application function,
// while the application's (undefined-at-compile-time) references into
// funcname() will resolve to __wrap_funcname().
// 
// Must use mangled function name for C++. Use `objdump -t filename.o`
// to check the mangled symbol name.
extern "C" Status __real__ZN7leveldb7Version3GetERKNS_11ReadOptionsERKNS_9LookupKeyEPNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS0_8GetStatsE(
        Version *me,
        const ReadOptions& options, const LookupKey& k,
        std::string *value, Version::GetStats *stats);

extern "C" Status __wrap__ZN7leveldb7Version3GetERKNS_11ReadOptionsERKNS_9LookupKeyEPNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS0_8GetStatsE(
        Version *me,
        const ReadOptions& options, const LookupKey& k,
        std::string *value, Version::GetStats *stats) {
    fa::SCGraph *scgraph = fa::WrapperFuncEnter(&ring, /*graph_id*/ 0);

    // Build SCGraph if using foreactor.
    if (fa::UseForeactor)
        BuildVersionGetSCGraph(scgraph, me, options, k, value, stats);

    // Call the original function.
    auto ret = __real__ZN7leveldb7Version3GetERKNS_11ReadOptionsERKNS_9LookupKeyEPNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS0_8GetStatsE(
        me, options, k, value, stats);

    fa::WrapperFuncLeave(scgraph);
    return ret;
}


}
