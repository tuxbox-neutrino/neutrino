INCLUDES = \
        -I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src \
	$(FREETYPE_CFLAGS)

if BOXTYPE_COOL
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
if BOXTYPE_TRIPLE
INCLUDES += -I$(top_srcdir)/lib/libtriple
endif

AM_CPPFLAGS = -fno-rtti -fno-exceptions

noinst_LIBRARIES = libtuxtxt.a

libtuxtxt_a_SOURCES = libtuxtxt.cpp tuxtxt.cpp

## not necessary - the defaults in the code are exactly like
## what is in the config file
#configdir = $(CONFIGDIR)/tuxtxt
#config_DATA = tuxtxt2.conf
