AM_CXXFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

SUBDIRS = pictureviewer audiodec

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_driver.a libneutrino_driver_netfile.a

libneutrino_driver_a_SOURCES = \
	abstime.c \
	audiofile.cpp \
	audiometadata.cpp \
	audioplay.cpp \
	colorgradient.cpp \
	fade.cpp \
	fb_accel.cpp \
	fb_window.cpp \
	fb_generic.cpp \
	file.cpp \
	fontrenderer.cpp \
	genpsi.cpp \
	moviecut.cpp \
	neutrinofonts.cpp \
	radiotext.cpp \
	radiotools.cpp \
	rcinput.cpp \
	record.cpp \
	scanepg.cpp \
	screen_max.cpp \
	screenshot.cpp \
	shutdown_count.cpp \
	streamts.cpp \
	volume.cpp

if BOXTYPE_COOL
libneutrino_driver_a_SOURCES += \
	fb_accel_cs_nevis.cpp \
	vfd.cpp
endif

if BOXTYPE_TRIPLE
libneutrino_driver_a_SOURCES += \
	newclock.cpp \
	lcdd.cpp
endif

#if BOXTYPE_SPARK
#libneutrino_driver_a_SOURCES += \
#	fb_accel_sti.cpp \
#	simple_display.cpp
#endif

libneutrino_driver_netfile_a_SOURCES = netfile.cpp
