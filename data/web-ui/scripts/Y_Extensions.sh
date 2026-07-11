#!/bin/sh
# Y_Extensions.sh — install/update/uninstall yWeb extensions for the
# modern extension manager (settings.yhtm, exec contract).
#
# Archive contract: a .tar.gz with exactly ONE top-level folder that
# contains manifest.json; it unpacks to <webroot>/extensions/<folder>.
# Shell scripts shipped in <folder>/scripts/*.sh are copied to
# <webroot>/scripts on install and removed again on uninstall.
# An update is simply a re-install: config keys (ext_<dir>_enabled/
# _slot and extension settings) are not touched and survive.

# conventional library header; degrade gracefully when the build-time
# placeholders were not expanded (dev webroot)
[ -r "%(PRIVATE_HTTPDDIR)/scripts/_Y_Globals.sh" ] && . "%(PRIVATE_HTTPDDIR)/scripts/_Y_Globals.sh"
[ -r "%(PRIVATE_HTTPDDIR)/scripts/_Y_Library.sh" ] && . "%(PRIVATE_HTTPDDIR)/scripts/_Y_Library.sh"

BNAME=${0##*/}

# resolve the webroot from the script location (<webroot>/scripts/) so
# the script works independent of placeholder expansion
SCRIPTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WEBROOT=${SCRIPTDIR%/scripts}
EXTROOT="$WEBROOT/extensions"

UNPACK="/tmp/yweb-ext-unpack.$$"
DOWNLOAD="/tmp/yweb-ext-download.$$.tar.gz"
cleanup() { rm -rf "${UNPACK:?}" "$DOWNLOAD"; }
trap cleanup EXIT

fail() {
	echo "[$BNAME] error: $*"
	exit 0
}

# extension folder names are config keys and URL parts — keep them strict
check_name() {
	echo "$1" | grep -Eq '^[a-zA-Z0-9][a-zA-Z0-9_-]*$'
}

manifest_version() {
	sed -n 's/.*"version"[^"]*"\([^"]*\)".*/\1/p' "$1" | head -n 1
}

do_install_archive() {
	archive=$1
	[ -n "$archive" ] && [ -f "$archive" ] || fail "archive not found: $archive"

	# reject entries that could escape the target folder
	if tar -tzf "$archive" 2>/dev/null | grep -Eq '(^|/)\.\.(/|$)|^/'; then
		fail "archive contains unsafe paths"
	fi

	rm -rf "${UNPACK:?}"
	mkdir -p "$UNPACK" || fail "cannot create unpack dir"
	tar -xzf "$archive" -C "$UNPACK" 2>/dev/null || fail "cannot unpack archive"

	# exactly ONE top-level folder wanted
	count=0
	extname=
	for e in "$UNPACK"/* "$UNPACK"/.[!.]*; do
		[ -e "$e" ] || continue
		count=$((count + 1))
		extname=${e##*/}
	done
	{ [ "$count" = "1" ] && [ -d "$UNPACK/$extname" ]; } || fail "archive must contain exactly one top-level folder"
	check_name "$extname" || fail "invalid extension folder name: $extname"
	[ -f "$UNPACK/$extname/manifest.json" ] || fail "manifest.json missing in archive"

	extdir="$EXTROOT/$extname"
	mkdir -p "$EXTROOT"
	rm -rf "${extdir:?}"
	mv "$UNPACK/$extname" "$extdir" || fail "cannot move extension into $EXTROOT"

	# script contract: shipped shell scripts go to <webroot>/scripts
	if [ -d "$extdir/scripts" ]; then
		for s in "$extdir"/scripts/*.sh; do
			[ -f "$s" ] || continue
			cp "$s" "$WEBROOT/scripts/" && chmod 755 "$WEBROOT/scripts/${s##*/}"
		done
	fi

	echo "ok: $extname $(manifest_version "$extdir/manifest.json")"
}

do_install_url() {
	url=$1
	[ -n "$url" ] || fail "no url given"
	rm -f "$DOWNLOAD"
	wget -q -O "$DOWNLOAD" "$url" || fail "download failed: $url"
	do_install_archive "$DOWNLOAD"
}

do_uninstall() {
	extname=$1
	check_name "$extname" || fail "invalid extension folder name: $extname"
	extdir="$EXTROOT/$extname"
	[ -d "$extdir" ] || fail "not installed: $extname"

	# remove the scripts this extension shipped (by name, see contract)
	if [ -d "$extdir/scripts" ]; then
		for s in "$extdir"/scripts/*.sh; do
			[ -f "$s" ] || continue
			rm -f "$WEBROOT/scripts/${s##*/}"
		done
	fi

	rm -rf "${extdir:?}"
	echo "ok: $extname removed"
}

case "$1" in
	install_archive)
		do_install_archive "$2"
	;;
	install_url)
		do_install_url "$2"
	;;
	uninstall)
		do_uninstall "$2"
	;;
	fetch_index)
		# CORS/network fallback for update checks: the box fetches the index
		[ -n "$2" ] || fail "no url given"
		wget -q -O - "$2" || fail "download failed: $2"
	;;
	*)
		echo "[$BNAME] Parameter wrong: $*"
	;;
esac

exit 0
