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

noinst_LIBRARIES = libtuxtxt.a

libtuxtxt_a_SOURCES = libtuxtxt.cpp tuxtxt.cpp

configdir = $(CONFIGDIR)/tuxtxt
config_DATA = tuxtxt2.conf
