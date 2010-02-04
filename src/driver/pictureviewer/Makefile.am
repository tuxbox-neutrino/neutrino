AM_CXXFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libcoolstream \
	@FREETYPE_CFLAGS@ \
	-I$(top_srcdir)/lib
 
noinst_LIBRARIES = libneutrino_pictureviewer.a

libneutrino_pictureviewer_a_SOURCES = \
			pictureviewer.cpp fb_display.cpp \
			png.cpp bmp.cpp resize.cpp jpeg.cpp \
			gif.cpp crw.cpp make15color_errdiff.cpp
