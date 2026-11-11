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
for f in src/neutrino.cpp src/zapit/zapit.cpp src/zapit/streamholdcmd.cpp src/gui/cam_menu.cpp src/driver/streamts.cpp src/zapit/capmt.cpp; do
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
for call in 'ops.tune(' 'ops.readPids(' 'ops.offer(' 'ops.answer('; do
	c=`first "$tmp/run" "$call"`
	if [ "$c" -gt "$l" ] && [ "$c" -lt "$u" ]; then
		echo "streamholdcmd::run: $call sits in the hold of the frontend manager's lock" >&2
		fail=1
	fi
done

# ... and zapit's facts are where the flag is read, under that lock: the
# hold's facts are zapit's StreamFacts, and that reads it.
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC/src/zapit/zapit.cpp" > "$tmp/zapit"
awk '
	/^class CZapit::StreamHoldOps/ { inside = 1 }
	inside && /^};/ { inside = 0 }
	inside { print }
' "$tmp/zapit" > "$tmp/ops"
awk '/void facts\(/ { f = 1 } f { print } f && (/^\t\t}/ || /}[ \t]*$/) { exit }' "$tmp/ops" > "$tmp/facts"
grep -q -F 'StreamFacts(' "$tmp/facts" || { echo "zapit.cpp: StreamHoldOps::facts does not read CZapit::StreamFacts" >&2; fail=1; }
awk '/^void CZapit::StreamFacts\(/ { f = 1 } f { print } f && /^}/ { exit }' "$tmp/zapit" > "$tmp/facts"
grep -q -F 'avinput_pip' "$tmp/facts" || { echo "zapit.cpp: CZapit::StreamFacts does not read avinput_pip" >&2; fail=1; }
# The live frontend's transponder is read with whether a scan may have
# tuned it away from that id, in the same facts.
grep -q -F 'f.live_tp =' "$tmp/facts" || { echo "zapit.cpp: CZapit::StreamFacts does not read the live frontend's transponder" >&2; fail=1; }
# A scan counts from its start, which clears the current channel, until a
# zap sets one again, not only while the scan thread runs.
grep -q -F 'f.scanning = current_channel == NULL || CServiceScan::getInstance()->Scanning();' "$tmp/facts" ||
	{ echo "zapit.cpp: CZapit::StreamFacts does not read whether a scan runs or left no current channel" >&2; fail=1; }
# ... and the CI rule with them. A service counts where the CA manager has
# the module descramble it, by its own rule (CCamManager::SetMode): a module
# is ready, the channel is scrambled, with the channel filter on it is marked
# for the module, and with the module bound to a tuner it comes from that
# one. So count the picture's channel from the live frontend, every service
# on the CA manager's list from the frontend its demux reads, and every
# service a stream's hold was granted for, from the grant until the hold is
# given back; without a ready module none. The stream asked for is read as
# from the bound tuner, which the facts carry for the admission to weigh
# each way's frontend against.
awk '/^static bool ciDescrambles\(/ { f = 1 } f { print } f && /^}/ { exit }' "$tmp/zapit" > "$tmp/cirule"
awk '/^static bool ciModuleReady\(/ { f = 1 } f { print } f && /^}/ { exit }' "$tmp/zapit" > "$tmp/cimodule"
grep -q -F 'if (!module || c == NULL || !c->scrambled)' "$tmp/cirule" &&
	grep -q -F 'if (cam->getChannelFilter() && !c->bUseCI)' "$tmp/cirule" &&
	grep -q -F 'const int tuner = cam->GetCITuner();' "$tmp/cirule" &&
	grep -q -F 'return tuner < 0 || source < 0 || source == tuner;' "$tmp/cirule" &&
	grep -q -F 'slots->ModulePresent(CA_SLOT_TYPE_CI, i)' "$tmp/cimodule" ||
	{ echo "zapit.cpp: ciDescrambles is not the CA manager's rule for what the module descrambles" >&2; fail=1; }
grep -q -F 'const bool module = ciModuleReady();' "$tmp/facts" &&
	grep -q -F 'f.live_ci = current_channel != NULL && current_channel->getChannelID() == live_channel_id &&' "$tmp/facts" &&
	grep -q -F 'ciDescrambles(current_channel, live_fe ? live_fe->getNumber() : -1, module);' "$tmp/facts" ||
	{ echo "zapit.cpp: CZapit::StreamFacts does not read whether the module descrambles the live channel" >&2; fail=1; }
grep -q -F 'f.ci_mode = CCamManager::getInstance()->GetCIMode();' "$tmp/facts" ||
	{ echo "zapit.cpp: CZapit::StreamFacts does not read the CI mode" >&2; fail=1; }
grep -q -F 'f.ci_tuner = CCamManager::getInstance()->GetCITuner();' "$tmp/facts" ||
	{ echo "zapit.cpp: CZapit::StreamFacts does not read the tuner the CI module is bound to" >&2; fail=1; }
grep -q -F 'CCamManager::getInstance()->GetServices(' "$tmp/facts" &&
	grep -q -F 'if (ciDescrambles(c, served[i].source, module))' "$tmp/facts" ||
	{ echo "zapit.cpp: CZapit::StreamFacts does not read the services the CI module descrambles" >&2; fail=1; }
grep -q -F 'module ? streamhold::ciServices(' "$tmp/facts" ||
	{ echo "zapit.cpp: CZapit::StreamFacts does not count the CI services streams were granted" >&2; fail=1; }
awk '/void facts\(/ { f = 1 } f { print } f && /^\t\t}/ { exit }' "$tmp/ops" > "$tmp/opsfacts"
grep -q -F 'f.channel_ci = ciService(-1) != 0;' "$tmp/opsfacts" ||
	{ echo "zapit.cpp: StreamHoldOps::facts does not read whether the module would descramble the stream" >&2; fail=1; }
grep -q -F 'return ca && ciDescrambles(channel_obj, source, ciModuleReady()) ? channel_obj->getChannelID() : 0;' "$tmp/ops" ||
	{ echo "zapit.cpp: StreamHoldOps::ciService is not the CA manager's rule for a stream the box descrambles" >&2; fail=1; }
grep -q -F 'return streamhold::offer(fe, streamhold::nowMs(), ciService(number(fe)));' "$tmp/ops" ||
	{ echo "zapit.cpp: StreamHoldOps::offer does not reserve the stream's CI service with its hold" >&2; fail=1; }
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC/src/zapit/capmt.cpp" |
	awk '/^void CCamManager::GetServices\(/ { f = 1 } f { print } f && /^}/ { exit }' |
	grep -q -F 's.source = cDemux::GetSource(it->second->getSource());' ||
	{ echo "capmt.cpp: CCamManager::GetServices does not say which frontend each service comes from" >&2; fail=1; }
awk '/^void CZapit::HoldForStream\(/ { f = 1 } f { print } f && /^}/ { exit }' "$tmp/zapit" |
	grep -q -F 'StreamHoldOps ops(*this, connfd, msg.no_ca == 0);' ||
	{ echo "zapit.cpp: CZapit::HoldForStream does not take whether the box descrambles the stream" >&2; fail=1; }
# The stream server says so by the rule its instance starts the CA by, read
# once for both.
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC/src/driver/streamts.cpp" > "$tmp/streamts"
grep -q -F 'uses_ca = !t.e2 || g_settings.streaming_decryptmode;' "$tmp/streamts" &&
	[ `grep -c -F 'streaming_decryptmode' "$tmp/streamts"` -eq 1 ] ||
	{ echo "streamts.cpp: whether the box descrambles a stream is not read once, by the e2 rule" >&2; fail=1; }
grep -q -F 'holdForStream(channel, (unsigned) left, reply, ask_switch, uses_ca)' "$tmp/streamts" &&
	grep -q -F 'stream->uses_ca = uses_ca;' "$tmp/streamts" ||
	{ echo "streamts.cpp: the hold and the instance are not told the same whether the box descrambles" >&2; fail=1; }
awk '/CCamManager::getInstance\(\)->(Start|Stop)\(channel_id, CCamManager::STREAM\)/ { n++; if (prev !~ /if \(uses_ca\)[ \t]*$/) bad = 1 }
	{ prev = $0 } END { exit !(n == 2 && !bad) }' "$tmp/streamts" ||
	{ echo "streamts.cpp: CStreamInstance::run starts or stops the CA other than by uses_ca" >&2; fail=1; }
grep -F 'void lockFrontends()' "$tmp/ops" | grep -q -F "$LOCK" || { echo "zapit.cpp: StreamHoldOps::lockFrontends does not take the frontend manager's lock" >&2; fail=1; }

# The CI mode those facts read is the user's: handed over by every read of
# the settings file, the one before zapit starts included, and again
# whenever the setting changes, on the screen or through its applier.
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC/src/neutrino.cpp" > "$tmp/neutrino"
awk '/^int CNeutrinoApp::loadSetup\(/ { f = 1 } f { print } f && /^}/ { exit }' "$tmp/neutrino" |
	awk '/g_settings\.ci_mode = configfile\.getInt32\(/ && !r { r = NR } /SetCIMode\(g_settings\.ci_mode\)/ && !s { s = NR }
	END { exit !(r > 0 && s > r) }' ||
	{ echo "neutrino.cpp: loadSetup does not hand the CI mode it reads to the CAM manager" >&2; fail=1; }
awk '/= loadSetup\(NEUTRINO_SETTINGS_FILE\)/ && !s { s = NR } /CZapit::getInstance\(\)->Start\(/ && !z { z = NR }
	END { exit !(s > 0 && z > 0 && s < z) }' "$tmp/neutrino" ||
	{ echo "neutrino.cpp: the settings are not read before zapit starts" >&2; fail=1; }
awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$SRC/src/gui/cam_menu.cpp" > "$tmp/cam"
grep -F 'LOCALE_CI_MODE, &g_settings.ci_mode' "$tmp/cam" | grep -q -F 'true, this)' ||
	{ echo "cam_menu.cpp: the CI mode chooser does not notify its handler" >&2; fail=1; }
awk '/ARE_LOCALES_EQUAL\(OptionName, LOCALE_CI_MODE\)/ { f = 1 } f { print } f && /^\t}/ { exit }' "$tmp/cam" |
	grep -q -F 'SetCIMode(g_settings.ci_mode)' ||
	{ echo "cam_menu.cpp: CCAMMenuHandler::changeNotify does not hand a changed CI mode to the CAM manager" >&2; fail=1; }

exit $fail
