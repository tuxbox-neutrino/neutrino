AM_CXXFLAGS = -fno-rtti -fno-exceptions

SUBDIRS = zapit gui daemonc driver system sectionsd timerd nhttpd

INCLUDES = \
	-I$(top_srcdir)/daemons \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src/zapit/include \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/libnet \
	-I$(top_srcdir)/lib/xmltree \
	-I$(top_srcdir)/lib/libmd5sum \
	-I$(top_srcdir)/lib/libupnpclient \
	@FREETYPE_CFLAGS@ \
	@LIBCS_CFLAGS@

if BOXTYPE_COOL
INCLUDES += -I$(top_srcdir)/lib/libcoolstream
endif
if BOXTYPE_TRIPLE
SUBDIRS += lcddisplay
INCLUDES += -I$(top_srcdir)/lib/libtriple
endif

if USE_TREMOR
if USE_TREMOR_STATIC
# i feel a guilty pleasure for those ugly hacks...
VORBISLIBS_TMP1 = $(subst -L,,@VORBISIDEC_LIBS@)
VORBISLIBS_TMP2 = $(subst -lvorbisidec,,$(VORBISLIBS_TMP1))
VORBISLIBS = $(strip $(VORBISLIBS_TMP2))/libvorbisidec.a
else
VORBISLIBS = @VORBISIDEC_LIBS@
endif
else
VORBISLIBS = -lvorbisfile -lvorbis -logg
endif

bin_PROGRAMS = neutrino

neutrino_SOURCES = neutrino_menue.cpp neutrino.cpp

AM_CPPFLAGS =

if ENABLE_FLAC
#FLACLIBS = -lFLAC
FLACLIBS = $(DESTDIR)/lib/libFLAC.a
AM_CPPFLAGS += -DENABLE_FLAC
else
FLACLIBS =
endif

neutrino_LDADD = \
	daemonc/libneutrino_daemonc.a \
	gui/bedit/libneutrino_gui_bedit.a \
	gui/libtimerlist.a \
	gui/libneutrino_gui.a \
	gui/widget/libneutrino_gui_widget.a \
	gui/widget/libneutrino_gui_widget2.a \
	driver/pictureviewer/libneutrino_pictureviewer.a \
	system/libneutrino_system.a \
	gui/movieinfo.o \
	gui/libneutrino_gui2.a \
	sectionsd/libsectionsd.a \
	driver/libneutrino_driver.a \
	driver/audiodec/libneutrino_driver_audiodec.a \
	driver/libneutrino_driver_netfile.a \
	timerd/libtimerd.a \
	zapit/src/libzapit.a \
	nhttpd/libnhttpd.a \
	nhttpd/tuxboxapi/coolstream/libnhttpd_tuxboxapi.a \
	nhttpd/yhttpd_mods/libyhttpdmods.a \
	nhttpd/yhttpd_core/libyhttpd.a \
	$(top_builddir)/lib/sectionsdclient/libsectionsdclient.a \
	$(top_builddir)/lib/timerdclient/libtimerdclient.a \
	$(top_builddir)/src/zapit/lib/libzapitclient.a \
	$(top_builddir)/lib/libconfigfile/libtuxbox-configfile.a \
	$(top_builddir)/lib/connection/libtuxbox-connection.a \
	$(top_builddir)/lib/libeventserver/libtuxbox-eventserver.a \
	$(top_builddir)/lib/xmltree/libtuxbox-xmltree.a \
	$(top_builddir)/lib/libnet/libtuxbox-net.a \
	$(top_builddir)/lib/libmd5sum/libtuxbox-md5sum.a \
	$(top_builddir)/lib/libtuxtxt/libtuxtxt.a \
	$(top_builddir)/lib/libdvbsub/libdvbsub.a \
	$(top_builddir)/lib/libupnpclient/libtuxbox-upnpclient.a \
	@CURL_LIBS@ \
	@FREETYPE_LIBS@ \
	@PNG_LIBS@ \
	@MAD_LIBS@ \
	@ID3TAG_LIBS@ \
	@LIBCS_LIBS@ \
	@AVFORMAT_LIBS@ \
	@AVUTIL_LIBS@ \
	$(FLACLIBS) \
	$(VORBISLIBS) \
	-lungif \
	-ljpeg \
	-lrt -lpthread

if BOXTYPE_COOL
neutrino_LDADD += \
	-lcoolstream \
	-lOpenThreads \
	-lnxp

bin_PROGRAMS += drivertool

drivertool_SOURCES = drivertool.c
endif
if BOXTYPE_TRIPLE
neutrino_LDADD += \
	$(top_builddir)/lib/libtriple/libtriple.a \
	@DIRECTFB_LIBS@ \
	$(top_builddir)/src/lcddisplay/liblcddisplay.a
endif

