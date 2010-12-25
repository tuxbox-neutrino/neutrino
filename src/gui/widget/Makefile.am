AM_CPPFLAGS = -fno-rtti -fno-exceptions

INCLUDES = \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	@FREETYPE_CFLAGS@

if BOXTYPE_COOL
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
if BOXTYPE_TRIPLE
INCLUDES += -I$(top_srcdir)/lib/libtriple
endif

noinst_LIBRARIES = libneutrino_gui_widget.a libneutrino_gui_widget2.a 

libneutrino_gui_widget_a_SOURCES = \
	buttons.cpp \
	colorchooser.cpp \
	keychooser.cpp \
	menue.cpp \
	stringinput.cpp \
	stringinput_ext.cpp \
	messagebox.cpp \
	hintbox.cpp \
	listbox.cpp \
	listbox_legacy.cpp \
	drawable.cpp \
	hintboxext.cpp \
	helpbox.cpp \
	mountchooser.cpp \
	listframe.cpp \
	msgbox.cpp \
	textbox.cpp \
	progressbar.cpp

libneutrino_gui_widget2_a_SOURCES = \
	progresswindow.cpp vfdcontroler.cpp
