AC_DEFUN([TUXBOX_APPS],[
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_GNU_SOURCE

AC_ARG_WITH(target,
	[  --with-target=TARGET    target for compilation [[native,cdk]]],
	[TARGET="$withval"],[TARGET="native"])

AC_ARG_WITH(targetprefix,
	[  --with-targetprefix=PATH  prefix relative to target root (only applicable in cdk mode)],
	[TARGET_PREFIX="$withval"],[TARGET_PREFIX="NONE"])

AC_ARG_WITH(debug,
	[  --without-debug         disable debugging code],
	[DEBUG="$withval"],[DEBUG="yes"])

if test "$DEBUG" = "yes"; then
	DEBUG_CFLAGS="-g3 -ggdb"
	AC_DEFINE(DEBUG,1,[Enable debug messages])
fi

AC_ARG_ENABLE(reschange,
       AS_HELP_STRING(--enable-reschange,enable change the osd resolution (default for hd2)))

AM_CONDITIONAL(ENABLE_RESCHANGE,test "$enable_reschange" = "yes")
if test "$enable_reschange" = "yes"; then
	AC_DEFINE(ENABLE_CHANGE_OSD_RESOLUTION,1,[enable change the osd resolution])
fi

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
		CC=powerpc-tuxbox-linux-gnu-gcc CXX=powerpc-tuxbox-linux-gnu-g++
	fi
	if test "$CFLAGS" = "" -a "$CXXFLAGS" = ""; then
		CFLAGS="-Wall -Os -mcpu=823 -pipe $DEBUG_CFLAGS"
		CXXFLAGS="-Wall -Os -mcpu=823 -pipe $DEBUG_CFLAGS"
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
	if test "$host_alias" = ""; then
		cross_compiling=yes
		host_alias=powerpc-tuxbox-linux-gnu
	fi
else
	AC_MSG_RESULT(none)
	AC_MSG_ERROR([invalid target $TARGET, choose on from native,cdk]);
fi

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_SYS_LARGEFILE

check_path () {
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
AC_ARG_WITH($1,[  $6$7 [[PREFIX$4$5]]],[
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
fi

TUXBOX_APPS_DIRECTORY_ONE(configdir,CONFIGDIR,localstatedir,/var,/tuxbox/config,
	[--with-configdir=PATH         ],[where to find the config files])

TUXBOX_APPS_DIRECTORY_ONE(datadir,DATADIR,datadir,/share,/tuxbox,
	[--with-datadir=PATH           ],[where to find data])

TUXBOX_APPS_DIRECTORY_ONE(fontdir,FONTDIR,datadir,/share,/fonts,
	[--with-fontdir=PATH           ],[where to find the fonts])

TUXBOX_APPS_DIRECTORY_ONE(gamesdir,GAMESDIR,localstatedir,/var,/tuxbox/games,
	[--with-gamesdir=PATH          ],[where games data is stored])

TUXBOX_APPS_DIRECTORY_ONE(libdir,LIBDIR,libdir,/lib,/tuxbox,
	[--with-libdir=PATH            ],[where to find the internal libs])

TUXBOX_APPS_DIRECTORY_ONE(plugindir,PLUGINDIR,libdir,/lib,/tuxbox/plugins,
	[--with-plugindir=PATH         ],[where to find the plugins])

TUXBOX_APPS_DIRECTORY_ONE(plugindir_var,PLUGINDIR_VAR,localstatedir,/var,/tuxbox/plugins,
	[--with-plugindir_var=PATH     ],[where to find the plugins in /var])

TUXBOX_APPS_DIRECTORY_ONE(webtvdir_var,WEBTVDIR_VAR,localstatedir,/var,/tuxbox/plugins/webtv,
	[--with-webtvdir_var=PATH      ],[where to find the livestreamScriptPath in /var])

TUXBOX_APPS_DIRECTORY_ONE(plugindir_mnt,PLUGINDIR_MNT,mntdir,/mnt,/plugins,
	[--with-plugindir_mnt=PATH     ],[where to find the the extern plugins])

TUXBOX_APPS_DIRECTORY_ONE(luaplugindir,LUAPLUGINDIR,libdir,/lib,/tuxbox/luaplugins,
	[--with-luaplugindir=PATH      ],[where to find Lua plugins])

TUXBOX_APPS_DIRECTORY_ONE(localedir,LOCALEDIR,datadir,/share, /tuxbox/neutrino/locale,
	[--with-localedir=PATH         ],[where to find the locale])

TUXBOX_APPS_DIRECTORY_ONE(localedir_var,LOCALEDIR_VAR,localstatedir,/var,/tuxbox/locale,
	[--with-localedir_var=PATH     ],[where to find the locale in /var])

TUXBOX_APPS_DIRECTORY_ONE(themesdir,THEMESDIR,datadir,/share, /tuxbox/neutrino/themes,
	[--with-themesdir=PATH         ],[where to find the themes])

TUXBOX_APPS_DIRECTORY_ONE(themesdir_var,THEMESDIR_VAR,localstatedir,/var,/tuxbox/themes,
	[--with-themesdir_var=PATH     ],[where to find the themes in /var])

TUXBOX_APPS_DIRECTORY_ONE(iconsdir,ICONSDIR,datadir,/share, /tuxbox/neutrino/icons,
	[--with-iconsdir=PATH          ],[where to find the icons])

TUXBOX_APPS_DIRECTORY_ONE(iconsdir_var,ICONSDIR_VAR,localstatedir,/var,/tuxbox/icons,
	[--with-iconsdir_var=PATH      ],[where to find the icons in /var])

TUXBOX_APPS_DIRECTORY_ONE(private_httpddir,PRIVATE_HTTPDDIR,datadir,/share,/tuxbox/neutrino/httpd,
	[--with-private_httpddir=PATH  ],[where to find the the private httpd files])

TUXBOX_APPS_DIRECTORY_ONE(public_httpddir,PUBLIC_HTTPDDIR,localstatedir,/var,/httpd,
	[--with-public_httpddir=PATH   ],[where to find the the public httpd files])

TUXBOX_APPS_DIRECTORY_ONE(hosted_httpddir,HOSTED_HTTPDDIR,mntdir,/mnt,/hosted,
	[--with-hosted_httpddir=PATH   ],[where to find the the hosted files])
])

dnl automake <= 1.6 needs this specifications
AC_SUBST(CONFIGDIR)
AC_SUBST(DATADIR)
AC_SUBST(FONTDIR)
AC_SUBST(GAMESDIR)
AC_SUBST(LIBDIR)
AC_SUBST(MNTDIR)
AC_SUBST(PLUGINDIR)
AC_SUBST(LUAPLUGINDIR)
AC_SUBST(LOCALEDIR)
AC_SUBST(THEMESDIR)
AC_SUBST(ICONSDIR)
AC_SUBST(PRIVATE_HTTPDDIR)
AC_SUBST(PUBLIC_HTTPDDIR)
AC_SUBST(HOSTED_HTTPDDIR)
dnl end workaround

AC_DEFUN([TUXBOX_APPS_ENDIAN],[
AC_CHECK_HEADERS(endian.h)
AC_C_BIGENDIAN
])

AC_DEFUN([TUXBOX_APPS_DRIVER],[
AC_ARG_WITH(driver,
	[  --with-driver=PATH      path for driver sources [[NONE]]],
	[DRIVER="$withval"],[DRIVER=""])

if test -d "$DRIVER/include"; then
	AC_DEFINE(HAVE_DBOX2_DRIVER,1,[Define to 1 if you have the dbox2 driver sources])
else
	AC_MSG_ERROR([can't find driver sources])
fi

AC_SUBST(DRIVER)

CPPFLAGS="$CPPFLAGS -I$DRIVER/include"
])

AC_DEFUN([TUXBOX_APPS_DVB],[
AC_ARG_WITH(dvbincludes,
	[  --with-dvbincludes=PATH  path for dvb includes [[NONE]]],
	[DVBINCLUDES="$withval"],[DVBINCLUDES=""])

if test "$DVBINCLUDES"; then
	CPPFLAGS="-I$DVBINCLUDES $CPPFLAGS"
	CFLAGS="-I$DVBINCLUDES $CFLAGS"
	CXXFLAGS="-I$DVBINCLUDES $CXXFLAGS"
fi

AC_CHECK_HEADERS(linux/dvb/version.h,[
	AC_LANG_PREPROC_REQUIRE()
	AC_REQUIRE([AC_PROG_EGREP])
	AC_LANG_CONFTEST([AC_LANG_SOURCE([[
#include <linux/dvb/version.h>
version DVB_API_VERSION
	]])])
	DVB_API_VERSION=`(eval "$ac_cpp -traditional-cpp conftest.$ac_ext") 2>&AS_MESSAGE_LOG_FD | $EGREP "^version" | sed "s,version\ ,,"`

	AC_LANG_CONFTEST([AC_LANG_SOURCE([[
#include <linux/dvb/version.h>
version DVB_API_VERSION_MINOR
	]])])
	DVB_API_VERSION_MINOR=`(eval "$ac_cpp -traditional-cpp conftest.$ac_ext") 2>&AS_MESSAGE_LOG_FD | $EGREP "^version" | sed "s,version\ ,,"`
	rm -f conftest*

	AC_MSG_NOTICE([found dvb version $DVB_API_VERSION.$DVB_API_VERSION_MINOR])
])

if test "$DVB_API_VERSION"; then
	AC_DEFINE(HAVE_DVB,1,[Define to 1 if you have the dvb includes])
	AC_DEFINE_UNQUOTED(HAVE_DVB_API_VERSION,$DVB_API_VERSION,[Define to the version of the dvb api])
else
	AC_MSG_ERROR([can't find dvb headers])
fi

if test "$DVB_API_VERSION_MINOR"; then
	AC_DEFINE_UNQUOTED(HAVE_DVB_API_VERSION_MINOR,$DVB_API_VERSION_MINOR,[Define to the minor version of the dvb api])
else
	AC_DEFINE_UNQUOTED(HAVE_DVB_API_VERSION_MINOR,0,[Define to the minor version of the dvb api])
fi
])

AC_DEFUN([TUXBOX_APPS_CAPTURE],[
AC_CHECK_HEADER(linux/dvb/avia/avia_gt_capture.h,[
	AC_DEFINE(HAVE_OLD_CAPTURE_API,1,[Define this if you want to use the old dbox2 capture API])
	AC_MSG_NOTICE([using old demux capture API])],[
	AC_MSG_NOTICE([using v4l2 capture API])
	])
])	

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
AC_PATH_PROG(PKG_CONFIG, pkg-config,no)
if test "$PKG_CONFIG" = "no" ; then
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

AC_DEFUN([_TUXBOX_APPS_LIB_SYMBOL],[
AC_CHECK_LIB($2,$3,HAVE_$1="yes",HAVE_$1="no")
if test "$HAVE_$1" = "yes"; then
	$1_LIBS=-l$2
fi

AC_SUBST($1_LIBS)
])

AC_DEFUN([TUXBOX_APPS_LIB_SYMBOL],[
_TUXBOX_APPS_LIB_SYMBOL($1,$2,$3,ERROR)
if test "$HAVE_$1" = "no"; then
	AC_MSG_ERROR([could not find $2]);
fi
])

AC_DEFUN([TUXBOX_APPS_LIB_CONFIG_SYMBOL],[
_TUXBOX_APPS_LIB_SYMBOL($1,$2,$3,WARN)
])

AC_DEFUN([TUXBOX_APPS_GETTEXT],[
AC_PATH_PROG(MSGFMT, msgfmt, no)
AC_PATH_PROG(GMSGFMT, gmsgfmt, $MSGFMT)
AC_PATH_PROG(XGETTEXT, xgettext, no)
AC_PATH_PROG(MSGMERGE, msgmerge, no)

AC_MSG_CHECKING([whether NLS is requested])
AC_ARG_ENABLE(nls,
	[  --disable-nls           do not use Native Language Support],
	USE_NLS=$enableval, USE_NLS=yes)
AC_MSG_RESULT($USE_NLS)
AC_SUBST(USE_NLS)

if test "$USE_NLS" = "yes"; then
	AC_CACHE_CHECK([for GNU gettext in libc], gt_cv_func_gnugettext_libc,[
		AC_TRY_LINK([
			#include <libintl.h>
			#ifndef __GNU_GETTEXT_SUPPORTED_REVISION
			#define __GNU_GETTEXT_SUPPORTED_REVISION(major) ((major) == 0 ? 0 : -1)
			#endif
			extern int _nl_msg_cat_cntr;
			extern int *_nl_domain_bindings;
			],[
			bindtextdomain ("", "");
			return (int) gettext ("") + _nl_msg_cat_cntr + *_nl_domain_bindings;
			], gt_cv_func_gnugettext_libc=yes, gt_cv_func_gnugettext_libc=no
		)]
	)

	if test "$gt_cv_func_gnugettext_libc" = "yes"; then
		AC_DEFINE(ENABLE_NLS, 1, [Define to 1 if translation of program messages to the user's native language is requested.])
		gt_use_preinstalled_gnugettext=yes
	else
		USE_NLS=no
	fi
fi

if test -f "$srcdir/po/LINGUAS"; then
	ALL_LINGUAS=$(sed -e "/^#/d" "$srcdir/po/LINGUAS")
fi

POFILES=
GMOFILES=
UPDATEPOFILES=
DUMMYPOFILES=
for lang in $ALL_LINGUAS; do
	POFILES="$POFILES $srcdirpre$lang.po"
	GMOFILES="$GMOFILES $srcdirpre$lang.gmo"
	UPDATEPOFILES="$UPDATEPOFILES $lang.po-update"
	DUMMYPOFILES="$DUMMYPOFILES $lang.nop"
done
INST_LINGUAS=
if test -n "$ALL_LINGUAS"; then
	for presentlang in $ALL_LINGUAS; do
		useit=no
		if test -n "$LINGUAS"; then
			desiredlanguages="$LINGUAS"
		else
			desiredlanguages="$ALL_LINGUAS"
		fi
		for desiredlang in $desiredlanguages; do
			case "$desiredlang" in
				"$presentlang"*) useit=yes;;
			esac
		done
		if test $useit = yes; then
			INST_LINGUAS="$INST_LINGUAS $presentlang"
		fi
	done
fi
CATALOGS=
if test -n "$INST_LINGUAS"; then
	for lang in $INST_LINGUAS; do
		CATALOGS="$CATALOGS $lang.gmo"
	done
fi
AC_SUBST(POFILES)
AC_SUBST(GMOFILES)
AC_SUBST(UPDATEPOFILES)
AC_SUBST(DUMMYPOFILES)
AC_SUBST(CATALOGS)
])

AC_DEFUN([TUXBOX_BOXTYPE],[
AC_ARG_WITH(boxtype,
	[  --with-boxtype          valid values: dbox2,tripledragon,dreambox,ipbox,coolstream,generic],
	[case "${withval}" in
		dbox2|dreambox|ipbox|tripledragon|coolstream|generic)
			BOXTYPE="$withval"
			;;
		dm*)
			BOXTYPE="dreambox"
			BOXMODEL="$withval"
			;;
		*)
			AC_MSG_ERROR([bad value $withval for --with-boxtype]) ;;
	esac], [BOXTYPE="coolstream"])

AC_ARG_WITH(boxmodel,
	[  --with-boxmodel         valid for coolstream: hd1, hd2
                          valid for dreambox: dm500, dm500plus, dm600pvr, dm56x0, dm7000, dm7020, dm7025
                          valid for ipbox: ip200, ip250, ip350, ip400],
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
		dm500|dm500plus|dm600pvr|dm56x0|dm7000|dm7020|dm7025)
			if test "$BOXTYPE" = "dreambox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		ip200|ip250|ip350|ip400)
			if test "$BOXTYPE" = "ipbox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
			;;
		*)
			AC_MSG_ERROR([unsupported value $withval for --with-boxmodel])
			;;
	esac], [test "$BOXTYPE" = "coolstream" && BOXMODEL="hd1" || true]
	[if test "$BOXTYPE" = "dreambox" -o "$BOXTYPE" = "ipbox" && test -z "$BOXMODEL"; then
		AC_MSG_ERROR([Dreambox/IPBox needs --with-boxmodel])
	fi])

AC_SUBST(BOXTYPE)
AC_SUBST(BOXMODEL)

AM_CONDITIONAL(BOXTYPE_DBOX2, test "$BOXTYPE" = "dbox2")
AM_CONDITIONAL(BOXTYPE_TRIPLE, test "$BOXTYPE" = "tripledragon")
AM_CONDITIONAL(BOXTYPE_DREAMBOX, test "$BOXTYPE" = "dreambox")
AM_CONDITIONAL(BOXTYPE_IPBOX, test "$BOXTYPE" = "ipbox")
AM_CONDITIONAL(BOXTYPE_COOL, test "$BOXTYPE" = "coolstream")
AM_CONDITIONAL(BOXTYPE_GENERIC, test "$BOXTYPE" = "generic")

AM_CONDITIONAL(BOXMODEL_CS_HD1,test "$BOXMODEL" = "hd1")
AM_CONDITIONAL(BOXMODEL_CS_HD2,test "$BOXMODEL" = "hd2")

AM_CONDITIONAL(BOXMODEL_DM500,test "$BOXMODEL" = "dm500")
AM_CONDITIONAL(BOXMODEL_DM500PLUS,test "$BOXMODEL" = "dm500plus")
AM_CONDITIONAL(BOXMODEL_DM600PVR,test "$BOXMODEL" = "dm600pvr")
AM_CONDITIONAL(BOXMODEL_DM56x0,test "$BOXMODEL" = "dm56x0")
AM_CONDITIONAL(BOXMODEL_DM7000,test "$BOXMODEL" = "dm7000" -o "$BOXMODEL" = "dm7020" -o "$BOXMODEL" = "dm7025")

AM_CONDITIONAL(BOXMODEL_IP200,test "$BOXMODEL" = "ip200")
AM_CONDITIONAL(BOXMODEL_IP250,test "$BOXMODEL" = "ip250")
AM_CONDITIONAL(BOXMODEL_IP350,test "$BOXMODEL" = "ip350")
AM_CONDITIONAL(BOXMODEL_IP400,test "$BOXMODEL" = "ip400")

if test "$BOXTYPE" = "dbox2"; then
	AC_DEFINE(HAVE_DBOX_HARDWARE, 1, [building for a dbox2])
elif test "$BOXTYPE" = "tripledragon"; then
	AC_DEFINE(HAVE_TRIPLEDRAGON, 1, [building for a tripledragon])
elif test "$BOXTYPE" = "dreambox"; then
	AC_DEFINE(HAVE_DREAMBOX_HARDWARE, 1, [building for a dreambox])
elif test "$BOXTYPE" = "ipbox"; then
	AC_DEFINE(HAVE_IPBOX_HARDWARE, 1, [building for an ipbox])
elif test "$BOXTYPE" = "coolstream"; then
	AC_DEFINE(HAVE_COOL_HARDWARE, 1, [building for a coolstream])
elif test "$BOXTYPE" = "generic"; then
	AC_DEFINE(HAVE_GENERIC_HARDWARE, 1, [building for a generic device like a standard PC])
fi

# TODO: do we need more defines?
if test "$BOXMODEL" = "hd1"; then
	AC_DEFINE(BOXMODEL_CS_HD1, 1, [coolstream hd1/neo/neo2/zee])
elif test "$BOXMODEL" = "hd2"; then
	AC_DEFINE(BOXMODEL_CS_HD2, 1, [coolstream tank/trinity/trinity v2/trinity duo/zee²/link])
	AC_DEFINE(ENABLE_CHANGE_OSD_RESOLUTION,1,[enable change the osd resolution])
elif test "$BOXMODEL" = "dm500"; then
	AC_DEFINE(BOXMODEL_DM500, 1, [dreambox 500])
elif test "$BOXMODEL" = "ip200"; then
	AC_DEFINE(BOXMODEL_IP200, 1, [ipbox 200])
elif test "$BOXMODEL" = "ip250"; then
	AC_DEFINE(BOXMODEL_IP250, 1, [ipbox 250])
elif test "$BOXMODEL" = "ip350"; then
	AC_DEFINE(BOXMODEL_IP350, 1, [ipbox 350])
elif test "$BOXMODEL" = "ip400"; then
	AC_DEFINE(BOXMODEL_IP400, 1, [ipbox 400])
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

