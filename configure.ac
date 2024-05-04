# explicit defines for separate revision handling
define(ver_major, 3)
define(ver_minor, 24)
define(ver_micro, 0)
define(rel_cycle, "ver_major.0")

# sync with current git
define(ver_git, m4_esyscmd([
	GITBRANCH=$(git rev-parse --abbrev-ref HEAD);
	GITDESCRIBE=$(git describe --always --tags --dirty);
	printf "$GITDESCRIBE $GITBRANCH"
]))

define(ver_date, m4_esyscmd([
	BUILTDATE=$(date);
	printf "$BUILTDATE"
]))

AC_INIT(Tuxbox-Neutrino,ver_major.ver_minor.ver_micro)
AM_INIT_AUTOMAKE([nostdinc foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_DEFINE(PACKAGE_RELEASE_CYCLE, rel_cycle, [Release cycle information; used in update code; only relevant for flash image releases ])

AC_DEFINE(PACKAGE_VERSION_MAJOR, ver_major, [Major version number])
AC_DEFINE(PACKAGE_VERSION_MINOR, ver_minor, [Minor version number])
AC_DEFINE(PACKAGE_VERSION_MICRO, ver_micro, [Micro version number])
AC_DEFINE(PACKAGE_VERSION_GIT, "ver_git", [internal vcs version info])
AC_DEFINE(PACKAGE_VERSION_DATE, "ver_date", [internal built date])

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY
TUXBOX_APPS_PKGCONFIG
TUXBOX_BOXTYPE

AC_PROG_CC
AC_PROG_CXX
AC_DISABLE_STATIC
LT_INIT

# Set default for ARFLAGS, since autoconf does not have a macro for it.
# This allows people to set it when running configure or make
test -n "$ARFLAGS" || ARFLAGS="cr"
AC_SUBST(ARFLAGS)

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
	AS_HELP_STRING([--enable-ffmpegdec], [enable ffmpeg decoder support @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_FFMPEGDEC, 1, [include ffmpeg decoder support]))
AM_CONDITIONAL(ENABLE_FFMPEGDEC, test "$enable_ffmpegdec" = "yes")

if test "$enable_ffmpegdec" != "yes"; then

AC_ARG_WITH(tremor,
	AS_HELP_STRING([--with-tremor], [use libvorbisidec instead of libogg/libvorbis @<:@default=no@:>@]),
	[TREMOR="$withval"],
	[TREMOR=no])

AC_ARG_WITH(tremor-static,
	AS_HELP_STRING([--with-tremor-static], [use statically linked libvorbisidec instead of libogg/libvorbis @<:@default=no@:>@]),
	[TREMOR_STATIC="$withval"],
	[TREMOR_STATIC=no])

if test "$TREMOR_STATIC" = "yes"; then
	TREMOR=yes
fi

if test "$TREMOR" = "yes"; then
	TUXBOX_APPS_LIB_PKGCONFIG(VORBISIDEC,vorbisidec)
	AC_DEFINE(USE_TREMOR, 1, use libvorbisidec/tremor library)
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

fi # test "$enable_ffmpegdec"

TUXBOX_APPS_LIB_PKGCONFIG(SWRESAMPLE,libswresample)

AM_CONDITIONAL(USE_TREMOR, test "$TREMOR" = "yes")

# TUXBOX_APPS_LIB_PKGCONFIG(OPENSSL,openssl)
TUXBOX_APPS_LIB_PKGCONFIG(CURL,libcurl)

## For the check you must use the corresponding libtool number as version number,
## see freetype source code docs/VERSIONS.TXT
PKG_CHECK_MODULES([FREETYPE], [freetype2 >= 16.2.10], [
	echo "freetype2 >= 2.5.0 found"
], [
	echo "freetype2 >= 2.5.0 not found, use alternative search method with freetype-config"
	#AC_MSG_ERROR([freetype2 >= 2.5.0 not found])
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
	], [
		AC_MSG_RESULT(yes)
	], [
		AC_MSG_ERROR([Need FreeType library version 2.5.0 or higher])
	])
fi

TUXBOX_APPS_LIB_PKGCONFIG(PNG,libpng)
TUXBOX_APPS_LIB_PKGCONFIG(AVFORMAT,libavformat)
TUXBOX_APPS_LIB_PKGCONFIG(AVCODEC,libavcodec)
TUXBOX_APPS_LIB_PKGCONFIG(AVUTIL,libavutil)
TUXBOX_APPS_LIB_PKGCONFIG(SIGC,sigc++-2.0)

if test "$BOXTYPE" = "armbox" -o "$BOXTYPE" = "mipsbox"; then
	TUXBOX_APPS_LIB_PKGCONFIG(SWSCALE,libswscale)
fi

if test "$BOXTYPE" = "coolstream"; then
	TUXBOX_APPS_LIB_PKGCONFIG(BLURAY,libbluray)
fi

if test "$BOXTYPE" = "generic" ||
   test "$BOXTYPE" = "coolstream" ||
   test "$BOXTYPE" = "armbox" ||
   test "$BOXTYPE" = "mipsbox"; then
	AC_DEFINE(SCREENSHOT, 1, [Define to 1 if the platform supports screenshots])
fi

AC_ARG_ENABLE(lcd4linux,
	AS_HELP_STRING([--enable-lcd4linux], [enable LCD4Linux support @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_LCD4LINUX, 1, [enable LCD4Linux support]))
AM_CONDITIONAL(ENABLE_LCD4LINUX, test "$enable_lcd4linux" = "yes")

# lcd
AC_ARG_ENABLE(lcd,
	AS_HELP_STRING([--enable-lcd], [enable internal lcd support @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_LCD, 1, [enable internal lcd support]))
AM_CONDITIONAL(ENABLE_LCD, test "$enable_lcd" = "yes")

# lcd update
if test "$enable_lcd" = "yes"; then
	AC_DEFINE(LCD_UPDATE, 1, [include internal lcd update support])
fi
AM_CONDITIONAL(LCD_UPDATE, test "$enable_lcd" = "yes")

AC_ARG_ENABLE(graphlcd,
	AS_HELP_STRING([--enable-graphlcd], [enable GraphLCD support @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_GRAPHLCD, 1, [enable GraphLCD support]))
AM_CONDITIONAL(ENABLE_GRAPHLCD, test "$enable_graphlcd" = "yes")

# weather
AC_ARG_WITH(weather-api-key,
	AS_HELP_STRING([--with-weather-api-key=KEY], [API key to get data from weather data base, required for additional weather informations]),
	[WEATHER_API_KEY="$withval"],
	[WEATHER_API_KEY=""])
AC_DEFINE_UNQUOTED([WEATHER_API_KEY], ["$WEATHER_API_KEY"], [API key to get data from weather data base, required for additional weather informations])

AC_ARG_WITH(weather-api-version,
	AS_HELP_STRING([--with-weather-api-version=VERSION], [API version to get data from weather data base, required for additional weather informations]),
	[WEATHER_API_VERSION="$withval"],
	[WEATHER_API_VERSION=""])
AC_DEFINE_UNQUOTED([WEATHER_API_VERSION], ["$WEATHER_API_VERSION"], [API version to get data from weather data base, required for additional weather informations])

AC_ARG_ENABLE([weather-key-manage],
	AS_HELP_STRING([--enable-weather-key-manage], [enable manage weather api key via gui for additional weather informations @<:@default=yes@:>@]),
	[enable_weather_key_manage="$enableval"],
	[enable_weather_key_manage="yes"])

if test "$enable_weather_key_manage" = "yes" ; then
	AC_DEFINE([ENABLE_WEATHER_KEY_MANAGE], 1, [enable manage weather api key via gui for additional weather informations])
fi
# weather end

# tmdb
AC_ARG_WITH(tmdb-dev-key,
	AS_HELP_STRING([--with-tmdb-dev-key=KEY], [API dev key to get data from tmdb data base, required for additional movie informations]),
	[TMDB_DEV_KEY="$withval"],
	[TMDB_DEV_KEY=""])
AC_DEFINE_UNQUOTED([TMDB_DEV_KEY], ["$TMDB_DEV_KEY"], [API dev key to get data from tmdb data base, required for additional movie informations])

AC_ARG_ENABLE([tmdb-key-manage],
	AS_HELP_STRING([--enable-tmdb-key-manage], [enable manage tmdb api dev key via gui for additional movie informations @<:@default=yes@:>@]),
	[enable_tmdb_key_manage="$enableval"],
	[enable_tmdb_key_manage="yes"])

if test "$enable_tmdb_key_manage" = "yes" ; then
	AC_DEFINE([ENABLE_TMDB_KEY_MANAGE], 1, [enable manage tmdb api dev key via gui for additional movie informations])
fi
# tmdb end

# omdb
AC_ARG_WITH(omdb-api-key,
	AS_HELP_STRING([--with-omdb-api-key=KEY], [API key to get additional epg data from omdb database at http://www.omdbapi.com, this database gets data from imdb service]),
	[OMDB_API_KEY="$withval"],
	[OMDB_API_KEY=""])
AC_DEFINE_UNQUOTED([OMDB_API_KEY], ["$OMDB_API_KEY"], [API key to get additional epg data from omdb database at http://www.omdbapi.com, this database gets data from imdb service])

AC_ARG_ENABLE([omdb-key-manage],
	AS_HELP_STRING([--enable-omdb-key-manage], [enable manage omdb (imdb) api dev key via gui for additional movie informations @<:@default=yes@:>@]),
	[enable_omdb_key_manage="$enableval"],
	[enable_omdb_key_manage="yes"])

if test "$enable_omdb_key_manage" = "yes" ; then
	AC_DEFINE([ENABLE_OMDB_KEY_MANAGE], 1, [enable manage omdb (imdb) api dev key via gui for additional movie informations])
fi
# omdb end

# shoutcast
AC_ARG_WITH(shoutcast-dev-key,
	AS_HELP_STRING([--with-shoutcast-dev-key=KEY], [API dev key to get stream data lists from ShoutCast service]),
	[SHOUTCAST_DEV_KEY="$withval"],
	[SHOUTCAST_DEV_KEY=""])
AC_DEFINE_UNQUOTED([SHOUTCAST_DEV_KEY], ["$SHOUTCAST_DEV_KEY"], [API dev key to get stream data lists from ShoutCast service])

AC_ARG_ENABLE([shoutcast-key-manage],
	AS_HELP_STRING([--enable-shoutcast-key-manage], [enable manage of api dev key to get stream data lists from ShoutCast service via gui @<:@default=yes@:>@]),
	[enable_shoutcast_key_manage="$enableval"],
	[enable_shoutcast_key_manage="yes"])

if test "$enable_shoutcast_key_manage" = "yes" ; then
	AC_DEFINE([ENABLE_SHOUTCAST_KEY_MANAGE], 1, [enable manage of api dev key to get stream data lists from ShoutCast service via gui])
fi
# shoutcast end

# youtube
AC_ARG_WITH(youtube-dev-key,
	AS_HELP_STRING([--with-youtube-dev-key=KEY], [API dev key for YouTube streaming]),
	[YOUTUBE_DEV_KEY="$withval"],
	[YOUTUBE_DEV_KEY=""])
AC_DEFINE_UNQUOTED([YOUTUBE_DEV_KEY], ["$YOUTUBE_DEV_KEY"], [API dev key for YouTube streaming])

AC_ARG_ENABLE([youtube-key-manage],
	AS_HELP_STRING([--enable-youtube-key-manage], [enable manage YouTube dev key via gui @<:@default=yes@:>@]),
	[enable_youtube_key_manage="$enableval"],
	[enable_youtube_key_manage="yes"])

if test "$enable_youtube_key_manage" = "yes" ; then
	AC_DEFINE([ENABLE_YOUTUBE_KEY_MANAGE], 1, [enable manage YouTube dev key via gui])
fi
# youtube end

AC_ARG_ENABLE(keyboard-no-rc,
	AS_HELP_STRING([--enable-keyboard-no-rc], [enable keyboard control, disable rc control @<:@default=no@:>@]),
	AC_DEFINE(KEYBOARD_INSTEAD_OF_REMOTE_CONTROL, 1, [enable keyboard control, disable rc control]))

AC_ARG_ENABLE(mdev,
	AS_HELP_STRING([--enable-mdev], [use mdev instead of broken neutrino mount hack @<:@default=no@:>@]),
	AC_DEFINE(ASSUME_MDEV, 1, [use mdev instead of broken neutrino mount hack]))

AC_ARG_ENABLE(freesatepg,
	AS_HELP_STRING([--enable-freesatepg], [enable Freesat EPG code @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_FREESATEPG, 1, [enable Freesat EPG code]))

AC_ARG_ENABLE(viasatepg,
	AS_HELP_STRING([--enable-viasatepg], [enable ViaSat EPG code @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_VIASATEPG, 1, [enable ViaSat EPG code]))

AC_ARG_ENABLE(fastscan,
	AS_HELP_STRING([--enable-fastscan], [enable fastscan code @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_FASTSCAN, 1, [enable fastscan code]))
AM_CONDITIONAL(ENABLE_FASTSCAN, test "$enable_fastscan" = "yes")

AC_ARG_ENABLE(aitscan,
	AS_HELP_STRING([--enable-aitscan], [enable Application Information Table support @<:@default=yes@:>@]),
	[enable_aitscan="$enableval"],
	[enable_aitscan="yes"])

if test "$enable_aitscan" = "yes"; then
	AC_DEFINE(ENABLE_AITSCAN, 1, [enable Application Information Table support])
fi
AM_CONDITIONAL(ENABLE_AITSCAN, test "$enable_aitscan" = "yes")

AC_ARG_ENABLE(giflib,
	AS_HELP_STRING([--enable-giflib], [use giflib instead of libungif @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_GIFLIB, 1, [use giflib instead of libungif]))
AM_CONDITIONAL(ENABLE_GIFLIB, test "$enable_giflib" = "yes")

AC_ARG_ENABLE(pugixml,
	AS_HELP_STRING([--enable-pugixml], [use pugixml instead of xmltree @<:@default=no@:>@]),
	AC_DEFINE(USE_PUGIXML, 1, [use pugixml instead of xmltree]))
AM_CONDITIONAL(USE_PUGIXML, test "$enable_pugixml" = "yes")

AC_ARG_ENABLE(flac,
	AS_HELP_STRING([--enable-flac], [enable FLAC support @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_FLAC, 1, [enable FLAC support]))
AM_CONDITIONAL(ENABLE_FLAC, test "$enable_flac" = "yes")

AC_ARG_ENABLE(upnp,
	AS_HELP_STRING([--enable-upnp], [enable UPNP support @<:@default=yes@:>@]),
	[enable_upnp="$enableval"],
	[enable_upnp="yes"])

if test "$enable_upnp" = "yes"; then
	AC_DEFINE(ENABLE_UPNP, 1, [enable UPNP support])
fi
AM_CONDITIONAL(ENABLE_UPNP, test "$enable_upnp" = "yes")

AC_ARG_ENABLE(arm-acc,
	AS_HELP_STRING([--enable-arm-acc], [enable arm hardware acceleration @<:@default=yes@:>@]]),
	[enable_arm_acc="$enableval"],
	[enable_arm_acc="yes"])

if test "$enable_arm_acc" = "yes"; then
	AC_DEFINE(ENABLE_ARM_ACC, 1, [enable arm hardware acceleration])
fi
AM_CONDITIONAL(ENABLE_ARM_ACC, test "$enable_arm_acc" = "yes")

AC_ARG_ENABLE(mips-acc,
	AS_HELP_STRING([--enable-mips-acc], [enable mips hardware acceleration @<:@default=yes@:>@]]),
	[enable_mips_acc="$enableval"],
	[enable_mips_acc="yes"])

if test "$enable_mips_acc" = "yes"; then
	AC_DEFINE(ENABLE_MIPS_ACC, 1, [enable mips hardware acceleration])
fi
AM_CONDITIONAL(ENABLE_MIPS_ACC, test "$enable_mips_acc" = "yes")

AC_ARG_ENABLE(extupdate,
	AS_HELP_STRING([--enable-extupdate], [enable extended update routine @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_EXTUPDATE, 1, [enable extended update routine]))
AM_CONDITIONAL(ENABLE_EXTUPDATE, test "$enable_extupdate" = "yes")

AC_ARG_ENABLE(pkg-management,
	AS_HELP_STRING([--enable-pkg-management], [enable gui package management @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_PKG_MANAGEMENT, 1, [enable package management]))
AM_CONDITIONAL(ENABLE_PKG_MANAGEMENT, test "$enable_pkg_management" = "yes")

AC_ARG_ENABLE(lua,
	AS_HELP_STRING([--enable-lua], [enable Lua support @<:@default=yes@:>@]),
	[enable_lua="$enableval"],
	[enable_lua="yes"])

if test "$enable_lua" = "yes"; then
	AC_DEFINE(ENABLE_LUA, 1, [include Lua support])
	PKG_CHECK_MODULES(LUA, lua, LLUA="yes", LLUA="no")
	PKG_CHECK_MODULES(luajit, luajit, LLUAJIT="yes", LLUAJIT="no")
		if test "x$LLUAJIT" = "xyes"; then
			LUA_LIBS="-lluajit-5.1"
			AC_DEFINE(LUA_COMPAT_5_2, 1, [needed for build with lua 5.1])
		elif test "x$LLUA" = "xyes"; then
			LUA_LIBS="-llua"
			AC_DEFINE(LUA_COMPAT_5_2, 0, [needed for build with lua 5.1])
		else
			echo "lualib not found, assuming static lua in linker path..."
			LUA_LIBS="-llua -ldl"
			AC_DEFINE(STATIC_LUAPOSIX, 1, [Define to 1 for static lua build.])
			AC_DEFINE(LUA_COMPAT_5_2, 0, [needed for build with lua 5.1])
		fi
fi
AM_CONDITIONAL(ENABLE_LUA, test "$enable_lua" = "yes")

AC_ARG_ENABLE(cleanup,
	AS_HELP_STRING([--enable-cleanup], [enable cleanup on exit @<:@default=no@:>@]),
	AC_DEFINE(EXIT_CLEANUP, 1, [enable cleanup on exit]))

AC_ARG_ENABLE(pip,
	AS_HELP_STRING([--enable-pip], [enable picture in picture support @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_PIP, 1, [enable picture in picture support]))
AM_CONDITIONAL(ENABLE_PIP, test "$enable_pip" = "yes")

case "$BOXMODEL" in
	vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse)
		AC_ARG_ENABLE(quadpip,
			AS_HELP_STRING([--enable-quadpip], [enable quad picture in picture support @<:@default=no@:>@]),
			AC_DEFINE(ENABLE_QUADPIP, 1, [enable quad picture in picture support]))

		AS_IF(test "$enable_quadpip" = "yes" -a "$enable_pip" != "yes",
			AC_MSG_ERROR([--enable-quadpip depends on --enable-pip]))
	;;
esac
AM_CONDITIONAL(ENABLE_QUADPIP, test "$enable_quadpip" = "yes")

AC_ARG_ENABLE(dynamicdemux,
	AS_HELP_STRING([--enable-dynamicdemux], [enable dynamic demux @<:@default=no@:>@]),
	AC_DEFINE(DYNAMIC_DEMUX, 1, [enable dynamic demux]))

AC_ARG_ENABLE(testing,
	AS_HELP_STRING([--enable-testing], [enable development code @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_TESTING, 1, [enable development code]))
AM_CONDITIONAL(ENABLE_TESTING, test "$enable_testing" = "yes")

AC_ARG_ENABLE(fribidi,
	AS_HELP_STRING([--enable-fribidi], [enable fribidi support @<:@default=no@:>@]),
	AC_DEFINE(ENABLE_FRIBIDI, 1, [enable fribidi support]))
AM_CONDITIONAL(ENABLE_FRIBIDI, test "$enable_fribidi" = "yes")

# default theme
AC_ARG_WITH(default-theme,
	AS_HELP_STRING([--with-default-theme=THEMENAME], [default theme for gui @<:@default is empty@:>@]),
	[default_theme="$withval"],
	[default_theme=""])
AC_DEFINE_UNQUOTED([DEFAULT_THEME], ["$default_theme"], [default theme for gui])

# default oled theme
AC_ARG_WITH(default-oled-theme,
	AS_HELP_STRING([--with-default-oled-theme=THEMENAME], [default theme for oled @<:@default is empty@:>@]),
	[default_oled_theme="$withval"],
	[default_oled_theme=""])
AC_DEFINE_UNQUOTED([DEFAULT_OLED_THEME], ["$default_oled_theme"], [default theme for oled])

AC_ARG_ENABLE(change-osd-resolution,
	AS_HELP_STRING([--enable-change-osd-resolution], [enable to change osd resolution @<:@default=yes@:>@]]),
	[enable_change_osd_resolution="$enableval"],
	[enable_change_osd_resolution="yes"])

# BOXMODELs that *not* allows to change osd resolution
case "$BOXMODEL" in
	hd1)
		if test "$enable_change_osd_resolution" = "yes"; then
			AC_MSG_WARN([--enable-change-osd-resolution=yes is not allowed for boxmodel $BOXMODEL])
			enable_change_osd_resolution="no"
		fi
	;;
esac

if test "$enable_change_osd_resolution" = "yes"; then
	AC_DEFINE(ENABLE_CHANGE_OSD_RESOLUTION, 1, [enable to change osd resolution])
fi
AM_CONDITIONAL(ENABLE_CHANGE_OSD_RESOLUTION, test "$enable_change_osd_resolution" = "yes")

# webif content
AC_ARG_WITH(webif,
	AS_HELP_STRING([--without-webif], [disable installation of webif content within defined public httpd path provided by yWeb @<:@default it is enabled@:>@]),
	[ENABLE_WEBIF="$withval"],
	[ENABLE_WEBIF="yes"])
AM_CONDITIONAL(ENABLE_WEBIF,test "$ENABLE_WEBIF" = "yes")
if test "$ENABLE_WEBIF" = "yes"; then
	AC_DEFINE(ENABLE_WEBIF, 1, [Enable installation of webif content within defined public httpd path provided by yWeb.])
fi

AC_ARG_WITH(stb-hal-includes,
	AS_HELP_STRING([--with-stb-hal-includes=PATH], [path to libstb-hal includes [[NONE]]]),
	[STB_HAL_INC="$withval"],
	[STB_HAL_INC=""])

AC_ARG_WITH(stb-hal-build,
	AS_HELP_STRING([--with-stb-hal-build=PATH], [path where libstb-hal is built [[NONE]]]),
	[STB_HAL_LIB="-L$withval"],
	[STB_HAL_LIB=""])

AC_ARG_WITH(libcoolstream-static-dir,
	AS_HELP_STRING([--with-libcoolstream-static-dir=PATH], [path to static libcoolstream [[NONE]]]),
	[LIBCOOLSTREAM_STATIC_DIR="$withval"],
	[LIBCOOLSTREAM_STATIC_DIR=""])

AC_ARG_ENABLE(libcoolstream-static,
	AS_HELP_STRING([--enable-libcoolstream-static], [libcoolstream static linked for testing @<:@default=no@:>@]))
AM_CONDITIONAL(ENABLE_LIBCOOLSTREAM_STATIC, test "$enable_libcoolstream_static" = "yes")

# BOXTYPEs that use libstb-hal
case "$BOXTYPE" in
	coolstream)
		libstb_hal=no
	;;
	*)
		libstb_hal=yes
		AC_DEFINE(HAVE_LIBSTB_HAL, 1, [use libstb-hal])
	;;
esac
AM_CONDITIONAL(HAVE_LIBSTB_HAL, test "$libstb_hal" = "yes")

if test "$libstb_hal" = "yes"; then
	if test "$STB_HAL_INC" = ""; then
		AC_MSG_ERROR([need libstb-hal includes path with --with-stb-hal-includes=...])
	fi
	HWLIB_CFLAGS="-I$STB_HAL_INC"
else
	if test "$BOXMODEL" = "hd1"; then
		if test -e ${srcdir}/lib/hardware/coolstream/hd1/libcoolstream/nevis_ir.h; then
			AC_DEFINE(HAVE_COOLSTREAM_NEVIS_IR_H, 1, [Define to 1 if you have the <nevis_ir.h> header file.])
		fi
		HWLIB_CFLAGS='-I$(top_srcdir)/lib/hardware/coolstream/hd1/libcoolstream'
	elif test "$BOXMODEL" = "hd2"; then
		if test -e ${srcdir}/lib/hardware/coolstream/hd2/libcoolstream/cs_ir_generic.h; then
			AC_DEFINE(HAVE_COOLSTREAM_CS_IR_GENERIC_H, 1, [Define to 1 if you have the <cs_ir_generic.h> header file.])
		fi
		if test -e ${srcdir}/lib/hardware/coolstream/hd2/libcoolstream/cs_frontpanel.h; then
			AC_DEFINE(HAVE_COOLSTREAM_CS_FRONTPANEL_H, 1, [Define to 1 if you have the <cs_frontpanel.h> header file.])
		fi
		HWLIB_CFLAGS='-I$(top_srcdir)/lib/hardware/coolstream/hd2/libcoolstream'
	fi
	HWLIB_CFLAGS="$HWLIB_CFLAGS "'-I$(top_srcdir)/lib/hardware/coolstream'
fi

# uclibc
AM_CONDITIONAL(HAVE_UCLIBC, $CC --version | grep -q "uclibc")

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
AC_SUBST(LIBCOOLSTREAM_STATIC_DIR)

# Makefiles
AC_CONFIG_FILES([
Makefile
data/Makefile
data/config/Makefile
data/control/Makefile
data/fonts/Makefile
data/icons/Makefile
data/icons/buttons/Makefile
data/icons/filetypes/Makefile
data/icons/headers/Makefile
data/icons/hints/Makefile
data/icons/hourglass/Makefile
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
data/icons/weather/Makefile
data/inetradio/Makefile
data/initial/Makefile
data/iso-codes/Makefile
data/lcd/Makefile
data/lcd/clock/Makefile
data/lcd/icons/Makefile
data/lcd4l/Makefile
data/license/Makefile
data/locale/Makefile
data/luaplugins/Makefile
data/oled/Makefile
data/oled/fonts/Makefile
data/oled/icons/Makefile
data/oled/icons/clock/Makefile
data/oled/icons/weather/Makefile
data/oled/themes/Makefile
data/oled/themes/220x176/Makefile
data/oled/themes/400x240/Makefile
data/oled/themes/480x320/Makefile
data/oled/themes/800x480/Makefile
data/pictures/Makefile
data/pictures/backgrounds/Makefile
data/pictures/screensaver/Makefile
data/policy/Makefile
data/scripts/Makefile
data/themes/Makefile
data/themes/Grey-3.0/Makefile
data/themes/Grey-3.0/icons/Makefile
data/themes/Grey-3.0/icons/buttons/Makefile
data/themes/Grey-Blue/Makefile
data/themes/Grey-Blue/icons/Makefile
data/themes/Grey-Blue/icons/buttons/Makefile
data/themes/Neutrino-3.0/Makefile
data/themes/Neutrino-3.0/icons/Makefile
data/themes/Neutrino-3.0/icons/buttons/Makefile
data/themes/Olive-3.0/Makefile
data/themes/Olive-3.0/icons/Makefile
data/themes/Olive-3.0/icons/buttons/Makefile
data/weather/Makefile
data/webradio/Makefile
data/webtv/Makefile
data/y-web/Makefile
data/y-web/images/Makefile
data/y-web/languages/Makefile
data/y-web/scripts/Makefile
data/y-web/styles/Makefile
data/zapit/Makefile
lib/Makefile
lib/connection/Makefile
lib/hardware/coolstream/Makefile
lib/jsoncpp/Makefile
lib/libconfigfile/Makefile
lib/liblcddisplay/Makefile
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
src/driver/glcd/Makefile
src/driver/pictureviewer/Makefile
src/eitd/Makefile
src/gui/Makefile
src/gui/bedit/Makefile
src/gui/components/Makefile
src/gui/lua/Makefile
src/gui/moviedb/Makefile
src/gui/moviebrowser/Makefile
src/gui/widget/Makefile
src/gui/widget/yaft/Makefile
src/nhttpd/Makefile
src/nhttpd/tuxboxapi/Makefile
src/nhttpd/yhttpd_core/Makefile
src/nhttpd/yhttpd_mods/Makefile
src/system/Makefile
src/system/mtdutils/Makefile
src/system/mtdutils/lib/Makefile
src/timerd/Makefile
src/zapit/Makefile
src/zapit/lib/Makefile
src/zapit/src/Makefile
])

# Webserver config
AC_CONFIG_FILES([
src/nhttpd/nhttpd.conf
])

## Migration script
#AC_CONFIG_FILES([
#data/control/migration.sh
#])

# Backup scripts
AC_CONFIG_FILES([
data/scripts/backup.sh
data/scripts/restore.sh
data/scripts/backup_flash.sh
data/scripts/restore_flash.sh
])

if test "$BOXTYPE" = "generic"; then
	AC_CONFIG_FILES([
	data/scripts/neutrino.sh
	])
fi

# Backup configs
AC_CONFIG_FILES([
data/config/settingsupdate.conf
data/config/tobackup.conf
])

# Themes
#AC_CONFIG_FILES([
#data/themes/Material_Blue-Grey.theme
#])

# Oled themes
AC_CONFIG_FILES([
data/oled/themes/220x176/Default.otheme
data/oled/themes/400x240/Default.otheme
data/oled/themes/480x320/Default.otheme
data/oled/themes/480x320/Advanced.otheme
data/oled/themes/480x320/Easy.otheme
data/oled/themes/480x320/Medium.otheme
data/oled/themes/480x320/Simple.otheme
data/oled/themes/480x320/Weather.otheme
data/oled/themes/800x480/Default.otheme
])

AC_OUTPUT
