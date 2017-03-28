AM_CPPFLAGS = -fno-rtti -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libupnpclient \
	-I$(top_srcdir)/lib/jsoncpp \
	@SIGC_CFLAGS@ \
	@CURL_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@LUA_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_gui_lua.a

libneutrino_gui_lua_a_SOURCES = \
	luainstance.cpp \
	luainstance_helpers.cpp \
	lua_cc_header.cpp \
	lua_cc_picture.cpp \
	lua_cc_signalbox.cpp \
	lua_cc_text.cpp \
	lua_cc_window.cpp \
	lua_configfile.cpp \
	lua_curl.cpp \
	lua_filehelpers.cpp \
	lua_hintbox.cpp \
	lua_menue.cpp \
	lua_messagebox.cpp \
	lua_misc.cpp \
	lua_stringinput.cpp \
	lua_threads.cpp \
	lua_threads_copy.cpp \
	lua_threads_functions.cpp \
	lua_video.cpp

