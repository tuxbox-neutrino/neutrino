#!/bin/sh
# A recording timer names the guide event that runs in the middle of the
# time asked for. A timer made with the safety margin is stored with the
# margins added, and with a margin after the event larger than the one
# before, the middle of the whole time can lie in the next event. Both
# places that look an event up for a recording, the one that names it and
# the one that moves the timer onto it, take the moment from timerepg, with
# the timer's own choice of the margin.
#
# The unit cases drive timerepg over times; this holds the timer daemon to
# asking it. The timer manager cannot be linked into the test binary, so it
# is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-timer-epgid.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/timerd/timermanager.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-timer-epgid.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-timer-epgid.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
strip() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1"
}
body() {
	awk -v start="$1" 'index($0, start) == 1 { f = 1 } f { print } f && /^}/ { exit }'
}

strip "$SRC/src/timerd/timermanager.cpp" > "$tmp/manager"
for f in 'void CTimerEvent_Record::getEpgId(' 'bool CTimerEvent_Record::adjustToCurrentEPG('; do
	body "$f" < "$tmp/manager" > "$tmp/fn"
	[ -s "$tmp/fn" ] || { echo "check-timer-epgid.sh: no $f...) in src/timerd/timermanager.cpp" >&2; exit 1; }
	grep -q -E 'check_time[[:space:]]*=[[:space:]]*timerepg::checkTime\([[:space:]]*alarmTime[[:space:]]*,[[:space:]]*stopTime[[:space:]]*,[[:space:]]*recordingSafety[[:space:]]*,[[:space:]]*pre[[:space:]]*,[[:space:]]*post[[:space:]]*\)' "$tmp/fn" || {
		echo "src/timerd/timermanager.cpp: $f...) does not take the moment it looks the event up at from timerepg::checkTime with the timer's margin" >&2
		fail=1
	}
	grep -q -E 'getRecordingSafety\([[:space:]]*pre[[:space:]]*,[[:space:]]*post[[:space:]]*\)' "$tmp/fn" || {
		echo "src/timerd/timermanager.cpp: $f...) does not read the margins it hands timerepg::checkTime" >&2
		fail=1
	}
	if grep -n -E 'check_time[[:space:]]*=[[:space:]]*_?alarmTime[[:space:]]*/' "$tmp/fn" > "$tmp/hits"; then
		echo "src/timerd/timermanager.cpp: $f...) works out the middle itself:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done

exit $fail
