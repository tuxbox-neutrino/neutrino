AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/xmltree \
	@FREETYPE_CFLAGS@

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

noinst_LIBRARIES = libneutrino_gui_components.a


libneutrino_gui_components_a_SOURCES = \
	components.cpp
