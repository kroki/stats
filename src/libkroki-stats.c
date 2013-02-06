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
#include "kroki/bits/module.h"
#include "stats_file.h"
#include "syscall.h"
#include <kroki/error.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>


#define unlikely(expr)  __builtin_expect(!! (expr), 0)


struct _kroki_stats_module *_kroki_stats_module_head = NULL;

static long page_mask;
static long cache_line_mask;

static uint32_t slot_size;
static uint32_t value_count;


struct file_state
{
  int fd;
  size_t file_size;
  intptr_t head_free_offset;
};

static struct file_state *state = NULL;

static pthread_key_t thread_slot_key;


static
void
extend_file(size_t offset, size_t size)
{
  /*
    Extend the file to at least 'offset + size' bytes rounded upward
    to the next page boundary (in order to avoid invalidation of
    existing mappings).
  */
  size_t total = (offset + size + page_mask) & ~page_mask;
  POSIX(posix_fallocate(state->fd, offset, total - offset));
}


static
struct thread_slot *
thread_slot_map(size_t offset)
{
  size_t map_size = (offset & page_mask) + slot_size;
  char *map = CHECK(mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, state->fd, (offset & ~page_mask)),
                    == MAP_FAILED, die, "%m");
  SYS(madvise(map, map_size, MADV_DONTFORK));
  return (struct thread_slot *) (map + (offset & page_mask));
}


static
void
thread_slot_unmap(struct thread_slot *slot)
{
  char *map_end = (char *) slot + slot_size;
  char *map = (char *) ((intptr_t) slot & ~page_mask);
  SYS(munmap(map, map_end - map));
}


static __attribute__((__noinline__))
void
init_file(void)
{
  size_t names_size = 0;
  size_t count = 0;
  const struct _kroki_stats_module *module = _kroki_stats_module_head;
  while (module)
    {
      names_size += module->names_size;
      count += module->value_count;
      module = module->next;
    }
  size_t header_size = (sizeof(struct stats_file) + sizeof(uint32_t) * count
                        + names_size + cache_line_mask) & ~cache_line_mask;

  // Several threads may store the same values here.
  slot_size = ((sizeof(struct thread_slot) + sizeof(intptr_t) * count
                + cache_line_mask) & ~cache_line_mask);
  value_count = count;

  size_t zero = 0;
  if (unlikely(! __atomic_compare_exchange_n(&state->file_size,
                                             &zero, header_size, 0,
                                             __ATOMIC_RELEASE,
                                             __ATOMIC_ACQUIRE)))
    return;

  // Only a single thread will reach here.

  extend_file(0, header_size);

  struct stats_file *file =
    CHECK(mmap(NULL, header_size, PROT_READ | PROT_WRITE,
               MAP_SHARED, state->fd, 0),
          == MAP_FAILED, die, "%m");

  uint32_t *name_ref = file->data;
  char *name = (char *) (name_ref + value_count);
  size_t offset = name - (char *) file->data;
  module = _kroki_stats_module_head;
  while (module)
    {
      memcpy(name, module->name_refs[0], module->names_size);
      for (uint32_t i = 0; i < module->value_count; ++i)
        *name_ref++ = offset + (module->name_refs[i] - module->name_refs[0]);
      name += module->names_size;
      offset += module->names_size;
      module = module->next;
    }

  file->slot_offset = header_size - offsetof(struct stats_file, data);
  file->slot_size = slot_size;
  // Synchronize with ACQUIRE in kroki-stats.c.
  __atomic_store_n(&file->value_count, value_count, __ATOMIC_RELEASE);

  SYS(munmap(file, header_size));
}


static __thread __attribute__((__tls_model__("initial-exec")))
intptr_t slot_offset = 0;


static
void
thread_slot_destroy(void *arg)
{
  struct thread_slot *slot = arg;

  if (slot_offset != -1)
    {
      intptr_t offset = slot_offset;
      intptr_t free_head = __atomic_load_n(&state->head_free_offset,
                                           __ATOMIC_RELAXED);
      do
        slot->next_free_offset = free_head;
      while (unlikely(! __atomic_compare_exchange_n(&state->head_free_offset,
                                                    &free_head, offset, 1,
                                                    __ATOMIC_RELEASE,
                                                    __ATOMIC_RELAXED)));

      thread_slot_unmap(slot);
    }
  else
    {
      SYS(munmap(slot, sizeof(*slot)));
    }
}


void
_kroki_stats_thread_slot_create(void)
{
  int save_cancelstate;
  POSIX(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &save_cancelstate));

  struct thread_slot *slot = NULL;
  if (state)
    {
      if (unlikely(! __atomic_load_n(&state->file_size, __ATOMIC_ACQUIRE)))
        init_file();

      intptr_t offset = __atomic_load_n(&state->head_free_offset,
                                        __ATOMIC_ACQUIRE);
      if (offset)
        {
          intptr_t free_next;
          do
            {
              if (slot)
                thread_slot_unmap(slot);

              slot = thread_slot_map(offset);

              free_next = slot->next_free_offset;
            }
          while (unlikely(
                   ! __atomic_compare_exchange_n(&state->head_free_offset,
                                                 &offset, free_next, 1,
                                                 __ATOMIC_RELEASE,
                                                 __ATOMIC_ACQUIRE))
                 && offset);

          if (offset)
            memset(slot->values, 0, sizeof(intptr_t) * value_count);
          else
            thread_slot_unmap(slot);
        }
      if (! offset)
        {
          offset = __atomic_fetch_add(&state->file_size, slot_size,
                                      __ATOMIC_RELAXED);

          extend_file(offset, slot_size);

          slot = thread_slot_map(offset);
        }

      // Synchronize with ACQUIRE in kroki-stats.c.
      __atomic_store_n(&slot->tid_neg, -gettid(), __ATOMIC_RELEASE);
      slot_offset = offset;
    }
  else
    {
      slot = CHECK(mmap(NULL, sizeof(*slot), PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0),
                   == MAP_FAILED, die, "%m");
      SYS(madvise(slot, sizeof(*slot), MADV_DONTFORK));
      slot_offset = -1;
    }

  uint32_t count = 0;
  const struct _kroki_stats_module *module = _kroki_stats_module_head;
  while (module)
    {
      *module->thread_offset() =
        (char *) &slot->values[count] - (char *) module->name_refs;
      count += module->value_count;
      module = module->next;
    }

  /*
    Despite the use of __thread we still need pthread_setspecific() to
    arrange the call to thread_slot_destroy() on thread termination.
  */
  POSIX(pthread_setspecific(thread_slot_key, slot));

  // Glibc allows NULL for 'oldstate'.
  POSIX(pthread_setcancelstate(save_cancelstate, NULL));
}


void
kroki_stats_atfork_child(void)
{
  if (slot_offset)
    {
      /*
        Because of MADV_DONTFORK no slot mapping is cloned into the
        child(*).  However forked thread in the child still has
        pointers to the place where mapping existed in the parent, so
        we reset them here.

        (*): there is a small race window between the calls to mmap()
        and madvise() hence in multi-threaded case some slot mappings
        of threads other than the one that called fork() may leak.
        However in multi-threaded program only async-signal-safe
        (i.e. re-entrant) calls are allowed in the child after the
        fork() before it calls exec*(), because fork() asynchronously
        interrupts other threads (thus multi-threading is incompatible
        with multitasking that uses fork() without exec*()).  Any
        leaks before exec*() are not an issue.  OTOH in
        single-threaded program where fork() may be used without
        following exec*() we are guaranteed to unmap parent slot in
        the child so below we only reset references to it.
      */
      const struct _kroki_stats_module *module = _kroki_stats_module_head;
      while (module)
        {
          *module->thread_offset() = 0;
          module = module->next;
        }

      /*
        Also prevent the call to thread_slot_destroy() for parent slot
        (that does not exists in child) when child exits.  Not
        async-signal-safe per POSIX, but OK with Glibc when value is
        NULL.
      */
      POSIX(pthread_setspecific(thread_slot_key, NULL));

      slot_offset = 0;
    }
}


int
kroki_stats_open(const char *filename)
{
  if (slot_offset)
    {
      // Avoid using pthread_getspecific().
      const struct _kroki_stats_module *module = _kroki_stats_module_head;
      struct thread_slot *slot = (struct thread_slot *)
        ((char *) module->name_refs + *module->thread_offset()
         - offsetof(struct thread_slot, values));
      thread_slot_destroy(slot);

      /*
        kroki_stats_atfork_child() resets 'slot_offset' and so must be
        called after thread_slot_destroy().
      */
      kroki_stats_atfork_child();
    }

  if (state)
    {
      SYS(close(state->fd));
      SYS(munmap(state, sizeof(*state)));
      state = NULL;
    }

  if (! filename)
    return 0;

  int old_fd = open(filename, O_RDONLY | O_CREAT | O_CLOEXEC,
                    S_IRUSR | S_IWUSR);
  if (old_fd == -1)
    return -1;

  /*
    Stats file is protected by flock(LOCK_EX) from concurrent
    overwrite by another process that uses kroki/stats.  Such lock is
    shared among related (via fork()) processes, while preventing
    unrelated processes from acquiring it.  It is also an advisory
    lock so it doesn't interfere with the reads by 'kroki-stats'
    utility.
  */
  int res = flock(old_fd, LOCK_EX | LOCK_NB);
  if (res == -1)
    goto old_fd_err;

  /*
    At this point we know that no other process is using stats file
    'filename' for stats output.  Truncating it may interfere with
    'kroki-stats' processes reading the file(*), so we atomically
    replace the old file with a new one, first acquiring the lock so
    that the new file will appear locked from the start.

    (*): even when the file is empty it is better to create a fresh
    one to avoid a possible security hole of foreign ownership or
    relaxed permissions of the existing file.
  */

  size_t filename_len = strlen(filename);
  char *tempname = malloc(filename_len + 1 + 6 + 1);
  if (! tempname)
    goto old_fd_err;

  memcpy(tempname, filename, filename_len);
  memcpy(tempname + filename_len, ".XXXXXX", 1 + 6 + 1);

  int new_fd = mkostemp(tempname, O_CLOEXEC);
  if (new_fd == -1)
    goto tempname_err;

  res = flock(new_fd, LOCK_EX | LOCK_NB);
  if (res == -1)
    goto new_fd_err;

  state = mmap(NULL, sizeof(*state), PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (state == MAP_FAILED)
    goto state_err;

  /*
    rename() is the last operation that may fail so in the case of any
    error the old file is not affected.
  */
  res = rename(tempname, filename);
  if (res == -1)
    goto mmap_err;

  SYS(close(old_fd));
  free(tempname);

  state->fd = new_fd;

  return 0;

 mmap_err:
  SYS(munmap(state, sizeof(*state)));

 state_err:
  state = NULL;

 new_fd_err:
  SYS(unlink(tempname));
  SYS(close(new_fd));

 tempname_err:
  {
    int save_errno = errno;
    free(tempname);
    errno = save_errno;
  }

 old_fd_err:
  SYS(close(old_fd));

  return -1;
}


static __attribute__((__constructor__))
void
init(void)
{
  page_mask = SYS(sysconf(_SC_PAGESIZE)) - 1;
  cache_line_mask = SYS(sysconf(_SC_LEVEL1_DCACHE_LINESIZE)) - 1;

  POSIX(pthread_key_create(&thread_slot_key, thread_slot_destroy));

  const char *filename = getenv("KROKI_STATS_FILE");
  if (filename)
    {
      /*
        Unset KROKI_STATS_FILE here in order to not affect possible
        future exec*() and also because we can't call
        non-async-signal-safe functions in kroki_stats_atfork_child().
      */
      SYS(unsetenv("KROKI_STATS_FILE"));

      /*
        At this point it's possible that not every kroki/stats-blessed
        module has linked itself into the module list, so we can't
        call init_file() yet.
      */
      int res = kroki_stats_open(filename);
      if (res == -1)
        error("libkroki-stats: environment KROKI_STATS_FILE=%s: %m", filename);
    }
}
