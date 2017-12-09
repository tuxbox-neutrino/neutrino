AC_DEFUN([TUXBOX_APPS],[
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

AC_ARG_WITH(libcoolstream-static-dir,
	AS_HELP_STRING([--with-libcoolstream-static-dir=PATH], [path for static libcoolstream [[NONE]]]),
	[LIBCOOLSTREAM_STATIC_DIR="$withval"],
	[LIBCOOLSTREAM_STATIC_DIR=""])

AC_ARG_ENABLE(libcoolstream-static,
	AS_HELP_STRING([--enable-libcoolstream-static], [libcoolstream static linked for testing  @<:@default=no@:>@]))
AM_CONDITIONAL(ENABLE_LIBCOOLSTREAM_STATIC, test "$enable_libcoolstream_static" = "yes")

AC_ARG_ENABLE(reschange,
	AS_HELP_STRING([--enable-reschange], [enable change the osd resolution @<:@default for hd2 and hd51@:>@]),
	AC_DEFINE(ENABLE_CHANGE_OSD_RESOLUTION, 1, [enable change the osd resolution]))
AM_CONDITIONAL(ENABLE_RESCHANGE, test "$enable_reschange" = "yes")

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
	TARGET_PREFIX=$prefix
	if test "$exec_prefix" = "NONE"; then
		exec_prefix=$prefix
	fi
	targetprefix=$prefix
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
		AC_DEFINE_UNQUOTED(TARGET_PREFIX, "$TARGET_PREFIX",[The targets prefix])
	fi
	if test "$TARGET_PREFIX" = "NONE"; then
		AC_MSG_ERROR([invalid targetprefix, you need to specify one in cdk mode])
		TARGET_PREFIX=""
	fi
else
	AC_MSG_RESULT(none)
	AC_MSG_ERROR([invalid target $TARGET, choose on from native,cdk]);
fi

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_SYS_LARGEFILE

check_path() {
	return $(perl -e "if(\"$1\"=~m#^/usr/(local/)?bin#){print \"0\"}else{print \"1\";}")
}

])

dnl expand nested ${foo}/bar
AC_DEFUN([TUXBOX_EXPAND_VARIABLE],[__$1="$2"
	for __CNT in false false false false true; do dnl max 5 levels of indirection

		$1=`eval echo "$__$1"`
		echo ${$1} | grep -q '\$' || break # 'grep -q' is POSIX, exit if no $ in variable
		__$1="${$1}"
	done
	$__CNT && AC_MSG_ERROR([can't expand variable $1=$2]) dnl bail out if we did not expand
])

AC_DEFUN([TUXBOX_APPS_DIRECTORY_ONE],[
AC_ARG_WITH($1, AS_HELP_STRING([$6], [$7 [[PREFIX$4$5]]], [32], [79]), [
	_$2=$withval
	if test "$TARGET" = "cdk"; then
		$2=`eval echo "$TARGET_PREFIX$withval"` # no indirection possible IMNSHO
	else
		$2=$withval
	fi
	TARGET_$2=${$2}
],[
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

AC_DEFUN([TUXBOX_APPS_DIRECTORY],[
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

TUXBOX_APPS_DIRECTORY_ONE(fontdir, FONTDIR, datadir, /share, /fonts,
	[--with-fontdir=PATH], [where to find fonts])

TUXBOX_APPS_DIRECTORY_ONE(fontdir_var, FONTDIR_VAR, localstatedir, /var, /tuxbox/fonts,
	[--with-fontdir_var=PATH], [where to find fonts in /var])

TUXBOX_APPS_DIRECTORY_ONE(gamesdir, GAMESDIR, localstatedir, /var, /tuxbox/games,
	[--with-gamesdir=PATH], [where to find games])

TUXBOX_APPS_DIRECTORY_ONE(libdir, LIBDIR, libdir, /lib, /tuxbox,
	[--with-libdir=PATH], [where to find internal libs])

TUXBOX_APPS_DIRECTORY_ONE(plugindir, PLUGINDIR, libdir, /lib, /tuxbox/plugins,
	[--with-plugindir=PATH], [where to find plugins])

TUXBOX_APPS_DIRECTORY_ONE(plugindir_var, PLUGINDIR_VAR, localstatedir, /var, /tuxbox/plugins,
	[--with-plugindir_var=PATH], [where to find plugins in /var])

TUXBOX_APPS_DIRECTORY_ONE(plugindir_mnt, PLUGINDIR_MNT, mntdir, /mnt, /plugins,
	[--with-plugindir_mnt=PATH], [where to find external plugins])

TUXBOX_APPS_DIRECTORY_ONE(luaplugindir, LUAPLUGINDIR, libdir, /lib, /tuxbox/luaplugins,
	[--with-luaplugindir=PATH], [where to find Lua plugins])

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
AC_SUBST(FONTDIR)
AC_SUBST(FONTDIR_VAR)
AC_SUBST(GAMESDIR)
AC_SUBST(LIBDIR)
AC_SUBST(MNTDIR)
AC_SUBST(PLUGINDIR)
AC_SUBST(PLUGINDIR_VAR)
AC_SUBST(PLUGINDIR_MNT)
AC_SUBST(LUAPLUGINDIR)
AC_SUBST(WEBTVDIR)
AC_SUBST(WEBTVDIR_VAR)
AC_SUBST(LOCALEDIR)
AC_SUBST(LOCALEDIR_VAR)
AC_SUBST(THEMESDIR)
AC_SUBST(THEMESDIR_VAR)
AC_SUBST(ICONSDIR)
AC_SUBST(ICONSDIR_VAR)
AC_SUBST(PRIVATE_HTTPDDIR)
AC_SUBST(PUBLIC_HTTPDDIR)
AC_SUBST(HOSTED_HTTPDDIR)
dnl end workaround

AC_DEFUN([_TUXBOX_APPS_LIB_CONFIG],[
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

AC_DEFUN([TUXBOX_APPS_LIB_CONFIG],[
_TUXBOX_APPS_LIB_CONFIG($1,$2,ERROR)
if test "$$1_CONFIG" = "no"; then
	AC_MSG_ERROR([could not find $2]);
fi
])

AC_DEFUN([TUXBOX_APPS_LIB_CONFIG_CHECK],[
_TUXBOX_APPS_LIB_CONFIG($1,$2,WARN)
])

AC_DEFUN([TUXBOX_APPS_PKGCONFIG],[
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

AC_DEFUN([_TUXBOX_APPS_LIB_PKGCONFIG],[
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

AC_DEFUN([TUXBOX_APPS_LIB_PKGCONFIG],[
_TUXBOX_APPS_LIB_PKGCONFIG($1,$2)
if test x"$$1_EXISTS" != xyes; then
	AC_MSG_ERROR([could not find package $2]);
fi
])

AC_DEFUN([TUXBOX_APPS_LIB_PKGCONFIG_CHECK],[
_TUXBOX_APPS_LIB_PKGCONFIG($1,$2)
])

AC_DEFUN([TUXBOX_BOXTYPE],[
AC_ARG_WITH(boxtype,
	AS_HELP_STRING([--with-boxtype], [valid values: tripledragon, coolstream, spark, azbox, generic, armbox]),
	[case "${withval}" in
		tripledragon|coolstream|spark|azbox|generic|armbox)
			BOXTYPE="$withval"
			;;
		*)
			AC_MSG_ERROR([bad value $withval for --with-boxtype]) ;;
	esac],
	[BOXTYPE="coolstream"])

AC_ARG_WITH(boxmodel,
	AS_HELP_STRING([--with-boxmodel], [valid for coolstream: hd1, hd2])
AS_HELP_STRING([], [valid for armbox: hd51])
AS_HELP_STRING([], [valid for generic: raspi]),
	[case "${withval}" in
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
		hd51)
			if test "$BOXTYPE" = "armbox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		raspi)
			if test "$BOXTYPE" = "generic"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		*)
			AC_MSG_ERROR([unsupported value $withval for --with-boxmodel])
			;;
	esac],
	[test "$BOXTYPE" = "coolstream" && BOXMODEL="hd1" || true])

AC_SUBST(BOXTYPE)
AC_SUBST(BOXMODEL)

AM_CONDITIONAL(BOXTYPE_AZBOX, test "$BOXTYPE" = "azbox")
AM_CONDITIONAL(BOXTYPE_TRIPLE, test "$BOXTYPE" = "tripledragon")
AM_CONDITIONAL(BOXTYPE_COOL, test "$BOXTYPE" = "coolstream")
AM_CONDITIONAL(BOXTYPE_SPARK, test "$BOXTYPE" = "spark")
AM_CONDITIONAL(BOXTYPE_GENERIC, test "$BOXTYPE" = "generic")
AM_CONDITIONAL(BOXTYPE_ARMBOX, test "$BOXTYPE" = "armbox")

AM_CONDITIONAL(BOXMODEL_CS_HD1,test "$BOXMODEL" = "hd1")
AM_CONDITIONAL(BOXMODEL_CS_HD2,test "$BOXMODEL" = "hd2")

AM_CONDITIONAL(BOXMODEL_RASPI,test "$BOXMODEL" = "raspi")

if test "$BOXTYPE" = "azbox"; then
	AC_DEFINE(HAVE_AZBOX_HARDWARE, 1, [building for an azbox])
elif test "$BOXTYPE" = "tripledragon"; then
	AC_DEFINE(HAVE_TRIPLEDRAGON, 1, [building for a tripledragon])
elif test "$BOXTYPE" = "coolstream"; then
	AC_DEFINE(HAVE_COOL_HARDWARE, 1, [building for a coolstream])
elif test "$BOXTYPE" = "spark"; then
	AC_DEFINE(HAVE_SPARK_HARDWARE, 1, [building for a goldenmedia 990 or edision pingulux])
elif test "$BOXTYPE" = "generic"; then
	AC_DEFINE(HAVE_GENERIC_HARDWARE, 1, [building for a generic device like a standard PC])
elif test "$BOXTYPE" = "armbox"; then
	AC_DEFINE(HAVE_ARM_HARDWARE, 1, [building for an armbox])
fi

# TODO: do we need more defines?
if test "$BOXMODEL" = "hd1"; then
	AC_DEFINE(BOXMODEL_CS_HD1, 1, [coolstream hd1/neo/neo2/zee])
elif test "$BOXMODEL" = "hd2"; then
	AC_DEFINE(BOXMODEL_CS_HD2, 1, [coolstream tank/trinity/trinity v2/trinity duo/zee2/link])
	AC_DEFINE(ENABLE_CHANGE_OSD_RESOLUTION, 1, [enable change the osd resolution])
elif test "$BOXMODEL" = "hd51"; then
	AC_DEFINE(ENABLE_CHANGE_OSD_RESOLUTION, 1, [enable change the osd resolution])
elif test "$BOXMODEL" = "raspi"; then
	AC_DEFINE(BOXMODEL_RASPI, 1, [Raspberry pi])
fi
])

dnl backward compatiblity
AC_DEFUN([AC_GNU_SOURCE],
[AH_VERBATIM([_GNU_SOURCE],
[/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# undef _GNU_SOURCE
#endif])dnl
AC_BEFORE([$0], [AC_COMPILE_IFELSE])dnl
AC_BEFORE([$0], [AC_RUN_IFELSE])dnl
AC_DEFINE([_GNU_SOURCE])
])

AC_DEFUN([AC_PROG_EGREP],
[AC_CACHE_CHECK([for egrep], [ac_cv_prog_egrep],
   [if echo a | (grep -E '(a|b)') >/dev/null 2>&1
    then ac_cv_prog_egrep='grep -E'
    else ac_cv_prog_egrep='egrep'
    fi])
 EGREP=$ac_cv_prog_egrep
 AC_SUBST([EGREP])
])
