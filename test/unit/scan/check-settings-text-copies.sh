#!/bin/sh
# settingsText() hands back a copy, and the copy dies with the statement it was
# read in. A reader that uses the value at once is fine with that. A menu item
# is not: CMenuForwarder and CMenuDForwarder keep the address of a non-empty
# option string they are given and paint through that address every time the
# menu is drawn, so an item built on a settingsText() result paints through a
# pointer to freed memory from its first frame on.
#
# So no settingsText() result may be an argument of a menu item constructor. A
# page that shows a text setting another thread writes keeps a copy of its own,
# hands the item that, and refreshes the copy when it changes the setting.
# setOption() copies what it is given and is not affected.
#
# None of this links into the test binary, so it is read as text, one statement
# at a time, with comments and #if 0 blocks blanked the way the compiler would.
#
# What it can see is the call written into the constructor's own statement. A
# result first kept in a local of the same lifetime and that local handed on,
# or a pointer to the copy taken in one statement and given in the next, is
# the same defect and passes here; that one is left to review.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC/src" ] || {
	echo "usage: check-settings-text-copies.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-settings-text-copies.sh: cannot read $f" >&2; exit 1; }
done

# Below this many forwarder constructions the scan is not reading the tree any
# more; there were 900 or so when this was written.
FLOOR_ITEMS=500

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

: > "$tmp/found"
: > "$tmp/count"
find "$SRC/src" -name '*.cpp' | sort | while read -r f; do
	rel=${f#"$SRC"/}
	awk -v keepstrings=0 -f "$STRIP" "$f" | awk -f "$BLANK" | awk -v file="$rel" \
		-v found="$tmp/found" -v count="$tmp/count" '
		# One statement at a time: the text up to the next semicolon, and the
		# line it started on. Preprocessor lines belong to no statement.
		/^[ \t]*#/ { next }
		{
			t = stmt
			gsub(/[ \t{}]/, "", t)
			if (t == "") {
				start = NR
				stmt = ""
			}
			stmt = stmt " " $0
			while ((i = index(stmt, ";")) > 0) {
				s = substr(stmt, 1, i)
				stmt = substr(stmt, i + 1)
				if (s ~ /CMenuD?Forwarder[ \t]*\(/) {
					n++
					if (s ~ /settingsText[ \t]*\(/)
						printf "%s:%d\n", file, start >> found
				}
			}
		}
		END { printf "%d\n", n + 0 >> count }
	'
done

items=`awk '{ n += $1 } END { print n + 0 }' "$tmp/count"`
if [ "$items" -lt "$FLOOR_ITEMS" ]; then
	echo "check-settings-text-copies.sh: $items menu item constructions read," >&2
	echo "  below the floor of $FLOOR_ITEMS: the scan has stopped matching" >&2
	exit 1
fi

if [ -s "$tmp/found" ]; then
	echo "check-settings-text-copies.sh: a menu item keeps the address of its" >&2
	echo "  option string, and these hand it the temporary settingsText() returns:" >&2
	sed 's/^/  /' "$tmp/found" >&2
	exit 1
fi

exit 0
