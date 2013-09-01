AM_CXXFLAGS = -D__STDC_FORMAT_MACROS

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/system/mtdutils/include

noinst_LIBRARIES = libneutrino_system_mtdutils_lib.a

libneutrino_system_mtdutils_lib_a_SOURCES = \
	libcrc32.cpp \
	libfec.cpp \
	libmtd.cpp \
	libmtd_legacy.cpp
