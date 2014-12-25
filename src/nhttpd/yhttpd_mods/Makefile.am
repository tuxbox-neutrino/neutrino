AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/nhttpd \
	-I$(top_srcdir)/src/nhttpd/yhttpd_core \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	@FREETYPE_CFLAGS@ 

AM_CPPFLAGS += -fno-rtti -fno-exceptions -D_FILE_OFFSET_BITS=64

noinst_LIBRARIES = libyhttpdmods.a

libyhttpdmods_a_SOURCES = \
	mod_yparser.cpp mod_testhook.cpp mod_auth.cpp mod_cache.cpp mod_sendfile.cpp mod_weblog.cpp

