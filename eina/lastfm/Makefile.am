include $(top_srcdir)/build/Makefile.am.common

plugindir = $(pkglibdir)/lastfm
plugin_LTLIBRARIES = liblastfm.la
liblastfm_la_CFLAGS  = @EINA_CFLAGS@
liblastfm_la_LDFLAGS = @EINA_LIBS@ -module -avoid-version
liblastfm_la_SOURCES = \
	lastfm-priv.h \
	eina-lastfm-plugin.c \
	resources.c

plugin_DATA = \
	lastfm.plugin

eina_gresources_xml  = resources.xml
eina_gresources_name = eina_lastfm
include $(top_srcdir)/build/Makefile.am.gresources

EXTRA_DIST += $(plugin_DATA) preferences.ui lastfm.png

lastfmsubmitddir = $(plugindir)/lastfmsubmitd
lastfmsubmitd_DATA = \
	lastfmsubmitd/lastfmsubmitd \
	lastfmsubmitd/COPYRIGHT \
	lastfmsubmitd/info.txt
lastfmsubmitdlibdir = $(lastfmsubmitddir)/lastfm
lastfmsubmitdlib_DATA = \
	lastfmsubmitd/lastfm/__init__.py   \
	lastfmsubmitd/lastfm/marshaller.py \
	lastfmsubmitd/lastfm/config.py     \
	lastfmsubmitd/lastfm/client.py

CLEANFILES += \
	lastfmsubmitd/lastfm/__init__.pyc   \
	lastfmsubmitd/lastfm/marshaller.pyc \
	lastfmsubmitd/lastfm/config.pyc     \
	lastfmsubmitd/lastfm/client.pyc

EXTRA_DIST += \
	$(lastfmsubmitd_DATA) \
	$(lastfmsubmitdlib_DATA)


install-data-hook:
	chmod +x $(DESTDIR)/$(plugindir)/lastfmsubmitd/lastfmsubmitd

-include $(top_srcdir)/git.mk

