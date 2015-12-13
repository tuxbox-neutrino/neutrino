AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)

if USE_PUGIXML
else
AM_CPPFLAGS += -I$(srcdir)/xmltok
endif

AM_CXXFLAGS = -fno-rtti -fno-exceptions

noinst_LIBRARIES = libtuxbox-xmltree.a

libtuxbox_xmltree_a_SOURCES = xmlinterface.cpp
if USE_PUGIXML
else
libtuxbox_xmltree_a_SOURCES += hashtab.c xmlparse.cpp xmlrole.c xmltok.c xmltree.cpp
endif
