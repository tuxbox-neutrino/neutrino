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
if BOXMODEL_APOLLO
INCLUDES += -I$(top_srcdir)/lib/libcoolstream2
else
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
endif
if USE_STB_HAL
INCLUDES += -I$(STB_HAL_INC)
endif

noinst_LIBRARIES = libneutrino_gui_bedit.a

libneutrino_gui_bedit_a_SOURCES = \
	bouqueteditor_bouquets.cpp \
	bouqueteditor_channels.cpp \
	bouqueteditor_chanselect.cpp

