include $(top_srcdir)/build/Makefile.am.common

plugindir = $(pkglibdir)/mpris
plugin_LTLIBRARIES = libmpris.la

includedir = $(prefix)/include/@PACKAGE@-@EINA_API_VERSION@/eina/mpris
include_HEADERS =

libmpris_la_CFLAGS  = @EINA_CFLAGS@
libmpris_la_LDFLAGS = @EINA_LIBS@ -module -avoid-version
libmpris_la_SOURCES = \
	eina-mpris-plugin.c      \
	eina-mpris-plugin.h      \
	eina-mpris-player-spec.h \
	eina-mpris-player.c      \
	eina-mpris-player.h

if BUILD_INDICATE
libmpris_la_CFLAGS  += @INDICATE_CFLAGS@
libmpris_la_LDFLAGS += @INDICATE_LIBS@
endif

plugin_DATA = mpris.plugin

EXTRA_DIST += $(plugin_DATA)

-include $(top_srcdir)/git.mk
