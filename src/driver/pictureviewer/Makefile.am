AM_CXXFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
	@FREETYPE_CFLAGS@ \
	-I$(top_srcdir)/lib

if BOXTYPE_COOL
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
if USE_STB_HAL
INCLUDES += -I$(STB_HAL_INC)
else
if BOXTYPE_TRIPLE
INCLUDES += -I$(top_srcdir)/lib/libtriple
endif
endif

noinst_LIBRARIES = libneutrino_pictureviewer.a

libneutrino_pictureviewer_a_SOURCES = \
			pictureviewer.cpp png.cpp bmp.cpp \
			jpeg.cpp gif.cpp crw.cpp
