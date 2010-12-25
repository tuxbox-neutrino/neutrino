INCLUDES = \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src

if BOXTYPE_COOL
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
if BOXTYPE_TRIPLE
INCLUDES += -I$(top_srcdir)/lib/libtriple
endif

AM_CPPFLAGS = -fno-rtti -fno-exceptions

noinst_LIBRARIES = libdvbsub.a

libdvbsub_a_SOURCES = dvbsub.cpp dvbsubtitle.cpp \
	tools.cpp PacketQueue.cpp helpers.cpp Debug.cpp
