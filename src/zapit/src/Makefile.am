AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/xmltree \
	@FREETYPE_CFLAGS@ \
	@SIGC_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libzapit.a

libzapit_a_SOURCES = \
	bouquets.cpp \
	capmt.cpp \
	channel.cpp \
	fastscan.cpp \
	femanager.cpp \
	frontend.cpp \
	getservices.cpp \
	pat.cpp \
	scanbat.cpp \
	scan.cpp \
	scannit.cpp \
	scanpmt.cpp \
	scansdt.cpp \
	transponder.cpp \
	zapit.cpp

bin_PROGRAMS = pzapit

pzapit_SOURCES = \
	pzapit.cpp

pzapit_LDADD = \
	$(top_builddir)/src/zapit/lib/libzapitclient.a \
	$(top_builddir)/lib/connection/libtuxbox-connection.a \
	-lOpenThreads \
	-lpthread
