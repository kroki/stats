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

#ifndef SYSCALL_H
#define SYSCALL_H 1

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>


inline
pid_t
gettid(void)
{
  return syscall(SYS_gettid);
}


#endif  /* ! SYSCALL_H */
