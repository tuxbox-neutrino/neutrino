#!/bin/sh
# A recording of the live channel takes the live frontend only when that
# frontend carries the channel's transponder, and it zaps the picture only
# when the live frontend does not carry it. Both are read off the frontend
# (recordtuner.h): the live channel id says nothing about where the live
# frontend stands, so CutBackNeutrino must not decide either by comparing
# it with the recorded channel's id.
#
# The unit cases drive the decisions as plain functions; this holds the
# recording's code to using them. CutBackNeutrino cannot be linked into the
# test binary, so it is checked as text.
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

awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SRC/$f" | awk '
	/^bool CRecordManager::CutBackNeutrino\(/ { inside = 1 }
	inside { print }
	inside && /^}/ { inside = 0 }
' > "$tmp/cutback"
[ -s "$tmp/cutback" ] || { echo "check-record-tuner.sh: no CutBackNeutrino in $f" >&2; exit 1; }

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
grep -q -E 'if[ \t]*\([ \t]*live_channel_id[ \t]*!=[ \t]*channel_id[ \t]*\|\|[ \t]*!live_serves[ \t]*\)' "$tmp/cutback" || {
	echo "$f: CutBackNeutrino takes the live frontend for the live channel without asking whether it serves" >&2
	fail=1
}
grep -q -E 'found[ \t]*=[ \t]*!recordtuner::fullZap\(' "$tmp/cutback" || {
	echo "$f: CutBackNeutrino decides the full zap without recordtuner::fullZap" >&2
	fail=1
}

exit $fail
