#!/bin/sh
# A timer that a stop finishes - a recording that ends, the program that stops
# them all at its end - is written to the timer file: CTimerManager::stopEvent
# marks the list for saving under the timer lock, as every other change of a
# timer's state does, and shutdown() writes a marked list after the timer
# thread is gone. Without the mark a stop the program's end sends just before
# shutdown() stays in memory, and the file keeps the timer running.
# The timer daemon cannot be linked into the test binary, so it is checked as
# text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-timer-stop-saved.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
F=src/timerd/timermanager.cpp
[ -r "$SRC/$F" ] || { echo "check-timer-stop-saved.sh: cannot read $SRC/$F" >&2; exit 1; }
[ -r "$HERE/strip-comments.awk" ] || { echo "check-timer-stop-saved.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SRC/$F" > "$tmp/src"
first() {
	grep -n -E -- "$2" "$1" | head -1 | cut -d: -f1
}

awk 'index($0, "bool CTimerManager::stopEvent(") == 1 { f = 1 } f { print } f && /^}/ { exit }' "$tmp/src" > "$tmp/stop"
[ -s "$tmp/stop" ] || { echo "$F: no CTimerManager::stopEvent" >&2; exit 1; }
l=`first "$tmp/stop" 'pthread_mutex_lock\(&tm_eventsMutex\)'`
st=`first "$tmp/stop" 'eventState[[:blank:]]*=[[:blank:]]*CTimerd::TIMERSTATE_HASFINISHED'`
m=`first "$tmp/stop" 'm_saveEvents[[:blank:]]*=[[:blank:]]*true'`
e=`first "$tmp/stop" '^[[:blank:]]*else'`
u=`first "$tmp/stop" 'pthread_mutex_unlock\(&tm_eventsMutex\)'`
if [ -z "$l" ] || [ -z "$st" ] || [ -z "$m" ] || [ -z "$e" ] || [ -z "$u" ] || [ "$m" -lt "$st" ] || [ "$m" -gt "$e" ] || [ "$m" -gt "$u" ]; then
	echo "$F: CTimerManager::stopEvent finishes a timer without marking the list for saving under the timer lock" >&2
	fail=1
fi

awk 'index($0, "bool CTimerManager::shutdown(") == 1 { f = 1 } f { print } f && /^}/ { exit }' "$tmp/src" > "$tmp/shutdown"
j=`first "$tmp/shutdown" 'pthread_join\(thrTimer'`
s=`first "$tmp/shutdown" 'if[[:blank:]]*\([[:blank:]]*m_saveEvents[[:blank:]]*\)'`
w=`first "$tmp/shutdown" 'saveEventsToConfig\(\)'`
if [ -z "$j" ] || [ -z "$s" ] || [ -z "$w" ] || [ "$j" -gt "$s" ] || [ "$s" -gt "$w" ]; then
	echo "$F: CTimerManager::shutdown does not write a marked list once the timer thread is gone" >&2
	fail=1
fi

[ $fail -eq 0 ] && echo "check-timer-stop-saved.sh: a stopped timer is marked for saving and written at shutdown"
exit $fail
