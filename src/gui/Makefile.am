AM_CPPFLAGS = -fno-rtti -D__STDC_FORMAT_MACROS
#AM_CPPFLAGS = -fno-rtti
BUILT_SOURCES = version.h
version.h:
	@rm -f $@; \
	echo '#define BUILT_DATE "'`date`'"' > $@
	@if test -d $(top_srcdir)/.git ; then \
		pushd $(top_srcdir) ; \
		GITDESCRIBE=$$(git describe --always --dirty || echo 'VCS failed') ; \
		popd ; \
		echo '#define VCS "'$${GITDESCRIBE}'"' >> $@ ; \
	fi
noinst_HEADERS = version.h
.PHONY: version.h

SUBDIRS = bedit components widget

AM_CPPFLAGS += \
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
	@LUA_CFLAGS@ \
	@FREETYPE_CFLAGS@

if BOXTYPE_COOL
if BOXMODEL_APOLLO
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream2
else
AM_CPPFLAGS += -I$(top_srcdir)/lib/libcoolstream
endif
endif

if USE_STB_HAL
AM_CPPFLAGS += -I$(STB_HAL_INC)
endif

noinst_LIBRARIES = libtimerlist.a libneutrino_gui.a libneutrino_gui2.a

libneutrino_gui_a_SOURCES = \
	audio_select.cpp \
	audio_setup.cpp \
	audiomute.cpp \
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
	pipsetup.cpp \
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
	themes.cpp \
	timeosd.cpp \
	update.cpp \
	update_menue.cpp \
	update_settings.cpp \
	user_menue.cpp \
	user_menue_setup.cpp \
	vfd_setup.cpp \
	videosettings.cpp \
	volumebar.cpp \
	zapit_setup.cpp

if ENABLE_EXTUPDATE
libneutrino_gui_a_SOURCES += \
	update_ext.cpp
endif

if ENABLE_TEST_MENU
libneutrino_gui_a_SOURCES += \
	test_menu.cpp
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
