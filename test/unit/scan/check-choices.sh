#!/bin/sh
# A row whose set of values is the box's own names that set, and a screen answers
# under that name. The two are written in two files and nothing at run time can see
# them disagree: a row asking under a name nobody answers offers a setting with no
# values at all, on the box and nowhere else, because the answer a case would get
# comes from a fake.
#
# So the two lists are compared here as text, both directions: a name nothing answers
# is a setting closed on the box, and a name nothing asks under is a list kept for
# nobody.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-choices.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
SCREEN="$SRC/src/gui/videosettings.cpp"
for f in "$SCREEN" "$HERE/strip-comments.awk"; do
	[ -r "$f" ] || { echo "check-choices.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The name is the second argument of the macro a row writes.
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC"/src/coreapi/settings/settingstable*.cpp \
	| grep -oE 'COREAPI_NUMBER_FIELD_ASKED\([A-Za-z_][A-Za-z_0-9]*, *"[^"]*"' \
	| sed -E 's/.*"([^"]*)"$/\1/' | sort -u > "$tmp/asked"

# What the screen answers for, read off the comparisons its own answer makes.
# Comments are blanked first, so a name behind // does not satisfy this.
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SCREEN" \
	| grep -oE 'strcmp\(name, *"[^"]*"\)' \
	| sed -E 's/.*"([^"]*)".*/\1/' | sort -u > "$tmp/answered"

asked=`awk 'END { print NR }' "$tmp/asked"`
answered=`awk 'END { print NR }' "$tmp/answered"`

# Below these the scan is not reading either file any more, and a comparison of
# two empty sets holds every time.
if [ "$asked" -lt 1 ]; then
	echo "check-choices.sh: no row asks for a set of values, the scan has stopped matching" >&2
	exit 1
fi
if [ "$answered" -lt 1 ]; then
	echo "check-choices.sh: $SCREEN answers for no set, the scan has stopped matching" >&2
	exit 1
fi

fail=0

unanswered=`comm -23 "$tmp/asked" "$tmp/answered"`
if [ -n "$unanswered" ]; then
	echo "a row asks for a set of values nothing answers for:" >&2
	printf '%s\n' "$unanswered" | sed 's/^/  /' >&2
	fail=1
fi

unasked=`comm -13 "$tmp/asked" "$tmp/answered"`
if [ -n "$unasked" ]; then
	echo "a set of values is answered for and no row asks for it:" >&2
	printf '%s\n' "$unasked" | sed 's/^/  /' >&2
	fail=1
fi

[ "$fail" -eq 0 ] || exit 1

echo "sets of values a row asks for                      $asked"
exit 0
