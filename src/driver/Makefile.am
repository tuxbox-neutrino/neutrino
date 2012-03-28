AM_CXXFLAGS = -fno-rtti -fno-exceptions

SUBDIRS = pictureviewer audiodec

INCLUDES = \
	-I$(top_srcdir)/daemons \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	@FREETYPE_CFLAGS@ \
	-I$(top_srcdir)/lib

noinst_LIBRARIES = libneutrino_driver.a libneutrino_driver_netfile.a

libneutrino_driver_a_SOURCES = \
	abstime.c \
	fontrenderer.cpp \
	fb_window.cpp \
	rcinput.cpp \
	record.cpp \
	audioplay.cpp \
	screen_max.cpp \
	file.cpp \
	audiometadata.cpp \
	audiofile.cpp \
	shutdown_count.cpp \
	genpsi.c \
	streamts.cpp \
	radiotools.cpp \
	radiotext.cpp \
	fader.cpp \
	ringbuffer.c \
	volume.cpp

if BOXTYPE_SPARK
libneutrino_driver_a_SOURCES += \
	framebuffer_spark.cpp
else
libneutrino_driver_a_SOURCES += \
	framebuffer.cpp
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
	spark_led.cpp
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

