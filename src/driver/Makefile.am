AM_CXXFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

SUBDIRS = pictureviewer audiodec

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	@FREETYPE_CFLAGS@

noinst_LIBRARIES = libneutrino_driver.a libneutrino_driver_netfile.a

libneutrino_driver_a_SOURCES = \
	abstime.c \
	audiofile.cpp \
	audiometadata.cpp \
	audioplay.cpp \
	fade.cpp \
	fb_window.cpp \
	fbaccel.cpp \
	file.cpp \
	fontrenderer.cpp \
	framebuffer_ng.cpp \
	genpsi.cpp \
	radiotext.cpp \
	radiotools.cpp \
	rcinput.cpp \
	record.cpp \
	ringbuffer.c \
	screen_max.cpp \
	shutdown_count.cpp \
	streamts.cpp \
	volume.cpp
#	screenshot.cpp

if BOXTYPE_GENERIC
libneutrino_driver_a_SOURCES += \
	glthread.cpp
endif
if BOXTYPE_COOL
libneutrino_driver_a_SOURCES += \
	vfd.cpp

INCLUDES += \
	-I$(top_srcdir)/lib/libcoolstream
endif
if BOXTYPE_TRIPLE
libneutrino_driver_a_SOURCES += \
	newclock.cpp \
	lcdd.cpp
endif

if BOXTYPE_SPARK
libneutrino_driver_a_SOURCES += \
	simple_display.cpp
endif
if BOXTYPE_AZBOX
libneutrino_driver_a_SOURCES += \
	simple_display.cpp
endif
if BOXTYPE_GENERIC
libneutrino_driver_a_SOURCES += \
	simple_display.cpp
endif
if USE_STB_HAL
INCLUDES += \
	-I$(STB_HAL_INC) \
	@DIRECTFB_CFLAGS@
else
if BOXTYPE_TRIPLE
INCLUDES += \
	-I$(top_srcdir)/lib/libtriple \
	@DIRECTFB_CFLAGS@
endif
endif

libneutrino_driver_netfile_a_SOURCES = netfile.cpp

