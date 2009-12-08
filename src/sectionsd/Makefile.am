INCLUDES = \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libcoolstream \
	-I$(top_srcdir)/lib/xmltree

AM_CPPFLAGS = -D DO_NOT_INCLUDE_STUFF_NOT_NEEDED_FOR_SECTIONSD

noinst_LIBRARIES = libsectionsd.a
libsectionsd_a_SOURCES = sectionsd.cpp dmxapi.cpp debug.cpp dmx.cpp SIsections.cpp SIevents.cpp SIutils.cpp SIlanguage.cpp edvbstring.cpp

