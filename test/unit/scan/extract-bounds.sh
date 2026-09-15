#!/bin/sh
# The bounds a number setting is offered under, as the screen that offers it states
# them. CMenuOptionNumberChooser takes its label, the value it edits, whether it is
# active, then the minimum and the maximum in that order (src/gui/widget/menue.h). A
# table written from memory rather than from here has had the minimum and the maximum
# the wrong way round.
#
# One row per call site, tab between: field, minimum, maximum, label key, and where it
# was read. A bound no scan can turn into a number is printed as ? rather than dropped,
# so a row that cannot be checked is visible as one.
#
# With -l instead, every locale name the program has: the weaker question, and the only
# one that can be asked of a label the screens state somewhere other than in the call
# this reads. With -m, the enumerator beside the name it stands for, which is what a
# scan reading a locale out of some other call needs.
set -e
LC_ALL=C
export LC_ALL

LOCALE_ONLY=""
MAP_ONLY=""
if [ "$1" = "-l" ]; then
	LOCALE_ONLY=1
	shift
elif [ "$1" = "-m" ]; then
	MAP_ONLY=1
	shift
fi

SRC="$1"
[ -n "$SRC" ] || { echo "usage: extract-bounds.sh [-l|-m] <source directory>" >&2; exit 2; }

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"

LOCALS="$SRC/system/locals.h"
NAMES="$SRC/system/locals_intern.h"
for f in "$LOCALS" "$NAMES"; do
	[ -r "$f" ] || { echo "extract-bounds.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The label a call site names is an enumerator and a row declares the string it
# stands for, so one is turned into the other here. The two lists are written in
# step and nothing but their order binds them, which is why the lengths are
# compared before either is used.
grep -oE '^[ 	]*(NONEXISTANT_LOCALE|LOCALE_[A-Z0-9_]+),' "$LOCALS" \
	| sed -E 's/^[ 	]*([A-Z0-9_]+),$/\1/' > "$tmp/symbols"
grep -oE '^[ 	]*"[^"]*",' "$NAMES" | sed -E 's/^[ 	]*"(.*)",$/\1/' > "$tmp/strings"
ns=`wc -l < "$tmp/symbols"`
nn=`wc -l < "$tmp/strings"`
if [ "$ns" -ne "$nn" ] || [ "$ns" -lt 3000 ]; then
	echo "extract-bounds.sh: $ns locale names against $nn strings, the two lists no longer line up" >&2
	exit 1
fi
paste "$tmp/symbols" "$tmp/strings" > "$tmp/locale"

if [ -n "$LOCALE_ONLY" ]; then
	cut -f2 "$tmp/locale" | sort -u
	exit 0
fi

if [ -n "$MAP_ONLY" ]; then
	cat "$tmp/locale"
	exit 0
fi

# The names a bound can be written under, resolved through the one map both
# scans read. The one in the tree today is the value the long press duration
# means off by.
sh "$HERE/extract-defines.sh" "$SRC" > "$tmp/defines"

find "$SRC/gui" -name '*.cpp' -o -name '*.h' | LC_ALL=C sort > "$tmp/files"
while read -r f; do
	awk -v keepstrings=0 -f "$STRIP" "$f" | awk -v where="$f" -f "$HERE/bounds.awk"
done < "$tmp/files" | sort -u > "$tmp/raw"

# The names are resolved after the scan rather than inside it, so the scan has
# one job and a name it cannot resolve stays visible as the name it was.
awk -F'\t' -v locfile="$tmp/locale" -v deffile="$tmp/defines" '
	FILENAME == locfile { loc[$1] = $2; next }
	FILENAME == deffile { def[$1] = $2; next }
	{ print $1 "\t" resolve($2, def) "\t" resolve($3, def) "\t" (($4 in loc) ? loc[$4] : "?") "\t" $5 }
	function resolve(v, d) {
		if (v ~ /^-?[0-9]+$/) return v
		if (v in d) return d[v]
		return "?"
	}
' "$tmp/locale" "$tmp/defines" "$tmp/raw" > "$tmp/out"

# How many rows this prints is not held here. Every one of them is counted
# again by the suite, which holds all of its counts in counts.txt beside it: a
# scan that has stopped matching and a screen somebody took out on purpose look
# the same from in here, and one file that carries every such number is what
# lets the two be told apart in one diff rather than in a number per script.
# What is left here is the one answer that can only be a fault.
n=`awk -F'\t' '$2 != "?" && $3 != "?"' "$tmp/out" | wc -l`
if [ "$n" -eq 0 ]; then
	echo "extract-bounds.sh: not one row came back with both bounds resolved," >&2
	echo "  out of `wc -l < "$tmp/raw"` call sites read" >&2
	exit 1
fi

cat "$tmp/out"
