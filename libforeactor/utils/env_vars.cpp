#include <iostream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <stdlib.h>

#include "debug.hpp"
#include "env_vars.hpp"


namespace foreactor {


// Externed in header.
thread_local bool EnvParsed = false;
thread_local bool UseForeactor = false;

thread_local std::unordered_map<unsigned, int> pre_issue_depths;
thread_local std::unordered_map<unsigned, int> uring_queue_lens;
thread_local std::unordered_map<unsigned, bool> uring_async_flags;
thread_local std::unordered_map<unsigned, int> thread_pool_sizes;


void ParseEnvValues() {
    assert(!EnvParsed);

    char *env_use_foreactor = getenv("USE_FOREACTOR");
    if (env_use_foreactor != nullptr &&
        std::string(env_use_foreactor) == "yes") {
        UseForeactor = true;
    } else {
        UseForeactor = false;
    }

    size_t i = 0;
    while (environ[i] != nullptr) {
        std::string pair(environ[i]);
        std::string key = pair.substr(0, pair.find("="));
        std::string val = pair.substr(pair.find("=") + 1);
        if (key.length() == 0 || val.length() == 0) {
            i++;
            continue;
        }

        if (!key.compare(0, 6, "DEPTH_")) {
            unsigned graph_id = (unsigned) std::stoi(key.substr(6));
            int pre_issue_depth = std::stoi(val);
            PANIC_IF(pre_issue_depth < 0,
                     "env %s is < 0\n", pair.c_str());
            pre_issue_depths[graph_id] = pre_issue_depth;
        } else if (!key.compare(0, 6, "QUEUE_")) {
            unsigned graph_id = (unsigned) std::stoi(key.substr(6));
            int uring_queue_len = std::stoi(val);
            PANIC_IF(uring_queue_len <= 0 || uring_queue_len > 1024,
                     "env %s not in range (0, 1024]\n", pair.c_str());
            uring_queue_lens[graph_id] = uring_queue_len;
        } else if (!key.compare(0, 15, "SQE_ASYNC_FLAG_")) {
            unsigned graph_id = (unsigned) std::stoi(key.substr(15));
            bool sqe_async_flag = false;
            if (val == "yes")
                sqe_async_flag = true;
            uring_async_flags[graph_id] = sqe_async_flag;
        } else if (!key.compare(0, 9, "UTHREADS_")) {
            unsigned graph_id = (unsigned) std::stoi(key.substr(9));
            int thread_pool_size = std::stoi(val);
            PANIC_IF(thread_pool_size <= 0,
                     "env %s is <= 0\n", pair.c_str());
            thread_pool_sizes[graph_id] = thread_pool_size;
        }

        i++;
    }

    DEBUG("foreactor env config: using foreactor %s\n",
          UseForeactor ? "YES" : "NO");
    DEBUG("foreactor env config: graph depth queue sqe_async uthreads\n");
    for (auto& [graph_id, depth] : pre_issue_depths) {
        [[maybe_unused]] int uring_queue_len =
            uring_queue_lens.contains(graph_id) ? uring_queue_lens[graph_id]
                                                : 0;
        [[maybe_unused]] bool sqe_async_flag =
            uring_async_flags.contains(graph_id) ? uring_async_flags[graph_id]
                                                 : false;
        [[maybe_unused]] int thread_pool_size =
            thread_pool_sizes.contains(graph_id) ? thread_pool_sizes[graph_id]
                                                 : 0;
        DEBUG("                      %5u %5d %5d %9d %8d\n", graph_id,
              depth, uring_queue_len, sqe_async_flag, thread_pool_size);
    }

    EnvParsed = true;
}


int EnvPreIssueDepth(unsigned graph_id) {
    PANIC_IF(!pre_issue_depths.contains(graph_id),
             "graph_id %u not found in pre_issue_depths\n", graph_id);
    return pre_issue_depths[graph_id];
}

int EnvUringQueueLen(unsigned graph_id) {
    PANIC_IF(!uring_queue_lens.contains(graph_id),
             "graph_id %u not found in uring_queue_lens\n", graph_id);
    int uring_queue_len = uring_queue_lens[graph_id];
    int depth = EnvPreIssueDepth(graph_id);
    PANIC_IF(uring_queue_len < depth,
             "graph_id %u has DEPTH_ %d > QUEUE_ %d\n",
             graph_id, depth, uring_queue_len);
    return uring_queue_len;
}

bool EnvUringAsyncFlag(unsigned graph_id) {
    PANIC_IF(!uring_async_flags.contains(graph_id),
             "graph_id %u not found in uring_async_flags\n", graph_id);
    return uring_async_flags[graph_id];
}

int EnvThreadPoolSize(unsigned graph_id) {
    if (!thread_pool_sizes.contains(graph_id))
        return 0;   // using io_uring backend
    return thread_pool_sizes[graph_id];
}


}
