AM_CPPFLAGS = -fno-rtti -fno-exceptions
#AM_CPPFLAGS = -fno-rtti

SUBDIRS = widget bedit

INCLUDES = \
	-I$(top_srcdir)/daemons \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libcoolstream \
	-I$(top_srcdir)/lib/libupnpclient \
	@CURL_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	-I$(top_srcdir)/lib 

noinst_LIBRARIES = libtimerlist.a libneutrino_gui.a libneutrino_gui2.a

libneutrino_gui_a_SOURCES = \
	bouquetlist.cpp channellist.cpp eventlist.cpp pluginlist.cpp \
	favorites.cpp sleeptimer.cpp screensetup.cpp \
	epgview.cpp infoviewer.cpp scan.cpp update.cpp \
	filebrowser.cpp audioplayer.cpp nfs.cpp pictureviewer.cpp \
	movieplayer.cpp rc_lock.cpp bookmarkmanager.cpp \
	timeosd.cpp epgplus.cpp epg_menu.cpp \
	streaminfo2.cpp dboxinfo.cpp \
	plugins.cpp imageinfo.cpp audio_select.cpp moviebrowser.cpp movieinfo.cpp

libneutrino_gui2_a_SOURCES = \
	color.cpp alphasetup.cpp motorcontrol.cpp scale.cpp hdd_menu.cpp cam_menu.cpp infoclock.cpp upnpbrowser.cpp

libtimerlist_a_SOURCES = \
	timerlist.cpp

