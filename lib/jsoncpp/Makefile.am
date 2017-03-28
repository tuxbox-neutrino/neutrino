AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(srcdir)/include

AM_CXXFLAGS = -fno-rtti

noinst_LIBRARIES = libjsoncpp.a

libjsoncpp_a_SOURCES = \
	src/lib_json/json_tool.h \
	src/lib_json/json_reader.cpp \
	src/lib_json/json_value.cpp \
	src/lib_json/json_valueiterator.inl \
	src/lib_json/json_writer.cpp
