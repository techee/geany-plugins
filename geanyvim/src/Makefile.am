include $(top_srcdir)/build/vars.build.mk
plugin = geanyvim

geanyplugins_LTLIBRARIES = geanyvim.la

noinst_PROGRAMS = viw

vi_srcs = \
	vi.h \
	vi.c \
	cmds.h \
	cmds.c \
	ex-cmds.h \
	ex-cmds.c \
	ex-prompt.h \
	ex-prompt.c \
	utils.h \
	utils.c

geanyvim_la_SOURCES = \
	backend-geany.c \
	$(vi_srcs)

viw_SOURCES = \
	backend-viw.c \
	$(vi_srcs)
    
geanyvim_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"GeanyVim\"
geanyvim_la_CFLAGS = $(AM_CFLAGS)
geanyvim_la_LIBADD = $(COMMONLIBS)

viw_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"GeanyVim\"
viw_CFLAGS = $(AM_CFLAGS)
viw_LDADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
