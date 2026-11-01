#!/bin/sh
# zapit may stay awake while the box is in standby: for the EPG scan, for a
# stream or a recording that ran when the box went into standby, or woken
# again by the scan. Its own standby flag then says nothing about whether a
# picture is shown, so the program tells zapit itself: the box's standby is
# set before standbyMode decides whether zapit sleeps, and cleared before
# zapit wakes and in tvMode and radioMode, which saveEpg reaches without
# leaving standby the usual way. A stream's admission reads it with the
# other facts.
#
# The unit cases drive the admission with the flag; this holds the program
# to setting it where they assume. Neither the program nor zapit can be
# linked into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-standby-flag.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/neutrino.cpp src/zapit/zapit.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-standby-flag.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-standby-flag.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

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

strip "$SRC/src/neutrino.cpp" > "$tmp/neutrino"

# 1. standbyMode: set before zapit is put to sleep or kept awake, cleared
#    before zapit wakes.
body 'void CNeutrinoApp::standbyMode(' < "$tmp/neutrino" > "$tmp/standby"
[ -s "$tmp/standby" ] || { echo "check-standby-flag.sh: no standbyMode in src/neutrino.cpp" >&2; exit 1; }
on=`first "$tmp/standby" 'SetBoxStandby(true)'`
branch=`first "$tmp/standby" 'g_settings.epg_scan_mode == CEpgScan::MODE_OFF'`
sleep=`first "$tmp/standby" 'g_Zapit->setStandby(true)'`
off=`first "$tmp/standby" 'SetBoxStandby(false)'`
# The last wake is the way out; an earlier one is the way back for a box woken
# while the guide was written, where tvMode or radioMode cleared the flag.
wake=`grep -n -F -- 'g_Zapit->setStandby(false)' "$tmp/standby" | tail -1 | cut -d: -f1`
if [ -z "$on" ] || [ -z "$branch" ] || [ -z "$sleep" ] || [ "$on" -gt "$branch" ] || [ "$on" -gt "$sleep" ]; then
	echo "standbyMode: SetBoxStandby(true) has to come before zapit is put to sleep or kept awake (set ${on:-none}, branch ${branch:-none})" >&2
	fail=1
fi
if [ -z "$off" ] || [ -z "$wake" ] || [ "$off" -gt "$wake" ] || [ "$off" -lt "${sleep:-0}" ]; then
	echo "standbyMode: SetBoxStandby(false) has to come at the start of the way out, before zapit wakes (cleared ${off:-none}, wake ${wake:-none})" >&2
	fail=1
fi

# 2. tvMode and radioMode clear it.
for f in 'void CNeutrinoApp::tvMode(' 'void CNeutrinoApp::radioMode('; do
	body "$f" < "$tmp/neutrino" > "$tmp/mode"
	grep -q -F 'SetBoxStandby(false)' "$tmp/mode" || {
		echo "$f...): does not clear the box's standby" >&2
		fail=1
	}
done

# 3. Nothing else sets it.
n=`grep -c -F 'SetBoxStandby(' "$tmp/neutrino" || true`
if [ "$n" != 4 ]; then
	echo "src/neutrino.cpp sets the box's standby in $n places; standbyMode twice, tvMode and radioMode" >&2
	fail=1
fi

# 4. The admission's facts read it.
strip "$SRC/src/zapit/zapit.cpp" | body 'void CZapit::StreamFacts(' > "$tmp/facts"
grep -q -E 'f\.box_standby[ \t]*=[ \t]*box_standby' "$tmp/facts" || {
	echo "CZapit::StreamFacts does not read the box's standby" >&2
	fail=1
}

exit $fail
