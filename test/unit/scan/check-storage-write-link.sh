#!/bin/sh
# The file manager's write puts its bytes beside the name and moves them into
# place at the end. Should a link have taken the name between the look the
# write takes and that move, the link is replaced and not followed: followed,
# it would carry the bytes to wherever it points, out of the roots.
#
# Which of the two the writer does is settled when the writer is made, before
# any moment a case in the test binary can get between, so the choice is
# checked here as text: storage::writeFile makes its writer with ReplaceLink.
# That the writer then replaces the link is held by the writer's own cases.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-storage-write-link.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STORAGE="$SRC/src/coreapi/storage.cpp"
for f in "$STORAGE" "$HERE/strip-comments.awk" "$HERE/blank-if0.awk"; do
	[ -r "$f" ] || { echo "check-storage-write-link.sh: cannot read $f" >&2; exit 1; }
done

body=`awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$STORAGE" | awk -f "$HERE/blank-if0.awk" | awk '
	found == 0 && /^Result<Wrote> writeFile[(]/ { found = 1 }
	found == 1 {
		print
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") { depth++; seen = 1 }
			else if (c == "}") { depth--; if (seen && depth == 0) exit }
		}
	}
' | tr -d '[:space:]'`

case "$body" in
	"")
		echo "check-storage-write-link.sh: no storage::writeFile definition in $STORAGE" >&2
		exit 1
		;;
	*"CAtomicFileWriterwriter(target,CAtomicFileWriter::DefaultMode,CAtomicFileWriter::ReplaceLink);"*) ;;
	*)
		echo "check-storage-write-link.sh: storage::writeFile does not make its writer with" >&2
		echo "  CAtomicFileWriter::ReplaceLink, so a link put at the name is followed" >&2
		exit 1
		;;
esac

exit 0
