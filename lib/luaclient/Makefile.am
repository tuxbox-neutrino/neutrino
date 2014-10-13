AM_CPPFLAGS = \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/lib/luaclient \
	-I$(top_srcdir)/lib/connection

AM_CPPFLAGS += -fno-rtti -fno-exceptions

bin_PROGRAMS = luaclient

luaclient_SOURCES = luaclient.cpp
luaclient_LDADD = \
	$(top_builddir)/lib/connection/libtuxbox-connection.a
