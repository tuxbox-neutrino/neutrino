#!/bin/sh
# Every name a header binds to a plain integer without arithmetic, one name and one
# value per line. The preprocessor scan beside this reads the names a define gives;
# these are the ones it cannot see, and a table of choices states its values under them
# far more often than under a define.
#
# Two forms and no others. An enumerator, counted the way the standard counts it, and
# only out of a block whose every enumerator is a bare name or a plain integer: one
# carrying an expression is skipped whole rather than counted past, because a value
# guessed from part of a block would read as the program's own. And a constant of
# integer type given a literal.
#
# A name two files give different values to is dropped rather than resolved, for the
# reason the define scan drops one: which of them a source file sees depends on what it
# includes. Dropped, the caller prints it as unresolved and the entry is visibly
# unchecked.
#
# The sources are read beside the headers. A caller writes the name a type declares as
# the type and the name, and answering that from the bare name alone is only safe while
# the bare name is unique; a type declared in a source file whose members went unread is
# exactly the case that would let an unrelated name answer for it, and read, it collides
# and both are dropped.
set -e
LC_ALL=C
export LC_ALL

[ -n "$1" ] || { echo "usage: extract-constants.sh <source directory>..." >&2; exit 2; }

# Below this the scan is not reading the sources any more. The tree held 5687
# names when this was written.
FLOOR=5000

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

MARK='@@file@@'

find "$@" -name '*.h' -o -name '*.cpp' | LC_ALL=C sort > "$tmp/files"
xargs awk -v keepstrings=0 -v mark="$MARK" -f "$STRIP" < "$tmp/files" > "$tmp/code"

{
	awk -v mark="$MARK" -f "$HERE/enums.awk" "$tmp/code"
	grep -oE '(^|[^A-Za-z_0-9])(static[ 	]+)?const[ 	]+(int|long)[ 	]+[A-Za-z_][A-Za-z_0-9]*[ 	]*=[ 	]*\(?-?(0[xX][0-9a-fA-F]+|[0-9]+)\)?[ 	]*;' "$tmp/code" \
		| sed -E 's/.*[ 	]([A-Za-z_][A-Za-z_0-9]*)[ 	]*=[ 	]*\(?(-?(0[xX][0-9a-fA-F]+|[0-9]+))\)?[ 	]*;/\1	\2/' || true
} \
	| sort -u \
	| awk -F'	' '{ v[$1] = $2; n[$1]++ } END { for (k in n) if (n[k] == 1) print k "\t" v[k] }' \
	| LC_ALL=C sort > "$tmp/names"

n=`wc -l < "$tmp/names"`
if [ "$n" -lt "$FLOOR" ]; then
	echo "extract-constants.sh: $n names is below the floor of $FLOOR, the scan has stopped matching" >&2
	exit 1
fi

cat "$tmp/names"
