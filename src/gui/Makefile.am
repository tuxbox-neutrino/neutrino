AM_CPPFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS
#AM_CPPFLAGS = -fno-rtti
BUILT_SOURCES  = git_version.h
git_version.h: makeversion
	@if test -d ../../.git; then \
		echo "#define GITVERSION \"$$(git describe --dirty || echo `date` )\" " > git_version.h.tmp ; \
		if diff -q git_version.h git_version.h.tmp >/dev/null 2>&1 ; then \
			rm -f git_version.h.tmp ; \
		else \
			rm -f git_version.h ; \
			mv git_version.h.tmp git_version.h ; \
		fi; \
	else \
		rm git_version.h; echo '#define BUILT_DATE "'`date`'"' > git_version.h ; \
	fi
noinst_HEADERS = git_version.h

.PHONY: makeversion


SUBDIRS = bedit components widget

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libupnpclient \
	@CURL_CFLAGS@ \
	@FREETYPE_CFLAGS@

if BOXTYPE_COOL
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif

if USE_STB_HAL
INCLUDES += -I$(STB_HAL_INC)
endif

noinst_LIBRARIES = libtimerlist.a libneutrino_gui.a libneutrino_gui2.a

libneutrino_gui_a_SOURCES = \
	audio_select.cpp \
	audio_setup.cpp \
	audioplayer.cpp \
	audioplayer_setup.cpp\
	bookmarkmanager.cpp \
	bouquetlist.cpp \
	channellist.cpp \
	cec_setup.cpp \
	dboxinfo.cpp \
	epgplus.cpp \
	epgview.cpp \
	eventlist.cpp \
	favorites.cpp \
	filebrowser.cpp \
	imageinfo.cpp \
	info_menue.cpp \
	infoviewer.cpp \
	infoviewer_bb.cpp \
	keybind_setup.cpp \
	mediaplayer.cpp \
	mediaplayer_setup.cpp \
	miscsettings_menu.cpp \
	moviebrowser.cpp \
	movieinfo.cpp \
	movieplayer.cpp \
	network_service.cpp \
	network_setup.cpp \
	nfs.cpp \
	osd_setup.cpp \
	osdlang_setup.cpp \
	parentallock_setup.cpp \
	personalize.cpp \
	pictureviewer.cpp \
	pictureviewer_setup.cpp \
	pluginlist.cpp \
	plugins.cpp \
	proxyserver_setup.cpp \
	rc_lock.cpp \
	record_setup.cpp \
	scan.cpp \
	scan_setup.cpp \
	screensetup.cpp \
	settings_manager.cpp \
	sleeptimer.cpp \
	start_wizard.cpp \
	streaminfo2.cpp \
	subchannel_select.cpp \
	test_menu.cpp \
	themes.cpp \
	timeosd.cpp \
	update.cpp \
	update_menue.cpp \
	update_settings.cpp \
	user_menue.cpp \
	user_menue_setup.cpp \
	vfd_setup.cpp \
	videosettings.cpp \
	zapit_setup.cpp

if ENABLE_EXTUPDATE
libneutrino_gui_a_SOURCES += \
	update_ext.cpp
endif

libneutrino_gui_a_SOURCES += \
	luainstance.cpp

libneutrino_gui2_a_SOURCES = \
	cam_menu.cpp \
	color.cpp \
	hdd_menu.cpp \
	infoclock.cpp \
	motorcontrol.cpp

if ENABLE_UPNP
libneutrino_gui2_a_SOURCES += \
	upnpbrowser.cpp
endif

libtimerlist_a_SOURCES = \
	timerlist.cpp
