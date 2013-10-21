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
	@FREETYPE_CFLAGS@

AM_CPPFLAGS += -fno-rtti -fno-exceptions

noinst_LIBRARIES = libyhttpd.a

libyhttpd_a_SOURCES = \
	ylogging.cpp helper.cpp ylanguage.cpp\
	ywebserver.cpp yconnection.cpp yrequest.cpp yresponse.cpp yhook.cpp ysocket.cpp

