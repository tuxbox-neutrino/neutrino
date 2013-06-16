AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/xmltree \
	@FREETYPE_CFLAGS@

if BOXTYPE_COOL
if BOXMODEL_APOLLO
INCLUDES += -I$(top_srcdir)/lib/libcoolstream2
else
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
endif

if USE_STB_HAL
INCLUDES += -I$(STB_HAL_INC)
endif

noinst_LIBRARIES = libneutrino_gui_components.a


libneutrino_gui_components_a_SOURCES = \
	cc_base.cpp \
	cc_detailsline.cpp \
	cc_frm_button.cpp \
	cc_frm.cpp \
	cc_frm_clock.cpp \
	cc_frm_footer.cpp \
	cc_frm_header.cpp \
	cc_frm_icons.cpp \
	cc_frm_window.cpp \
	cc_item.cpp \
	cc_item_infobox.cpp \
	cc_item_picture.cpp \
	cc_item_progressbar.cpp \
	cc_item_shapes.cpp \
	cc_item_text.cpp \
	cc_item_tvpic.cpp
