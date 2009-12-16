#not allowed cppflags for c files - fix your code to re-enable
#AM_CPPFLAGS = -fno-rtti -fno-exceptions

AM_CPPFLAGS =

INCLUDES = \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libcoolstream \
	-I$(top_srcdir)/lib/libmd5sum \
	-I$(top_srcdir)/lib/xmltree \
	@FREETYPE_CFLAGS@

noinst_LIBRARIES = libneutrino_system.a

libneutrino_system_a_SOURCES = \
	localize.cpp setting_helpers.cpp debug.cpp \
	ping.c flashtool.cpp httptool.cpp \
	settings.cpp lastchannel.cpp \
	configure_network.cpp fsmounter.cpp
