#include <stdlib.h>


#pragma once


namespace foreactor {


extern bool EnvParsed;
extern bool UseForeactor;


void ParseEnvValues();
int EnvUringQueueLen(unsigned graph_id);
int EnvPreIssueDepth(unsigned graph_id);


}
