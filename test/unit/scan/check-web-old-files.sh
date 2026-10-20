#!/bin/sh
# check-web-old-files.sh - the install over an earlier interface takes out
# what that one had and this one does not, and nothing else
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# data/web-ui/old-files.txt names the files earlier versions installed into
# the web folder, and remove-old-files.sh takes them out when this version is
# installed. Two things can go wrong with that: a file that is in the interface
# again but still on the list would be deleted by every install, and the
# removal could reach further than its list. So: no listed name is in the web
# folder's source, and a web folder laid out as an old install leaves the
# removal with the files of this version, the extensions somebody installed
# and the viewer package of the API reference intact, and none of the list.
#
# usage: check-web-old-files.sh <top_srcdir>

SRC=$1
WEB=$SRC/data/web-ui
LIST=$WEB/old-files.txt
[ -f "$LIST" ] && [ -f "$WEB/remove-old-files.sh" ] || {
	echo "check-web-old-files.sh: $LIST or remove-old-files.sh missing" >&2
	exit 1
}
fail=0
names=`grep -v -e '^#' -e '^$' "$LIST"`

for p in $names; do
	case "$p" in
	/*|*..*|extensions|extensions/*)
		echo "check-web-old-files.sh: '$p' is not a name below the web folder the list may hold" >&2
		fail=1
		;;
	esac
	if [ -e "$WEB/${p%/}" ]; then
		echo "check-web-old-files.sh: '$p' is part of the interface again; take it off old-files.txt" >&2
		fail=1
	fi
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
root=$tmp/web
mkdir -p "$root"
for p in $names; do
	case "$p" in
	*/) mkdir -p "$root/$p"; echo copy > "$root/${p}copied-extension-script.sh" ;;
	*) mkdir -p "$root/`dirname "$p"`"; echo old > "$root/$p" ;;
	esac
done
# what has to stay: a page of this version, an extension, the viewer package
mkdir -p "$root/extensions/camds" "$root/swagger/vendor" "$root/js"
echo page > "$root/index.html"
echo js > "$root/js/webui-header.js"
echo ext > "$root/extensions/camds/manifest.json"
echo ext > "$root/extensions/camds/Y_Tools.sh"
echo vendor > "$root/swagger/vendor/swagger-ui-bundle.js.gz"

sh "$WEB/remove-old-files.sh" "$LIST" "$root" || {
	echo "check-web-old-files.sh: remove-old-files.sh failed" >&2
	exit 1
}

# links: a listed folder that is a link goes as the link and leaves what it
# points to, and a name whose way leads through a link is left alone
links=$tmp/links
mkdir -p "$links/web/extensions/foo/scripts" "$links/web/extensions/foo/img"
echo keep > "$links/web/extensions/foo/scripts/Y_Tools.sh"
echo keep > "$links/web/extensions/foo/img/accept.png"
ln -s extensions/foo/scripts "$links/web/scripts"
ln -s extensions/foo/img "$links/web/images"
printf 'scripts/\nimages/accept.png\n' > "$links/list"
sh "$WEB/remove-old-files.sh" "$links/list" "$links/web"
[ -L "$links/web/scripts" ] && {
	echo "check-web-old-files.sh: the listed folder that is a link is still there" >&2
	fail=1
}
for p in scripts/Y_Tools.sh img/accept.png; do
	[ -f "$links/web/extensions/foo/$p" ] || {
		echo "check-web-old-files.sh: the removal followed a link into extensions/foo/$p" >&2
		fail=1
	}
done

for p in $names; do
	if [ -e "$root/${p%/}" ]; then
		echo "check-web-old-files.sh: '$p' is still there after the removal" >&2
		fail=1
	fi
done
for p in index.html js/webui-header.js extensions/camds/manifest.json extensions/camds/Y_Tools.sh \
		swagger/vendor/swagger-ui-bundle.js.gz; do
	[ -f "$root/$p" ] || {
		echo "check-web-old-files.sh: '$p' was taken out but is not on the list" >&2
		fail=1
	}
done
for d in images jquery styles prototype; do
	[ -d "$root/$d" ] && {
		echo "check-web-old-files.sh: the emptied folder '$d' is still there" >&2
		fail=1
	}
done

[ $fail -eq 0 ] && echo "check-web-old-files.sh: `echo "$names" | wc -l` old names, the install takes out exactly these"
exit $fail
