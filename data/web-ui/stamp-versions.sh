#!/bin/sh
# stamp-versions.sh - names every script and style the pages load by its
# content, at install time
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# A page loads /js/<name>.js?v=... and /css/<name>.css?v=...; the server
# answers those with a lifetime of a day, and the page itself is asked for
# again every time. So the part after ?v= decides when a browser reads a
# script or style anew: it is set here to the first eight hex digits of the
# file's md5, which changes exactly when the file does, instead of a number
# somebody has to remember to raise. Scripts that load other scripts are
# stamped first, so a page names them as they are after their own stamping.
#
# usage: stamp-versions.sh <installed web directory>

root=$1
[ -n "$root" ] && [ -d "$root" ] || { echo "usage: $0 <web directory>" >&2; exit 2; }

stamp() {
	file=$1
	# every distinct reference with a version, as /dir/name.ext
	refs=$(grep -oE '/(js|css)/[A-Za-z0-9._-]+\.(js|css)\?v=[A-Za-z0-9]+' "$file" | sed 's/?v=.*//' | sort -u)
	for ref in $refs; do
		target="$root$ref"
		[ -f "$target" ] || continue
		sum=$(md5sum "$target" | cut -c1-8)
		esc=$(printf '%s' "$ref" | sed 's/[.]/\\./g')
		sed -i "s|$esc?v=[A-Za-z0-9]*|$ref?v=$sum|g" "$file" || return 1
	done
}

for f in "$root"/js/*.js; do
	[ -f "$f" ] && { stamp "$f" || exit 1; }
done
for f in "$root"/*.html; do
	[ -f "$f" ] && { stamp "$f" || exit 1; }
done
exit 0
