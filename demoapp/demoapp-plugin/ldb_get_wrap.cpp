#include <vector>
#include <fcntl.h>
#include <unistd.h>

#include "ldb_get.hpp"

#include <foreactor.hpp>
namespace fa = foreactor;


//////////////////////
// IOUring instance //
//////////////////////

// There is one IOUring instance per wrapped function per thread.
thread_local fa::IOUring ring;


/////////////////////////
// SCGraph value pools //
/////////////////////////

static constexpr unsigned ldb_get_graph_id = 0;
thread_local fa::SCGraph scgraph(ldb_get_graph_id, /*max_dims*/ 1);

// Meaning of dimension values:
//   - max_dims: total number of loops (i.e., back-pointing edges) in scgraph;
//               must equal the max_dims of scgraph
//   - num_dims: specifies across how many loop dimensions does this value
//               change during an execution of scgraph; 0 means scalar, etc.
//   - dim_idx vector: length num_dims, specifying the loop dimensions

thread_local fa::ValuePool<int, /*max_dims*/ 1, /*num_dims*/ 1> branch0_decision(/*dim_idx*/ {0});

thread_local fa::ValuePool<fa::SyscallStage, 1, 1> open_stage({0});
thread_local fa::ValuePool<long, 1, 1> open_rc({0});
thread_local fa::ValuePool<std::string, 1, 1> open_pathname({0});
thread_local fa::ValuePool<int, 1, 0> open_flags({});
thread_local fa::ValuePool<mode_t, 1, 0> open_mode({});

thread_local fa::ValuePool<fa::SyscallStage, 1, 1> pread_stage({0});
thread_local fa::ValuePool<long, 1, 1> pread_rc({0});
thread_local fa::ValuePool<int, 1, 1> pread_fd({0});
thread_local fa::ValuePool<size_t, 1, 0> pread_count({});
thread_local fa::ValuePool<off_t, 1, 0> pread_offset({});
thread_local fa::ValuePool<char *, 1, 1> pread_internal_buf({0});

thread_local fa::ValuePool<int, 1, 1> branch1_decision({0});


////////////////////////////
// SCGraph static builder //
////////////////////////////

// Called only once.
void build_ldb_get_scgraph() {
    auto node_branch0 = new fa::BranchNode(&branch0_decision);
    auto node_open = new fa::SyscallOpen(&open_stage,
                                         &open_rc,
                                         &open_pathname,
                                         &open_flags,
                                         &open_mode);
    auto node_pread = new fa::SyscallPread(&pread_stage,
                                           &pread_rc,
                                           &pread_fd,
                                           &pread_count,
                                           &pread_offset,
                                           &pread_internal_buf);
    auto node_branch1 = new fa::BranchNode(&branch1_decision);

    scgraph.AddNode(node_branch0, /*is_start*/ true);
    scgraph.AddNode(node_open);
    scgraph.AddNode(node_pread);
    scgraph.AddNode(node_branch1);

    node_branch0->AppendChild(node_open);
    node_branch0->AppendChild(node_pread);
    node_open->SetNext(node_pread, /*weak_edge*/ false);
    node_pread->SetNext(node_branch1, /*weak_edge*/ true);
    node_branch1->AppendChild(nullptr);
    node_branch1->AppendChild(node_branch0, /*dim_idx*/ 0);
}


///////////////////////////////////
// SCGraph pool setter & cleaner //
///////////////////////////////////

// Called upon every entrance of wrapped function.
void flush_ldb_get_pools(std::vector<std::vector<int>>& files) {
    branch0_decision.data.reserve(FILES_PER_LEVEL);
    branch0_decision.ready.reserve(FILES_PER_LEVEL);

    open_stage.data.reserve(FILES_PER_LEVEL);
    open_stage.ready.reserve(FILES_PER_LEVEL);
    open_rc.data.reserve(FILES_PER_LEVEL);
    open_rc.ready.reserve(FILES_PER_LEVEL);
    open_pathname.data.reserve(FILES_PER_LEVEL);
    open_pathname.ready.reserve(FILES_PER_LEVEL);

    pread_stage.data.reserve(FILES_PER_LEVEL);
    pread_stage.ready.reserve(FILES_PER_LEVEL);
    pread_rc.data.reserve(FILES_PER_LEVEL);
    pread_rc.ready.reserve(FILES_PER_LEVEL);
    pread_fd.data.reserve(FILES_PER_LEVEL);
    pread_fd.ready.reserve(FILES_PER_LEVEL);
    pread_internal_buf.data.reserve(FILES_PER_LEVEL);
    pread_internal_buf.ready.reserve(FILES_PER_LEVEL);

    branch1_decision.data.reserve(FILES_PER_LEVEL);
    branch1_decision.ready.reserve(FILES_PER_LEVEL);

    for (int i = FILES_PER_LEVEL - 1; i >= 0; --i) {
        int fd = files[0][i];

        branch0_decision.data.push_back(fd < 0 ? 0 : 1);
        branch0_decision.ready.push_back(true);

        open_stage.data.push_back(fa::STAGE_UNISSUED);
        open_stage.ready.push_back(true);
        open_rc.data.push_back(-1);
        open_rc.ready.push_back(false);
        open_pathname.data.push_back(table_name(0, i));
        open_pathname.ready.push_back(true);
        open_flags.data = 0;
        open_flags.ready = true;
        open_mode.data = O_RDONLY;
        open_mode.ready = true;

        pread_stage.data.push_back(fd < 0 ? fa::STAGE_NOTREADY : fa::STAGE_UNISSUED);
        pread_stage.ready.push_back(true);
        pread_rc.data.push_back(-1);
        pread_rc.ready.push_back(false);
        pread_fd.data.push_back(fd);
        pread_fd.ready.push_back(fd < 0 ? false : true);
        pread_count.data = FILE_SIZE;
        pread_count.ready = true;
        pread_offset.data = 0;
        pread_offset.ready = true;
        pread_internal_buf.data.push_back(nullptr);
        pread_internal_buf.ready.push_back(false);

        branch1_decision.data.push_back(i == 0 ? 0 : 1);
        branch1_decision.ready.push_back(true);
    }

    // std::vector<int> branch0_decision_data;
    // branch0_decision_data.reserve(FILES_PER_LEVEL);
    // for (auto it = files[0].crbegin(); it != files[0].crend(); ++it)
    //     branch0_decision_data.push_back(*it < 0 ? 0 : 1);
    // branch0_decision.SetValueBatch(branch0_decision_data);

    // open_stage.SetValueBatch(std::vector<fa::SyscallStage>(FILES_PER_LEVEL, fa::STAGE_UNISSUED));
    // open_rc.SetValueBatch(std::vector<long>(FILES_PER_LEVEL, -1));
    // std::vector<std::string> open_pathname_data;
    // open_pathname_data.reserve(FILES_PER_LEVEL);
    // for (int i = FILES_PER_LEVEL - 1; i >= 0; --i)
    //     open_pathname_data.push_back(table_name(0, i));
    // open_pathname.SetValueBatch(open_pathname_data);
    // open_flags.SetValueBatch(0);
    // open_mode.SetValueBatch(O_RDONLY);

    // std::vector<fa::SyscallStage> pread_stage_data;
    // pread_stage_data.reserve(FILES_PER_LEVEL);
    // for (auto it = files[0].crbegin(); it != files[0].crend(); ++it)
    //     pread_stage_data.push_back(*it < 0 ? fa::STAGE_NOTREADY : fa::STAGE_UNISSUED);
    // pread_stage.SetValueBatch(pread_stage_data);
    // pread_rc.SetValueBatch(std::vector<long>(FILES_PER_LEVEL, -1));
    // std::vector<int> pread_fd_data;
    // std::vector<bool> pread_fd_ready;
    // pread_fd_data.reserve(FILES_PER_LEVEL);
    // pread_fd_ready.reserve(FILES_PER_LEVEL);
    // for (auto it = files[0].crbegin(); it != files[0].crend(); ++it) {
    //     pread_fd_data.push_back(*it);
    //     pread_fd_ready.push_back(*it < 0 ? false : true);
    // }
    // pread_fd.SetValueBatch(pread_fd_data, pread_fd_ready);
    // pread_count.SetValueBatch(FILE_SIZE);
    // pread_offset.SetValueBatch(0);
    // pread_internal_buf.SetValueBatch(std::vector<char *>(FILES_PER_LEVEL, nullptr));

    // std::vector<int> branch1_decision_data(FILES_PER_LEVEL, 1);
    // branch1_decision_data.back() = 0;
    // branch1_decision.SetValueBatch(branch1_decision_data);
}

// Called upon every exit of wrapped function.
void clear_ldb_get_pools() {
    branch0_decision.ClearValues();

    open_stage.ClearValues();
    open_rc.ClearValues();
    open_pathname.ClearValues();
    open_flags.ClearValues();
    open_mode.ClearValues();

    pread_stage.ClearValues();
    pread_rc.ClearValues();
    pread_fd.ClearValues();
    pread_count.ClearValues();
    pread_offset.ClearValues();
    pread_internal_buf.ClearValues(/*do_delete*/ true);

    branch1_decision.ClearValues();
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
extern "C" std::vector<std::string> __real__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(
        std::vector<std::vector<int>>& files);

extern "C" std::vector<std::string> __wrap__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(
        std::vector<std::vector<int>>& files) {
    fa::WrapperFuncEnter(&scgraph, &ring, ldb_get_graph_id,
                         build_ldb_get_scgraph, flush_ldb_get_pools, files);

    // Call the original function.
    auto ret = __real__Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE(files);

    fa::WrapperFuncLeave(&scgraph, clear_ldb_get_pools);
    return ret;
}
