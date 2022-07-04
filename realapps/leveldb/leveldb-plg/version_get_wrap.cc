#include <vector>
#include <unordered_set>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <foreactor.h>

// Dirty trick to allow access to private members because we need them
// when building the SCGraph. This trick is used only because we want to
// leave original application code 100% unmodified -- otherwise, the
// correct thing to do would be to simply insert a few friend declarations.
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
  delete iiter;
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

static constexpr unsigned graph_id = 0;


// Some global state for arggen and rcsave functions.
static Version *curr_version = nullptr;
static const LookupKey *curr_lookup_key = nullptr;
static std::vector<FileMetaData *> curr_tables;
static std::vector<int> curr_levels;
static std::vector<bool> curr_open_states;
static std::unordered_set<std::string> curr_table_names;


static bool branch_table_open_arggen(const int *epoch, int *decision) {
    if (epoch[0] > curr_tables.size()) {
        *decision = 2;
        return true;
    }

    // if running past level 0 tables, may need to look for the next level table
    if (epoch[0] == curr_tables.size()) {
        int level = (curr_tables.size() == 0) ? 1 : (curr_levels.back() + 1);
        const Comparator *ucmp = curr_version->vset_->icmp_.user_comparator();
        for (; level < config::kNumLevels; level++) {
            size_t num_files = curr_version->files_[level].size();
            if (num_files == 0) continue;
            // binary search to find earliest index whose largest key >= internal_key
            uint32_t index = FindFile(curr_version->vset_->icmp_, curr_version->files_[level], curr_lookup_key->internal_key());
            if (index < num_files) {
                FileMetaData *f = curr_version->files_[level][index];
                if (ucmp->Compare(curr_lookup_key->user_key(), f->smallest.user_key()) < 0) {
                    // all of "f" is past any data for user_key
                } else {
                    // found candidate
                    curr_tables.push_back(f);
                    curr_levels.push_back(level);
                    goto decide_open;
                }
            }
        }
        // no more files
        *decision = 2;
        return true;
    }

decide_open:
    FileMetaData *table_f = curr_tables[epoch[0]];
    int level = curr_levels[epoch[0]];
    Cache::Handle *handle = TryFindTable(curr_version->vset_->table_cache_, table_f->number);
    Table *table = CacheHandleToTable(curr_version->vset_->table_cache_, handle);
    if (table == nullptr) {
        // needs open
        curr_open_states.push_back(false);
        *decision = 0;
    } else {
        // already open
        curr_open_states.push_back(true);
        *decision = 1;
    }
    if (handle != nullptr)
        ReleaseCacheHandle(curr_version->vset_->table_cache_, handle);
    return true;
}

static bool open_arggen(const int *epoch, const char **pathname, int *flags, mode_t *mode) {
    assert(epoch[0] < curr_tables.size());
    std::string filename = TableFileName(curr_version->vset_->dbname_, curr_tables[epoch[0]]->number);
    *pathname = filename.c_str();
    curr_table_names.insert(std::move(filename));
    *flags = O_CLOEXEC;
    *mode = O_RDONLY;
    return true;
}

static void open_rcsave(const int *epoch, int fd) {
    // turn off page cache readahead since they're guaranteed useless for
    // Get requests
    struct stat st;
    int ret = fstat(fd, &st);
    assert(ret == 0);
    ret = posix_fadvise(fd, 0, st.st_size, POSIX_FADV_RANDOM);
    assert(ret == 0);
}

static bool pread_footer_arggen(const int *epoch, int *fd, char **buf, size_t *count, off_t *offset, bool *buf_ready) {
    return false;   // depends on preceding open anyway
}

static void pread_footer_rcsave(const int *epoch, ssize_t res) {
    return;
}

static bool pread_index_arggen(const int *epoch, int *fd, char **buf, size_t *count, off_t *offset, bool *buf_ready) {
    return false;   // depends on preceding footer pread anyway
}

static void pread_index_rcsave(const int *epoch, ssize_t res) {
    return;
}

static bool pread_data_arggen(const int *epoch, int *fd, char **buf, size_t *count, off_t *offset, bool *buf_ready) {
    assert(epoch[0] < curr_tables.size());
    if (!curr_open_states[epoch[0]])
        return false;   // depends on preceding index pread anyway

    // need to re-grab the cache handle
    Cache::Handle *handle = TryFindTable(curr_version->vset_->table_cache_, curr_tables[epoch[0]]->number);
    Table *table = CacheHandleToTable(curr_version->vset_->table_cache_, handle);
    assert(table != nullptr);

    BlockHandle block_handle;
    bool searched __attribute__((unused)) = SearchBlockHandle(table, curr_lookup_key->internal_key(), block_handle);
    assert(searched);

    *fd = GetFileFd(table);
    *buf = nullptr;
    *count = BlockHandleToSize(block_handle);
    *offset = BlockHandleToOffset(block_handle);
    *buf_ready = false;

    if (handle != nullptr)
        ReleaseCacheHandle(curr_version->vset_->table_cache_, handle);
    return true;
}

static void pread_data_rcsave(const int *epoch, ssize_t res) {
    return;
}

static bool branch_deepest_level_arggen(const int *epoch, int *decision) {
    if (epoch[0] + 1 < curr_tables.size())
        *decision = 0;
    else {
        int next_level = (curr_tables.size() == 0) ? 1 : (curr_levels.back() + 1);
        *decision = (next_level < config::kNumLevels) ? 0 : 1;
    }
    return true;
}


static void BuildSCGraph() {
    foreactor_CreateSCGraph(graph_id, 1);

    int common_assoc_dims[1] = {0};

    foreactor_AddBranchNode(graph_id, 0, "table_open", common_assoc_dims, 1, branch_table_open_arggen, 3, /*is_start*/ true);
    foreactor_AddSyscallOpen(graph_id, 1, "open", common_assoc_dims, 1, open_arggen, open_rcsave, false);
    foreactor_AddSyscallPread(graph_id, 2, "pread_footer", common_assoc_dims, 1, pread_footer_arggen, pread_footer_rcsave, 0, false);
    foreactor_AddSyscallPread(graph_id, 3, "pread_index", common_assoc_dims, 1, pread_index_arggen, pread_index_rcsave, 0, false);
    foreactor_AddSyscallPread(graph_id, 4, "pread_data", common_assoc_dims, 1, pread_data_arggen, pread_data_rcsave, (1 << 20) + 4096, false);
    foreactor_AddBranchNode(graph_id, 5, "deepest_level", common_assoc_dims, 1, branch_deepest_level_arggen, 2, false);

    foreactor_BranchAppendChild(graph_id, 0, 1, /*epoch_dim*/ -1);
    foreactor_BranchAppendChild(graph_id, 0, 4, -1);
    foreactor_BranchAppendEndNode(graph_id, 0);
    foreactor_SyscallSetNext(graph_id, 1, 2, /*weak_edge*/ false);
    foreactor_SyscallSetNext(graph_id, 2, 3, false);
    foreactor_SyscallSetNext(graph_id, 3, 4, false);
    foreactor_SyscallSetNext(graph_id, 4, 5, true);
    foreactor_BranchAppendChild(graph_id, 5, 0, 0);
    foreactor_BranchAppendEndNode(graph_id, 5);

    foreactor_SetSCGraphBuilt(graph_id);

    // foreactor_DumpDotImg(graph_id, "version_get");
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
extern "C"
Status __real__ZN7leveldb7Version3GetERKNS_11ReadOptionsERKNS_9LookupKeyEPNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS0_8GetStatsE(
        Version *me,    // is a method of class Version
        const ReadOptions& options, const LookupKey& k,
        std::string *value, Version::GetStats *stats);

extern "C"
Status __wrap__ZN7leveldb7Version3GetERKNS_11ReadOptionsERKNS_9LookupKeyEPNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS0_8GetStatsE(
        Version *me,
        const ReadOptions& options, const LookupKey& k,
        std::string *value, Version::GetStats *stats) {
    if (!foreactor_UsingForeactor()) {
        // call the original function
        return __real__ZN7leveldb7Version3GetERKNS_11ReadOptionsERKNS_9LookupKeyEPNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS0_8GetStatsE(
                   me, options, k, value, stats);
    } else {
        // build SCGraph once if haven't done yet
        if (!foreactor_HasSCGraph(graph_id))
            BuildSCGraph();

        curr_version = me;
        curr_lookup_key = &k;
        size_t max_num_tables = me->files_[0].size() + config::kNumLevels - 1;
        // gather level-0 tables in order from newest to oldest
        curr_tables.reserve(max_num_tables);
        const Comparator *ucmp = me->vset_->icmp_.user_comparator();
        for (uint32_t i = 0; i < me->files_[0].size(); i++) {
            FileMetaData *f = me->files_[0][i];
            if (ucmp->Compare(k.user_key(), f->smallest.user_key()) >= 0 &&
                ucmp->Compare(k.user_key(), f->largest.user_key()) <= 0) {
                curr_tables.push_back(f);
            }
        }
        if (!curr_tables.empty())
            std::sort(curr_tables.begin(), curr_tables.end(), NewestFirst);
        // for recording the level of each table in this vector
        curr_levels.reserve(max_num_tables);
        curr_levels.insert(curr_levels.end(), curr_tables.size(), 0);
        // for recording if the table was open upon entering or not
        curr_open_states.reserve(max_num_tables);

        // call the original function with corresponding SCGraph activated
        foreactor_EnterSCGraph(graph_id);
        auto ret = __real__ZN7leveldb7Version3GetERKNS_11ReadOptionsERKNS_9LookupKeyEPNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS0_8GetStatsE(
                       me, options, k, value, stats);
        foreactor_LeaveSCGraph(graph_id);

        curr_version = nullptr;
        curr_lookup_key = nullptr;
        curr_tables.clear();
        curr_levels.clear();
        curr_open_states.clear();
        curr_table_names.clear();

        return ret;
    }
}


}
