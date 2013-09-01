AM_CXXFLAGS = -D__STDC_FORMAT_MACROS

SUBDIRS = lib

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/system/mtdutils/include

if BOXMODEL_APOLLO
INCLUDES += -I$(top_srcdir)/lib/libcoolstream2
else
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
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
