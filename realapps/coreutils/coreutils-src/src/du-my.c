/* [foreactor] for interception */

#include <config.h>

#include "xfts.h"
#include "du.h"

bool
process_file_my (FTS *fts, FTSENT *ent) {
  return process_file (fts, ent);
}

bool
du_files_my (char **files, int bit_flags) {
  return du_files (files, bit_flags);
}
