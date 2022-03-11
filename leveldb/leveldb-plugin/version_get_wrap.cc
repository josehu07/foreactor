#include <vector>

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


/////////////////////////
// SCGraph value pools //
/////////////////////////

static constexpr unsigned version_get_graph_id = 0;
thread_local fa::SCGraph scgraph(version_get_graph_id, /*max-dims*/ 1);

// Meaning of dimension values:
//   - max_dims: total number of loops (i.e., back-pointing edges) in scgraph;
//               must equal the max_dims of scgraph
//   - num_dims: specifies across how many loop dimensions does this value
//               change during an execution of scgraph; 0 means scalar, etc.
//   - dim_idx vector: length num_dims, specifying the loop dimensions

thread_local fa::ValuePool<int, /*max_dims*/ 1, /*num_dims*/ 0> branch_empty_decision({});

thread_local fa::ValuePool<int, 1, 1> branch_open_decision({0});

thread_local fa::ValuePool<fa::SyscallStage, 1, 1> open_stage({0});
thread_local fa::ValuePool<long, 1, 1> open_rc({0});
thread_local fa::ValuePool<std::string, 1, 1> open_pathname({0});
thread_local fa::ValuePool<int, 1, 0> open_flags({});
thread_local fa::ValuePool<mode_t, 1, 0> open_mode({});

thread_local fa::ValuePool<fa::SyscallStage, 1, 1> pread_footer_stage({0});
thread_local fa::ValuePool<long, 1, 1> pread_footer_rc({0});
thread_local fa::ValuePool<int, 1, 1> pread_footer_fd({0});
thread_local fa::ValuePool<size_t, 1, 0> pread_footer_count({});
thread_local fa::ValuePool<off_t, 1, 1> pread_footer_offset({0});
thread_local fa::ValuePool<char *, 1, 1> pread_footer_internal_buf({0});

thread_local fa::ValuePool<fa::SyscallStage, 1, 1> pread_index_stage({0});
thread_local fa::ValuePool<long, 1, 1> pread_index_rc({0});
thread_local fa::ValuePool<int, 1, 1> pread_index_fd({0});
thread_local fa::ValuePool<size_t, 1, 1> pread_index_count({0});
thread_local fa::ValuePool<off_t, 1, 1> pread_index_offset({0});
thread_local fa::ValuePool<char *, 1, 1> pread_index_internal_buf({0});

thread_local fa::ValuePool<fa::SyscallStage, 1, 1> pread_data_stage({0});
thread_local fa::ValuePool<long, 1, 1> pread_data_rc({0});
thread_local fa::ValuePool<int, 1, 1> pread_data_fd({0});
thread_local fa::ValuePool<size_t, 1, 1> pread_data_count({0});
thread_local fa::ValuePool<off_t, 1, 1> pread_data_offset({0});
thread_local fa::ValuePool<char *, 1, 1> pread_data_internal_buf({0});

thread_local fa::ValuePool<int, 1, 1> branch_loop_decision({0});


////////////////////////////
// SCGraph static builder //
////////////////////////////

// Called only once.
void BuildVersionGetSCGraph() {
    auto node_branch_empty = new fa::BranchNode(&branch_empty_decision);
    auto node_branch_open = new fa::BranchNode(&branch_open_decision);
    auto node_open = new fa::SyscallOpen(&open_stage,
                                         &open_rc,
                                         &open_pathname,
                                         &open_flags,
                                         &open_mode);
    auto node_pread_footer = new fa::SyscallPread(&pread_footer_stage,
                                                  &pread_footer_rc,
                                                  &pread_footer_fd,
                                                  &pread_footer_count,
                                                  &pread_footer_offset,
                                                  &pread_footer_internal_buf);
    auto node_pread_index = new fa::SyscallPread(&pread_index_stage,
                                                 &pread_index_rc,
                                                 &pread_index_fd,
                                                 &pread_index_count,
                                                 &pread_index_offset,
                                                 &pread_index_internal_buf);
    auto node_pread_data = new fa::SyscallPread(&pread_data_stage,
                                                &pread_data_rc,
                                                &pread_data_fd,
                                                &pread_data_count,
                                                &pread_data_offset,
                                                &pread_data_internal_buf);
    auto node_branch_loop = new fa::BranchNode(&branch_loop_decision);

    scgraph.AddNode(node_branch_empty, /*is_start*/ true);
    scgraph.AddNode(node_branch_open);
    scgraph.AddNode(node_open);
    scgraph.AddNode(node_pread_footer);
    scgraph.AddNode(node_pread_index);
    scgraph.AddNode(node_pread_data);
    scgraph.AddNode(node_branch_loop);

    node_branch_empty->AppendChild(nullptr);
    node_branch_empty->AppendChild(node_branch_open);
    node_branch_open->AppendChild(node_open);
    node_branch_open->AppendChild(node_pread_data);
    node_open->SetNext(node_pread_footer, /*weak_edge*/ false);
    node_pread_footer->SetNext(node_pread_index, /*weak_edge*/ false);
    node_pread_index->SetNext(node_pread_data, /*weak_edge*/ false);
    node_pread_data->SetNext(node_branch_loop, /*weak_edge*/ true);
    node_branch_loop->AppendChild(nullptr);
    node_branch_loop->AppendChild(node_branch_open, /*dim_idx*/ 0);

    open_flags.data = O_CLOEXEC;
    open_flags.ready = true;
    open_mode.data = O_RDONLY;
    open_mode.ready = true;

    pread_footer_count.data = Footer::kEncodedLength;
    pread_footer_count.ready = true;
}


///////////////////////////////////
// SCGraph pool setter & cleaner //
///////////////////////////////////

// Called upon every entrance of wrapped function.
void FlushVersionGetPools(Version *me, const ReadOptions& options,
                          const LookupKey& k, std::string *value,
                          Version::GetStats *stats) {
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
    tables.reserve(config::kNumLevels - 1);
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

    if (!tables.empty()) {
        size_t tables_size = tables.size();

        branch_empty_decision.data = 1;
        branch_empty_decision.ready = true;

        branch_open_decision.data.resize(tables_size, 1);
        branch_open_decision.ready.resize(tables_size, true);

        open_stage.data.resize(tables_size, fa::STAGE_UNISSUED);
        open_stage.ready.resize(tables_size, true);
        open_rc.data.resize(tables_size);
        open_rc.ready.resize(tables_size, false);
        open_pathname.data.resize(tables_size);
        open_pathname.ready.resize(tables_size, true);

        pread_footer_stage.data.resize(tables_size, fa::STAGE_NOTREADY);
        pread_footer_stage.ready.resize(tables_size, true);
        pread_footer_rc.data.resize(tables_size);
        pread_footer_rc.ready.resize(tables_size, false);
        pread_footer_fd.data.resize(tables_size);
        pread_footer_fd.ready.resize(tables_size, false);
        pread_footer_offset.data.resize(tables_size);
        pread_footer_offset.ready.resize(tables_size, true);
        pread_footer_internal_buf.data.resize(tables_size);
        pread_footer_internal_buf.ready.resize(tables_size, false);

        pread_index_stage.data.resize(tables_size, fa::STAGE_NOTREADY);
        pread_index_stage.ready.resize(tables_size, true);
        pread_index_rc.data.resize(tables_size);
        pread_index_rc.ready.resize(tables_size, false);
        pread_index_fd.data.resize(tables_size);
        pread_index_fd.ready.resize(tables_size, false);
        pread_index_count.data.resize(tables_size);
        pread_index_count.ready.resize(tables_size, false);
        pread_index_offset.data.resize(tables_size);
        pread_index_offset.ready.resize(tables_size, false);
        pread_index_internal_buf.data.resize(tables_size);
        pread_index_internal_buf.ready.resize(tables_size, false);

        pread_data_stage.data.resize(tables_size, fa::STAGE_UNISSUED);
        pread_data_stage.ready.resize(tables_size, true);
        pread_data_rc.data.resize(tables_size);
        pread_data_rc.ready.resize(tables_size, false);
        pread_data_fd.data.resize(tables_size);
        pread_data_fd.ready.resize(tables_size, true);
        pread_data_count.data.resize(tables_size);
        pread_data_count.ready.resize(tables_size, true);
        pread_data_offset.data.resize(tables_size);
        pread_data_offset.ready.resize(tables_size, true);
        pread_data_internal_buf.data.resize(tables_size);
        pread_data_internal_buf.ready.resize(tables_size, false);

        branch_loop_decision.data.resize(tables_size, 1);
        branch_loop_decision.ready.resize(tables_size, true);

        for (size_t i = 0; i < tables_size; i++) {
            Cache::Handle *handle = TryFindTable(me->vset_->table_cache_, tables[i]->number);
            Table *table = CacheHandleToTable(me->vset_->table_cache_, handle);

            if (table == nullptr) {
                branch_open_decision.data[i] = 0;
                open_pathname.data[i] = TableFileName(me->vset_->dbname_, tables[i]->number);
                pread_footer_offset.data[i] = tables[i]->file_size - Footer::kEncodedLength;
                pread_data_stage.data[i] = fa::STAGE_NOTREADY;
                pread_data_fd.ready[i] = false;
                pread_data_count.ready[i] = false;
                pread_data_offset.ready[i] = false;
            } else {
                BlockHandle block_handle;
                bool searched = SearchBlockHandle(table, k.internal_key(), block_handle);
                assert(searched);
                pread_data_fd.data[i] = GetFileFd(table);
                pread_data_count.data[i] = BlockHandleToSize(block_handle);
                pread_data_offset.data[i] = BlockHandleToOffset(block_handle);
            }

            if (i == tables_size - 1)
                branch_loop_decision.data[i] = 0;

            if (handle != nullptr)
                ReleaseCacheHandle(me->vset_->table_cache_, handle);
        }
    } else {
        branch_empty_decision.data = 0;
        branch_empty_decision.ready = true;
    }
}

// Called upon every exit of wrapped function.
void ClearVersionGetPools() {
    branch_empty_decision.ClearValues();

    branch_open_decision.ClearValues();

    open_stage.ClearValues();
    open_rc.ClearValues();
    open_pathname.ClearValues();

    pread_footer_stage.ClearValues();
    pread_footer_rc.ClearValues();
    pread_footer_fd.ClearValues();
    pread_footer_offset.ClearValues();
    pread_footer_internal_buf.ClearValues(/*do_delete*/ true);

    pread_index_stage.ClearValues();
    pread_index_rc.ClearValues();
    pread_index_fd.ClearValues();
    pread_index_count.ClearValues();
    pread_index_offset.ClearValues();
    pread_index_internal_buf.ClearValues(/*do_delete*/ true);

    pread_data_stage.ClearValues();
    pread_data_rc.ClearValues();
    pread_data_fd.ClearValues();
    pread_data_count.ClearValues();
    pread_data_offset.ClearValues();
    pread_data_internal_buf.ClearValues(/*do_delete*/ true);

    branch_loop_decision.ClearValues();
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
    fa::WrapperFuncEnter(&scgraph, &ring, version_get_graph_id,
                         BuildVersionGetSCGraph, FlushVersionGetPools,
                         me, options, k, value, stats);

    // Call the original function.
    auto ret = __real__ZN7leveldb7Version3GetERKNS_11ReadOptionsERKNS_9LookupKeyEPNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEPNS0_8GetStatsE(
        me, options, k, value, stats);

    fa::WrapperFuncLeave(&scgraph, ClearVersionGetPools);
    return ret;
}


}
