AM_CPPFLAGS = -fno-rtti -fno-exceptions
#AM_CPPFLAGS = -fno-rtti

BUILT_SOURCES  = svn_version.h

svn_version.h:
	@if test -d .svn ; then \
		rm svn_version.h; echo "#define SVNVERSION \"$$(svnversion -n || echo svn_oops!)\" " > svn_version.h ; \
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

noinst_LIBRARIES = libtimerlist.a libneutrino_gui.a libneutrino_gui2.a

libneutrino_gui_a_SOURCES = \
	audio_select.cpp \
	audioplayer.cpp \
	bookmarkmanager.cpp \
	bouquetlist.cpp \
	channellist.cpp \
	dboxinfo.cpp \
	epg_menu.cpp \
	epgplus.cpp \
	epgview.cpp \
	eventlist.cpp \
	favorites.cpp \
	filebrowser.cpp \
	imageinfo.cpp \
	infoviewer.cpp \
	moviebrowser.cpp \
	movieinfo.cpp \
	movieplayer.cpp \
	nfs.cpp \
	pictureviewer.cpp \
	pluginlist.cpp \
	plugins.cpp \
	rc_lock.cpp \
	scan.cpp \
	update.cpp \
	screensetup.cpp \
	sleeptimer.cpp \
	streaminfo2.cpp \
	timeosd.cpp

libneutrino_gui2_a_SOURCES = \
	alphasetup.cpp \
	cam_menu.cpp \
	color.cpp \
	hdd_menu.cpp \
	infoclock.cpp \
	motorcontrol.cpp \
	upnpbrowser.cpp

libtimerlist_a_SOURCES = \
	timerlist.cpp
