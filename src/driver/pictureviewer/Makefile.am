AM_CXXFLAGS = -fno-rtti -fno-exceptions

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@LUA_CFLAGS@ \
	@HWLIB_CFLAGS@


noinst_LIBRARIES = libneutrino_pictureviewer.a

libneutrino_pictureviewer_a_SOURCES = \
	crw.cpp \
	gif.cpp \
	jpeg.cpp \
	pictureviewer.cpp \
	png.cpp bmp.cpp
