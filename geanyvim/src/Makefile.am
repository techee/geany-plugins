include $(top_srcdir)/build/vars.build.mk
plugin = geanyvim

geanyplugins_LTLIBRARIES = geanyvim.la


geanyvim_la_SOURCES  = vim.c
geanyvim_la_CPPFLAGS = $(AM_CPPFLAGS) \
                        -DG_LOG_DOMAIN=\"GeanyVim\"
geanyvim_la_CFLAGS   = $(AM_CFLAGS) \
                        $(COMMANDER_CFLAGS)
geanyvim_la_LIBADD   = $(COMMONLIBS) \
                        $(COMMANDER_LIBS)


include $(top_srcdir)/build/cppcheck.mk
