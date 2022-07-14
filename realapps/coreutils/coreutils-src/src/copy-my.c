/* [foreactor] for interception */

#include <config.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#include "backupfile.h"
#include "copy.h"

bool
sparse_copy_my (int src_fd, int dest_fd, char **abuf, size_t buf_size,
                size_t hole_size, bool punch_holes, bool allow_reflink,
                char const *src_name, char const *dst_name,
                uintmax_t max_n_read, off_t *total_n_read,
                bool *last_write_made_hole) {
  return sparse_copy (src_fd, dest_fd, abuf, buf_size, hole_size, punch_holes,
                      allow_reflink, src_name, dst_name, max_n_read,
                      total_n_read, last_write_made_hole);
}

void
inform_src_file_size (size_t src_file_size) {
  // inform src_file_size through linker interception
  return;
}
