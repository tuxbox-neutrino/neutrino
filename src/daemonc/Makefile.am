AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/daemons \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libcoolstream \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/xmltree \
	@FREETYPE_CFLAGS@ \
	-I$(top_srcdir)/lib 

noinst_LIBRARIES = libneutrino_daemonc.a

libneutrino_daemonc_a_SOURCES = remotecontrol.cpp
