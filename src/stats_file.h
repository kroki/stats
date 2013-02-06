/*
  Copyright (C) 2012 Tomash Brechko.  All rights reserved.

  This file is part of kroki/stats.

  Kroki/stats is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Kroki/stats is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with kroki/stats.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef STATS_FILE_H
#define STATS_FILE_H 1

#include <stdint.h>


struct thread_slot
{
  union {
    intptr_t tid_neg;           /* < 0 */
    intptr_t next_free_offset;  /* >= 0 */
  };
  intptr_t values[];
};


struct stats_file
{
  uint32_t value_count; /* Number of stats values.  */
  uint32_t slot_size;   /* Size of thread_slot, multiple of cache line size.  */
  uint32_t slot_offset; /* Offset of the first struct thread_slot,
                           bytes from &data[0] */
  /*
    data[] layout:

      uint32_t x count         - name offsets, bytes from &data[0]
      char x L x count         - name strings
      struct thread_slot x T   - per thread slots, each structure
                                 aligned to the next cache line
                                 and occupies slot_size bytes
  */
  uint32_t data[];
};


#endif  /* ! STATS_FILE_H */
