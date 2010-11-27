AC_INIT(tuxbox-neutrino,1.0.1)
AM_INIT_AUTOMAKE(tuxbox-neutrino,1.0.1)

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY
TUXBOX_APPS_PKGCONFIG
TUXBOX_BOXTYPE

AC_PROG_CC
AC_PROG_CXX
AC_DISABLE_STATIC
AM_PROG_LIBTOOL

if test "$BOXTYPE" != "tripledragon"; then
echo "BOXTYPE: $BOXTYPE"
	TUXBOX_APPS_DVB
	if test "$BOXTYPE" != "coolstream"; then
		TUXBOX_APPS_DRIVER
		TUXBOX_APPS_LIB_PKGCONFIG(TUXBOX,tuxbox)
	fi
fi
TUXBOX_APPS_LIB_CONFIG(CURL,curl-config)
TUXBOX_APPS_LIB_CONFIG(FREETYPE,freetype-config)
# TUXBOX_APPS_LIB_PKGCONFIG(OPENSSL,openssl)
TUXBOX_APPS_LIB_PKGCONFIG(ID3TAG,libid3tag)
TUXBOX_APPS_LIB_PKGCONFIG(MAD,libmad)
#TUXBOX_APPS_LIB_PKGCONFIG(VORBISIDEC,tremor)
TUXBOX_APPS_LIB_PKGCONFIG(PNG,libpng)
TUXBOX_APPS_LIB_PKGCONFIG(AVFORMAT,libavformat)
TUXBOX_APPS_LIB_PKGCONFIG(AVCODEC,libavcodec)
TUXBOX_APPS_LIB_PKGCONFIG(AVUTIL,libavutil)
#TUXBOX_APPS_LIB_PKGCONFIG(CONFIGFILE,tuxbox-configfile)
#TUXBOX_APPS_LIB_PKGCONFIG(CONNECTION,tuxbox-connection)
#TUXBOX_APPS_LIB_PKGCONFIG(EVENTSERVER,tuxbox-eventserver)
#TUXBOX_APPS_LIB_PKGCONFIG(LCDDISPLAY,tuxbox-lcddisplay)
#TUXBOX_APPS_LIB_PKGCONFIG(LIRCDCLIENT,tuxbox-lircdclient)
#TUXBOX_APPS_LIB_PKGCONFIG(NET,tuxbox-net)
#TUXBOX_APPS_LIB_PKGCONFIG(PLUGINS,tuxbox-plugins)
#TUXBOX_APPS_LIB_PKGCONFIG(TUXBOX,tuxbox)
#TUXBOX_APPS_LIB_PKGCONFIG(UCODES,tuxbox-ucodes)
#TUXBOX_APPS_LIB_PKGCONFIG(MPEGTOOLS,tuxbox-mpegtools)
#TUXBOX_APPS_LIB_PKGCONFIG(TUXTXT,tuxbox-tuxtxt)

AC_ARG_ENABLE(keyboard-no-rc,
         [  --enable-keyboard-no-rc             enable keyboard control, disable rc control],
         [AC_DEFINE(KEYBOARD_INSTEAD_OF_REMOTE_CONTROL,1,[enable keyboard control, disable rc control])])

AC_ARG_ENABLE(restore-prev-mode,
         [  --enable-restore-prev-mode          enable return from graphics mode],
         [AC_DEFINE(RETURN_FROM_GRAPHICS_MODE,1,[enable return from graphics mode])])

#
# Check for libtdservicedb - the new one - for testing only
#
#CSL_VERSION=0.0.1
#FCSL_VERSION=`$PKG_CONFIG --modversion libcoolstream`
#AC_MSG_CHECKING(for package libcoolstream >= $CSL_VERSION)
#if $PKG_CONFIG --atleast-version $CSL_VERSION libcoolstream ; then
      #AC_MSG_RESULT(found (version $FCSL_VERSION))
      #LIBCS_CFLAGS=`$PKG_CONFIG --cflags libcoolstream`
      #LIBCS_LIBS=`$PKG_CONFIG --libs libcoolstream`
#else
      #AC_MSG_ERROR([
#*** libcoolstream $CSL_VERSION or newer is required! ***
  #])
#fi
AC_SUBST(LIBCS_CFLAGS)
AC_SUBST(LIBCS_LIBS)
AC_SUBST(FREETYPE_CFLAGS)
AC_SUBST(FREETYPE_LIBS)
AC_SUBST(VORBISIDEC_CFLAGS)
AC_SUBST(VORBISIDEC_LIBS)

AC_OUTPUT([
Makefile
lib/Makefile
lib/libconfigfile/Makefile
lib/connection/Makefile
lib/libeventserver/Makefile
lib/libmd5sum/Makefile
lib/libnet/Makefile
lib/xmltree/Makefile
lib/controldclient/Makefile
lib/sectionsdclient/Makefile
lib/timerdclient/Makefile
lib/libtuxtxt/Makefile
lib/libdvbsub/Makefile
lib/libupnpclient/Makefile
src/nhttpd/Makefile
src/nhttpd/web/Makefile
src/nhttpd/web/images/Makefile
src/nhttpd/web/scripts/Makefile
src/nhttpd/web/languages/Makefile
src/nhttpd/tuxboxapi/Makefile
src/nhttpd/tuxboxapi/coolstream/Makefile
src/nhttpd/yhttpd_core/Makefile
src/nhttpd/yhttpd_mods/Makefile
src/Makefile
src/daemonc/Makefile
src/driver/pictureviewer/Makefile
src/driver/audiodec/Makefile
src/driver/Makefile
src/gui/Makefile
src/gui/bedit/Makefile
src/gui/widget/Makefile
src/system/Makefile
data/Makefile
data/fonts/Makefile
data/icons/Makefile
data/inetradio/Makefile
data/iso-codes/Makefile
data/lcd/Makefile
data/lcd/icons/Makefile
data/lcd/clock/Makefile
data/locale/Makefile
data/scripts/Makefile
data/themes/Makefile
src/sectionsd/Makefile
src/timerd/Makefile
src/zapit/Makefile
src/zapit/lib/Makefile
src/zapit/src/Makefile
])

