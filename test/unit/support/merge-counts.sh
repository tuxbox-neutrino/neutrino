#!/bin/sh
# Resolves a conflict in test/unit/support/counts.txt by addition from the merge base.
#
# The file is a set of coverage figures, one per line. Two branches that each
# added cases both raise the same lines, so neither side is the answer and
# taking one loses the other silently: the suite still passes, it just compares
# less than it should, and nothing says so. The answer is, per line,
# base + (ours - base) + (theirs - base).
#
# Doing this by eye has gone wrong twice in this project, both times on a line
# that happened to carry the same figure on both sides, which is exactly the
# case an eye reads as agreement.
#
# Run from the top of the tree while a merge is conflicted.
set -eu

F=test/unit/support/counts.txt
BASE=$(git merge-base HEAD MERGE_HEAD)

# The base is read out of the conflict and not out of the merge base commit.
# This file has moved once already, and a branch cut before the move carries it
# under its old name: asking the base commit for the new path then answers
# "does not exist", the script stops, and whatever was written to run after it
# on the same line runs anyway. Git has the answer either way, because the
# stage it records is the base blob of the conflict it detected, rename and all.
if git cat-file -e ":1:$F" 2>/dev/null; then
	git show ":1:$F" > /tmp/mc.base
else
	# No stage 1: both sides added the file and there is nothing under it. Zero
	# is then the honest base, and addition reduces to taking both sides' rises.
	: > /tmp/mc.base
fi
git show ":2:$F"    > /tmp/mc.ours
git show ":3:$F"    > /tmp/mc.theirs

awk -F'\t' -v OFS='\t' '
	FILENAME == b { if ($0 !~ /^#/ && $0 != "") base[$1] = $2; next }
	FILENAME == t { if ($0 !~ /^#/ && $0 != "") { theirs[$1] = $2; have[$1] = 1 } next }

	# Ours carries the comment block and the usual order, so it leads.
	{
		if ($0 ~ /^#/ || $0 == "") { print; next }
		mine[$1] = 1
		if (($1 in base) && ($1 in theirs))
			print $1, base[$1] + ($2 - base[$1]) + (theirs[$1] - base[$1])
		else
			print
	}

	END {
		# A line only the other side has is one ours never saw. Dropping it
		# would take a whole figure out of the file without a word.
		for (k in have)
			if (!(k in mine))
				print k, theirs[k]
	}
' b=/tmp/mc.base t=/tmp/mc.theirs /tmp/mc.base /tmp/mc.theirs /tmp/mc.ours > /tmp/mc.out

{ grep '^#' /tmp/mc.out; grep -v '^#' /tmp/mc.out | grep -v '^$' | LC_ALL=C sort; } > "$F"

echo "counts.txt aufgeloest. Gegenueber der Basis $(git rev-parse --short "$BASE"):"
diff /tmp/mc.base "$F" | grep -E '^[<>]' || echo "  (nichts)"
