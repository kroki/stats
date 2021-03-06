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


  DESCRIPTION:

  Fast lock-free per thread statistic counters.

  In a long-running (server) software it is often desirable to have
  some statistic counters that reflect on system load and performance.
  Quite often such counters implemented as a key-value data structure
  (like std::map from C++ STL) and the server code itself is modified
  to respond to statistic queries.  Similar technics are sometimes
  applied to monitor and debug non-server software.  The drawbacks of
  this approach is that value lookup in a key-value data structure
  adds runtime overhead (plus often the data structure has to be
  shared among threads which adds locking or atomics overhead) and
  software logic is modified in the orthogonal way to its original
  purpose.

  Kroki/stats provides a sufficiently generic framework for statistic
  counters while avoiding common drawbacks:

    - no software modification is required other than injecting the
      counters themselves,

    - counter name to value mapping is resolved at compile time with
      no subsequent runtime overhead,

    - all counters are thread-local and operated on without any locks
      or atomics,

    - value queries are performed by an external utility and do not
      disturb software operations in any way.


  Usage:

    #include <kroki/stats.h>
    -lkroki-stats

      To use kroki/stats in the code you have to include kroki/stats.h
      header and link with the libkroki-stats.so.


    int stats_open(const char *filename) function
    KROKI_STATS_FILE environment variable

      Internally kroki/stats uses file-backed shared memory region to
      store statistic counters of all threads.  To provide the name
      for the file you should call stats_open() (you do not have to
      provide the file name but you won't be able to query counter
      values if no file name is specified).  The function returns 0 on
      success, or -1 on error setting 'errno' to system error code.
      When called with NULL argument it will close currently open file
      (if any) without opening a new one.  stats_open() always affects
      the calling thread or any thread that hasn't yet called stats()
      macro (described below).  However if some other thread has
      already called stats() such thread will continue to use the old
      file (or none at all if that was in effect by the time of the
      first call).  stats_open() itself is not thread-safe and should
      be called in a synchronized way (or simply before any other
      thread is created).  Normally you call stats_open() at most
      once, stats file has a fixed size so no rotation is required.

      Alternatively to calling stats_open() you may provide stats file
      name with KROKI_STATS_FILE environment variable.  This way the
      file will be created during program startup.  This is convenient
      when kroki/stats is used in a shared library and the main
      application is not aware of kroki/stats.  Note that using
      kroki/stats in a shared library requires that the library is
      available at program startup (via direct or indirect linking or
      by LD_PRELOAD), loading it later with dlopen() is not supported
      for performance reasons (however it's possible to LD_PRELOAD the
      library and later dlopen() it if execution flow requires so).


    stats(some.stats.name) macro

      Statistic counters are injected into the code with stats()
      macro.  It takes the name of a counter and expands to an aligned
      signed 64-bit (or 32-bit for 32-bit CPU) integer lvalue with
      initial value of zero.  Counter name is a sequence of one or
      several C language identifiers separated by dots (i.e. it is
      _not_ a string).  Identifiers used in a name do not have to be
      defined in any way and live in a separate namespace so no
      collision with ordinary program identifiers is possible.  Within
      one executable or shared library the same counter name refers to
      the same counter value.  Same names across different executables
      or shared libraries do not collide and refer to different
      statistic values.  They will be reported separately but it won't
      be impossible to tell them apart so it is advised to use
      subsystem namespaces to govern counter names, like

        ++stats(my.app.http.requests);

      There's certain initialization penalty when stats() is called
      for the first time, and a smaller one when stats() is called for
      the first time in a given thread.  To avoid them on a critical
      path you can perform the first no-op call to stats() like

         stats(some.stat.name) = 0;

      in some well-defined place, for instance at the beginning of a
      program (to hide larger penalty) and at the beginning of thread
      function (to hide smaller penalty).  Subsequent calls to stats()
      are very efficient (only few assembler instructions without any
      locks or atomics).

      stats() macro is thread-safe and also async-cancellation-safe.


    /usr/bin/kroki-stats command-line utility

      'kroki-stats' utility takes the stats file name as an argument
      and dumps current counter values in a human-readable form.  For
      instance:

        $ kroki-stats /dev/shm/myapp.stats
        [24629] my.app.iterations: 3
        [24629] my.app.updates: 1
        [24629] my.app.nsec: 862349346
        [24608] my.app.iterations: 4
        [24608] my.app.updates: 1
        [24608] my.app.nsec: 993483703

      Here 24608 and 24629 are thread IDs of two threads each having
      three counters.  It may be convenient to pipe the output to the
      'sort' (ordering by counter name within each thread) or to the
      'sort -sk2' (stable ordering by counter name across all
      threads).  But normally the output is further processed and/or
      send to monitoring/graphing software.

      'kroki-stats' reads the values asynchronously with respect to
      the application that updates the counters.  While each
      individual value is read atomically, no two values a
      synchronized in any way.  For instance, if application code has

        ++stats(my.stat1);
        ++stats(my.stat2);

      'kroki-stats' may see _any_ increment before the other (subject
      to compiler and CPU reordering).


    void stats_atfork_child(void) function

      If child process is going to use stats() after the fork() the
      child process must call stats_atfork_child() to disassociate
      itself from the parent thread counter values.  From this moment
      child threads will share stats file with the parent process and
      each child thread will create a separate thread values on the
      first call to stats().  Child threads will appear in the
      'kroki-stats' output simply as additional threads with their own
      unique thread IDs.  'pstree' may be used to figure out process
      hierarchy.

      Alternatively, while there is still only one thread in the child
      process it may call stats_open() to create a new independent
      stats file.


  Defining KROKI_STATS_NOPOLLUTE will result in omitting alias
  definitions, but functionality will still be available with the
  namespace prefix 'kroki_'.


  Limitations:

  Implementation requires Linux kernel, GCC 4.7.3+, GNU ld, Glibc.

  kroki/stats cannot be used in object module intended to be loaded
  with dlopen() (unless it is preloaded with LD_PRELOAD beforehand,
  see the description of stats_open() above).

  IA-64 architecture is not supported (this is _not_ x86-64, which is
  supported).
*/

#ifndef KROKI_STATS_NOPOLLUTE

#define stats_open(filename)  kroki_stats_open(filename)
#define stats(name)  kroki_stats(name)
#define stats_atfork_child()  kroki_stats_atfork_child()

#endif  /* ! KROKI_STATS_NOPOLLUTE */


#ifndef KROKI_STATS_H
#define KROKI_STATS_H 1

#include "bits/stats-module.h"


#define kroki_stats(name)  _kroki_stats_eval(#name, __COUNTER__)
#define _kroki_stats_eval(name, unique)  _kroki_stats_impl(name, unique)
#define _kroki_stats_impl(name, unique)                                 \
  (*({                                                                  \
    extern __attribute__((__visibility__("hidden")))                    \
      const char *const n##unique __asm__("._kroki_stats_value_" name); \
    intptr_t *pvalue;                                                   \
                                                                        \
    __asm__(                                                            \
      ".ifndef ._kroki_stats_value_" name "\n"                          \
                                                                        \
      "  .pushsection _kroki_stats_names\n"                             \
      "   0:\n"                                                         \
      "    .string \"" name "\"\n"                                      \
      "  .popsection\n"                                                 \
                                                                        \
      "  .pushsection _kroki_stats_name_refs\n"                         \
      "   ._kroki_stats_value_" name ":\n"                              \
      "    " _KROKI_STATS_ASM_PTR " 0b\n"                               \
      "  .popsection\n"                                                 \
                                                                        \
      ".endif\n"                                                        \
    );                                                                  \
                                                                        \
    if (__builtin_expect(! _kroki_stats_module_thread_offset, 0))       \
      _kroki_stats_thread_slot_create();                                \
    /*                                                                  \
      Tell GCC that _kroki_stats_module_thread_offset is defined now.   \
    */                                                                  \
    if (! _kroki_stats_module_thread_offset)                            \
      __builtin_unreachable();                                          \
                                                                        \
    pvalue = (intptr_t *)                                               \
      __builtin_assume_aligned((char *) &n##unique                      \
                               + _kroki_stats_module_thread_offset,     \
                               __SIZEOF_POINTER__);                     \
    pvalue;                                                             \
  }))


#if (__SIZEOF_POINTER__ == 8)
#define _KROKI_STATS_ASM_PTR  ".quad"
#elif (__SIZEOF_POINTER__ == 4)
#define _KROKI_STATS_ASM_PTR  ".int"
#else
#error kroki/stats supports only 32-bit or 64-bit pointers
#define _KROKI_STATS_ASM_PTR
#endif


__asm__(
  ".section _kroki_stats_names, \"aS\", @progbits; .previous\n"
  ".section _kroki_stats_name_refs, \"a\", @progbits; .previous\n"
);


static __thread __attribute__((__section__(".gnu.linkonce.tb._kroki_stats"),
                               __tls_model__("initial-exec")))
intptr_t _kroki_stats_module_thread_offset = 0;


__attribute__((__section__(".gnu.linkonce"),
               __visibility__("hidden")))
intptr_t *
_kroki_stats_get_module_thread_offset(void)
{
  return &_kroki_stats_module_thread_offset;
}


static __attribute__((__section__(".gnu.linkonce.b._kroki_stats")))
struct _kroki_stats_module _kroki_stats_module;


__attribute__((__section__(".gnu.linkonce"),
               __visibility__("hidden"),
               __constructor__))
void
_kroki_stats_init(void)
{
  extern __attribute__((__visibility__("hidden")))
    const char __start__kroki_stats_names, __stop__kroki_stats_names;

  extern __attribute__((__visibility__("hidden")))
    const char *const __start__kroki_stats_name_refs,
               *const __stop__kroki_stats_name_refs;

  static __attribute__((__section__(".gnu.linkonce.b._kroki_stats")))
    int called = 0;
  if (called++)
    return;

  _kroki_stats_module.thread_offset = _kroki_stats_get_module_thread_offset;
  _kroki_stats_module.name_refs = &__start__kroki_stats_name_refs;
  _kroki_stats_module.names_size =
    &__stop__kroki_stats_names - &__start__kroki_stats_names;
  _kroki_stats_module.value_count =
    &__stop__kroki_stats_name_refs - &__start__kroki_stats_name_refs;

  _kroki_stats_module.next = _kroki_stats_module_head;
  _kroki_stats_module_head = &_kroki_stats_module;
}


#endif  /* ! KROKI_STATS_H */
