#!/bin/sh
# A recording of the live channel takes the live frontend only when that
# frontend carries the channel's transponder, and it zaps the picture only
# when the live frontend does not carry it. Both are read off the frontend
# (recordtuner.h): the live channel id says nothing about where the live
# frontend stands, so CutBackNeutrino must not decide either by comparing
# it with the recorded channel's id.
#
# While a recording starts - from before it looks for its frontend until its
# start has locked that frontend - no stream is admitted, so none can take
# the frontend in between. Record counts itself as starting through a guard
# that is set up before CutBackNeutrino and ended right after the start.
#
# The unit cases drive the decisions as plain functions; this holds the
# recording's code to using them. The recording manager cannot be linked
# into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-record-tuner.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
f=src/driver/record.cpp
[ -r "$SRC/$f" ] || { echo "check-record-tuner.sh: cannot read $SRC/$f" >&2; exit 1; }
[ -r "$HERE/strip-comments.awk" ] || { echo "check-record-tuner.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0

# CutBackNeutrino and the steps it chooses the frontend through.
awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SRC/$f" | awk '
	/^class CRecordTunerOps/ { inside = 1 }
	/^bool CRecordManager::CutBackNeutrino\(/ { inside = 1 }
	inside { print }
	inside && /^};?$/ { inside = 0 }
' > "$tmp/cutback"
grep -q '^bool CRecordManager::CutBackNeutrino(' "$tmp/cutback" || { echo "check-record-tuner.sh: no CutBackNeutrino in $f" >&2; exit 1; }
grep -q '^class CRecordTunerOps' "$tmp/cutback" || { echo "check-record-tuner.sh: no CRecordTunerOps in $f" >&2; exit 1; }

# 1. No decision by the two channel ids' transponder bits.
if grep -n -E 'SAME_TRANSPONDER[ \t]*\([ \t]*live_channel_id' "$tmp/cutback" > "$tmp/hits"; then
	echo "$f: CutBackNeutrino compares the live channel id's transponder; read the live frontend instead:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi

# 2. The live channel is recorded off the live frontend only when it serves,
#    and the full zap is decided by the frontend's state.
grep -q -F 'recordtuner::liveServes(' "$tmp/cutback" || {
	echo "$f: CutBackNeutrino does not ask recordtuner::liveServes whether the live frontend carries the channel" >&2
	fail=1
}
grep -q -E 'return[ \t]+live_channel_id[ \t]*==[ \t]*channel->getChannelID\(\)[ \t]*&&[ \t]*live_serves[ \t]*;' "$tmp/cutback" || {
	echo "$f: CutBackNeutrino takes the live frontend for the live channel without asking whether it serves" >&2
	fail=1
}
grep -q -F 'recordtuner::choose(' "$tmp/cutback" || {
	echo "$f: CutBackNeutrino does not choose the frontend through recordtuner::choose" >&2
	fail=1
}
# A zap while streams ended may have moved the picture: the live frontend
# the zap is decided by, and unlocked after it, is the one the choice read.
grep -q -E 'live_fe[ \t]*=[ \t]*static_cast<CFrontend \*>\(choice\.live\)' "$tmp/cutback" || {
	echo "$f: CutBackNeutrino does not take the live frontend the choice last read" >&2
	fail=1
}
grep -q -E 'found[ \t]*=[ \t]*!recordtuner::fullZap\(' "$tmp/cutback" || {
	echo "$f: CutBackNeutrino decides the full zap without recordtuner::fullZap" >&2
	fail=1
}
# Whether the live channel is recorded off the live frontend is asked of the
# frontend the choice read under the frontend manager's lock, not of the one
# zapit names when the question comes.
awk '
	/bool liveRecords\(void \*fe\)/ { f = 1 }
	f { print }
	f && /^\t\t}$/ { exit }
' "$tmp/cutback" > "$tmp/liverecords"
if [ ! -s "$tmp/liverecords" ]; then
	echo "$f: CRecordTunerOps has no liveRecords(void *fe)" >&2
	fail=1
else
	grep -q -E 'static_cast<CFrontend \*>\(fe\)' "$tmp/liverecords" || {
		echo "$f: CRecordTunerOps::liveRecords does not read the frontend it is given" >&2
		fail=1
	}
	if grep -n -E 'GetLiveFrontend|getLiveFE' "$tmp/liverecords" > "$tmp/hits"; then
		echo "$f: CRecordTunerOps::liveRecords reads the live frontend again instead of the one it is given:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
fi

# 3. Record counts itself as starting around the frontend search and its start.
awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SRC/$f" | awk '
	index($0, "bool CRecordManager::Record(const CTimerd::RecordingInfo") == 1 { inside = 1 }
	inside { print }
	inside && /^}/ { inside = 0 }
' > "$tmp/record"
[ -s "$tmp/record" ] || { echo "check-record-tuner.sh: no CRecordManager::Record in $f" >&2; exit 1; }
g=`grep -n -E 'CRecordStartGuard[ \t]+starting;' "$tmp/record" | head -1 | cut -d: -f1`
c=`grep -n -F 'CutBackNeutrino(' "$tmp/record" | head -1 | cut -d: -f1`
r=`awk -v c="${c:-0}" 'NR > c && index($0, "inst->Record()") { print NR; exit }' "$tmp/record"`
e=`grep -n -F 'starting.end()' "$tmp/record" | head -1 | cut -d: -f1`
if [ -z "$g" ] || [ -z "$c" ] || [ -z "$r" ] || [ -z "$e" ] || [ "$g" -gt "$c" ] || [ "$e" -lt "$r" ]; then
	echo "$f: Record has to count itself as starting before CutBackNeutrino and stop after the recording's start (guard ${g:-none}, search ${c:-none}, start ${r:-none}, end ${e:-none})" >&2
	fail=1
fi
grep -q -F 'beginRecordingStart()' "$SRC/$f" && grep -q -F 'endRecordingStart()' "$SRC/$f" || {
	echo "$f: the guard does not count with the frontend manager" >&2
	fail=1
}

exit $fail
