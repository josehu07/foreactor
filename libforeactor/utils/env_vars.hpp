#include <stdlib.h>


#pragma once


namespace foreactor {


extern thread_local bool EnvParsed;
extern thread_local bool UseForeactor;


void ParseEnvValues();

int EnvPreIssueDepth(unsigned graph_id);
int EnvUringQueueLen(unsigned graph_id);
bool EnvUringAsyncFlag(unsigned graph_id);
int EnvThreadPoolSize(unsigned graph_id);


}
