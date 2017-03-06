noinst_LIBRARIES = libhwcaps.a

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

if BOXMODEL_CS_HD2
AM_CPPFLAGS = -I$(top_srcdir)/lib/hardware/coolstream/hd2/libcoolstream
else
AM_CPPFLAGS = -I$(top_srcdir)/lib/hardware/coolstream/hd1/libcoolstream
endif

libhwcaps_a_SOURCES = \
	hardware_caps.cpp
