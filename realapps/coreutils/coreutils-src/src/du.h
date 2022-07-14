/* [foreactor] for interception */

#include "xfts.h"

#ifndef __DU_H__
# define __DU_H__

bool process_file (FTS *fts, FTSENT *ent);
bool process_file_my (FTS *fts, FTSENT *ent);

bool du_files (char **files, int bit_flags);
bool du_files_my (char **files, int bit_flags);

#endif
