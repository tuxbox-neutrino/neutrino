AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libcoolstream \
	-I$(top_srcdir)/lib/xmltree \
	@FREETYPE_CFLAGS@


noinst_LIBRARIES = libneutrino_gui_components.a


libneutrino_gui_components_a_SOURCES = \
	cc_base.cpp
