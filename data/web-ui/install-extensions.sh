#!/bin/sh
# install-extensions.sh - installs every extension the pages carry along
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# Every folder under extensions/ is an extension and goes in as it is, with
# no line in the core naming it: directories 0755, files 0644, the shell
# scripts in its scripts/ 0755, as the installer of the pages lays out one it
# takes from an archive. Links are left out, as the installer refuses them.
# Nothing in an extension is touched on the way, so one carried along reads
# the same as one installed later.
#
# usage: install-extensions.sh <source extensions/> <installed extensions/>
#        install-extensions.sh --remove <source extensions/> <installed extensions/>

remove=
if [ "$1" = "--remove" ]; then
	remove=1
	shift
fi
src=$1
dst=$2
[ -n "$src" ] && [ -n "$dst" ] || { echo "usage: $0 [--remove] <source> <installed>" >&2; exit 2; }
[ -d "$src" ] || exit 0

for d in "$src"/*/; do
	[ -d "$d" ] || continue
	[ -L "${d%/}" ] && continue
	name=$(basename "$d")
	if [ -n "$remove" ]; then
		rm -rf "${dst:?}/${name:?}"
		continue
	fi
	( cd "$d" && find . -type d ) | while IFS= read -r sub; do
		mkdir -p "$dst/$name/$sub" && chmod 755 "$dst/$name/$sub" || exit 1
	done || exit 1
	( cd "$d" && find . -type f ) | while IFS= read -r f; do
		mode=644
		case "$f" in ./scripts/*.sh) mode=755 ;; esac
		cp "$d/$f" "$dst/$name/$f" && chmod "$mode" "$dst/$name/$f" || exit 1
	done || exit 1
done
exit 0
