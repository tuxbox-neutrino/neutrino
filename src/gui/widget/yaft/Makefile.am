AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@LUA_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_gui_terminal.a

HDR = glyph.h yaft.h conf.h color.h parse.h terminal.h util.h \
	ctrlseq/esc.h ctrlseq/csi.h ctrlseq/osc.h ctrlseq/dcs.h \
	fb/common.h fb/linux.h fb/freebsd.h fb/netbsd.h fb/openbsd.h \
	x/x.h

libneutrino_gui_terminal_a_SOURCES = \
	yaft_class.cpp

#	glyph.h \
#	yaft.h \
#	conf.h \
#	color.h \
#	parse.h \
#	terminal.h \
#	util.h \
#	ctrlseq/esc.h \
#	ctrlseq/csi.h \
#	ctrlseq/osc.h \
#	ctrlseq/dcs.h \
#	fb/common.h \
#	fb/linux.h \
#	fb/freebsd.h \
#	fb/netbsd.h \
#	fb/openbsd.h \
#	x/x.h
