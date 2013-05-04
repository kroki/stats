## Process this file with automake to produce Makefile.in.
##

include $(top_srcdir)/Makefile.common


nobase_include_HEADERS =			\
	kroki/stats.h				\
	kroki/bits/stats-module.h


lib_LTLIBRARIES =				\
	libkroki-stats.la


libkroki_stats_la_SOURCES =			\
	libkroki-stats.c			\
	pthread_weak.c				\
	pthread_weak.h				\
	syscall.c				\
	syscall.h


libkroki_stats_la_LDFLAGS =			\
	-Wl,-znodlopen


## See 'info libtool versioning updating' for how to update version number.
libkroki_stats_la_LDFLAGS +=			\
	-version-info 0:0:0


bin_PROGRAMS =					\
	kroki-stats


noinst_HEADERS =				\
	stats_file.h
