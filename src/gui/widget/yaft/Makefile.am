AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(srcdir) \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@LUA_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_gui_terminal.a

libneutrino_gui_terminal_a_SOURCES = \
	ctrlseq/esc.cpp \
	ctrlseq/csi.cpp \
	ctrlseq/osc.cpp \
	yaft_priv.cpp \
	yaft_class.cpp
