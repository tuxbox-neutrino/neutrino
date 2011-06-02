AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/daemons \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libcoolstream \
	-I$(top_srcdir)/lib/xmltree \
	@FREETYPE_CFLAGS@ \
	-I$(top_srcdir)/lib


noinst_LIBRARIES = libneutrino_gui_bedit.a

libneutrino_gui_bedit_a_SOURCES = \
	bouqueteditor_bouquets.cpp \
	bouqueteditor_channels.cpp \
	bouqueteditor_chanselect.cpp

