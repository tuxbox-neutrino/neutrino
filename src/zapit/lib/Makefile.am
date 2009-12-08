AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile

noinst_LIBRARIES = libzapitclient.a
libzapitclient_a_SOURCES = zapitclient.cpp zapittools.cpp
