AM_CXXFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/driver/audiodec \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libconfigfile \
	@FREETYPE_CFLAGS@ \
	@VORBISIDEC_CFLAGS@

if BOXTYPE_COOL
if BOXMODEL_APOLLO
INCLUDES += -I$(top_srcdir)/lib/libcoolstream2
else
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
endif
if USE_STB_HAL
INCLUDES += -I$(STB_HAL_INC)
endif

noinst_LIBRARIES = libneutrino_driver_audiodec.a

if ENABLE_FLAC
FLACdec= flacdec.cpp
endif

libneutrino_driver_audiodec_a_SOURCES = \
	basedec.cpp \
	cdrdec.cpp \
	crc.c \
	$(FLACdec) \
	mp3dec.cpp \
	oggdec.cpp \
	tag.c \
	wavdec.cpp
#	vis.cpp
