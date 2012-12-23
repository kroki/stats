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

#include "../src/kroki/stats.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <omp.h>

#define PRAGMA(a)  _Pragma(#a)
#define OMP(a)  PRAGMA(omp a)


int
main(void)
{
  alarm(60);

  OMP(parallel)
  {
    unsigned int seed = time(NULL) + omp_get_thread_num();
    long total_nsec = 0;
    while (1)
      {
        ++stats(kroki.stats.iterations);

        int r = rand_r(&seed);
        long nsec = r / (RAND_MAX + 1.0) * 1000000000;
        struct timespec timeout = {
          .tv_sec = 0,
          .tv_nsec = nsec
        };
        nanosleep(&timeout, NULL);
        total_nsec += nsec;

        if (total_nsec >= 1000000000)
          {
            total_nsec = 0;

            ++stats(kroki.stats.updates);
            stats(kroki.stats.nsec) = nsec;
          }
      }
  }

  return EXIT_SUCCESS;
}
