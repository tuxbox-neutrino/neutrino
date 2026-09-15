#!/bin/sh
# Every plain integer a header in the tree gives a name to, one name and one value per
# line. A bound and a default are both written as one of these where the number itself
# would say nothing, so both scans resolve names through here rather than each keeping
# a map of its own.
#
# A name two headers give different values to is dropped rather than resolved, because
# which of the two a source file sees depends on what it includes. Dropped, the caller
# prints it as unresolved and the row is visibly unchecked; guessed, it would read as a
# value the program does not use.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] || { echo "usage: extract-defines.sh <source directory>" >&2; exit 2; }

# Below this the scan is not reading the headers any more. The tree held 479
# names when this was written, out of 480 the headers give, the one dropped
# being a name three of them give three different values.

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

find "$SRC" -name '*.h' | LC_ALL=C sort > "$tmp/headers"
while read -r f; do
	grep -hoE '^[ 	]*#[ 	]*define[ 	]+[A-Za-z_][A-Za-z_0-9]*[ 	]+\(?-?[0-9]+\)?[ 	]*$' "$f" || true
done < "$tmp/headers" \
	| sed -E 's/^[ 	]*#[ 	]*define[ 	]+([A-Za-z_][A-Za-z_0-9]*)[ 	]+\(?(-?[0-9]+)\)?[ 	]*$/\1	\2/' \
	| sort -u \
	| awk -F'\t' '{ v[$1] = $2; n[$1]++ } END { for (k in n) if (n[k] == 1) print k "\t" v[k] }' \
	| LC_ALL=C sort > "$tmp/defines"

n=`wc -l < "$tmp/defines"`
if [ "$n" -lt "$FLOOR" ]; then
	echo "extract-defines.sh: $n names is below the floor of $FLOOR, the scan has stopped matching" >&2
	exit 1
fi

cat "$tmp/defines"
