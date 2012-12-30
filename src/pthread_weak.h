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

#ifndef PTHREAD_WEAK_H
#define PTHREAD_WEAK_H 1

#include <pthread.h>


__attribute__((__weak__))
int pthread_key_create(pthread_key_t *key, void (*destructor)(void*));

__attribute__((__weak__))
int pthread_setspecific(pthread_key_t key, const void *value);

__attribute__((__weak__))
int pthread_setcancelstate(int state, int *oldstate);


inline
int
key_create(pthread_key_t *key, void (*destructor)(void*))
{
  if (pthread_key_create)
    return pthread_key_create(key, destructor);

  return 0;
}


inline
int
setspecific(pthread_key_t key, const void *value)
{
  if (pthread_setspecific)
    return pthread_setspecific(key, value);

  return 0;
}


inline
int
setcancelstate(int state, int *oldstate)
{
  if (pthread_setcancelstate)
    return pthread_setcancelstate(state, oldstate);

  return 0;
}


#define pthread_key_create(k, d)  key_create(k, d)
#define pthread_setspecific(k, v)  setspecific(k, v)
#define pthread_setcancelstate(s, o)  setcancelstate(s, o)


#endif  /* ! PTHREAD_WEAK_H */
