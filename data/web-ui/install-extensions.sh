#!/bin/sh
# install-extensions.sh - installs every extension the pages carry along
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# Every folder under extensions/ is an extension and goes in as it is, with
# no line in the core naming it: directories 0755, files 0644, and the shell
# scripts its manifest names under "scripts" (scripts/<name>.sh) 0755, as the
# installer of the pages lays out one it takes from an archive. The manifest
# is read as the box reads it, strict JSON, by python3; a manifest that is
# not, or a script it names that is not there, stops the install. Links are
# left out, as the installer refuses them. Nothing in an extension is touched
# on the way.
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

# the scripts a manifest names, one per line
named_scripts() {
	python3 -I - "$1" <<'EOF'
import json, re, sys
def refuse(constant):
    raise ValueError('not JSON: ' + constant)
manifest = json.loads(open(sys.argv[1], encoding='utf-8-sig').read(), parse_constant=refuse)
if not isinstance(manifest, dict):
    sys.exit('the manifest is not a JSON object')
names = manifest.get('scripts', [])
if not isinstance(names, list) or len(names) > 64:
    sys.exit('the manifest names its scripts in something other than a list of up to 64')
for n in names:
    if not isinstance(n, str) or not re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9._-]{0,63}', n):
        sys.exit('the manifest names a script other than by letters, digits, dots, dashes and underscores')
    print(n)
EOF
}

for d in "$src"/*/; do
	[ -d "$d" ] || continue
	[ -L "${d%/}" ] && continue
	name=$(basename "$d")
	if [ -n "$remove" ]; then
		rm -rf "${dst:?}/${name:?}"
		continue
	fi
	scripts=$(named_scripts "$d/manifest.json") || { echo "$0: $name: manifest.json" >&2; exit 1; }
	( cd "$d" && find . -type d ) | while IFS= read -r sub; do
		mkdir -p "$dst/$name/$sub" && chmod 755 "$dst/$name/$sub" || exit 1
	done || exit 1
	( cd "$d" && find . -type f ) | while IFS= read -r f; do
		cp "$d/$f" "$dst/$name/$f" && chmod 644 "$dst/$name/$f" || exit 1
	done || exit 1
	for s in $scripts; do
		[ -f "$d/scripts/$s.sh" ] && [ ! -L "$d/scripts/$s.sh" ] || { echo "$0: $name: no scripts/$s.sh" >&2; exit 1; }
		chmod 755 "$dst/$name/scripts/$s.sh" || exit 1
	done
done
exit 0
