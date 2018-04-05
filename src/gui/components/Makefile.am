AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/xmltree \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@LUA_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_gui_components.a


libneutrino_gui_components_a_SOURCES = \
	cc_base.cpp \
	cc_button_select.cpp \
	cc_detailsline.cpp \
	cc_draw.cpp \
	cc_extra.cpp \
	cc_frm_button.cpp \
	cc_frm.cpp \
	cc_frm_chain.cpp \
	cc_frm_clock.cpp \
	cc_frm_footer.cpp \
	cc_frm_header.cpp \
	cc_frm_ext_text.cpp \
	cc_frm_icons.cpp \
	cc_frm_scrollbar.cpp \
	cc_frm_signalbars.cpp \
	cc_frm_slider.cpp \
	cc_frm_window.cpp \
	cc_item.cpp \
	cc_item_infobox.cpp \
	cc_item_picture.cpp \
	cc_item_progressbar.cpp \
	cc_item_shapes.cpp \
	cc_item_text.cpp \
	cc_item_tvpic.cpp \
	cc_timer.cpp\
	cc_types.cpp
