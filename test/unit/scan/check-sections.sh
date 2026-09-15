#!/bin/sh
# One accessor per section, and three places that have to name the same set of them:
# the header declares them, the join calls them, and a case walks them.
#
# A section the join forgets loses its rows from the linked table, and every case
# beside it walks that table, so all of them go on passing over what is left. The
# case that answers for it can only walk accessors it names. Neither of those is
# visible from inside the suite, which is why the three lists are compared here as
# text.
#
# Comments are blanked first.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-sections.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
HEADER="$SRC/src/coreapi/settings/settingstable.h"
JOIN="$SRC/src/coreapi/settings/settingstable.cpp"
CASE="$HERE/../coreapi/test_settingstable.cpp"
for f in "$HEADER" "$JOIN" "$CASE" "$HERE/strip-comments.awk"; do
	[ -r "$f" ] || { echo "check-sections.sh: cannot read $f" >&2; exit 1; }
done

# Below this the scan is not reading the three any more. The tree holds fifteen
# section accessors.
FLOOR=10

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The count of the joined table is named the same way and is not a section.
names() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1" \
		| grep -oE 'settingsTable[A-Z][A-Za-z0-9]*' | grep -v '^settingsTableCount$' | sort -u
}

names "$HEADER" > "$tmp/declared"
names "$JOIN" > "$tmp/joined"
names "$CASE" > "$tmp/walked"

n=`awk 'END { print NR }' "$tmp/declared"`
if [ "$n" -lt "$FLOOR" ]; then
	echo "check-sections.sh: $n section accessors read out of $HEADER," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

fail=0
compare() {
	only=`comm -23 "$1" "$2"`
	[ -n "$only" ] || return 0
	echo "$3:" >&2
	printf '%s\n' "$only" | sed 's/^/  /' >&2
	fail=1
}

compare "$tmp/declared" "$tmp/joined" "a section accessor the header declares that the join never calls"
compare "$tmp/joined" "$tmp/declared" "a section accessor the join calls that the header does not declare"
compare "$tmp/declared" "$tmp/walked" "a section accessor no case walks"
compare "$tmp/walked" "$tmp/declared" "a section accessor a case walks that the header does not declare"

[ "$fail" -eq 0 ] || exit 1

echo "section accessors                                  $n"
exit 0
