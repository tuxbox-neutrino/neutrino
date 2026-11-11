#!/bin/sh
# The record manager deletes a recording's instance on the loop's thread when
# the recording stops, and its map is changed under the manager's lock. Code
# that runs on another thread - the web server's, the stream server's, the
# front displays', zapit's - reads the recordings as values taken under that
# lock (GetRunningRecordings, RecordingStatus, GetRecordMode,
# GetRecordCount), never through GetRecordMap(), whose copy is taken without
# the lock and holds pointers to the instances. So:
#  1. GetRecordMap( appears in none of the directories such threads run in,
#     and nowhere in src/driver but record.cpp;
#  2. GetRecordCount takes the manager's lock.
# The program cannot be linked into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-record-readers.sh <top source directory>" >&2
	exit 2
}
H=src/driver/record.h
[ -r "$SRC/$H" ] || { echo "check-record-readers.sh: cannot read $SRC/$H" >&2; exit 1; }
fail=0

# 1. No map copy off the loop.
found=`find "$SRC/src/nhttpd" "$SRC/src/coreapi" "$SRC/src/httpd" "$SRC/src/driver" "$SRC/src/system" \
	"$SRC/src/zapit" "$SRC/src/gui/lua" \( -name '*.cpp' -o -name '*.h' \) ! -path "$SRC/src/driver/record.*" \
	-exec grep -n -H -E 'GetRecordMap[[:blank:]]*\(' {} + | sed "s|^$SRC/||"` || true
if [ -n "$found" ]; then
	echo "the recording map is copied off the loop, with pointers to instances the loop deletes:" >&2
	echo "$found" | sed 's/^/  /' >&2
	fail=1
fi

# 2. The count is read under the lock.
grep -E 'int[[:blank:]]+GetRecordCount[[:blank:]]*\(' "$SRC/$H" | grep -q -E 'mutex\.lock\(\).*recmap\.size\(\).*mutex\.unlock\(\)' ||
	{ echo "$H: GetRecordCount reads the map without the manager's lock" >&2; fail=1; }

[ $fail -eq 0 ] && echo "check-record-readers.sh: off the loop the recordings are read as values, under the manager's lock"
exit $fail
