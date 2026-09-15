#!/bin/sh
# The label a screen states beside a setting it offers as neither a range nor a table of
# choices. The two scans beside this read the number chooser and the chooser with a
# table; between them they reach half the declared rows, and the rest are offered by a
# forwarder, a string chooser, one of the text inputs or the colour chooser, whose
# labels nothing compares. A row whose label belongs to the item above it on the screen
# is right on the page and wrong on the box.
#
# One row per pairing, tab between: field, label key, and where it was read.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] || { echo "usage: extract-labels.sh <source directory>" >&2; exit 2; }

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

MARK='@@file@@'

find "$SRC/gui" -name '*.cpp' -o -name '*.h' | LC_ALL=C sort > "$tmp/files"
xargs awk -v keepstrings=0 -v mark="$MARK" -f "$STRIP" < "$tmp/files" > "$tmp/code"
awk -v mark="$MARK" -f "$HERE/labels.awk" "$tmp/code" | sort -u > "$tmp/raw"

# The label a call site names is an enumerator and a row declares the string it
# stands for, so it is turned into the other through the one map every scan
# here reads.
sh "$HERE/extract-bounds.sh" -m "$SRC" > "$tmp/locale"

awk -F'\t' -v locfile="$tmp/locale" '
	FILENAME == locfile { loc[$1] = $2; next }
	{ print $1 "\t" (($2 in loc) ? loc[$2] : "?") "\t" $3 }
' "$tmp/locale" "$tmp/raw" > "$tmp/out"

# How many pairings this prints is not held here but in counts.txt beside it,
# which carries every such number the suite has in one place, for the reason
# the bound scan states: a scan that has stopped matching and a screen taken
# out on purpose are the same answer from in here. What is left is the one that
# can only be a fault.
pairs=`awk -F'\t' '$2 != "?"' "$tmp/out" | wc -l`

if [ "$pairs" -eq 0 ]; then
	echo "extract-labels.sh: not one pairing came back with a label the program has," >&2
	echo "  out of `wc -l < "$tmp/raw"` call sites read" >&2
	exit 1
fi

cat "$tmp/out"
