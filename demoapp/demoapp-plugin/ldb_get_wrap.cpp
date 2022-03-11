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
    auto node_branch0 = new fa::BranchNode("file_open",
                                           &branch0_decision);
    auto node_open = new fa::SyscallOpen("open",
                                         &open_stage,
                                         &open_rc,
                                         &open_pathname,
                                         &open_flags,
                                         &open_mode);
    auto node_pread = new fa::SyscallPread("pread",
                                           &pread_stage,
                                           &pread_rc,
                                           &pread_fd,
                                           &pread_count,
                                           &pread_offset,
                                           &pread_internal_buf);
    auto node_branch1 = new fa::BranchNode("has_more",
                                           &branch1_decision);

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

    open_flags.data = 0;
    open_flags.ready = true;
    open_mode.data = O_RDONLY;
    open_mode.ready = true;

    pread_count.data = FILE_SIZE;
    pread_count.ready = true;
    pread_offset.data = 0;
    pread_offset.ready = true;
}


///////////////////////////////////
// SCGraph pool setter & cleaner //
///////////////////////////////////

// Called upon every entrance of wrapped function.
void flush_ldb_get_pools(std::vector<std::vector<int>>& files) {
    branch0_decision.data.resize(FILES_PER_LEVEL, 1);
    branch0_decision.ready.resize(FILES_PER_LEVEL, true);

    open_stage.data.resize(FILES_PER_LEVEL, fa::STAGE_UNISSUED);
    open_stage.ready.resize(FILES_PER_LEVEL, true);
    open_rc.data.resize(FILES_PER_LEVEL);
    open_rc.ready.resize(FILES_PER_LEVEL, false);
    open_pathname.data.resize(FILES_PER_LEVEL);
    open_pathname.ready.resize(FILES_PER_LEVEL, true);

    pread_stage.data.resize(FILES_PER_LEVEL, fa::STAGE_UNISSUED);
    pread_stage.ready.resize(FILES_PER_LEVEL, true);
    pread_rc.data.resize(FILES_PER_LEVEL);
    pread_rc.ready.resize(FILES_PER_LEVEL, false);
    pread_fd.data.resize(FILES_PER_LEVEL);
    pread_fd.ready.resize(FILES_PER_LEVEL, true);
    pread_internal_buf.data.resize(FILES_PER_LEVEL, nullptr);
    pread_internal_buf.ready.resize(FILES_PER_LEVEL, false);

    branch1_decision.data.resize(FILES_PER_LEVEL, 1);
    branch1_decision.ready.resize(FILES_PER_LEVEL, true);

    size_t idx = 0;
    for (int i = FILES_PER_LEVEL - 1; i >= 0; --i) {
        int fd = files[0][i];

        if (fd < 0) {
            branch0_decision.data[idx] = 0;
            open_pathname.data[idx] = table_name(0, i);
            pread_stage.data[idx] = fa::STAGE_NOTREADY;
            pread_fd.ready[idx] = false;
        } else
            pread_fd.data[idx] = fd;

        if (i == 0)
            branch1_decision.data[idx] = 0;

        idx++;
    }
}

// Called upon every exit of wrapped function.
void clear_ldb_get_pools() {
    branch0_decision.ClearValues();

    open_stage.ClearValues();
    open_rc.ClearValues();
    open_pathname.ClearValues();

    pread_stage.ClearValues();
    pread_rc.ClearValues();
    pread_fd.ClearValues();
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
