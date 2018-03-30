include $(top_srcdir)/build/vars.build.mk
plugin = geanyvim

geanyplugins_LTLIBRARIES = geanyvim.la

geanyvim_la_SOURCES = \
	backend-geany.c \
	vi.h \
	vi.c \
	cmds.h \
	cmds.c \
	utils.h \
	utils.c
    
geanyvim_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"GeanyVim\"
geanyvim_la_CFLAGS = $(AM_CFLAGS)
geanyvim_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
