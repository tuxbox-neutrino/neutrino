AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/xmltree

if BOXTYPE_COOL
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif

if USE_STB_HAL
INCLUDES += -I$(STB_HAL_INC)
else
if BOXTYPE_TRIPLE
INCLUDES += -I$(top_srcdir)/lib/libtriple
endif
endif

noinst_LIBRARIES = libzapit.a

libzapit_a_SOURCES = \
	bouquets.cpp channel.cpp dvbstring.cpp getservices.cpp \
	scan.cpp zapit.cpp cam.cpp ci.cpp frontend.cpp \
	descriptors.cpp nit.cpp pat.cpp pmt.cpp sdt.cpp
#	fastscan.cpp

bin_PROGRAMS = pzapit
sbin_PROGRAMS = udpstreampes

pzapit_SOURCES = \
	pzapit.cpp

pzapit_LDADD = \
	$(top_builddir)/src/zapit/lib/libzapitclient.a \
	$(top_builddir)/lib/connection/libtuxbox-connection.a

udpstreampes_SOURCES = \
	udpstreampes.cpp

udpstreampes_LDADD = \
	$(top_builddir)/src/zapit/lib/libzapitclient.a \
	$(top_builddir)/lib/connection/libtuxbox-connection.a \
	-lpthread
