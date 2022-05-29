#include <iostream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <stdlib.h>

#include "debug.hpp"
#include "env_vars.hpp"


namespace foreactor {


// Externed in header.
bool EnvParsed = false;
bool UseForeactor = false;

static std::unordered_map<unsigned, int> uring_queue_lens;
static std::unordered_map<unsigned, int> pre_issue_depths;


void ParseEnvValues() {
    assert(!EnvParsed);

    // parse environment variables to config foreactor
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

        if (!key.compare(0, 6, "QUEUE_")) {
            unsigned graph_id = (unsigned) std::stoi(key.substr(6));
            int uring_queue_len = std::stoi(val);
            PANIC_IF(uring_queue_len <= 0 || uring_queue_len > 1024,
                     "env %s not in range (0, 1024]\n", pair.c_str());
            uring_queue_lens[graph_id] = uring_queue_len;
        } else if (!key.compare(0, 6, "DEPTH_")) {
            unsigned graph_id = (unsigned) std::stoi(key.substr(6));
            int pre_issue_depth = std::stoi(val);
            PANIC_IF(pre_issue_depth < 0,
                     "env %s is < 0\n", pair.c_str());
            pre_issue_depths[graph_id] = pre_issue_depth;
        }

        i++;
    }

    PANIC_IF(uring_queue_lens.size() != pre_issue_depths.size(),
             "not the same number of QUEUE_ and DEPTH_ env variables\n");
    DEBUG("foreactor env config: using foreactor %s\n",
          UseForeactor ? "YES" : "NO");
    DEBUG("foreactor env config: graph queue depth\n");
    for (auto& [graph_id, depth] : pre_issue_depths) {
        PANIC_IF(!uring_queue_lens.contains(graph_id),
                 "graph_id %u not found in QUEUE_ envs\n", graph_id);
        int uring_queue_len = uring_queue_lens[graph_id];
        PANIC_IF(uring_queue_len < depth,
                 "graph_id %u has DEPTH_ %d > QUEUE_ %d\n",
                 graph_id, depth, uring_queue_len);
        DEBUG("                      %5u %5d %5d\n",
              graph_id, uring_queue_len, depth);
    }

    EnvParsed = true;
}


int EnvUringQueueLen(unsigned graph_id) {
    PANIC_IF(!uring_queue_lens.contains(graph_id),
             "graph_id %u not found in uring_queue_lens\n", graph_id);
    return uring_queue_lens[graph_id];
}

int EnvPreIssueDepth(unsigned graph_id) {
    PANIC_IF(!pre_issue_depths.contains(graph_id),
             "graph_id %u not found in pre_issue_depths\n", graph_id);
    return pre_issue_depths[graph_id];
}


}
