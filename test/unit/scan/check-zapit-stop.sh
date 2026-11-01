#!/bin/sh
# A stop of the program reaches zapit before anything waits for zapit's
# thread: stop_daemons tells zapit it stops first, before the stream server
# is stopped and long before zapit is joined, and zapit's own Stop tells it
# again before it joins. A stream hold zapit is tuning for, and any wait
# for a tuner, then ends within a slice instead of running to its end
# while the program is killed for taking too long.
#
# The unit cases drive the hold's order and the slices; this holds the
# program's shutdown to calling the stop first. Neither can be linked into
# the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-zapit-stop.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/neutrino.cpp src/zapit/zapit.cpp src/zapit/frontend.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-zapit-stop.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-zapit-stop.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
strip() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1"
}
body() {
	awk -v start="$1" 'index($0, start) == 1 { f = 1 } f { print } f && /^}/ { exit }'
}
first() {
	grep -n -F -- "$2" "$1" | head -1 | cut -d: -f1
}

# 1. stop_daemons: zapit hears of the stop before the stream server stops.
strip "$SRC/src/neutrino.cpp" | body 'void stop_daemons(bool stopall, bool for_flash)' > "$tmp/stop"
[ -s "$tmp/stop" ] || { echo "check-zapit-stop.sh: no stop_daemons in src/neutrino.cpp" >&2; exit 1; }
r=`first "$tmp/stop" 'CZapit::getInstance()->RequestStop()'`
s=`first "$tmp/stop" 'CStreamManager::getInstance()->Stop()'`
z=`first "$tmp/stop" 'CZapit::getInstance()->Stop()'`
if [ -z "$r" ] || [ -z "$s" ] || [ -z "$z" ] || [ "$r" -gt "$s" ] || [ "$r" -gt "$z" ]; then
	echo "stop_daemons: CZapit::RequestStop has to come before the stream server and zapit are stopped (request ${r:-none}, streams ${s:-none}, zapit ${z:-none})" >&2
	fail=1
fi

# 2. CZapit::Stop: the request before the join.
strip "$SRC/src/zapit/zapit.cpp" | body 'bool CZapit::Stop(' > "$tmp/zstop"
r=`first "$tmp/zstop" 'RequestStop()'`
j=`first "$tmp/zstop" 'join()'`
if [ -z "$r" ] || [ -z "$j" ] || [ "$r" -gt "$j" ]; then
	echo "CZapit::Stop: RequestStop has to come before the join (request ${r:-none}, join ${j:-none})" >&2
	fail=1
fi

# 3. The wait for a tuner's event is taken in slices and ends on the stop.
strip "$SRC/src/zapit/frontend.cpp" | body 'struct dvb_frontend_event CFrontend::getEvent(' > "$tmp/event"
[ -s "$tmp/event" ] || { echo "check-zapit-stop.sh: no CFrontend::getEvent in src/zapit/frontend.cpp" >&2; exit 1; }
for what in 'waitslice::nextSliceMs(' 'abortWaits'; do
	grep -q -F -- "$what" "$tmp/event" || {
		echo "CFrontend::getEvent does not use $what: a stop would wait for the whole tuning timeout" >&2
		fail=1
	}
done

exit $fail
