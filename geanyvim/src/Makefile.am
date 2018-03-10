include $(top_srcdir)/build/vars.build.mk
plugin = geanyvim

geanyplugins_LTLIBRARIES = geanyvim.la

geanyvim_la_SOURCES = \
	state.h \
	ui.h \
	main.c \
	cmds.h \
	cmds.c \
	ui_cmds.h \
	ui_cmds.c \
	switch.h \
	switch.c
    
geanyvim_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"GeanyVim\"
geanyvim_la_CFLAGS = $(AM_CFLAGS)
geanyvim_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
