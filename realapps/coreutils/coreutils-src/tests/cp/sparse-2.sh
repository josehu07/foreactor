#!/bin/sh
# Exercise a few more corners of the copying code.

# Copyright (C) 2011-2022 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

. "${srcdir=.}/tests/init.sh"; path_prepend_ ./src
print_ver_ cp stat dd

touch sparse_chk
seek_data_capable_ sparse_chk \
  || skip_ "this file system lacks SEEK_DATA support"

# Exercise the code that handles a file ending in a hole.
printf x > k || framework_failure_
dd bs=1k seek=128 of=k < /dev/null || framework_failure_

# The first time through the outer loop, the input file, K, ends with a hole.
# The second time through, we append a byte so that it does not.
for append in no yes; do
  test $append = yes && printf y >> k
  for i in always never; do
    cp --reflink=never --sparse=$i k k2 || fail=1
    cmp k k2 || fail=1
  done
done

# Ensure that --sparse=always can restore holes.
rm -f k
# Create a file starting with an "x", followed by 256K-1 0 bytes.
printf x > k || framework_failure_
dd bs=1k seek=1 of=k count=255 < /dev/zero || framework_failure_

# cp should detect the all-zero blocks and convert some of them to holes.
# How many it detects/converts currently depends on io_blksize.
# Currently, on my F14/ext4 desktop, this K file starts off with size 256KiB,
# (note that the K in the preceding test starts off with size 4KiB).
# cp from coreutils-8.9 with --sparse=always reduces the size to 32KiB.
cp --reflink=never --sparse=always k k2 || fail=1
if test $(stat -c %b k2) -ge $(stat -c %b k); then
  # If not sparse, then double check by creating with dd
  # as we're not guaranteed that seek will create a hole.
  # apfs on darwin 19.2.0 for example was seen to not to create holes < 16MiB.
  hole_size=$(stat -c %o k2) || framework_failure_
  dd if=k of=k2.dd bs=$hole_size conv=sparse || framework_failure_
  test $(stat -c %b k2) -eq $(stat -c %b k2.dd) || fail=1
fi

Exit $fail
