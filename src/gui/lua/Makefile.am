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
	-I$(top_srcdir)/lib/jsoncpp/include \
	@SIGC_CFLAGS@ \
	@CURL_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@LUA_CFLAGS@

if BOXTYPE_COOL
if BOXMODEL_APOLLO
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream2
else
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream
endif
endif

noinst_LIBRARIES = libneutrino_gui_lua.a

libneutrino_gui_lua_a_SOURCES = \
	luainstance_helpers.cpp \
	luainstance.cpp \
	lua_video.cpp
