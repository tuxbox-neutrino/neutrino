#!/bin/sh
# Every enumerator of one scoped enum, in the order the header declares them, as name
# and value separated by a tab.
#
# The suite walks such a set by hand where the walk has to do something with each
# member, because a walk derived from the same text the thing under test is written
# beside would pass over a member neither of them mentions. What a hand written list
# cannot do is say whether it is still the whole set, and this is the other half.
#
# Comments are blanked first, so an enumerator behind // or inside a block comment
# counts as absent here exactly as it does to the compiler.
#
# An enumerator that states a value of its own is refused rather than read. The value
# printed here is the position, a walk casts it back to the enum, and that only stays
# true while every member takes the next one. Neither enum this is run over states one.
#
# A declaration of the enum without its members is passed over rather than entered: a
# scan that entered it would read the code between the two as members and then refuse
# the file over the first equals sign in it, which names a cause that is not the cause.
#
# An enum whose closing brace never arrives is its own refusal for the same reason.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
REL="$2"
ENUM="$3"
FLOOR="$4"
[ -n "$SRC" ] && [ -d "$SRC" ] && [ -n "$REL" ] && [ -n "$ENUM" ] && [ -n "$FLOOR" ] || {
	echo "usage: extract-enum.sh <top source directory> <header below it> <enum name> <floor>" >&2
	exit 2
}

HERE=`dirname "$0"`
HEADER="$SRC/$REL"
STRIP="$HERE/strip-comments.awk"
for f in "$HEADER" "$STRIP"; do
	[ -r "$f" ] || { echo "extract-enum.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$HEADER" > "$tmp/code"

# Read rather than discarded on both boundary lines: an enumerator written after
# the opening brace, or before the closing one, is one the compiler reads, and a
# scan that skipped either would leave it out of the set without saying so.
set +e
awk -v want="$ENUM" '
	BEGIN { inenum = 0; count = 0; valued = 0; ranon = 0 }
	!inenum && $0 ~ ("(^|[^A-Za-z_0-9])enum[ \t]+class[ \t]+" want "([^A-Za-z_0-9]|$)") {
		# A declaration and not the enum: it ends before any member is named,
		# so there is nothing here to read and the one with members is further
		# down.
		if (index($0, "{") == 0 && index($0, ";") > 0)
			next
		inenum = 1
		line = $0
		if (index(line, "{") == 0)
			next
		sub(/^[^{]*\{/, "", line)
		if (index(line, "}") > 0) {
			sub(/\}.*$/, "", line)
			inenum = 0
		}
		note(emit(line))
		next
	}
	inenum {
		line = $0
		closing = index(line, "}") > 0
		if (closing)
			sub(/\}.*$/, "", line)
		# Read to the end of the enum even after a member has been refused,
		# because whether the brace ever arrives is the difference between an
		# enum this cannot read and an enum this ran off the end of, and the two
		# send whoever reads the message to two different lines. Nothing more is
		# printed once one has been refused, and what was printed before it goes
		# to a file the caller does not cat on a refusal.
		note(emit(line))
		if (closing)
			inenum = 0
		next
	}
	# Read last, so that a scan which ran off the end of the enum answers for
	# that rather than for whatever it tripped over afterwards. The brace still
	# open at the end of the file is the same fault seen from the other side.
	END {
		if (inenum || ranon)
			exit 4
		if (valued)
			exit 3
	}
	function note(r) {
		if (r == 1)
			valued = 1
		else if (r == 2)
			ranon = 1
	}
	# Nought for a line of members, one for a member stating a value of its
	# own, and two for a line that is not a list of members at all, which is
	# what the scan reads when the enum it is inside was never closed.
	function emit(line,   n, parts, i, nm, eq, name) {
		gsub(/[ \t]/, "", line)
		n = split(line, parts, ",")
		for (i = 1; i <= n; i++) {
			nm = parts[i]
			if (nm == "")
				continue
			eq = index(nm, "=")
			if (eq > 0) {
				name = substr(nm, 1, eq - 1)
				if (name ~ /^[A-Za-z_][A-Za-z_0-9]*$/ && index(nm, ";") == 0)
					return 1
				return 2
			}
			if (nm ~ /^[A-Za-z_][A-Za-z_0-9]*$/) {
				print nm "\t" count
				count++
			}
		}
		return 0
	}
' "$tmp/code" > "$tmp/names"
rc=$?
set -e

if [ "$rc" -eq 3 ]; then
	echo "extract-enum.sh: $ENUM in $HEADER states a value of its own for an enumerator," >&2
	echo "  so the positions this prints are no longer the values the compiler assigns" >&2
	exit 1
fi

if [ "$rc" -eq 4 ]; then
	echo "extract-enum.sh: $ENUM in $HEADER is never closed," >&2
	echo "  so everything after it would have been read as one of its members" >&2
	exit 1
fi
[ "$rc" -eq 0 ] || exit "$rc"

n=`awk 'END { print NR }' "$tmp/names"`
if [ "$n" -lt "$FLOOR" ]; then
	echo "extract-enum.sh: $n enumerators read for $ENUM out of $HEADER," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

# One name twice would make the comparison against a walk read as agreement over
# a set neither side holds.
dupes=`cut -f1 "$tmp/names" | LC_ALL=C sort | uniq -d`
if [ -n "$dupes" ]; then
	echo "extract-enum.sh: read the same name twice for $ENUM out of $HEADER:" >&2
	echo "$dupes" >&2
	exit 1
fi

cat "$tmp/names"
