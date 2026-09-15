#!/bin/sh
# How many sections carry declared rows and nothing that applies them, and which. A
# section with no applier answers a write with ok and changes nothing on a running
# box, and at run time that is indistinguishable from a section with nothing to
# apply.
#
# A section is the wrong unit for coverage and this used to print it as though it
# were the right one. A section with an applier says nothing about how many of its
# rows are acted on; the rows are what a caller writes and the rows are what the
# figures below the section counts are in. Those come out of counts.txt.
#
# The list below is what may still be in that state and why. A section not on it that
# has no applier fails, and so does one on it that has gained one: the list only ever
# shrinks, and the day it is empty this refuses every section without an applier.
#
# It reads the registration table as text, because the object it lives in cannot be
# linked into the test binary. Comments are blanked first.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-appliers.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APPLIERS="$SRC/src/gui/settings_appliers.cpp"
for f in "$APPLIERS" "$HERE/strip-comments.awk"; do
	[ -r "$f" ] || { echo "check-appliers.sh: cannot read $f" >&2; exit 1; }
done

# One line per section that may carry rows and no applier, the reason after the
# name. Nothing else may.
WITHOUT='general	the language notifier is private to its screen and the timezone one only ever runs at a restart
channel	the scan screen is the only notifier there and it names none of the keys the section declares
player	neither the audio player nor the picture viewer setup screen has a notifier
network	the one notifier its rows have ends in a request the shared client answers by ending the process
parental	the screen has no notifier
weather	the screen notifier writes through a menu item pointer of its own page
hdd	the one notifier there is bound to a local of the screen and to no setting'

# Below this the scan is not reading the tables any more. The tree held sixteen
# sections when this was written.
SECTION_FLOOR=10

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The third string of a row is its section.
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC"/src/coreapi/settings/settingstable*.cpp \
	| grep -oE '"[A-Za-z_0-9]+", ValueType::[A-Za-z]+, "[a-z_]+"' \
	| sed -E 's/.*"([a-z_]+)"$/\1/' | sort -u > "$tmp/sections"

awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$APPLIERS" \
	| grep -oE '\{[ 	]*"[a-z_]+",[ 	]*&[A-Za-z_0-9]+' \
	| sed -E 's/^\{[ 	]*"([a-z_]+)".*$/\1/' | sort -u > "$tmp/registered"

sections=`awk 'END { print NR }' "$tmp/sections"`
registered=`awk 'END { print NR }' "$tmp/registered"`

if [ "$sections" -lt "$SECTION_FLOOR" ]; then
	echo "check-appliers.sh: $sections sections read out of the tables, the scan has stopped matching" >&2
	exit 1
fi
if [ "$registered" -lt 1 ]; then
	echo "check-appliers.sh: no registration read out of $APPLIERS, the scan has stopped matching" >&2
	exit 1
fi

printf '%s\n' "$WITHOUT" | cut -f1 | sort > "$tmp/allowed"

comm -23 "$tmp/sections" "$tmp/registered" > "$tmp/without"
without=`awk 'END { print NR }' "$tmp/without"`

fail=0

unreasoned=`comm -23 "$tmp/without" "$tmp/allowed"`
if [ -n "$unreasoned" ]; then
	echo "a section carries rows and has no applier, with no reason given:" >&2
	printf '%s\n' "$unreasoned" | sed 's/^/  /' >&2
	fail=1
fi

stale=`comm -13 "$tmp/without" "$tmp/allowed"`
if [ -n "$stale" ]; then
	echo "listed as having no applier and now has one, take it off the list:" >&2
	printf '%s\n' "$stale" | sed 's/^/  /' >&2
	fail=1
fi

orphan=`comm -13 "$tmp/sections" "$tmp/registered"`
if [ -n "$orphan" ]; then
	echo "an applier is registered for a section no row declares:" >&2
	printf '%s\n' "$orphan" | sed 's/^/  /' >&2
	fail=1
fi

[ "$fail" -eq 0 ] || exit 1

COUNTS="$HERE/../support/counts.txt"
[ -r "$COUNTS" ] || { echo "check-appliers.sh: cannot read $COUNTS" >&2; exit 1; }
figure()
{
	v=`awk -F'\t' -v n="$1" '$1 == n { print $2; found = 1 } END { exit found ? 0 : 1 }' "$COUNTS"` || {
		echo "check-appliers.sh: $COUNTS holds no figure named $1" >&2
		exit 1
	}
	printf '%s' "$v"
}

linked=`figure "rows this build links"`
covered=`figure "rows in a section with an applier"`
acted=`figure "rows an applier acts on"`

echo "settings sections                                  $sections"
echo "sections with an applier registered                `expr $sections - $without`"
echo "sections carrying rows and with none               $without"
while read -r name; do
	printf '  %s\n' "`printf '%s\n' "$WITHOUT" | awk -F'\t' -v n="$name" '$1 == n { printf "%-12s%s", $1, $2 }'`"
done < "$tmp/without"
echo "rows a section with an applier declares            $covered of $linked"
echo "rows an applier acts on                            $acted of $covered"
exit 0
