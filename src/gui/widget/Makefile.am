AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS

AM_CPPFLAGS += \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@HWLIB_CFLAGS@

noinst_LIBRARIES = libneutrino_gui_widget.a libneutrino_gui_widget2.a

libneutrino_gui_widget_a_SOURCES = \
	buttons.cpp \
	colorchooser.cpp \
	drawable.cpp \
	helpbox.cpp \
	hintbox.cpp \
	keychooser.cpp \
	keyboard_input.cpp \
	listbox.cpp \
	listframe.cpp \
	listhelpers.cpp \
	menue.cpp \
	mountchooser.cpp \
	msgbox.cpp \
	shellwindow.cpp \
	stringinput.cpp \
	stringinput_ext.cpp \
	textbox.cpp

libneutrino_gui_widget2_a_SOURCES = \
	progresswindow.cpp
