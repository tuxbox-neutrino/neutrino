AC_DEFUN([TUXBOX_APPS], [
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE

AC_GNU_SOURCE

AC_MSG_CHECKING(target)
AC_ARG_WITH(target,
	AS_HELP_STRING([--with-target=TARGET], [Target mode for compilation, either "native" or "cdk", keep empty for native]),
	[TARGET="$withval"],
	[TARGET="native"])

AC_MSG_CHECKING([targetroot (target system root)])
AC_ARG_WITH(targetroot,
	AS_HELP_STRING([--with-targetroot=PATH], [The target root (only applicable in native mode.]),
	[TARGET_ROOT="$withval"],
	[TARGET_ROOT="NONE"])

AC_ARG_WITH(targetprefix,
	AS_HELP_STRING([--with-targetprefix=PATH], [Prefix relative to target root, e.g. /usr (only applicable in cdk mode)]),
	[TARGET_PREFIX="$withval"],
	[TARGET_PREFIX="NONE"])

AC_ARG_WITH(debug,
	AS_HELP_STRING([--without-debug], [Disable debugging code @<:@default=no@:>@]),
	[DEBUG="$withval"],
	[DEBUG="yes"])

AC_MSG_CHECKING(debug enabled)
if test "$DEBUG" = "yes"; then
	AC_MSG_RESULT([$DEBUG])
	AC_DEFINE(DEBUG, 1, [enable debugging code])

	AC_MSG_CHECKING(debug flags)
	if test "$DEBUG_CFLAGS" = ""; then
		DEBUG_CFLAGS="-g3 -ggdb"
		AC_MSG_RESULT([Environment variable DEBUG_CFLAGS is not set, default debug flags will be used: $DEBUG_CFLAGS])
	else
		AC_MSG_RESULT([Defined by environment variable DEBUG_CFLAGS: $DEBUG_CFLAGS])
	fi
else
	AC_MSG_RESULT([$DEBUG])
fi

AC_MSG_CHECKING(target)
if test "$TARGET" = "native" || test "$TARGET" = "cdk"; then
	AC_MSG_RESULT($TARGET)
else
	AC_MSG_ERROR([Invalid target '$TARGET'. Use either 'native' or 'cdk' or leave empty for native.])
fi

if test "$TARGET" = "native"; then
	AC_MSG_CHECKING(prefix)
	if test "$prefix" = "NONE"; then
		prefix=/usr/local
	fi
	AC_MSG_RESULT($prefix)

	AC_MSG_CHECKING([targetroot])
	if test "$TARGET_ROOT" = "NONE"; then
		TARGET_ROOT=""
		AC_MSG_RESULT([Is not set, use default target system root: empty])
	else
		AC_MSG_RESULT([user defined target system root: $TARGET_ROOT])
	fi

	AC_MSG_CHECKING([targetprefix])
	if test "$TARGET_PREFIX" = "NONE"; then
		TARGET_PREFIX=
		AC_MSG_RESULT([Is not set, use default target prefix relative to target root: empty])
	else
		AC_MSG_RESULT([$TARGET_PREFIX])
	fi

	if test "$CFLAGS" = "" -a "$CXXFLAGS" = ""; then
		CFLAGS="-Wall -O2 -pipe $DEBUG_CFLAGS"
		CXXFLAGS="-Wall -O2 -pipe $DEBUG_CFLAGS"
	fi
elif test "$TARGET" = "cdk"; then
	AC_MSG_CHECKING(prefix in cdk mode)
	if test "$prefix" = "NONE"; then
		AC_MSG_ERROR([$prefix invalid prefix, you need to specify one in cdk mode])
	fi
	AC_MSG_RESULT($prefix)

	AC_MSG_CHECKING([targetroot in cdk mode])
	OLD_TARGET_ROOT=$TARGET_ROOT
	TARGET_ROOT=""
	if test "$TARGET_ROOT" = "NONE"; then
		AC_MSG_NOTICE([Is not set "$OLD_TARGET_ROOT", using fallback to default: empty])

	else
		AC_MSG_NOTICE([Is set to "$OLD_TARGET_ROOT", but using default: empty])
	fi

	AC_MSG_CHECKING([targetprefix in cdk mode])
	if test "$TARGET_PREFIX" = "NONE"; then
		TARGET_PREFIX=""
		AC_MSG_RESULT([default target prefix relative to target root: empty])
	else
		AC_MSG_RESULT([user defined target prefix relative to target root: $TARGET_PREFIX])
	fi

	AC_MSG_CHECKING([gcc/g++ in cdk mode])
	if test "$CC" = "" -a "$CXX" = ""; then
		AC_MSG_ERROR([you need to specify variables CC and CXX in cdk mode in your envirionment])
	fi
	AC_MSG_RESULT($CC / $CXX)

	AC_MSG_CHECKING([CFLAGS and CXXFLAGS in cdk mode])
	if test "$CFLAGS" = "" -o "$CXXFLAGS" = ""; then
		AC_MSG_ERROR([you need to specify variables CFLAGS and CXXFLAGS in cdk mode in your envirionment])
	fi
	AC_MSG_RESULT(CFLAGS: $CFLAGS)
	AC_MSG_RESULT(CXXLAGS: $CXXFLAGS)
fi

AC_MSG_CHECKING(exec_prefix)
if test "$exec_prefix" = "NONE"; then
	exec_prefix=$prefix
fi
AC_MSG_RESULT($exec_prefix)

AC_DEFINE_UNQUOTED([TARGET], ["$TARGET"], [target for compilation])
AC_DEFINE_UNQUOTED([TARGET_ROOT], ["$TARGET_ROOT"], [root path at the target system native: the target root; cdk: empty])
AC_DEFINE_UNQUOTED([TARGET_PREFIX], ["$TARGET_PREFIX"], [native: auto-assigned path; cdk: path relative to target root])


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

#
# FIXME:
# This breaks --sysconfdir= etc. switches in cdk mode.
#
if test "$TARGET" = "cdk"; then
	datadir="\${prefix}/share"
	targetdatadir="\${TARGET_PREFIX}/share"

	libdir="\${prefix}/lib"
	targetlibdir="\${TARGET_PREFIX}/lib"

	sysconfdir="/etc"
	targetsysconfdir="/etc"

	localstatedir="/var"
	targetlocalstatedir="/var"

	mntdir="/mnt"
	targetmntdir="/mnt"
	MNTDIR=$targetmntdir
else
	mntdir="/mnt"
	MNTDIR=$mntdir
fi

TUXBOX_APPS_DIRECTORY_ONE(configdir, CONFIGDIR, localstatedir, /var, /tuxbox/config,
	[--with-configdir=PATH], [where to find config files])

TUXBOX_APPS_DIRECTORY_ONE(zapitdir, ZAPITDIR, localstatedir, /var, /tuxbox/config/zapit,
	[--with-zapitdir=PATH], [where to find zapit files])

TUXBOX_APPS_DIRECTORY_ONE(datadir, DATADIR, datadir, /share, /tuxbox,
	[--with-datadir=PATH], [where to find data files])

TUXBOX_APPS_DIRECTORY_ONE(datadir_var, DATADIR_VAR, localstatedir, /var, /tuxbox,
	[--with-datadir_var=PATH], [where to find data files in /var])

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

TUXBOX_APPS_DIRECTORY_ONE(luaplugindir_var, LUAPLUGINDIR_VAR, localstatedir, /var, /tuxbox/luaplugins,
	[--with-luaplugindir_var=PATH], [where to find Lua plugins in /var])

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

TUXBOX_APPS_DIRECTORY_ONE(logodir, LOGODIR, datadir, /share, /tuxbox/neutrino/icons/logo,
	[--with-logodir=PATH], [where to find channel logos])

TUXBOX_APPS_DIRECTORY_ONE(logodir_var, LOGODIR_VAR, localstatedir, /var, /tuxbox/icons/logo,
	[--with-logodir_var=PATH], [where to find channel logos in /var])

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
AC_SUBST(ZAPITDIR)
AC_SUBST(DATADIR)
AC_SUBST(DATADIR_VAR)
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
AC_SUBST(LUAPLUGINDIR_VAR)
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
AC_SUBST(LOGODIR)
AC_SUBST(LOGODIR_VAR)
AC_SUBST(PRIVATE_HTTPDDIR)
AC_SUBST(PUBLIC_HTTPDDIR)
AC_SUBST(HOSTED_HTTPDDIR)
AC_SUBST(FLAGDIR)
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
	AS_HELP_STRING([--with-boxtype], [valid values: generic, coolstream, armbox, mipsbox]),
	[case "${withval}" in
		generic|coolstream|armbox|mipsbox)
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
AS_HELP_STRING([], [valid for armbox: hd60, hd61, multibox, multiboxse, hd51, bre2ze4k, h7, e4hdultra, protek4k, osmini4k, osmio4k, osmio4kplus, vusolo4k, vuduo4k, vuduo4kse, vuultimo4k, vuuno4k, vuuno4kse, vuzero4k])
AS_HELP_STRING([], [valid for mipsbox: vuduo, vuduo2]),
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
		hd60|hd61|multibox|multiboxse|hd51|bre2ze4k|h7|e4hdultra|protek4k|osmini4k|osmio4k|osmio4kplus|vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k)
			if test "$BOXTYPE" = "armbox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		vuduo|vuduo2|gb800se|osnino|osninoplus|osninopro)
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
AM_CONDITIONAL(BOXTYPE_CST, test "$BOXTYPE" = "coolstream")
AM_CONDITIONAL(BOXTYPE_ARMBOX, test "$BOXTYPE" = "armbox")
AM_CONDITIONAL(BOXTYPE_MIPSBOX, test "$BOXTYPE" = "mipsbox")

# generic
AM_CONDITIONAL(BOXMODEL_GENERIC, test "$BOXMODEL" = "generic")
AM_CONDITIONAL(BOXMODEL_RASPI, test "$BOXMODEL" = "raspi")

# coolstream
AM_CONDITIONAL(BOXMODEL_CST_HD1, test "$BOXMODEL" = "hd1")
AM_CONDITIONAL(BOXMODEL_CST_HD2, test "$BOXMODEL" = "hd2")

# armbox
AM_CONDITIONAL(BOXMODEL_HD60, test "$BOXMODEL" = "hd60")
AM_CONDITIONAL(BOXMODEL_HD61, test "$BOXMODEL" = "hd61")
AM_CONDITIONAL(BOXMODEL_MULTIBOX, test "$BOXMODEL" = "multibox")
AM_CONDITIONAL(BOXMODEL_MULTIBOXSE, test "$BOXMODEL" = "multiboxse")

AM_CONDITIONAL(BOXMODEL_HD51, test "$BOXMODEL" = "hd51")
AM_CONDITIONAL(BOXMODEL_BRE2ZE4K, test "$BOXMODEL" = "bre2ze4k")
AM_CONDITIONAL(BOXMODEL_H7, test "$BOXMODEL" = "h7")
AM_CONDITIONAL(BOXMODEL_E4HDULTRA, test "$BOXMODEL" = "e4hdultra")
AM_CONDITIONAL(BOXMODEL_PROTEK4K, test "$BOXMODEL" = "protek4k")

AM_CONDITIONAL(BOXMODEL_OSMINI4K, test "$BOXMODEL" = "osmini4k")
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
AM_CONDITIONAL(BOXMODEL_VUDUO2, test "$BOXMODEL" = "vuduo2")

AM_CONDITIONAL(BOXMODEL_GB800SE, test "$BOXMODEL" = "gb800se")

AM_CONDITIONAL(BOXMODEL_OSNINO, test "$BOXMODEL" = "osnino")
AM_CONDITIONAL(BOXMODEL_OSNINOPLUS, test "$BOXMODEL" = "osninoplus")
AM_CONDITIONAL(BOXMODEL_OSNINOPRO, test "$BOXMODEL" = "osninopro")

if test "$BOXTYPE" = "generic"; then
	AC_DEFINE(HAVE_GENERIC_HARDWARE, 1, [building for a generic device like a standard PC])
elif test "$BOXTYPE" = "coolstream"; then
	AC_DEFINE(HAVE_CST_HARDWARE, 1, [building for a coolstream])
elif test "$BOXTYPE" = "armbox"; then
	AC_DEFINE(HAVE_ARM_HARDWARE, 1, [building for an armbox])
elif test "$BOXTYPE" = "mipsbox"; then
	AC_DEFINE(HAVE_MIPS_HARDWARE, 1, [building for an mipsbox])
fi

# generic
if test "$BOXMODEL" = "generic"; then
	AC_DEFINE(BOXMODEL_GENERIC, 1, [generic pc])
elif test "$BOXMODEL" = "raspi"; then
	AC_DEFINE(BOXMODEL_RASPI, 1, [raspberry pi])

# coolstream
elif test "$BOXMODEL" = "hd1"; then
	AC_DEFINE(BOXMODEL_CST_HD1, 1, [coolstream hd1/neo/neo2/zee])
elif test "$BOXMODEL" = "hd2"; then
	AC_DEFINE(BOXMODEL_CST_HD2, 1, [coolstream tank/trinity/trinity v2/trinity duo/zee2/link])

# armbox
elif test "$BOXMODEL" = "hd60"; then
	AC_DEFINE(BOXMODEL_HD60, 1, [hd60])
elif test "$BOXMODEL" = "hd61"; then
	AC_DEFINE(BOXMODEL_HD61, 1, [hd61])
elif test "$BOXMODEL" = "multibox"; then
	AC_DEFINE(BOXMODEL_MULTIBOX, 1, [multibox])
elif test "$BOXMODEL" = "multiboxse"; then
	AC_DEFINE(BOXMODEL_MULTIBOXSE, 1, [multiboxse])

elif test "$BOXMODEL" = "hd51"; then
	AC_DEFINE(BOXMODEL_HD51, 1, [hd51])
elif test "$BOXMODEL" = "bre2ze4k"; then
	AC_DEFINE(BOXMODEL_BRE2ZE4K, 1, [bre2ze4k])
elif test "$BOXMODEL" = "h7"; then
	AC_DEFINE(BOXMODEL_H7, 1, [h7])
elif test "$BOXMODEL" = "e4hdultra"; then
	AC_DEFINE(BOXMODEL_E4HDULTRA, 1, [e4hdultra])
elif test "$BOXMODEL" = "protek4k"; then
	AC_DEFINE(BOXMODEL_PROTEK4K, 1, [protek4k])

elif test "$BOXMODEL" = "osmini4k"; then
	AC_DEFINE(BOXMODEL_OSMINI4K, 1, [osmini4k])
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

# mipsbox
elif test "$BOXMODEL" = "vuduo"; then
	AC_DEFINE(BOXMODEL_VUDUO, 1, [vuduo])
elif test "$BOXMODEL" = "vuduo2"; then
	AC_DEFINE(BOXMODEL_VUDUO2, 1, [vuduo2])

elif test "$BOXMODEL" = "gb800se"; then
	AC_DEFINE(BOXMODEL_GB800SE, 1, [gb800se])

elif test "$BOXMODEL" = "osnino"; then
	AC_DEFINE(BOXMODEL_OSNINO, 1, [osnino])
elif test "$BOXMODEL" = "osninoplus"; then
	AC_DEFINE(BOXMODEL_OSNINOPLUS, 1, [osninoplus])
elif test "$BOXMODEL" = "osninopro"; then
	AC_DEFINE(BOXMODEL_OSNINOPRO, 1, [osninopro])
fi

# all vuplus BOXMODELs
case "$BOXMODEL" in
	vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k|vuduo|vuduo2)
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
	vuduo|vuduo2)
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
	hd60|hd61|multibox|multiboxse)
		AC_DEFINE(BOXMODEL_HISILICON, 1, [hisilicon])
		hisilicon=true
	;;
	*)
		hisilicon=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_HISILICON, test "$hisilicon" = "true")
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
