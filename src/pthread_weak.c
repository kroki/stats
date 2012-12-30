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

#include "pthread_weak.h"


extern __attribute__((__visibility__("hidden")))
inline int key_create(pthread_key_t *key, void (*destructor)(void*));

extern __attribute__((__visibility__("hidden")))
inline int setspecific(pthread_key_t key, const void *value);

extern __attribute__((__visibility__("hidden")))
inline int setcancelstate(int state, int *oldstate);
