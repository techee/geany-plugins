include $(top_srcdir)/build/vars.build.mk
plugin = geanyscript

geanyplugins_LTLIBRARIES = geanyscript.la

geanyscript_la_SOURCES = \
	main.c

geanyscript_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"GeanyScript\"
geanyscript_la_CFLAGS = $(AM_CFLAGS)
geanyscript_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk

