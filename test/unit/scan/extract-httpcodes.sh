#!/bin/sh
# Every status code this server has a name for, as the name and the number separated by
# a tab, in the order the header declares them.
#
# The pair that turns a code into a domain status and back is checked against this
# rather than against a list written out beside it: the one this replaced was
# transcribed before the login throttle existed and never gained the code that throttle
# answers with, so the mapping for that code could be changed to the one its own comment
# calls the near miss and nothing said anything.
#
# Comments are blanked first.
set -e
LC_ALL=C
export LC_ALL

HEADER="$1"
FLOOR="$2"
[ -n "$HEADER" ] && [ -n "$FLOOR" ] || {
	echo "usage: extract-httpcodes.sh <header> <floor>" >&2
	exit 2
}
[ -r "$HEADER" ] || { echo "extract-httpcodes.sh: cannot read $HEADER" >&2; exit 1; }

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "extract-httpcodes.sh: cannot read $STRIP" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$HEADER" > "$tmp/code"

# A name bound to a plain number and nothing else. A value written as anything
# but digits is not read at all rather than read in part, because a number this
# works out from half an expression would read here as the program's own.
grep -oE '(^|[^A-Za-z_0-9])const[ 	]+int[ 	]+Status[A-Za-z_0-9]*[ 	]*=[ 	]*[0-9]+[ 	]*;' "$tmp/code" \
	| sed -E 's/.*[ 	](Status[A-Za-z_0-9]*)[ 	]*=[ 	]*([0-9]+)[ 	]*;/\1	\2/' > "$tmp/names" || true

n=`awk 'END { print NR }' "$tmp/names"`
if [ "$n" -lt "$FLOOR" ]; then
	echo "extract-httpcodes.sh: $n codes read out of $HEADER," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

# One name twice is two declarations of one code, and a walk over both would
# read as agreement about a set the header does not have.
dupes=`cut -f1 "$tmp/names" | LC_ALL=C sort | uniq -d`
if [ -n "$dupes" ]; then
	echo "extract-httpcodes.sh: read the same name twice out of $HEADER:" >&2
	echo "$dupes" >&2
	exit 1
fi

cat "$tmp/names"
