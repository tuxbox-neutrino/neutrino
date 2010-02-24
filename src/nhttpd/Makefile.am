SUBDIRS = yhttpd_core yhttpd_mods tuxboxapi web

AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src/nhttpd/yhttpd_core \
	-I$(top_srcdir)/src/nhttpd/yhttpd_mods \
	-I$(top_srcdir)/src/nhttpd/tuxboxapi \
	-I$(top_srcdir)/src/nhttpd/tuxboxapi/coolstream \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	@FREETYPE_CFLAGS@

noinst_LIBRARIES = libnhttpd.a
libnhttpd_a_SOURCES = yhttpd.cpp
