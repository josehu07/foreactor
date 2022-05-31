#include <stdlib.h>


#pragma once


namespace foreactor {


extern bool EnvParsed;
extern bool UseForeactor;


void ParseEnvValues();

int EnvPreIssueDepth(unsigned graph_id);
int EnvUringQueueLen(unsigned graph_id);
int EnvThreadPoolSize(unsigned graph_id);


}
