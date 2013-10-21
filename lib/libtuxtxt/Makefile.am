AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	$(FREETYPE_CFLAGS)

if BOXTYPE_COOL
if BOXMODEL_APOLLO
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream2
else
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream
endif
endif

if USE_STB_HAL
AM_CPPFLAGS += -I$(STB_HAL_INC)
endif

AM_CPPFLAGS += -fno-rtti -fno-exceptions

noinst_LIBRARIES = libtuxtxt.a

libtuxtxt_a_SOURCES = libtuxtxt.cpp tuxtxt.cpp

configdir = $(CONFIGDIR)/tuxtxt
## not necessary - the defaults in the code are exactly like
## what is in the config file
#config_DATA = tuxtxt2.conf
## ... but we need the $(configdir)... hence the empty config_DATA...
config_DATA =
