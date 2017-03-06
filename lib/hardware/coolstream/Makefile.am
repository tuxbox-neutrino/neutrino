noinst_LIBRARIES = libhwcaps.a

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

if BOXMODEL_CS_HD2
AM_CPPFLAGS = -I$(top_srcdir)/lib/libcoolstream2
else
AM_CPPFLAGS = -I$(top_srcdir)/lib/libcoolstream
endif

libhwcaps_a_SOURCES = \
	hardware_caps.cpp
