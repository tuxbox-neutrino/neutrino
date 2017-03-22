AC_INIT(Neutrino-MP,3.4.2)
AM_INIT_AUTOMAKE([1.0.1 nostdinc])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY
TUXBOX_APPS_PKGCONFIG
TUXBOX_BOXTYPE

AC_PROG_CC
AC_PROG_CXX
AC_DISABLE_STATIC
AM_PROG_LIBTOOL

# Add build information to local_build_config.h.tmp
# -------------------------------------------------
rm -f local_build_config.h.tmp
cat > local_build_config.h.tmp << EOF
/* Build system under which the program was compiled on. */
#define USED_BUILD "$build"
/* Define to the used CXXFLAGS to compile this package. */
#define USED_CXXFLAGS "$CXXFLAGS"
EOF
if COMPILER=`$CC --version | head -n 1`; then
	cat >> local_build_config.h.tmp <<-EOF
	/* Define to name and version of used compiler */
	#define USED_COMPILER "$COMPILER"
	EOF
fi
# only update header if it differs to avoid unnecessary rebuilds
if ! diff local_build_config.h.tmp local_build_config.h >/dev/null 2>&1; then
	rm -f local_build_config.h
	mv local_build_config.h.tmp local_build_config.h
else
	rm -f local_build_config.h.tmp
fi

AC_ARG_ENABLE(ffmpegdec,
	AS_HELP_STRING(--enable-ffmpegdec,include ffmpeg decoder support),
	,[enable_ffmpegdec=no])

AM_CONDITIONAL(ENABLE_FFMPEGDEC,test "$enable_ffmpegdec" = "yes")
if test "$enable_ffmpegdec" = "yes"; then
	AC_DEFINE(ENABLE_FFMPEGDEC,1,[include ffmpeg decoder support])
fi

if test x"$enable_ffmpegdec" != xyes; then

AC_ARG_WITH([tremor], [AS_HELP_STRING([--with-tremor],
	[use libvorbisidec instead of libogg/libvorbis])],
	[TREMOR="$withval"],
	[TREMOR=no])

AC_ARG_WITH([tremor-static], [AS_HELP_STRING([--with-tremor-static],
	[use statically linked libvorbisidec instead of libogg/libvorbis])],
	[TREMOR_STATIC="$withval"],
	[TREMOR_STATIC=no])

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

TUXBOX_APPS_LIB_PKGCONFIG_CHECK(ID3TAG,libid3tag)
if test x"$ID3TAG_EXISTS" != xyes; then
	TUXBOX_APPS_LIB_PKGCONFIG(ID3TAG,id3tag)
fi

TUXBOX_APPS_LIB_PKGCONFIG_CHECK(MAD,libmad)
if test x"$MAD_EXISTS" != xyes; then
	TUXBOX_APPS_LIB_PKGCONFIG(MAD,mad)
fi

TUXBOX_APPS_LIB_PKGCONFIG_CHECK(OGG,ogg)
if test -z "$OGG_CFLAGS" ; then
	TUXBOX_APPS_LIB_PKGCONFIG(OGG,ogg)
fi
TUXBOX_APPS_LIB_PKGCONFIG(SWRESAMPLE,libswresample)
else
TUXBOX_APPS_LIB_PKGCONFIG(SWRESAMPLE,libswresample)
fi

if test "$BOXTYPE" = "tripledragon"; then
	TUXBOX_APPS_LIB_PKGCONFIG(DIRECTFB, directfb)
fi

AM_CONDITIONAL(USE_TREMOR, test "$TREMOR" = "yes")

# TUXBOX_APPS_LIB_PKGCONFIG(OPENSSL,openssl)
TUXBOX_APPS_LIB_PKGCONFIG(CURL,libcurl)

## For the check you must use the corresponding libtool number as version number,
## see freetype source code docs/VERSIONS.TXT
PKG_CHECK_MODULES([FREETYPE], [freetype2 >= 16.2.10],
	[echo "freetype2 >= 2.5.0 found"], [
	## If freetype is not found:
	## Activate this line if you want to search with freetype-config instead
	[echo "freetype2 >= 2.5.0 not found, use alternative search method with freetype-config"]
	## Activate this line if you want to abort
#	AC_MSG_ERROR([freetype2 >= 2.5.0 not found])
	])

# fallback to curl-config (which is ugly for cross-compilation)
if test -z "$CURL_LIBS" -a -z "$CURL_CFLAGS"; then
	TUXBOX_APPS_LIB_CONFIG(CURL,curl-config)
fi
# fallback to freetype-config (which is ugly for cross-compilation)
if test -z "$FREETYPE_LIBS" -a -z "$FREETYPE_CFLAGS"; then
	TUXBOX_APPS_LIB_CONFIG(FREETYPE,freetype-config)
	CPPFLAGS+=" $FREETYPE_CFLAGS"
	AC_MSG_CHECKING([whether FreeType version is 2.5.0 or higher])
		AC_TRY_CPP([
			#include <ft2build.h>
			#include FT_FREETYPE_H
			#if FREETYPE_MAJOR < 2 || (FREETYPE_MAJOR == 2 && FREETYPE_MINOR < 5)
			#error Freetype version too low.
			#endif
			],
			[AC_MSG_RESULT(yes)],
			[AC_MSG_ERROR([Need FreeType library version 2.5.0 or higher])
		])
fi

TUXBOX_APPS_LIB_PKGCONFIG(PNG,libpng)
TUXBOX_APPS_LIB_PKGCONFIG(AVFORMAT,libavformat)
TUXBOX_APPS_LIB_PKGCONFIG(AVCODEC,libavcodec)
TUXBOX_APPS_LIB_PKGCONFIG(AVUTIL,libavutil)
if test "$BOXTYPE" = "coolstream"; then
	TUXBOX_APPS_LIB_PKGCONFIG(BLURAY,libbluray)
fi

if test "$BOXTYPE" = "coolstream" ||
   test "$BOXTYPE" = "generic" ||
   test "$BOXTYPE" = "tripledragon" ||
   test "$BOXTYPE" = "spark"; then
	AC_DEFINE(SCREENSHOT,1,[Define to 1 if the platform supports screenshots])
fi

TUXBOX_APPS_LIB_PKGCONFIG(SIGC,sigc++-2.0)

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

AC_ARG_ENABLE(mdev,
	[  --enable-mdev disable broken neutrino mount hack, use with mdev],
	[AC_DEFINE(ASSUME_MDEV,1,[disable broken neutrino mount hack, use with mdev])])

AC_ARG_ENABLE(freesatepg,
	[  --enable-freesatepg                  enable Freesat EPG code (experimental)],
	[AC_DEFINE(ENABLE_FREESATEPG,1,[enable Freesat EPG code])])

AC_ARG_ENABLE(viasatepg,
	[  --enable-viasatepg                  enable ViaSat EPG code (experimental)],
	[AC_DEFINE(ENABLE_VIASATEPG,1,[enable ViaSat EPG code])])

AC_ARG_ENABLE(fastscan,
	[  --enable-fastscan                   enable Fastscan code)],
	[AC_DEFINE(ENABLE_FASTSCAN,1,[enable fastscan code])])

AM_CONDITIONAL(ENABLE_FASTSCAN, test "$enable_fastscan" = "yes")

AC_ARG_ENABLE(giflib,
	AS_HELP_STRING(--enable-giflib,use giflib instead of libungif),
	,[enable_giflib=no])

AM_CONDITIONAL(ENABLE_GIFLIB,test "$enable_giflib" = "yes")
if test "$enable_giflib" = "yes"; then
	AC_DEFINE(ENABLE_GIFLIB,1,[use giflib instead of libungif])
fi

### USE_PUGIXML
AC_ARG_ENABLE(pugixml,
       AS_HELP_STRING(--enable-pugixml,use pugixml instead of xmltree),
       ,[enable_pugixml=no])

AM_CONDITIONAL(USE_PUGIXML,test "$enable_pugixml" = "yes")
if test "$enable_pugixml" = "yes"; then
       AC_DEFINE(USE_PUGIXML,1,[use pugixml instead of xmltree])
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

AC_ARG_ENABLE(extupdate,
	AS_HELP_STRING(--enable-extupdate,include extended update routine),
	,[enable_extupdate=no])

AM_CONDITIONAL(ENABLE_EXTUPDATE,test "$enable_extupdate" = "yes")
if test "$enable_extupdate" = "yes"; then
	AC_DEFINE(ENABLE_EXTUPDATE,1,[include extended update routine])
fi

AC_ARG_WITH(stb-hal-includes,
	[  --with-stb-hal-includes=PATH  path for libstb-hal includes [[NONE]]],
	[STB_HAL_INC="$withval"],[STB_HAL_INC=""])
AC_ARG_WITH(stb-hal-build,
	[  --with-stb-hal-build=PATH   path where libstb-hal is built [[NONE]]],
	[STB_HAL_LIB="-L$withval"],[STB_HAL_LIB=""])

AM_CONDITIONAL(USE_STB_HAL, test "$BOXTYPE" != "coolstream")
if test "$BOXTYPE" != coolstream; then
	AC_DEFINE(USE_STB_HAL, 1,[use libstb-hal])
	if test "$STB_HAL_INC" = ""; then
		AC_MSG_ERROR([need libstb-hal includes path with --with-stb-hal-includes=...])
	fi
	HWLIB_CFLAGS="-I$STB_HAL_INC"
fi

# Lua - embeddable scripting language
AC_ARG_ENABLE(lua,
	AS_HELP_STRING(--disable-lua,disable Lua support),
	,[enable_lua=yes])

AM_CONDITIONAL(ENABLE_LUA,test "$enable_lua" = "yes")
if test "$enable_lua" = "yes"; then
	AC_DEFINE(ENABLE_LUA,1,[include Lua support])
	PKG_CHECK_MODULES([LUA], [lua >= 5.2], echo "lua >= 5.2 found", [
		PKG_CHECK_MODULES([LUA], [lua5.2 >= 5.2], echo "lua5.2 found", [
			echo "=> lualib not found, assuming static lua in linker path..."
			LUA_LIBS="-llua -ldl"
			AC_DEFINE(STATIC_LUAPOSIX,1,[Define to 1 for static lua build.])
		])
	])
# hack...
AC_DEFINE(LUA_COMPAT_5_2,1,[does not really belong in config.h, but is needed for build with lua 5.3+])
fi

AC_ARG_ENABLE(cleanup,
	[  --enable-cleanup                  enable cleanup on exit],
	[AC_DEFINE(EXIT_CLEANUP,1,[enable cleanup on exit])])

AC_ARG_ENABLE(pip,
	[  --enable-pip                  enable picture in picture support],
	[AC_DEFINE(ENABLE_PIP,1,[enable picture in picture support])])


AC_ARG_ENABLE(testing,
       AS_HELP_STRING(--enable-testing,include devel code parts for neutrino tests))

AM_CONDITIONAL(ENABLE_TESTING,test "$enable_testing" = "yes")
if test "$enable_testing" = "yes"; then
	AC_DEFINE(ENABLE_TESTING,1,[include devel code parts for neutrino tests - not recommended for general users!])
fi


if test "$BOXTYPE" = "coolstream"; then
if test -e ${srcdir}/lib/hardware/coolstream/hd1/libcoolstream/nevis_ir.h; then
	AC_DEFINE(HAVE_COOLSTREAM_NEVIS_IR_H,1,[Define to 1 if you have the <nevis_ir.h> header file.])
fi
HWLIB_CFLAGS='-I$(top_srcdir)/lib/hardware/coolstream/hd1/libcoolstream'

if test "$BOXMODEL" = "hd2"; then
if test -e ${srcdir}/lib/hardware/coolstream/hd2/libcoolstream/cs_ir_generic.h; then
	AC_DEFINE(HAVE_COOLSTREAM_CS_IR_GENERIC_H,1,[Define to 1 if you have the <cs_ir_generic.h> header file.])
fi
if test -e ${srcdir}/lib/hardware/coolstream/hd2/libcoolstream/cs_frontpanel.h; then
	AC_DEFINE(HAVE_COOLSTREAM_CS_FRONTPANEL_H,1,[Define to 1 if you have the <cs_frontpanel.h> header file.])
fi
HWLIB_CFLAGS='-I$(top_srcdir)/lib/hardware/coolstream/hd2/libcoolstream'
fi
HWLIB_CFLAGS="$HWLIB_CFLAGS "'-I$(top_srcdir)/src/zapit/include/private'
HWLIB_CFLAGS="$HWLIB_CFLAGS "'-I$(top_srcdir)/lib/hardware/coolstream'
fi

# hack, so that framebuffer.h does not need to be included everywhere...
AC_DEFINE(fb_pixel_t, uint32_t, [hack, so that framebuffer.h does not need to be included everywhere...])

# hack to define a short filename also for out-of-tree build
if test `dirname $0` = `pwd` || test "$0" = ./configure; then
	HWLIB_CFLAGS="$HWLIB_CFLAGS "'-D__file__=__FILE__'
	HWLIB_CFLAGS="$HWLIB_CFLAGS "'-D__path_file__=__FILE__'
else
	HWLIB_CFLAGS="$HWLIB_CFLAGS "'-D__file__="\"$(subst $(srcdir)/,,$(abspath $<))\""'
	HWLIB_CFLAGS="$HWLIB_CFLAGS "'-D__path_file__="\"$(subst $(top_srcdir)/,,$(abspath $<))\""'
fi

AC_SUBST(HWLIB_CFLAGS)
AC_SUBST(FREETYPE_CFLAGS)
AC_SUBST(FREETYPE_LIBS)
AC_SUBST(VORBISIDEC_CFLAGS)
AC_SUBST(VORBISIDEC_LIBS)
AC_SUBST(STB_HAL_INC)
AC_SUBST(STB_HAL_LIB)
AC_SUBST(LUA_CFLAGS)
AC_SUBST(LUA_LIBS)
AC_SUBST(BLURAY_LIBS)

AC_OUTPUT([
Makefile
data/Makefile
data/fonts/Makefile
data/icons/Makefile
data/icons/buttons/Makefile
data/icons/filetypes/Makefile
data/icons/headers/Makefile
data/icons/hints/Makefile
data/icons/locale/Makefile
data/icons/movieplayer/Makefile
data/icons/radar/Makefile
data/icons/slider/Makefile
data/icons/status/Makefile
data/icons/status/ca/Makefile
data/icons/status/channel/Makefile
data/icons/status/markers/Makefile
data/icons/status/various/Makefile
data/icons/various/Makefile
data/inetradio/Makefile
data/initial/Makefile
data/iso-codes/Makefile
data/lcd/Makefile
data/lcd/clock/Makefile
data/lcd/icons/Makefile
data/license/Makefile
data/locale/Makefile
data/neutrino-scripts/Makefile
data/pictures/Makefile
data/pictures/backgrounds/Makefile
data/pictures/screensaver/Makefile
data/scripts/Makefile
data/themes/Makefile
data/y-web/Makefile
data/y-web/images/Makefile
data/y-web/languages/Makefile
data/y-web/scripts/Makefile
data/y-web/styles/Makefile
lib/Makefile
lib/connection/Makefile
lib/hardware/coolstream/Makefile
lib/jsoncpp/Makefile
lib/libconfigfile/Makefile
lib/libdvbsub/Makefile
lib/libeventserver/Makefile
lib/libiw/Makefile
lib/libmd5sum/Makefile
lib/libnet/Makefile
lib/libtuxtxt/Makefile
lib/libupnpclient/Makefile
lib/luaclient/Makefile
lib/sectionsdclient/Makefile
lib/timerdclient/Makefile
lib/xmltree/Makefile
src/Makefile
src/daemonc/Makefile
src/driver/Makefile
src/driver/audiodec/Makefile
src/driver/pictureviewer/Makefile
src/eitd/Makefile
src/gui/Makefile
src/gui/bedit/Makefile
src/gui/components/Makefile
src/gui/lua/Makefile
src/gui/moviebrowser/Makefile
src/gui/widget/Makefile
src/lcddisplay/Makefile
src/nhttpd/Makefile
src/nhttpd/nhttpd.conf
src/nhttpd/tuxboxapi/Makefile
src/nhttpd/yhttpd_core/Makefile
src/nhttpd/yhttpd_mods/Makefile
src/system/Makefile
src/system/mtdutils/Makefile
src/system/mtdutils/lib/Makefile
src/timerd/Makefile
src/zapit/Makefile
src/zapit/data/Makefile
src/zapit/lib/Makefile
src/zapit/src/Makefile
])
