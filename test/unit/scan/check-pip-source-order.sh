#!/bin/sh
# A stream admission reads, in one hold of the frontend manager's lock, which
# PiP reads which demux, and binds a frontend in the same hold. Two writers
# of that state run on other threads than zapit's, and are only safe if they
# publish under the same lock and in the right order:
#
#  - the AV-input PiP switches demux 1 to the input. It announces itself
#    under the lock before it does, so an admission either ran before and
#    its binding is overwritten by the switch, or sees the flag and leaves
#    demux 1 alone. It withdraws the announcement under the lock after it
#    stopped.
#  - StopPip, called from the GUI as well, says a slot is free only after the
#    slot's demux and decoders stopped, and says it under the lock: an
#    admission that sees the slot free never binds under a PiP still reading.
#
# And the admission itself reads the flag and binds in one hold: facts, the
# live binding and the idle search all sit between taking the lock and giving
# it up, and the flag is one of the facts.
#
# The unit cases drive this order on a model (test_streamadmission.cpp);
# this holds the program's code to the order the model was given. The GUI
# and zapit cannot be linked into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-pip-source-order.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/neutrino.cpp src/zapit/zapit.cpp src/zapit/streamholdcmd.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-pip-source-order.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-pip-source-order.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0

# The body of one function, without comments, from its definition line to the
# first closing brace at the start of a line; numbered within the body.
body() {
	awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$1" | awk -v re="$2" '
		!inside && $0 ~ re { inside = 1 }
		inside { print }
		inside && /^}/ { exit }
	' > "$3"
	[ -s "$3" ] || { echo "check-pip-source-order.sh: no $2 in $1" >&2; exit 1; }
}

# The line of the first or last match of a fixed text in a body, 0 for none.
first() {
	n=`grep -n -F -- "$2" "$1" | head -n 1 | cut -d: -f1`
	echo "${n:-0}"
}
last() {
	n=`grep -n -F -- "$2" "$1" | tail -n 1 | cut -d: -f1`
	echo "${n:-0}"
}
LOCK='CFEManager::getInstance()->Lock()'
UNLOCK='CFEManager::getInstance()->Unlock()'

# 1. The AV-input PiP announces itself under the lock, before demux 1 moves.
body "$SRC/src/neutrino.cpp" '^void CNeutrinoApp::StartAVInputPiP[(]' "$tmp/start"
l=`first "$tmp/start" "$LOCK"`
f=`first "$tmp/start" 'avinput_pip = true'`
u=`first "$tmp/start" "$UNLOCK"`
s=`first "$tmp/start" 'SetSource(1, 2)'`
if [ "$l" -eq 0 ] || [ "$f" -le "$l" ] || [ "$u" -le "$f" ] || [ "$s" -le "$u" ]; then
	echo "StartAVInputPiP: avinput_pip = true has to sit between the frontend manager's Lock and Unlock, before SetSource(1, 2) (lock $l, flag $f, unlock $u, source $s)" >&2
	fail=1
fi
n=`awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC/src/neutrino.cpp" | grep -c -F 'avinput_pip = true' || true`
if [ "$n" -ne 1 ]; then
	echo "src/neutrino.cpp: avinput_pip = true is set in $n places; only StartAVInputPiP may set it" >&2
	fail=1
fi

# 2. It withdraws under the lock, after its demux and decoder stopped.
body "$SRC/src/neutrino.cpp" '^void CNeutrinoApp::StopAVInputPiP[(]' "$tmp/stop"
st=`last "$tmp/stop" '->Stop()'`
l=`first "$tmp/stop" "$LOCK"`
f=`first "$tmp/stop" 'avinput_pip = false'`
u=`first "$tmp/stop" "$UNLOCK"`
if [ "$st" -eq 0 ] || [ "$l" -le "$st" ] || [ "$f" -le "$l" ] || [ "$u" -le "$f" ]; then
	echo "StopAVInputPiP: avinput_pip = false has to sit between the frontend manager's Lock and Unlock, after the last Stop() (stop $st, lock $l, flag $f, unlock $u)" >&2
	fail=1
fi

# 3. StopPip says a slot is free under the lock, after it stopped everything
#    in the slot, in each branch of its preprocessor conditionals.
body "$SRC/src/zapit/zapit.cpp" '^bool CZapit::StopPip[(]' "$tmp/stoppip"
awk -v lock="$LOCK" -v unlock="$UNLOCK" '
	function check(   i) {
		if (clears == 0)
			return
		if (lk == 0 || firstclear < lk || lastclear > ul || ul == 0) {
			print "StopPip: a slot is cleared outside the frontend manager'"'"'s lock (branch ending at line " NR ")"
			bad = 1
		}
		if (lk > 0 && laststop > lk) {
			print "StopPip: the slot'"'"'s demux or decoder is stopped after the lock that clears it (branch ending at line " NR ")"
			bad = 1
		}
		if (lk > 0 && cam > lk) {
			print "StopPip: the CAM is told inside the lock that clears the slot (branch ending at line " NR ")"
			bad = 1
		}
	}
	function reset() { clears = 0; lk = 0; ul = 0; laststop = 0; cam = 0; firstclear = 0; lastclear = 0 }
	BEGIN { reset() }
	/^#(else|elif|endif)/ { check(); reset(); next }
	index($0, lock) > 0 { if (lk == 0) lk = NR }
	index($0, unlock) > 0 { if (lk > 0 && ul == 0) ul = NR }
	/(->Stop\(|->closeDevice\(|ShowPig\(0\)|delete pip)/ { laststop = NR }
	/CCamManager::getInstance\(\)->Stop\(/ { cam = NR }
	/pip_fe\[pip\] = NULL|pip_channel_id\[pip\] = 0/ {
		clears++
		if (firstclear == 0) firstclear = NR
		lastclear = NR
	}
	END { check(); exit bad }
' "$tmp/stoppip" >&2 || fail=1
n=`grep -c -E 'pip_fe\[pip\] = NULL' "$tmp/stoppip" || true`
if [ "$n" -lt 1 ]; then
	echo "StopPip: no slot is cleared at all; the check above would hold nothing" >&2
	fail=1
fi

# 4. The admission reads the facts and binds in one hold of the lock.
body "$SRC/src/zapit/streamholdcmd.cpp" '^Reply run[(]' "$tmp/run"
l=`first "$tmp/run" 'ops.lockFrontends()'`
u=`first "$tmp/run" 'ops.unlockFrontends()'`
for call in 'ops.facts(' 'ops.holdLive(' 'ops.pickIdle(' 'ops.openFrontends('; do
	c=`first "$tmp/run" "$call"`
	if [ "$c" -le "$l" ] || [ "$c" -ge "$u" ]; then
		echo "streamholdcmd::run: $call has to sit between ops.lockFrontends() and ops.unlockFrontends() (lock $l, call $c, unlock $u)" >&2
		fail=1
	fi
done
fa=`first "$tmp/run" 'ops.facts('`
for call in 'ops.holdLive(' 'ops.pickIdle('; do
	c=`first "$tmp/run" "$call"`
	[ "$c" -gt "$fa" ] || { echo "streamholdcmd::run: $call comes before the facts are read" >&2; fail=1; }
done
for call in 'ops.tune(' 'ops.offer(' 'ops.answer('; do
	c=`first "$tmp/run" "$call"`
	if [ "$c" -gt "$l" ] && [ "$c" -lt "$u" ]; then
		echo "streamholdcmd::run: $call sits in the hold of the frontend manager's lock" >&2
		fail=1
	fi
done

# ... and zapit's facts are where the flag is read, under that lock.
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC/src/zapit/zapit.cpp" | awk '
	/^class CZapit::StreamHoldOps/ { inside = 1 }
	inside && /^};/ { inside = 0 }
	inside { print }
' > "$tmp/ops"
awk '/void facts\(/ { f = 1 } f { print } f && /^\t\t}/ { exit }' "$tmp/ops" > "$tmp/facts"
grep -q -F 'avinput_pip' "$tmp/facts" || { echo "zapit.cpp: StreamHoldOps::facts does not read avinput_pip" >&2; fail=1; }
grep -F 'void lockFrontends()' "$tmp/ops" | grep -q -F "$LOCK" || { echo "zapit.cpp: StreamHoldOps::lockFrontends does not take the frontend manager's lock" >&2; fail=1; }

exit $fail
