AM_CXXFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/src \
        -I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
        @FREETYPE_CFLAGS@ \
        @VORBISIDEC_CFLAGS@

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

noinst_LIBRARIES = libneutrino_driver_audiodec.a

if ENABLE_FLAC
FLACdec= flacdec.cpp
endif

libneutrino_driver_audiodec_a_SOURCES = \
	basedec.cpp \
	cdrdec.cpp \
	mp3dec.cpp \
	oggdec.cpp \
	wavdec.cpp \
	$(FLACdec) \
	tag.c \
	crc.c
#	vis.cpp
