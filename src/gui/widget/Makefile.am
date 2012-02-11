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
if USE_STB_HAL
INCLUDES += -I$(STB_HAL_INC)
else
if BOXTYPE_TRIPLE
INCLUDES += -I$(top_srcdir)/lib/libtriple
endif
endif

noinst_LIBRARIES = libneutrino_gui_widget.a libneutrino_gui_widget2.a 

libneutrino_gui_widget_a_SOURCES = \
	buttons.cpp \
	colorchooser.cpp \
	drawable.cpp \
	helpbox.cpp \
	hintbox.cpp \
	hintboxext.cpp \
	keychooser.cpp \
	listbox.cpp \
	listbox_legacy.cpp \
	listframe.cpp \
	menue.cpp \
	messagebox.cpp \
	mountchooser.cpp \
	msgbox.cpp \
	progressbar.cpp \
	stringinput.cpp \
	stringinput_ext.cpp \
	textbox.cpp

libneutrino_gui_widget2_a_SOURCES = \
	progresswindow.cpp
