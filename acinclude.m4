AC_DEFUN([TUXBOX_APPS], [
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_GNU_SOURCE

AC_ARG_WITH(target,
	AS_HELP_STRING([--with-target=TARGET], [target for compilation [[native,cdk]]]),
	[TARGET="$withval"],
	[TARGET="native"])

AC_ARG_WITH(targetprefix,
	AS_HELP_STRING([--with-targetprefix=PATH], [prefix relative to target root (only applicable in cdk mode)]),
	[TARGET_PREFIX="$withval"],
	[TARGET_PREFIX=""])

AC_ARG_WITH(debug,
	AS_HELP_STRING([--without-debug], [disable debugging code @<:@default=no@:>@]),
	[DEBUG="$withval"],
	[DEBUG="yes"])

if test "$DEBUG" = "yes"; then
	DEBUG_CFLAGS="-g3 -ggdb"
	AC_DEFINE(DEBUG, 1, [enable debugging code])
fi

# weather
AC_ARG_WITH(weather-dev-key,
	AS_HELP_STRING([--with-weather-dev-key=KEY], [API dev key to get data from weather data base, required for additional weather informations]),
	[WEATHER_DEV_KEY="$withval"],
	[WEATHER_DEV_KEY=""])
AC_DEFINE_UNQUOTED([WEATHER_DEV_KEY], ["$WEATHER_DEV_KEY"], [API dev key to get data from weather data base, required for additional weather informations])

AC_ARG_ENABLE([weather-key-manage],
	AS_HELP_STRING([--enable-weather-key-manage], [Enable manage weather api dev key via gui for additional weather informations @<:@default=yes@:>@]),
	[enable_weather_key_manage="$enableval"],
	[enable_weather_key_manage="yes"])

if test "$enable_weather_key_manage" = "yes" ; then
	AC_DEFINE([ENABLE_WEATHER_KEY_MANAGE], 1, [Enable manage weather api dev key via gui for additional weather informations])
fi
# weather end

# tmdb
AC_ARG_WITH(tmdb-dev-key,
	AS_HELP_STRING([--with-tmdb-dev-key=KEY], [API dev key to get data from tmdb data base, required for additional epg informations.]),
	[TMDB_DEV_KEY="$withval"],
	[TMDB_DEV_KEY=""])
AC_DEFINE_UNQUOTED([TMDB_DEV_KEY], ["$TMDB_DEV_KEY"], [API dev key to get data from tmdb data base, required for additional epg informations.])


AC_ARG_ENABLE([tmdb-key-manage],
	[AS_HELP_STRING([--enable-tmdb-key-manage], [Enable manage tmdb api dev key via gui for additional epg informations. @<:@default=yes@:>@])],
	[enable_tmdb_key_manage="$enableval"],
	[enable_tmdb_key_manage="yes"])
tmdb_key_manage_val=1
if test "$enable_tmdb_key_manage" = "no" ; then
	tmdb_key_manage_val=0
fi
AC_DEFINE_UNQUOTED([ENABLE_TMDB_KEY_MANAGE],[$tmdb_key_manage_val],[Enable manage tmdb api dev key via gui for additional epg informations.])
# end tmdb


# omdb
AC_ARG_WITH(omdb-api-key,
	AS_HELP_STRING([--with-omdb-api-key=KEY], [API key to get additional epg data from omdb database at http://www.omdbapi.com, this database gets data from imdb service.]),
	[OMDB_API_KEY="$withval"],
	[OMDB_API_KEY=""])
AC_DEFINE_UNQUOTED([OMDB_API_KEY], ["$OMDB_API_KEY"], [API key to get additional epg data from omdb database at http://www.omdbapi.com, this database gets data from imdb service.])

AC_ARG_ENABLE([omdb-key-manage],
	[AS_HELP_STRING([--enable-omdb-key-manage], [Enable manage omdb (imdb) api dev key via gui for additional epg informations. @<:@default=yes@:>@])],
	[enable_omdb_key_manage="$enableval"],
	[enable_omdb_key_manage="yes"])
omdb_key_manage_val=1
if test "$enable_omdb_key_manage" = "no" ; then
	omdb_key_manage_val=0
fi
AC_DEFINE_UNQUOTED([ENABLE_OMDB_KEY_MANAGE],[$omdb_key_manage_val],[Enable manage omdb (imdb) api dev key via gui for additional epg informations.])
# end omdb


# youtube
AC_ARG_WITH(youtube-dev-key,
	AS_HELP_STRING([--with-youtube-dev-key=KEY], [API dev key for YouTube streaming.]),
	[YT_DEV_KEY="$withval"],
	[YT_DEV_KEY=""])
AC_DEFINE_UNQUOTED([YT_DEV_KEY], ["$YT_DEV_KEY"], [API dev key for YouTube streaming.])

AC_ARG_ENABLE([youtube-key-manage],
	[AS_HELP_STRING([--enable-youtube-key-manage], [Enable manage youtube dev key via gui for additional epg informations. @<:@default=yes@:>@])],
	[enable_youtube_key_manage="$enableval"],
	[enable_youtube_key_manage="yes"])
youtube_key_manage_val=1
if test "$enable_youtube_key_manage" = "no" ; then
	youtube_key_manage_val=0
fi
AC_DEFINE_UNQUOTED([ENABLE_YOUTUBE_KEY_MANAGE],[$youtube_key_manage_val], [Enable manage youtube dev key via gui for additional epg informations.])

AC_ARG_ENABLE([youtube-player],
	[AS_HELP_STRING([--enable-youtube-player], [Enable play and control youtube streams with moviebrowser. @<:@default=yes@:>@])],
	[enable_youtube_player="$enableval"],
	[enable_youtube_player="yes"])
youtube_player_val=1
if test "$enable_youtube_player" = "no" ; then
	youtube_player_val=0
fi
AC_DEFINE_UNQUOTED([ENABLE_YOUTUBE_PLAYER],[$youtube_player_val], [Enable play and control YouTube streams with moviebrowser.])
# end youtube


# shoutcast
AC_ARG_WITH(shoutcast-dev-key,
	AS_HELP_STRING([--with-shoutcast-dev-key=KEY], [API dev key to get stream data lists from ShoutCast service.]),
	[SHOUTCAST_DEV_KEY="$withval"],
	[SHOUTCAST_DEV_KEY=""])
AC_DEFINE_UNQUOTED([SHOUTCAST_DEV_KEY], ["$SHOUTCAST_DEV_KEY"], [API dev key to get stream data lists from ShoutCast service.])

AC_ARG_ENABLE([shoutcast-key-manage],
	[AS_HELP_STRING([--enable-shoutcast-key-manage], [Enable manage of api dev key to get stream data lists from ShoutCast service via gui. @<:@default=yes@:>@])],
	[enable_shoutcast_key_manage="$enableval"],
	[enable_shoutcast_key_manage="yes"])
shoutcast_key_manage_val=1
if test "$enable_shoutcast_key_manage" = "no" ; then
	shoutcast_key_manage_val=0
fi
AC_DEFINE_UNQUOTED([ENABLE_SHOUTCAST_KEY_MANAGE],[$shoutcast_key_manage_val], [Enable manage of api dev key to get stream data lists from ShoutCast service via gui.])
# end shoutcast


AC_ARG_WITH(libcoolstream-static-dir,
	AS_HELP_STRING([--with-libcoolstream-static-dir=PATH], [path for static libcoolstream [[NONE]]]),
	[LIBCOOLSTREAM_STATIC_DIR="$withval"],
	[LIBCOOLSTREAM_STATIC_DIR=""])

AC_ARG_ENABLE(libcoolstream-static,
	AS_HELP_STRING([--enable-libcoolstream-static], [libcoolstream static linked for testing @<:@default=no@:>@]))
AM_CONDITIONAL(ENABLE_LIBCOOLSTREAM_STATIC, test "$enable_libcoolstream_static" = "yes")

AC_ARG_ENABLE(reschange,
	AS_HELP_STRING([--enable-reschange], [enable to change osd resolution]),
	AC_DEFINE(ENABLE_CHANGE_OSD_RESOLUTION, 1, [enable to change osd resolution]))
AM_CONDITIONAL(ENABLE_RESCHANGE, test "$enable_reschange" = "yes")

# default theme
AC_ARG_WITH(default-theme,
	AS_HELP_STRING([--with-default-theme=THEMENAME], [Default theme for gui. Default it is empty for internal fallback to default colors.]),
	[default_theme="$withval"],
	[default_theme=""])
AC_DEFINE_UNQUOTED([DEFAULT_THEME], ["$default_theme"], [Default theme for gui.])

# default oled theme
AC_ARG_WITH(default-oled-theme,
	AS_HELP_STRING([--with-default-oled-theme=THEMENAME], [Default theme for oled. @<:@default is empty@:>@]),
	[default_oled_theme="$withval"],
	[default_oled_theme=""])
AC_DEFINE_UNQUOTED([DEFAULT_OLED_THEME], ["$default_oled_theme"], [Default theme for oled.])

AC_MSG_CHECKING(target)

if test "$TARGET" = "native"; then
	AC_MSG_RESULT(native)

	if test "$CFLAGS" = "" -a "$CXXFLAGS" = ""; then
		CFLAGS="-Wall -O2 -pipe $DEBUG_CFLAGS"
		CXXFLAGS="-Wall -O2 -pipe $DEBUG_CFLAGS"
	fi
	if test "$prefix" = "NONE"; then
		prefix=/usr/local
	fi
	targetprefix=$prefix
	TARGET_PREFIX=$prefix
	AC_DEFINE_UNQUOTED(TARGET_PREFIX, "$TARGET_PREFIX", [The targets prefix])
elif test "$TARGET" = "cdk"; then
	AC_MSG_RESULT(cdk)

	if test "$CC" = "" -a "$CXX" = ""; then
		AC_MSG_ERROR([you need to specify variables CC or CXX in cdk])
	fi
	if test "$CFLAGS" = "" -o "$CXXFLAGS" = ""; then
		AC_MSG_ERROR([you need to specify variables CFLAGS and CXXFLAGS in cdk])
	fi
	if test "$prefix" = "NONE"; then
		AC_MSG_ERROR([invalid prefix, you need to specify one in cdk mode])
	fi
	if test "$TARGET_PREFIX" != "NONE"; then
		AC_DEFINE_UNQUOTED(TARGET_PREFIX, "$TARGET_PREFIX", [The targets prefix])
	fi
	if test "$TARGET_PREFIX" = "NONE"; then
		AC_MSG_ERROR([invalid targetprefix, you need to specify one in cdk mode])
		TARGET_PREFIX=""
	fi
else
	AC_MSG_RESULT(none)
	AC_MSG_ERROR([invalid target $TARGET, choose on from native,cdk]);
fi

if test "$exec_prefix" = "NONE"; then
	exec_prefix=$prefix
fi

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_SYS_LARGEFILE

check_path() {
	return $(perl -e "if(\"$1\"=~m#^/usr/(local/)?bin#){print \"0\"}else{print \"1\";}")
}

])

dnl expand nested ${foo}/bar
AC_DEFUN([TUXBOX_EXPAND_VARIABLE], [
	__$1="$2"
	for __CNT in false false false false true; do dnl max 5 levels of indirection
		$1=`eval echo "$__$1"`
		echo ${$1} | grep -q '\$' || break # 'grep -q' is POSIX, exit if no $ in variable
		__$1="${$1}"
	done
	$__CNT && AC_MSG_ERROR([can't expand variable $1=$2]) dnl bail out if we did not expand
])

AC_DEFUN([TUXBOX_APPS_DIRECTORY_ONE], [
AC_ARG_WITH($1, AS_HELP_STRING([$6], [$7 [[PREFIX$4$5]]], [32], [79]), [
	_$2=$withval
	if test "$TARGET" = "cdk"; then
		$2=`eval echo "$TARGET_PREFIX$withval"` # no indirection possible IMNSHO
	else
		$2=$withval
	fi
	TARGET_$2=${$2}
], [
	# RFC 1925: "you can always add another level of indirection..."
	TUXBOX_EXPAND_VARIABLE($2,"${$3}$5")
	if test "$TARGET" = "cdk"; then
		TUXBOX_EXPAND_VARIABLE(_$2,"${target$3}$5")
	else
		_$2=${$2}
	fi
	TARGET_$2=$_$2
])
dnl AC_SUBST($2)
AC_DEFINE_UNQUOTED($2,"$_$2",$7)
AC_SUBST(TARGET_$2)
])

AC_DEFUN([TUXBOX_APPS_DIRECTORY], [
AC_REQUIRE([TUXBOX_APPS])

if test "$TARGET" = "cdk"; then
	datadir="\${prefix}/share"
	sysconfdir="\${prefix}/etc"
	localstatedir="\${prefix}/var"
	libdir="\${prefix}/lib"
	mntdir="\${prefix}/mnt"
	targetdatadir="\${TARGET_PREFIX}/share"
	targetsysconfdir="\${TARGET_PREFIX}/etc"
	targetlocalstatedir="\${TARGET_PREFIX}/var"
	targetlibdir="\${TARGET_PREFIX}/lib"
	targetmntdir="\${TARGET_PREFIX}/mnt"
else
	mntdir="/mnt" # hack
fi

TUXBOX_APPS_DIRECTORY_ONE(configdir, CONFIGDIR, localstatedir, /var, /tuxbox/config,
	[--with-configdir=PATH], [where to find config files])

TUXBOX_APPS_DIRECTORY_ONE(datadir, DATADIR, datadir, /share, /tuxbox,
	[--with-datadir=PATH], [where to find data files])

TUXBOX_APPS_DIRECTORY_ONE(controldir, CONTROLDIR, datadir, /share, /tuxbox/neutrino/control,
	[--with-controldir=PATH], [where to find control scripts])

TUXBOX_APPS_DIRECTORY_ONE(controldir_var, CONTROLDIR_VAR, localstatedir, /var, /tuxbox/control,
	[--with-controldir_var=PATH], [where to find control scripts in /var])

TUXBOX_APPS_DIRECTORY_ONE(fontdir, FONTDIR, datadir, /share, /fonts,
	[--with-fontdir=PATH], [where to find fonts])

TUXBOX_APPS_DIRECTORY_ONE(fontdir_var, FONTDIR_VAR, localstatedir, /var, /tuxbox/fonts,
	[--with-fontdir_var=PATH], [where to find fonts in /var])

TUXBOX_APPS_DIRECTORY_ONE(gamesdir, GAMESDIR, localstatedir, /var, /tuxbox/games,
	[--with-gamesdir=PATH], [where to find games])

TUXBOX_APPS_DIRECTORY_ONE(libdir, LIBDIR, libdir, /lib, /tuxbox,
	[--with-libdir=PATH], [where to find internal libs])

TUXBOX_APPS_DIRECTORY_ONE(plugindir, PLUGINDIR, datadir, /share, /tuxbox/neutrino/plugins,
	[--with-plugindir=PATH], [where to find plugins])

TUXBOX_APPS_DIRECTORY_ONE(plugindir_var, PLUGINDIR_VAR, localstatedir, /var, /tuxbox/plugins,
	[--with-plugindir_var=PATH], [where to find plugins in /var])

TUXBOX_APPS_DIRECTORY_ONE(plugindir_mnt, PLUGINDIR_MNT, mntdir, /mnt, /plugins,
	[--with-plugindir_mnt=PATH], [where to find external plugins])

TUXBOX_APPS_DIRECTORY_ONE(luaplugindir, LUAPLUGINDIR, datadir, /share, /tuxbox/neutrino/luaplugins,
	[--with-luaplugindir=PATH], [where to find Lua plugins])

TUXBOX_APPS_DIRECTORY_ONE(webradiodir, WEBRADIODIR, datadir, /share, /tuxbox/neutrino/webradio,
	[--with-webradiodir=PATH], [where to find webradio content])

TUXBOX_APPS_DIRECTORY_ONE(webradiodir_var, WEBRADIODIR_VAR, localstatedir, /var, /tuxbox/webradio,
	[--with-webradiodir_var=PATH], [where to find webradio content in /var])

TUXBOX_APPS_DIRECTORY_ONE(webtvdir, WEBTVDIR, datadir, /share, /tuxbox/neutrino/webtv,
	[--with-webtvdir=PATH], [where to find webtv content])

TUXBOX_APPS_DIRECTORY_ONE(webtvdir_var, WEBTVDIR_VAR, localstatedir, /var, /tuxbox/webtv,
	[--with-webtvdir_var=PATH], [where to find webtv content in /var])

TUXBOX_APPS_DIRECTORY_ONE(localedir, LOCALEDIR,datadir, /share, /tuxbox/neutrino/locale,
	[--with-localedir=PATH], [where to find locale])

TUXBOX_APPS_DIRECTORY_ONE(localedir_var, LOCALEDIR_VAR, localstatedir, /var, /tuxbox/locale,
	[--with-localedir_var=PATH], [where to find locale in /var])

TUXBOX_APPS_DIRECTORY_ONE(themesdir, THEMESDIR, datadir, /share, /tuxbox/neutrino/themes,
	[--with-themesdir=PATH], [where to find themes])

TUXBOX_APPS_DIRECTORY_ONE(themesdir_var, THEMESDIR_VAR, localstatedir, /var, /tuxbox/themes,
	[--with-themesdir_var=PATH], [where to find themes in /var])

TUXBOX_APPS_DIRECTORY_ONE(iconsdir, ICONSDIR, datadir, /share, /tuxbox/neutrino/icons,
	[--with-iconsdir=PATH], [where to find icons])

TUXBOX_APPS_DIRECTORY_ONE(iconsdir_var, ICONSDIR_VAR, localstatedir, /var, /tuxbox/icons,
	[--with-iconsdir_var=PATH], [where to find icons in /var])

TUXBOX_APPS_DIRECTORY_ONE(lcd4liconsdir, LCD4L_ICONSDIR, datadir, /share, /tuxbox/neutrino/lcd/icons,
	[--with-lcd4liconsdir=PATH], [where to find lcd4linux icons])

TUXBOX_APPS_DIRECTORY_ONE(lcd4liconsdir_var, LCD4L_ICONSDIR_VAR, localstatedir, /var, /tuxbox/lcd/icons,
	[--with-lcd4liconsdir_var=PATH], [where to find lcd4linux icons in /var])

TUXBOX_APPS_DIRECTORY_ONE(private_httpddir, PRIVATE_HTTPDDIR, datadir, /share, /tuxbox/neutrino/httpd,
	[--with-private_httpddir=PATH], [where to find private httpd files])

TUXBOX_APPS_DIRECTORY_ONE(public_httpddir, PUBLIC_HTTPDDIR, localstatedir, /var, /tuxbox/httpd,
	[--with-public_httpddir=PATH], [where to find public httpd files])

TUXBOX_APPS_DIRECTORY_ONE(hosted_httpddir, HOSTED_HTTPDDIR, mntdir, /mnt, /hosted,
	[--with-hosted_httpddir=PATH], [where to find hosted files])

TUXBOX_APPS_DIRECTORY_ONE(flagdir, FLAGDIR, localstatedir, /var, /etc,
	[--with-flagdir=PATH], [where to find flag files])
])

dnl automake <= 1.6 needs this specifications
AC_SUBST(CONFIGDIR)
AC_SUBST(DATADIR)
AC_SUBST(CONTROLDIR)
AC_SUBST(CONTROLDIR_VAR)
AC_SUBST(FONTDIR)
AC_SUBST(FONTDIR_VAR)
AC_SUBST(GAMESDIR)
AC_SUBST(LIBDIR)
AC_SUBST(MNTDIR)
AC_SUBST(PLUGINDIR)
AC_SUBST(PLUGINDIR_VAR)
AC_SUBST(PLUGINDIR_MNT)
AC_SUBST(LUAPLUGINDIR)
AC_SUBST(WEBRADIODIR)
AC_SUBST(WEBRADIODIR_VAR)
AC_SUBST(WEBTVDIR)
AC_SUBST(WEBTVDIR_VAR)
AC_SUBST(LOCALEDIR)
AC_SUBST(LOCALEDIR_VAR)
AC_SUBST(THEMESDIR)
AC_SUBST(THEMESDIR_VAR)
AC_SUBST(ICONSDIR)
AC_SUBST(ICONSDIR_VAR)
AC_SUBST(LCD4L_ICONSDIR)
AC_SUBST(LCD4L_ICONSDIR_VAR)
AC_SUBST(PRIVATE_HTTPDDIR)
AC_SUBST(PUBLIC_HTTPDDIR)
AC_SUBST(HOSTED_HTTPDDIR)
dnl end workaround

AC_DEFUN([_TUXBOX_APPS_LIB_CONFIG], [
AC_PATH_PROG($1_CONFIG,$2,no)
if test "$$1_CONFIG" != "no"; then
	if test "$TARGET" = "cdk" && check_path "$$1_CONFIG"; then
		AC_MSG_$3([could not find a suitable version of $2]);
	else
		if test "$1" = "CURL"; then
			$1_CFLAGS=$($$1_CONFIG --cflags)
			$1_LIBS=$($$1_CONFIG --libs)
		else
			if test "$1" = "FREETYPE"; then
			$1_CFLAGS=$($$1_CONFIG --cflags)
				$1_LIBS=$($$1_CONFIG --libs)
			else
				$1_CFLAGS=$($$1_CONFIG --prefix=$TARGET_PREFIX --cflags)
				$1_LIBS=$($$1_CONFIG --prefix=$TARGET_PREFIX --libs)
			fi
		fi
	fi
fi
AC_SUBST($1_CFLAGS)
AC_SUBST($1_LIBS)
])

AC_DEFUN([TUXBOX_APPS_LIB_CONFIG], [
_TUXBOX_APPS_LIB_CONFIG($1,$2,ERROR)
if test "$$1_CONFIG" = "no"; then
	AC_MSG_ERROR([could not find $2]);
fi
])

AC_DEFUN([TUXBOX_APPS_LIB_CONFIG_CHECK], [
_TUXBOX_APPS_LIB_CONFIG($1,$2,WARN)
])

AC_DEFUN([TUXBOX_APPS_PKGCONFIG], [
m4_pattern_forbid([^_?PKG_[A-Z_]+$])
m4_pattern_allow([^PKG_CONFIG(_PATH)?$])
AC_ARG_VAR([PKG_CONFIG], [path to pkg-config utility])dnl
if test "x$ac_cv_env_PKG_CONFIG_set" != "xset"; then
	AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
fi
if test x"$PKG_CONFIG" = x"" ; then
	AC_MSG_ERROR([could not find pkg-config]);
fi
])

AC_DEFUN([_TUXBOX_APPS_LIB_PKGCONFIG], [
AC_REQUIRE([TUXBOX_APPS_PKGCONFIG])
AC_MSG_CHECKING(for package $2)
if $PKG_CONFIG --exists "$2" ; then
	AC_MSG_RESULT(yes)
	$1_CFLAGS=$($PKG_CONFIG --cflags "$2")
	$1_LIBS=$($PKG_CONFIG --libs "$2")
	$1_EXISTS=yes
else
	AC_MSG_RESULT(no)
fi
AC_SUBST($1_CFLAGS)
AC_SUBST($1_LIBS)
])

AC_DEFUN([TUXBOX_APPS_LIB_PKGCONFIG], [
_TUXBOX_APPS_LIB_PKGCONFIG($1,$2)
if test x"$$1_EXISTS" != xyes; then
	AC_MSG_ERROR([could not find package $2]);
fi
])

AC_DEFUN([TUXBOX_APPS_LIB_PKGCONFIG_CHECK], [
_TUXBOX_APPS_LIB_PKGCONFIG($1,$2)
])

AC_DEFUN([TUXBOX_BOXTYPE], [
AC_ARG_WITH(boxtype,
	AS_HELP_STRING([--with-boxtype], [valid values: generic, spark, coolstream, armbox, mipsbox]),
	[case "${withval}" in
		generic|spark|coolstream|armbox|mipsbox)
			BOXTYPE="$withval"
		;;
		*)
			AC_MSG_ERROR([bad value $withval for --with-boxtype])
		;;
	esac],
	[BOXTYPE="generic"])

AC_ARG_WITH(boxmodel,
	AS_HELP_STRING([--with-boxmodel], [valid for generic: generic, raspi])
AS_HELP_STRING([], [valid for coolstream: hd1, hd2])
AS_HELP_STRING([], [valid for armbox: hd51, hd60, hd61, bre2ze4k, h7, osmio4k, osmio4kplus, vusolo4k, vuduo4k, vuduo4kse, vuultimo4k, vuuno4k, vuuno4kse, vuzero4k])
AS_HELP_STRING([], [valid for mipsbox: vuduo]),
	[case "${withval}" in
		generic|raspi)
			if test "$BOXTYPE" = "generic"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		hd1|hd2)
			if test "$BOXTYPE" = "coolstream"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		nevis|apollo)
			if test "$BOXTYPE" = "coolstream"; then
				if test "$withval" = "nevis"; then
					BOXMODEL="hd1"
				fi
				if test "$withval" = "apollo"; then
					BOXMODEL="hd2"
				fi
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		hd51|hd60|hd61|bre2ze4k|h7|osmio4k|osmio4kplus|vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k)
			if test "$BOXTYPE" = "armbox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		vuduo)
			if test "$BOXTYPE" = "mipsbox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		*)
			AC_MSG_ERROR([unsupported value $withval for --with-boxmodel])
		;;
	esac],
	[BOXMODEL="generic"])

AC_SUBST(BOXTYPE)
AC_SUBST(BOXMODEL)

AM_CONDITIONAL(BOXTYPE_GENERIC, test "$BOXTYPE" = "generic")
AM_CONDITIONAL(BOXTYPE_SPARK, test "$BOXTYPE" = "spark")
AM_CONDITIONAL(BOXTYPE_COOL, test "$BOXTYPE" = "coolstream")
AM_CONDITIONAL(BOXTYPE_ARMBOX, test "$BOXTYPE" = "armbox")
AM_CONDITIONAL(BOXTYPE_MIPSBOX, test "$BOXTYPE" = "mipsbox")

# generic
AM_CONDITIONAL(BOXMODEL_GENERIC, test "$BOXMODEL" = "generic")
AM_CONDITIONAL(BOXMODEL_RASPI, test "$BOXMODEL" = "raspi")

# coolstream
AM_CONDITIONAL(BOXMODEL_CST_HD1, test "$BOXMODEL" = "hd1")
AM_CONDITIONAL(BOXMODEL_CST_HD2, test "$BOXMODEL" = "hd2")

# armbox
AM_CONDITIONAL(BOXMODEL_HD51, test "$BOXMODEL" = "hd51")
AM_CONDITIONAL(BOXMODEL_HD60, test "$BOXMODEL" = "hd60")
AM_CONDITIONAL(BOXMODEL_HD61, test "$BOXMODEL" = "hd61")
AM_CONDITIONAL(BOXMODEL_BRE2ZE4K, test "$BOXMODEL" = "bre2ze4k")
AM_CONDITIONAL(BOXMODEL_H7, test "$BOXMODEL" = "h7")
AM_CONDITIONAL(BOXMODEL_OSMIO4K, test "$BOXMODEL" = "osmio4k")
AM_CONDITIONAL(BOXMODEL_OSMIO4KPLUS, test "$BOXMODEL" = "osmio4kplus")
AM_CONDITIONAL(BOXMODEL_VUSOLO4K, test "$BOXMODEL" = "vusolo4k")
AM_CONDITIONAL(BOXMODEL_VUDUO4K, test "$BOXMODEL" = "vuduo4k")
AM_CONDITIONAL(BOXMODEL_VUDUO4KSE, test "$BOXMODEL" = "vuduo4kse")
AM_CONDITIONAL(BOXMODEL_VUULTIMO4K, test "$BOXMODEL" = "vuultimo4k")
AM_CONDITIONAL(BOXMODEL_VUUNO4K, test "$BOXMODEL" = "vuuno4k")
AM_CONDITIONAL(BOXMODEL_VUUNO4KSE, test "$BOXMODEL" = "vuuno4kse")
AM_CONDITIONAL(BOXMODEL_VUZERO4K, test "$BOXMODEL" = "vuzero4k")

# mipsbox
AM_CONDITIONAL(BOXMODEL_VUDUO, test "$BOXMODEL" = "vuduo")

if test "$BOXTYPE" = "generic"; then
	AC_DEFINE(HAVE_GENERIC_HARDWARE, 1, [building for a generic device like a standard PC])
elif test "$BOXTYPE" = "spark"; then
	AC_DEFINE(HAVE_SPARK_HARDWARE, 1, [building for a goldenmedia 990 or edision pingulux])
elif test "$BOXTYPE" = "coolstream"; then
	AC_DEFINE(HAVE_CST_HARDWARE, 1, [building for a coolstream])
elif test "$BOXTYPE" = "armbox"; then
	AC_DEFINE(HAVE_ARM_HARDWARE, 1, [building for an armbox])
elif test "$BOXTYPE" = "mipsbox"; then
	AC_DEFINE(HAVE_MIPS_HARDWARE, 1, [building for an mipsbox])
fi

# BOXTYPEs that use libstb-hal
case "$BOXTYPE" in
	coolstream)
		libstb_hal=no
	;;
	*)
		AC_DEFINE(HAVE_LIBSTB_HAL, 1, [use libstb-hal])
		libstb_hal=yes
	;;
esac
AM_CONDITIONAL(HAVE_LIBSTB_HAL, test "$libstb_hal" = "yes")

# TODO: do we need more defines?
if test "$BOXMODEL" = "generic"; then
	AC_DEFINE(BOXMODEL_GENERIC, 1, [generic pc])
elif test "$BOXMODEL" = "raspi"; then
	AC_DEFINE(BOXMODEL_RASPI, 1, [raspberry pi])
elif test "$BOXMODEL" = "hd1"; then
	AC_DEFINE(BOXMODEL_CST_HD1, 1, [coolstream hd1/neo/neo2/zee])
elif test "$BOXMODEL" = "hd2"; then
	AC_DEFINE(BOXMODEL_CST_HD2, 1, [coolstream tank/trinity/trinity v2/trinity duo/zee2/link])
elif test "$BOXMODEL" = "hd51"; then
	AC_DEFINE(BOXMODEL_HD51, 1, [hd51])
elif test "$BOXMODEL" = "hd60"; then
	AC_DEFINE(BOXMODEL_HD60, 1, [hd60])
elif test "$BOXMODEL" = "hd61"; then
	AC_DEFINE(BOXMODEL_HD61, 1, [hd61])
elif test "$BOXMODEL" = "bre2ze4k"; then
	AC_DEFINE(BOXMODEL_BRE2ZE4K, 1, [bre2ze4k])
elif test "$BOXMODEL" = "h7"; then
	AC_DEFINE(BOXMODEL_H7, 1, [h7])
elif test "$BOXMODEL" = "osmio4k"; then
        AC_DEFINE(BOXMODEL_OSMIO4K, 1, [osmio4k])
elif test "$BOXMODEL" = "osmio4kplus"; then
        AC_DEFINE(BOXMODEL_OSMIO4KPLUS, 1, [osmio4kplus])
elif test "$BOXMODEL" = "vusolo4k"; then
	AC_DEFINE(BOXMODEL_VUSOLO4K, 1, [vusolo4k])
elif test "$BOXMODEL" = "vuduo4k"; then
	AC_DEFINE(BOXMODEL_VUDUO4K, 1, [vuduo4k])
elif test "$BOXMODEL" = "vuduo4kse"; then
	AC_DEFINE(BOXMODEL_VUDUO4KSE, 1, [vuduo4kse])
elif test "$BOXMODEL" = "vuultimo4k"; then
	AC_DEFINE(BOXMODEL_VUULTIMO4K, 1, [vuultimo4k])
elif test "$BOXMODEL" = "vuuno4k"; then
	AC_DEFINE(BOXMODEL_VUUNO4K, 1, [vuuno4k])
elif test "$BOXMODEL" = "vuuno4kse"; then
	AC_DEFINE(BOXMODEL_VUUNO4KSE, 1, [vuuno4kse])
elif test "$BOXMODEL" = "vuzero4k"; then
	AC_DEFINE(BOXMODEL_VUZERO4K, 1, [vuzero4k])
elif test "$BOXMODEL" = "vuduo"; then
	AC_DEFINE(BOXMODEL_VUDUO, 1, [vuduo])
fi

# all vuplus BOXMODELs
case "$BOXMODEL" in
	vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k|vuduo)
		AC_DEFINE(BOXMODEL_VUPLUS_ALL, 1, [vuplus_all])
		vuplus_all=true
	;;
	*)
		vuplus_all=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_VUPLUS_ALL, test "$vuplus_all" = "true")

# all vuplus arm BOXMODELs
case "$BOXMODEL" in
	vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k)
		AC_DEFINE(BOXMODEL_VUPLUS_ARM, 1, [vuplus_arm])
		vuplus_arm=true
	;;
	*)
		vuplus_arm=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_VUPLUS_ARM, test "$vuplus_arm" = "true")

# all vuplus mips BOXMODELs
case "$BOXMODEL" in
	vuduo)
		AC_DEFINE(BOXMODEL_VUPLUS_MIPS, 1, [vuplus_mips])
		vuplus_mips=true
	;;
	*)
		vuplus_mips=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_VUPLUS_MIPS, test "$vuplus_mips" = "true")

# all hisilicon BOXMODELs
case "$BOXMODEL" in
	hd60|hd61)
		AC_DEFINE(BOXMODEL_HISILICON, 1, [hisilicon])
		hisilicon=true
	;;
	*)
		hisilicon=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_HISILICON, test "$hisilicon" = "true")

# BOXMODELs that allows to change osd resolution
case "$BOXMODEL" in
	hd2|hd51|hd60|hd61|bre2ze4k|h7|osmio4k|osmio4kplus|vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k|vuduo)
		AC_DEFINE(ENABLE_CHANGE_OSD_RESOLUTION, 1, [enable to change osd resolution])
	;;
esac

# BOXMODELs that needs GraphLCD support
case "$BOXMODEL" in
	vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4kse)
		AC_DEFINE(ENABLE_GRAPHLCD, 1, [include GraphLCD support])
		enable_graphlcd=yes
	;;
	*)
		enable_graphlcd=no
	;;
esac
AM_CONDITIONAL(ENABLE_GRAPHLCD, test "$enable_graphlcd" = "yes")
])

dnl backward compatiblity
AC_DEFUN([AC_GNU_SOURCE], [
AH_VERBATIM([_GNU_SOURCE], [
/* Enable GNU extensions on systems that have them. */
#ifndef _GNU_SOURCE
# undef _GNU_SOURCE
#endif
])dnl
AC_BEFORE([$0], [AC_COMPILE_IFELSE])dnl
AC_BEFORE([$0], [AC_RUN_IFELSE])dnl
AC_DEFINE([_GNU_SOURCE])
])

AC_DEFUN([AC_PROG_EGREP], [
AC_CACHE_CHECK([for egrep], [ac_cv_prog_egrep], [
if echo a | (grep -E '(a|b)') >/dev/null 2>&1; then
	ac_cv_prog_egrep='grep -E'
else
	ac_cv_prog_egrep='egrep'
fi
])
EGREP=$ac_cv_prog_egrep
AC_SUBST([EGREP])
])
