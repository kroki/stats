## Process this file with automake to produce Makefile.in.
##

include $(top_srcdir)/Makefile.common


TESTS =						\
	stats.sh


EXTRA_DIST =					\
	stats.sh


check_PROGRAMS =				\
	stats


stats_CFLAGS =					\
	-fopenmp


stats_LDFLAGS =					\
	../src/libkroki-stats.la
