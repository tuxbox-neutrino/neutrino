AM_CXXFLAGS = -fno-rtti -fno-exceptions

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/driver/audiodec \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@

if BOXTYPE_COOL
if BOXMODEL_APOLLO
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream2
else
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream
endif
endif
if BOXTYPE_TRIPLE
AM_CPPFLAGS += -I$(top_srcdir)/lib/libtriple
endif

noinst_LIBRARIES = libneutrino_driver_audiodec.a

if ENABLE_FFMPEGDEC
ASOURCES = ffmpegdec.cpp
else
ASOURCES = cdrdec.cpp \
	crc.c \
	flacdec.cpp \
	mp3dec.cpp \
	oggdec.cpp \
	tag.c \
	vis.cpp \
	wavdec.cpp
endif

libneutrino_driver_audiodec_a_SOURCES = \
	basedec.cpp \
	$(ASOURCES)
