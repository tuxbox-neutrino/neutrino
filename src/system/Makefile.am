if BOXMODEL_APOLLO
SUBDIRS = mtdutils
endif

AM_CXXFLAGS = -fno-rtti -fno-exceptions

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libmd5sum \
	-I$(top_srcdir)/lib/xmltree \
	@FREETYPE_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_system.a

libneutrino_system_a_SOURCES = \
	configure_network.cpp \
	debug.cpp \
	flashtool.cpp \
	fsmounter.cpp \
	httptool.cpp \
	lastchannel.cpp \
	localize.cpp \
	helpers.cpp \
	ping.c \
	settings.cpp \
	ytparser.cpp \
	setting_helpers.cpp
#	safe_system.c
