AM_CPPFLAGS = -fno-rtti -fno-exceptions
#AM_CPPFLAGS = -fno-rtti

BUILT_SOURCES  = svn_version.h

svn_version.h:
	@if test -d .svn ; then \
		sleep 180 && rm svn_version.h & echo "#define SVNVERSION \"$$(svnversion -n || echo svn_oops!)\" " > svn_version.h ; \
	else \
		rm svn_version.h; echo '#define BUILT_DATE "'`date`'"' > svn_version.h ; \
	fi

noinst_HEADERS = svn_version.h

SUBDIRS = widget bedit

INCLUDES = \
	-I$(top_srcdir)/daemons \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libupnpclient \
	@CURL_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	-I$(top_srcdir)/lib 

if BOXTYPE_COOL
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
if BOXTYPE_TRIPLE
INCLUDES += -I$(top_srcdir)/lib/libtriple
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
	epg_menu.cpp \
	epgplus.cpp \
	epgview.cpp \
	eventlist.cpp \
	favorites.cpp \
	filebrowser.cpp \
	imageinfo.cpp \
	info_menue.cpp \
	infoviewer.cpp \
	keybind_setup.cpp \
	mediaplayer.cpp \
	mediaplayer_setup.cpp \
	miscsettings_menu.cpp \
	moviebrowser.cpp \
	movieinfo.cpp \
	movieplayer.cpp \
	network_setup.cpp \
	nfs.cpp \
	osd_setup.cpp \
	osdlang_setup.cpp \
	parentallock_setup.cpp \
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
	software_update.cpp \
	streaminfo2.cpp \
	subchannel_select.cpp \
	test_menu.cpp \
	themes.cpp \
	timeosd.cpp \
	update.cpp \
	user_menue.cpp \
	user_menue_setup.cpp \
	vfd_setup.cpp \
	videosettings.cpp \
	zapit_setup.cpp

libneutrino_gui2_a_SOURCES = \
	alphasetup.cpp \
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
