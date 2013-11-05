AM_CXXFLAGS = -D__STDC_FORMAT_MACROS

SUBDIRS = lib

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libmd5sum \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/src/system/mtdutils/include \
	@FREETYPE_CFLAGS@

if BOXMODEL_APOLLO
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream2
else
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream
endif

noinst_LIBRARIES = libneutrino_system_mtdutils.a

libneutrino_system_mtdutils_a_SOURCES = \
	compr.cpp \
	compr_lzo.cpp \
	compr_rtime.cpp \
	compr_zlib.cpp \
	mkfs.jffs2.cpp \
	rbtree.cpp \
	sumtool.cpp
