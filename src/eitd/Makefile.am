AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/xmltree \
	@HWLIB_CFLAGS@

#AM_CPPFLAGS =
AM_CPPFLAGS += -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

noinst_LIBRARIES = libsectionsd.a
libsectionsd_a_SOURCES = \
	debug.cpp \
	dmx.cpp \
	edvbstring.cpp \
	sectionsd.cpp \
	SIevents.cpp \
	SIlanguage.cpp \
	SIsections.cpp \
	SIutils.cpp \
	xmlutil.cpp
