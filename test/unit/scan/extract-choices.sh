#!/bin/sh
# The values a setting is offered under, as the screen that offers it states them.
# CMenuOptionChooser takes its label, the value it edits and a table of choices
# (src/gui/widget/menue.h), and that table is what a declared choice has to agree with:
# a value the screen does not offer, one it offers that the declaration leaves out, and
# a value paired with the wrong words are all invisible to every other check.
#
# One row per call site, tab between: field, label key, where it was read, the table it
# names, and the entries of that table.
#
# An entry is written as value=label, with ! after one a preprocessor arm inside the
# table gates: a choice only one box model is given is one a declaration may leave out
# while an ungated one is not. A value or a label no scan can resolve is written as ?,
# and an empty entry list is a table this cannot read at all.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] || { echo "usage: extract-choices.sh <source directory> [more directories]" >&2; exit 2; }

# Below this the scan is not reading the screens any more. The tree held 210 call
# sites, 192 of them naming a table whose every entry resolved, over 674 entries in 129
# tables when this was written, and 313 of those entries stated their value under a
# name rather than as a number.
#
# The named ones are counted apart from the rest because the site count barely moves
# without them: two thirds of the entries state a plain number.
FLOOR_SITES=190
FLOOR_RESOLVED=170
FLOOR_ENTRIES=600
FLOOR_NAMED=250

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

MARK='@@file@@'

find "$@" -name '*.cpp' -o -name '*.h' | LC_ALL=C sort > "$tmp/files"
xargs awk -v keepstrings=0 -v mark="$MARK" -f "$STRIP" < "$tmp/files" > "$tmp/code"
awk -v mark="$MARK" -f "$HERE/keyvals.awk" "$tmp/code" > "$tmp/entries"
awk -v mark="$MARK" -f "$HERE/choices.awk" "$tmp/code" > "$tmp/sites"

[ -s "$tmp/entries" ] || { echo "extract-choices.sh: no table of choices was read at all" >&2; exit 1; }
[ -s "$tmp/sites" ] || { echo "extract-choices.sh: no call site was read at all" >&2; exit 1; }

entries=`wc -l < "$tmp/entries"`
if [ "$entries" -lt "$FLOOR_ENTRIES" ]; then
	echo "extract-choices.sh: $entries entries is below the floor of $FLOOR_ENTRIES, the scan has stopped matching" >&2
	exit 1
fi

# The names a value can be written under, and the words a label stands for. Both
# maps are the ones the scans beside this read, so a name resolves here to what
# it resolves to there.
sh "$HERE/extract-constants.sh" "$@" > "$tmp/names"
sh "$HERE/extract-bounds.sh" -m "$SRC" > "$tmp/locale"

# One line per table and call site, its entries in order, joined. An entry the
# scan could not read is left in as a question mark rather than dropped, so a
# table it read in part is not mistaken for one it read whole.
#
# The element on the left of an assignment is written before the right is read
# in some awks, so nothing here asks whether it is there from inside its own
# assignment.
awk -F'	' -v namefile="$tmp/names" -v locfile="$tmp/locale" '
	FILENAME == namefile { num[$1] = $2; next }
	FILENAME == locfile { loc[$1] = $2; next }
	{
		e = resolve($2) "=" (($3 in loc) ? loc[$3] : "?") ($4 == 1 ? "!" : "")
		site = $1 "\t" $5
		if (site in text)
			text[site] = text[site] "," e
		else {
			text[site] = e
			order[++n] = site
		}
	}
	END {
		for (i = 1; i <= n; i++) {
			split(order[i], p, "\t")
			print p[1] "\t" text[order[i]]
		}
	}
	# a name qualified by the type that declares it stands for the same value as
	# the bare one, and a value two declarations disagree on is in neither map
	function resolve(x,   b) {
		if (x ~ /^-?[0-9]+$/) return x + 0
		if (x ~ /^-?0[xX][0-9a-fA-F]+$/) return hex(x)
		if (x == "true") return 1
		if (x == "false") return 0
		b = x
		sub(/^.*::/, "", b)
		return (b in num) ? num[b] : "?"
	}
	function hex(x,   d, r, i, c, s, neg) {
		neg = (substr(x, 1, 1) == "-")
		s = tolower(x)
		sub(/^-/, "", s)
		s = substr(s, 3)
		d = "0123456789abcdef"
		r = 0
		for (i = 1; i <= length(s); i++) { c = substr(s, i, 1); r = r * 16 + index(d, c) - 1 }
		return neg ? -r : r
	}
' "$tmp/names" "$tmp/locale" "$tmp/entries" | sort > "$tmp/tables.raw"

# A name two files state with the same entries in the same order is one table;
# stated with different entries it is two, and neither of them answers for a
# call site that names it.
awk -F'	' '
	{
		if ($1 in text) {
			if (text[$1] != $2) text[$1] = ""
		} else {
			text[$1] = $2
			order[++n] = $1
		}
	}
	END { for (i = 1; i <= n; i++) print order[i] "\t" text[order[i]] }
' "$tmp/tables.raw" | sort > "$tmp/tables"

sort -u "$tmp/sites" \
	| awk -F'	' -v tabfile="$tmp/tables" -v locfile="$tmp/locale" '
		FILENAME == tabfile { text[$1] = $2; next }
		FILENAME == locfile { loc[$1] = $2; next }
		{ print $1 "\t" (($2 in loc) ? loc[$2] : "?") "\t" $4 "\t" $3 "\t" (($3 in text) ? text[$3] : "") }
	' "$tmp/tables" "$tmp/locale" - > "$tmp/out"

sites=`wc -l < "$tmp/out"`
if [ "$sites" -lt "$FLOOR_SITES" ]; then
	echo "extract-choices.sh: $sites call sites is below the floor of $FLOOR_SITES, the scan has stopped matching" >&2
	exit 1
fi

# A site whose every entry resolved, counted on its own: a change that left the
# sites standing and turned every value into a question mark would otherwise
# pass while checking none of them.
resolved=`awk -F'	' '$5 != "" && $5 !~ /(^|[,=])\?/' "$tmp/out" | wc -l`
if [ "$resolved" -lt "$FLOOR_RESOLVED" ]; then
	echo "extract-choices.sh: $resolved call sites with every entry resolved is below the floor of $FLOOR_RESOLVED," >&2
	echo "  the scan has stopped reading them; $sites sites and $entries entries were matched" >&2
	exit 1
fi

named=`awk -F'	' '$2 !~ /^-?([0-9]+|0[xX][0-9a-fA-F]+)$/ && $2 != "true" && $2 != "false"' "$tmp/entries" \
	| awk -F'	' -v namefile="$tmp/names" '
		FILENAME == namefile { num[$1] = 1; next }
		{ b = $2; sub(/^.*::/, "", b); if (b in num) n++ }
		END { print n + 0 }
	' "$tmp/names" -`
if [ "$named" -lt "$FLOOR_NAMED" ]; then
	echo "extract-choices.sh: $named entries stating their value under a name resolved, below the floor of $FLOOR_NAMED;" >&2
	echo "  the scan that reads those names has stopped matching" >&2
	exit 1
fi

cat "$tmp/out"
