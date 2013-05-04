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

#ifndef KROKI_STATS_BITS_STATS_MODULE_H
#define KROKI_STATS_BITS_STATS_MODULE_H 1

#include <stdint.h>


struct _kroki_stats_module
{
  struct _kroki_stats_module *next;
  intptr_t *(*thread_offset)(void);
  const char *const *name_refs;
  uint32_t names_size;
  uint32_t value_count;
};


extern struct _kroki_stats_module *_kroki_stats_module_head;


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


__attribute__((__nothrow__))
int
kroki_stats_open(const char *filename);


__attribute__((__nothrow__))
void
_kroki_stats_thread_slot_create(void);


__attribute__((__nothrow__))
void
kroki_stats_atfork_child(void);


#ifdef __cplusplus
}      /* extern "C" */
#endif  /* __cplusplus */


#endif  /* ! KROKI_STATS_BITS_STATS_MODULE_H */
