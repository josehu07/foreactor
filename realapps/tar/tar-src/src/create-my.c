/* [foreactor] dummy wrapper over `dump_dir0()` to force it to participate in
               the linking phase and allow it to be intercepted. */

#include <system.h>

#include "common.h"


void
dump_file_my (struct tar_stat_info *parent, char const *name,
              char const *fullname)
{
  dump_file (parent, name, fullname);
}
