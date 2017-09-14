AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/nhttpd \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	@FREETYPE_CFLAGS@ \
	@HWLIB_CFLAGS@

AM_CPPFLAGS += -fno-rtti -fno-exceptions -D_FILE_OFFSET_BITS=64

noinst_LIBRARIES = libyhttpd.a

libyhttpd_a_SOURCES = \
	helper.cpp \
	yconnection.cpp \
	yhook.cpp \
	ylanguage.cpp\
	ylogging.cpp \
	yrequest.cpp \
	yresponse.cpp \
	ysocket.cpp \
	ywebserver.cpp

