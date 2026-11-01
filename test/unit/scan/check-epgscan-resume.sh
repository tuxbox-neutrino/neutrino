#!/bin/sh
# The end of streams is announced so that the standby EPG scan goes on and
# zapit goes back to standby. A recording that ends streams to get its
# frontend must not have that happen while it starts: the handler would put
# zapit into standby before the recording's record mode is known to the
# program. So stopping streams for a recording announces nothing, and the
# recording announces their end only when it started nothing.
#
# The unit cases drive the choice of the recording's frontend; this holds
# the program's code to announcing only there. Neither the stream server
# nor the recording manager can be linked into the test binary, so it is
# checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-epgscan-resume.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/driver/streamts.cpp src/driver/record.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-epgscan-resume.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-epgscan-resume.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
strip() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1"
}
body() {
	awk -v start="$1" 'index($0, start) == 1 { f = 1 } f { print } f && /^}/ { exit }'
}

# 1. Stopping the streams for a recording announces nothing.
strip "$SRC/src/driver/streamts.cpp" | body 'bool CStreamManager::StopStreamsOn(' > "$tmp/stopon"
[ -s "$tmp/stopon" ] || { echo "check-epgscan-resume.sh: no CStreamManager::StopStreamsOn in src/driver/streamts.cpp" >&2; exit 1; }
if grep -n -F 'postMsg' "$tmp/stopon" > "$tmp/hits"; then
	echo "CStreamManager::StopStreamsOn announces something; the recording it stops streams for does:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi

# 2. The recording announces the streams' end only when it started nothing.
strip "$SRC/src/driver/record.cpp" | body 'bool CRecordManager::Record(const CTimerd::RecordingInfo' > "$tmp/record"
[ -s "$tmp/record" ] || { echo "check-epgscan-resume.sh: no CRecordManager::Record in src/driver/record.cpp" >&2; exit 1; }
n=`grep -c -F 'EVT_STREAM_STOP' "$tmp/record" || true`
if [ "$n" != 1 ]; then
	echo "CRecordManager::Record announces the end of streams in $n places; one, for a recording that started nothing" >&2
	fail=1
elif ! awk '/EVT_STREAM_STOP/ { print prev " " $0 } { prev = $0 }' "$tmp/record" | grep -q -E 'if[ \t]*\([ \t]*!started[ \t]*&&'; then
	echo "CRecordManager::Record announces the end of streams without asking whether it started nothing:" >&2
	grep -n -B1 -F 'EVT_STREAM_STOP' "$tmp/record" | sed 's/^/  /' >&2
	fail=1
fi

exit $fail
