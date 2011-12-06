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
	encoding.cpp \
	fontrenderer.cpp \
	framebuffer.cpp \
	fb_window.cpp \
	rcinput.cpp \
	record.cpp \
	audioplay.cpp \
	screen_max.cpp \
	slotbuffer.c \
	file.cpp \
	audiometadata.cpp \
	audiofile.cpp \
	shutdown_count.cpp \
	genpsi.c \
	streamts.cpp \
	rfmod.cpp \
	radiotools.cpp \
	radiotext.cpp \
	fade.cpp \
	ringbuffer.c

if BOXTYPE_COOL
libneutrino_driver_a_SOURCES += \
	vfd.cpp

INCLUDES += \
	-I$(top_srcdir)/lib/libcoolstream
endif
if BOXTYPE_TRIPLE
libneutrino_driver_a_SOURCES += \
	newclock.cpp \
	lcdfontrenderer.cpp \
	lcdd.cpp

INCLUDES += \
	-I$(top_srcdir)/lib/libtriple
endif

libneutrino_driver_netfile_a_SOURCES = netfile.cpp

