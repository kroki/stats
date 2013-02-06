/*
  Copyright (C) 2012-2013 Tomash Brechko.  All rights reserved.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "stats_file.h"
#include <kroki/error.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>


static struct option options[] = {
  { .name = "version", .val = 'v' },
  { .name = "help", .val = 'h' },
  { .name = NULL },
};


static
void
usage(FILE *out)
{
  fprintf(out,
          "Usage: %s [OPTIONS] STATSFILE\n"
          "\n"
          "Options are:\n"
          "  --version, -v               Print package version and copyright\n"
          "  --help, -h                  Print this message\n",
          program_invocation_short_name);
}


static
void
version(FILE *out)
{
  fprintf(out,
          "%s\n"
          "%s\n"
          "Report bugs to <%s> or file an issue at\n"
          "<%s>.\n",
          PACKAGE_STRING,
          PACKAGE_COPYRIGHT,
          PACKAGE_BUGREPORT,
          PACKAGE_URL);
}


static const char *stats_filename;


static
void
process_args(int argc, char *argv[])
{
  int opt;
  while ((opt = getopt_long(argc, argv, "vh", options, NULL)) != -1)
    {
      switch (opt)
        {
        case 'v':
          version(stdout);
          exit(EXIT_SUCCESS);

        case 'h':
          usage(stdout);
          exit(EXIT_SUCCESS);

        default:
          usage(stderr);
          exit(EXIT_FAILURE);
        }
    }
  if (optind != argc - 1)
    {
      usage(stderr);
      exit(EXIT_FAILURE);
    }

  stats_filename = argv[optind++];
}


static
void
output_stats(void)
{
  int fd = open(stats_filename, O_RDONLY);
  if (fd == -1)
    error("%s: %m", stats_filename);

  struct stat fstats;
  SYS(fstat(fd, &fstats));

  if (! S_ISREG(fstats.st_mode))
    error("%s: not a regular file", stats_filename);

  if (fstats.st_size == 0)
    {
      SYS(close(fd));
      return;
    }

  if ((size_t) fstats.st_size < sizeof(struct stats_file))
    error("%s: invalid file format", stats_filename);

  struct stats_file *file =
    CHECK(mmap(NULL, fstats.st_size, PROT_READ, MAP_SHARED, fd, 0),
          == MAP_FAILED, die, "%m");

  uint32_t count = __atomic_load_n(&file->value_count, __ATOMIC_ACQUIRE);
  if (count)
    {
      char *file_end = (char *) file + fstats.st_size;
      struct thread_slot *slot = (struct thread_slot *)
        ((char *) file->data + file->slot_offset);

      if ((file_end - (char *) slot) % file->slot_size != 0)
        error("%s: invalid file format", stats_filename);

      intptr_t *values = MEM(malloc(sizeof(*values) * count));

      while ((char *) slot < file_end)
        {
          /*
            We avoid processing values while they are being reset when
            thread slot is about to be reused.  As TIDs aren't reused
            right away this works very much like sequential lock.
          */
          long tid = -__atomic_load_n(&slot->tid_neg, __ATOMIC_ACQUIRE);
          while (tid > 0)
            {
              memcpy(values, slot->values, sizeof(intptr_t) * count);

              // Emit compiler barrier and load-load memory barrier.
              __atomic_signal_fence(__ATOMIC_ACQ_REL);
              __atomic_load_n(&slot->tid_neg, __ATOMIC_ACQUIRE);

              long new_tid = -__atomic_load_n(&slot->tid_neg, __ATOMIC_ACQUIRE);
              if (tid == new_tid)
                {
                  for (uint32_t i = 0; i < count; ++i)
                    {
                      char *name = (char *) file->data + file->data[i];
                      long value = values[i];
                      printf("[%ld] %s: %ld\n", tid, name, value);
                    }
                  break;
                }

              tid = new_tid;
            }

          slot = (struct thread_slot *) ((char *) slot + file->slot_size);
        }

      free(values);
    }

  SYS(munmap(file, fstats.st_size));
  SYS(close(fd));
}


int
main(int argc, char *argv[])
{
  process_args(argc, argv);

  output_stats();

  return EXIT_SUCCESS;
}
