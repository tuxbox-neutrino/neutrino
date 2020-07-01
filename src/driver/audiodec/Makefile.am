AM_CXXFLAGS = -fno-rtti -fno-exceptions -D__STDC_LIMIT_MACROS

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/driver/audiodec \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@LUA_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_driver_audiodec.a

if ENABLE_FFMPEGDEC
ASOURCES = ffmpegdec.cpp
else
ASOURCES = cdrdec.cpp \
	ffmpegdec.cpp \
	crc.c \
	flacdec.cpp \
	mp3dec.cpp \
	oggdec.cpp \
	tag.c \
	wavdec.cpp
endif

libneutrino_driver_audiodec_a_SOURCES = \
	basedec.cpp \
	$(ASOURCES)
