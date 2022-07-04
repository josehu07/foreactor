/* Convert decimal strings with bounds checking and exit on error.

   Copyright (C) 2014-2022 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef XDECTOINT_H_
# define XDECTOINT_H_ 1

# include <inttypes.h>

# define _DECLARE_XDECTOINT(name, type) \
  type name (char const *n_str, type min, type max, \
             char const *suffixes, char const *err, int err_exit) \
    _GL_ATTRIBUTE_NONNULL ((1, 5));
# define _DECLARE_XNUMTOINT(name, type) \
  type name (char const *n_str, int base, type min, type max, \
             char const *suffixes, char const *err, int err_exit) \
    _GL_ATTRIBUTE_NONNULL ((1, 6));

_DECLARE_XDECTOINT (xdectoimax, intmax_t)
_DECLARE_XDECTOINT (xdectoumax, uintmax_t)

_DECLARE_XNUMTOINT (xnumtoimax, intmax_t)
_DECLARE_XNUMTOINT (xnumtoumax, uintmax_t)

#endif  /* not XDECTOINT_H_ */
