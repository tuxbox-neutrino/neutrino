AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS -D_FILE_OFFSET_BITS=64

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/nhttpd \
	-I$(top_srcdir)/src/nhttpd/yhttpd_core \
	-I$(top_srcdir)/src/nhttpd/yhttpd_mods \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/xmltree \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libnhttpd_tuxboxapi.a

libnhttpd_tuxboxapi_a_SOURCES = \
	controlapi.cpp \
	neutrinoapi.cpp \
	neutrinoyparser.cpp
	
