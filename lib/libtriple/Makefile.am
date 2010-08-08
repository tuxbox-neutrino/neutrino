INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	@DIRECTFB_CFLAGS@

noinst_LIBRARIES = libtriple.a

AM_CPPFLAGS = -fno-rtti -fno-exceptions

libtriple_a_SOURCES = \
	lt_debug.cpp \
	dmx_td.cpp \
	video_td.cpp \
	audio_td.cpp \
	init_td.cpp \
	playback_td.cpp \
	pwrmngr.cpp \
	record_td.cpp

#libtriple_a_LIBADD = $(top_builddir)/src/driver/libneutrino_driver.a
