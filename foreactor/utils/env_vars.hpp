#include <stdlib.h>


#ifndef __FOREACTOR_ENV_VARS_H__
#define __FOREACTOR_ENV_VARS_H__


namespace foreactor {


extern bool EnvParsed;
extern bool UseForeactor;


void ParseEnvValues();
int EnvUringQueueLen(unsigned graph_id);
int EnvPreIssueDepth(unsigned graph_id);


}


#endif
