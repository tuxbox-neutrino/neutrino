INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/xmltree

if BOXTYPE_COOL
if BOXMODEL_APOLLO
INCLUDES += -I$(top_srcdir)/lib/libcoolstream2
else
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
endif
if USE_STB_HAL
INCLUDES += -I$(STB_HAL_INC)
endif

#AM_CPPFLAGS =
AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

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
