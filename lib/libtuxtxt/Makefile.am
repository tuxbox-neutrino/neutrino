INCLUDES = \
        -I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src \
	$(FREETYPE_CFLAGS)

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

AM_CPPFLAGS = -fno-rtti -fno-exceptions

noinst_LIBRARIES = libtuxtxt.a

libtuxtxt_a_SOURCES = libtuxtxt.cpp tuxtxt.cpp

configdir = $(CONFIGDIR)/tuxtxt
## not necessary - the defaults in the code are exactly like
## what is in the config file
#config_DATA = tuxtxt2.conf
## ... but we need the $(configdir)... hence the empty config_DATA...
config_DATA =
