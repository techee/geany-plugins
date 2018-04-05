include $(top_srcdir)/build/vars.build.mk
plugin = vimode

geanyplugins_LTLIBRARIES = vimode.la

noinst_PROGRAMS = viw

vi_srcs = \
	vi.h \
	vi.c \
	cmd-context.h \
	cmds.h \
	cmds.c \
	cmds-ex.h \
	cmds-ex.c \
	prompt-ex.h \
	prompt-ex.c \
	utils.h \
	utils.c

vimode_la_SOURCES = \
	backend-geany.c \
	$(vi_srcs)

viw_SOURCES = \
	backend-viw.c \
	$(vi_srcs)
    
vimode_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Vimode\"
vimode_la_CFLAGS = $(AM_CFLAGS)
vimode_la_LIBADD = $(COMMONLIBS)

viw_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"Vimode\"
viw_CFLAGS = $(AM_CFLAGS)
viw_LDADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
