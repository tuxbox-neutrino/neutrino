AC_INIT(tuxbox-neutrino,1.0.1)
AM_INIT_AUTOMAKE(tuxbox-neutrino,1.0.1)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY
TUXBOX_APPS_PKGCONFIG
TUXBOX_BOXTYPE

AC_PROG_CC
AC_PROG_CXX
AC_DISABLE_STATIC
AM_PROG_LIBTOOL

AC_ARG_WITH([tremor], [AS_HELP_STRING([--with-tremor],
	[use libvorbisidec instead of libogg/libvorbis])],
	[TREMOR="$withval"],
	[TREMOR=no])

AC_ARG_WITH([tremor-static], [AS_HELP_STRING([--with-tremor-static],
	[use statically linked libvorbisidec instead of libogg/libvorbis])],
	[TREMOR_STATIC="$withval"],
	[TREMOR_STATIC=no])

if test "$BOXTYPE" = "tripledragon"; then
	TUXBOX_APPS_LIB_PKGCONFIG(DIRECTFB, directfb)
fi

if test "$TREMOR_STATIC" = "yes"; then
	TREMOR=yes
fi

if test "$TREMOR" = "yes"; then
	TUXBOX_APPS_LIB_PKGCONFIG(VORBISIDEC,vorbisidec)
	AC_DEFINE(USE_TREMOR,1,use libvorbisidec/tremor library)
fi

if test "$TREMOR_STATIC" = "yes"; then
	# hack to get the static lib location from the pkg-config data
	VORBISIDEC_LIBS="$(echo $VORBISIDEC_LIBS | sed 's@-L@@; s@ -l.*@/libvorbisidec.a@;')"
fi

AM_CONDITIONAL(USE_TREMOR, test "$TREMOR" = "yes")

TUXBOX_APPS_LIB_CONFIG(CURL,curl-config)
TUXBOX_APPS_LIB_CONFIG(FREETYPE,freetype-config)
# TUXBOX_APPS_LIB_PKGCONFIG(OPENSSL,openssl)
TUXBOX_APPS_LIB_PKGCONFIG_CHECK(ID3TAG,libid3tag)
if test -z "$ID3TAG_CFLAGS" ; then
	TUXBOX_APPS_LIB_PKGCONFIG(ID3TAG,id3tag)
fi
TUXBOX_APPS_LIB_PKGCONFIG_CHECK(MAD,libmad)
if test -z "$MAD_CFLAGS" ; then
	TUXBOX_APPS_LIB_PKGCONFIG(MAD,mad)
fi
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

AC_ARG_ENABLE(mdev,
	[  --enable-mdev disable broken neutrino mount hack, use with mdev],
	[AC_DEFINE(ASSUME_MDEV,1,[disable broken neutrino mount hack, use with mdev])])

AC_ARG_ENABLE(freesatepg,
	[  --enable-freesatepg                  enable Freesat EPG code (experimental)],
	[AC_DEFINE(ENABLE_FREESATEPG,1,[enable Freesat EPG code])])

AC_ARG_ENABLE(giflib,
	AS_HELP_STRING(--enable-giflib,use giflib instead of libungif),
	,[enable_giflib=no])

AM_CONDITIONAL(ENABLE_GIFLIB,test "$enable_giflib" = "yes")
if test "$enable_giflib" = "yes"; then
	AC_DEFINE(ENABLE_GIFLIB,1,[use giflib instead of libungif])
fi

# FLAC - Free Lossless Audio Codec
AC_ARG_ENABLE(flac,
	AS_HELP_STRING(--enable-flac,include FLAC support),
	,[enable_flac=no])

AM_CONDITIONAL(ENABLE_FLAC,test "$enable_flac" = "yes")
if test "$enable_flac" = "yes"; then
	AC_DEFINE(ENABLE_FLAC,1,[include FLAC support])
fi

# UPNP - upnp browser (borken right now)
AC_ARG_ENABLE(upnp,
	AS_HELP_STRING(--disable-upnp,disable UPNP support),
	,[enable_upnp=yes])

AM_CONDITIONAL(ENABLE_UPNP,test "$enable_upnp" = "yes")
if test "$enable_upnp" = "yes"; then
	AC_DEFINE(ENABLE_UPNP,1,[include UPNP support - currently broken])
fi

AC_ARG_WITH(stb-hal-includes,
	[  --with-stb-hal-includes=PATH  path for libstb-hal includes [[NONE]]],
	[STB_HAL_INC="$withval"],[STB_HAL_INC=""])
AC_ARG_WITH(stb-hal-build,
	[  --with-stb-hal-build=PATH   path where libstb-hal is built [[NONE]]],
	[STB_HAL_LIB="$withval"],[STB_HAL_LIB=""])

AM_CONDITIONAL(USE_STB_HAL, test "$STB_HAL_INC")
if test "$STB_HAL_INC"; then
	AC_DEFINE(USE_STB_HAL, 1,[use libstb-hal])
fi


AC_CHECK_HEADERS(coolstream/nevis_ir.h)

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
AC_SUBST(STB_HAL_INC)
AC_SUBST(STB_HAL_LIB)

AC_OUTPUT([
Makefile
lib/Makefile
lib/libconfigfile/Makefile
lib/connection/Makefile
lib/libeventserver/Makefile
lib/libmd5sum/Makefile
lib/libnet/Makefile
lib/xmltree/Makefile
lib/sectionsdclient/Makefile
lib/timerdclient/Makefile
lib/libtriple/Makefile
lib/libtuxtxt/Makefile
lib/libdvbsub/Makefile
lib/libupnpclient/Makefile
src/lcddisplay/Makefile
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

