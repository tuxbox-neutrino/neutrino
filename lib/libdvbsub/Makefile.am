INCLUDES = \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/lib/libcoolstream

AM_CPPFLAGS = -fno-rtti -fno-exceptions

noinst_LIBRARIES = libdvbsub.a

libdvbsub_a_SOURCES = dvbsub.cpp dvbsubtitle.cpp \
	tools.cpp osd.cpp PacketQueue.cpp helpers.cpp Debug.cpp
