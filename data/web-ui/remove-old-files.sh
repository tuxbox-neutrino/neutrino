#!/bin/sh
# remove-old-files.sh - takes out of an installed web folder what an earlier
# version of the interface put there and this one no longer has
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# The list names files below the web folder, one per line; a line ending in
# / names a folder that goes with all it holds. A folder a removed file
# leaves empty goes as well. A line with .. or a leading / is skipped, and
# nothing under extensions/ is touched: an installed extension belongs to
# whoever installed it, not to this list. A name whose way leads through a
# link is skipped too, since the link may lead anywhere, extensions/
# included; a listed name that is a link itself goes as the link alone.
#
# usage: remove-old-files.sh <list> <installed web directory>

list=$1
root=$2
[ -f "$list" ] && [ -n "$root" ] && [ -d "$root" ] || {
	echo "usage: $0 <list> <web directory>" >&2
	exit 2
}

# whether a folder on the way to the name is a link
through_link() {
	at=$(dirname "$1")
	while [ "$at" != . ] && [ "$at" != / ]; do
		[ -L "$root/$at" ] && return 0
		at=$(dirname "$at")
	done
	return 1
}

grep -v -e '^#' -e '^$' "$list" | while IFS= read -r path; do
	case "$path" in
	/*|*..*|extensions|extensions/*) continue ;;
	esac
	through_link "$path" && continue
	name=${path%/}
	if [ -L "$root/$name" ]; then
		rm -f "$root/$name"
	elif [ "$name" != "$path" ]; then
		rm -rf "$root/$name"
	else
		rm -f "$root/$name"
	fi
	dir=$(dirname "$name")
	while [ "$dir" != . ] && [ "$dir" != / ]; do
		rmdir "$root/$dir" 2>/dev/null || break
		dir=$(dirname "$dir")
	done
done
exit 0
