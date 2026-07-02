AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/jsoncpp \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@LUA_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_gui_moviebrowser.a


libneutrino_gui_moviebrowser_a_SOURCES = \
	mb.cpp \
	mb_constants.cpp \
	mb_delete.cpp \
	mb_exec.cpp \
	mb_exec_loop.cpp \
	mb_functions.cpp \
	mb_input.cpp \
	mb_layout.cpp \
	mb_menu.cpp \
	mb_settings.cpp \
	mb_selection.cpp \
	mb_storage.cpp \
	mb_ui.cpp
