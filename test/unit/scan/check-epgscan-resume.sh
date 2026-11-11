#!/bin/sh
# The end of streams is announced so that the standby EPG scan goes on and
# zapit goes back to standby. A recording that ends streams to get its
# frontend must not have that happen while it starts: the handler would put
# zapit into standby before the recording's record mode is known to the
# program. So stopping streams for a recording announces nothing, and the
# recording announces their end only when it started nothing. The handler
# resumes only a scan that paused, after zapit collected the ended streams'
# holds within a deadline that has ended before the scan moves on; the
# scan pauses when its zap meets a stream; a read a stream started in, or
# one a setup that asked zapit for a frontend may have retuned, does not
# count and is repeated, a few times in a row at most; a setup that
# published nothing is announced like an ended stream, saying whether it
# asked zapit for a frontend; and whether zapit may sleep reads the
# recordings themselves. A read whose frontend was tuned since the scan's own
# zap does not count either: every frontend counts its tunes and its
# DiSEqC or voltage switches, zapit records the frontend and its count right
# after the scan's zap tuned it, the scan takes that record for the channel
# it hands over only, and compares the count when sectionsd reports.
#
# The unit cases drive the decisions as plain functions; this holds the
# program's code to them. Neither the program, the stream server nor the
# recording manager can be linked into the test binary, so it is checked as
# text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-epgscan-resume.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/driver/streamts.cpp src/driver/record.cpp src/driver/scanepg.cpp src/neutrino.cpp src/zapit/zapit.cpp src/zapit/frontend.cpp; do
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

# 3. The end of the last stream lets a paused scan go on, and only that:
#    its handler resumes rather than moves the scan on, and the resume has
#    zapit give back the ended streams' holds before it zaps.
strip "$SRC/src/neutrino.cpp" > "$tmp/neutrino"
awk '/msg == NeutrinoMessages::EVT_STREAM_STOP/ { f = 1 } f { print } f && /return messages_return::handled;/ { exit }' "$tmp/neutrino" > "$tmp/handler"
[ -s "$tmp/handler" ] || { echo "check-epgscan-resume.sh: no EVT_STREAM_STOP handler in src/neutrino.cpp" >&2; exit 1; }
grep -q -F 'CEpgScan::getInstance()->Resume()' "$tmp/handler" || { echo "src/neutrino.cpp: the EVT_STREAM_STOP handler does not resume the EPG scan" >&2; fail=1; }
if grep -q -F 'CEpgScan::getInstance()->Next()' "$tmp/handler"; then
	echo "src/neutrino.cpp: the EVT_STREAM_STOP handler moves the EPG scan on, also one that is reading" >&2
	fail=1
fi
strip "$SRC/src/driver/scanepg.cpp" > "$tmp/scanepg"
body 'void CEpgScan::Resume(' < "$tmp/scanepg" > "$tmp/resume"
[ -s "$tmp/resume" ] || { echo "check-epgscan-resume.sh: no CEpgScan::Resume in src/driver/scanepg.cpp" >&2; exit 1; }
p=`grep -n -F 'scanpause::resumeNow(paused, CheckMode())' "$tmp/resume" | head -1 | cut -d: -f1`
y=`grep -n -F 'yieldStreams(0' "$tmp/resume" | head -1 | cut -d: -f1`
n=`grep -n -F 'Next()' "$tmp/resume" | head -1 | cut -d: -f1`
if [ -z "$p" ] || [ -z "$y" ] || [ -z "$n" ] || [ "$p" -gt "$y" ] || [ "$y" -gt "$n" ]; then
	echo "CEpgScan::Resume has to ask scanpause::resumeNow with the scan's mode, then have zapit collect, then call Next (ask ${p:-none}, collect ${y:-none}, next ${n:-none})" >&2
	fail=1
fi
#    The deadline bounds every zapit call of the thread while it lives, so
#    its scope has ended where Next() is called: Next() can enter standby
#    and run a message loop whose zapit calls would fail at once.
scope=`awk '
	/CBasicClient::Deadline[ \t]/ && !d { d = depth + 1 }
	/Next\(\)/ && !n { n = 1; print (d && depth + 1 >= d) ? "inside" : "outside"; exit }
	{ o = gsub(/\{/, "{"); c = gsub(/\}/, "}"); depth += o - c }
' "$tmp/resume"`
if [ "$scope" != outside ]; then
	echo "CEpgScan::Resume calls Next() while the zapit deadline is still in scope (${scope:-no Next() found})" >&2
	fail=1
fi
grep -q -E 'CBasicClient::Deadline[[:space:]]' "$tmp/resume" || { echo "CEpgScan::Resume has no deadline for zapit's collect" >&2; fail=1; }

# 4. The scan's zap answer pauses for a stream instead of reading or skipping:
#    the branch of handleMsg that handles the answer while the scan is on.
body 'int CEpgScan::handleMsg(' < "$tmp/scanepg" | awk '
	/else if[ \t]*\(msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE\)[ \t]*\{/ { f = 1 }
	f { print }
	f && /return messages_return::handled;/ { exit }
' > "$tmp/backzap"
[ -s "$tmp/backzap" ] || { echo "check-epgscan-resume.sh: no EVT_BACK_ZAP_COMPLETE branch in CEpgScan::handleMsg" >&2; exit 1; }
grep -q -F 'scanpause::onBackZap(' "$tmp/backzap" || { echo "src/driver/scanepg.cpp: EVT_BACK_ZAP_COMPLETE does not ask scanpause::onBackZap" >&2; fail=1; }

# 5. A stream that starts while the scan reads makes that read not count:
#    the scan's zap answer remembers the transponder it hands over, the
#    start of a stream marks that read and leaves the message to the
#    program's own handler, and the report of a pass asks scanpause before
#    it takes the transponder off the list. The read is forgotten when the
#    scan is cleared or stopped, not when Next() pauses: sectionsd stays on
#    that service until another one is handed over.
grep -q -F 'reading_tp = chan_tpid' "$tmp/backzap" || { echo "src/driver/scanepg.cpp: EVT_BACK_ZAP_COMPLETE does not remember the transponder it hands to sectionsd" >&2; fail=1; }
body 'int CEpgScan::handleMsg(' < "$tmp/scanepg" | awk '
	/if[ \t]*\(msg == NeutrinoMessages::EVT_STREAM_START\)[ \t]*\{/ { f = 1 }
	f { print }
	f && /return messages_return::/ { exit }
' > "$tmp/streamstart"
if [ ! -s "$tmp/streamstart" ]; then
	echo "src/driver/scanepg.cpp: CEpgScan::handleMsg has no EVT_STREAM_START branch" >&2
	fail=1
else
	grep -q -F 'reading_disturbed = true' "$tmp/streamstart" || { echo "src/driver/scanepg.cpp: EVT_STREAM_START does not mark the read it comes into" >&2; fail=1; }
	tail -1 "$tmp/streamstart" | grep -q -F 'return messages_return::unhandled;' || {
		echo "src/driver/scanepg.cpp: EVT_STREAM_START is claimed by the scan; the program's own handler would not run:" >&2
		tail -1 "$tmp/streamstart" | sed 's/^/  /' >&2
		fail=1
	}
fi
#    A setup that started no stream but asked zapit for a frontend is
#    announced with the end of streams (data 1); that marks the read as
#    well, and the message stays the program's.
body 'int CEpgScan::handleMsg(' < "$tmp/scanepg" | awk '
	/if[ \t]*\(msg == NeutrinoMessages::EVT_STREAM_STOP\)[ \t]*\{/ { f = 1 }
	f { print }
	f && /return messages_return::/ { exit }
' > "$tmp/streamstop"
if [ ! -s "$tmp/streamstop" ]; then
	echo "src/driver/scanepg.cpp: CEpgScan::handleMsg has no EVT_STREAM_STOP branch" >&2
	fail=1
else
	grep -q -E 'if[[:space:]]*\([[:space:]]*reading_tp[[:space:]]*&&[[:space:]]*data[[:space:]]*==[[:space:]]*1[[:space:]]*\)' "$tmp/streamstop" &&
	grep -q -F 'reading_disturbed = true' "$tmp/streamstop" || {
		echo "src/driver/scanepg.cpp: EVT_STREAM_STOP after a setup that asked zapit for a frontend does not mark the read" >&2
		fail=1
	}
	tail -1 "$tmp/streamstop" | grep -q -F 'return messages_return::unhandled;' || {
		echo "src/driver/scanepg.cpp: EVT_STREAM_STOP is claimed by the scan; the program's own handler would not run:" >&2
		tail -1 "$tmp/streamstop" | sed 's/^/  /' >&2
		fail=1
	}
fi
body 'int CEpgScan::handleMsg(' < "$tmp/scanepg" | awk '
	/else if[ \t]*\(msg == NeutrinoMessages::EVT_EIT_COMPLETE\)[ \t]*\{/ { f = 1 }
	f && /else if[ \t]*\(msg == NeutrinoMessages::EVT_BACK_ZAP_COMPLETE\)/ { exit }
	f { print }
' > "$tmp/eitcomplete"
[ -s "$tmp/eitcomplete" ] || { echo "check-epgscan-resume.sh: no EVT_EIT_COMPLETE branch in CEpgScan::handleMsg" >&2; exit 1; }
a=`grep -n -F 'scanpause::onEitComplete(' "$tmp/eitcomplete" | head -1 | cut -d: -f1`
e=`grep -n -F 'scanmap.erase(' "$tmp/eitcomplete" | head -1 | cut -d: -f1`
i=`grep -n -F 'scanned.insert(' "$tmp/eitcomplete" | head -1 | cut -d: -f1`
if [ -z "$a" ] || [ -z "$e" ] || [ -z "$i" ] || [ "$a" -gt "$e" ] || [ "$a" -gt "$i" ]; then
	echo "src/driver/scanepg.cpp: EVT_EIT_COMPLETE has to ask scanpause::onEitComplete before it counts the transponder read (ask ${a:-none}, erase ${e:-none}, insert ${i:-none})" >&2
	fail=1
fi
grep -q -F 'Resume()' "$tmp/eitcomplete" || { echo "src/driver/scanepg.cpp: a read to repeat with no stream left does not resume the scan" >&2; fail=1; }
#    The read to repeat pauses the scan and keeps its mark, so the end of
#    the stream resumes it and a later pass reported meanwhile does not
#    count. It does not count a re-read: sectionsd repeats its report while
#    a long stream keeps the scan paused, and nothing is handed over then.
awk '/if[ \t]*\(again\)[ \t]*\{/ { f = 1 } f { print } f && /return messages_return::handled;/ { exit }' "$tmp/eitcomplete" > "$tmp/again"
if [ ! -s "$tmp/again" ]; then
	echo "src/driver/scanepg.cpp: EVT_EIT_COMPLETE has no branch for a read to repeat (if (again))" >&2
	fail=1
else
	grep -q -E '^[[:space:]]*paused[[:space:]]*=[[:space:]]*true[[:space:]]*;' "$tmp/again" || { echo "src/driver/scanepg.cpp: a read to repeat does not pause the scan" >&2; fail=1; }
	grep -q -E '^[[:space:]]*reading_disturbed[[:space:]]*=[[:space:]]*true[[:space:]]*;' "$tmp/again" || { echo "src/driver/scanepg.cpp: a read to repeat does not keep its mark" >&2; fail=1; }
	if grep -n -E 'reading_disturbed[[:space:]]*=[[:space:]]*false|reading_tp[[:space:]]*=[[:space:]]*0' "$tmp/again" > "$tmp/hits"; then
		echo "src/driver/scanepg.cpp: a read to repeat forgets its mark or its transponder:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
	if grep -n -F 'rereads' "$tmp/again" > "$tmp/hits"; then
		echo "src/driver/scanepg.cpp: a read to repeat changes the re-read count; a re-read counts when it is handed over:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
fi
#    The re-reads in a row are bounded: scanpause is asked with their count,
#    which is forgotten with the read and set by scanpause when a
#    transponder is handed over, from the transponder handed over before and
#    whether its read was disturbed - so before both are replaced. Nothing
#    else in the scan moves the count.
awk '/scanpause::onEitComplete\(/ { f = 1 } f { print } f && /\)[ \t]*;/ { exit }' "$tmp/eitcomplete" | tr -d '\n' | grep -q -E 'busy,[[:space:]]*rereads[[:space:]]*\)' || {
	echo "src/driver/scanepg.cpp: EVT_EIT_COMPLETE does not tell scanpause::onEitComplete how often the transponder was read again" >&2
	fail=1
}
awk '/rereads[ \t]*=[ \t]*scanpause::rereadsAtHandOver\(/ { f = 1 } f { print } f && /\)[ \t]*;/ { exit }' "$tmp/backzap" | tr -d '\n' |
	grep -q -E 'rereads[[:space:]]*=[[:space:]]*scanpause::rereadsAtHandOver\([[:space:]]*chan_tpid[[:space:]]*==[[:space:]]*reading_tp[[:space:]]*,[[:space:]]*reading_disturbed[[:space:]]*,[[:space:]]*rereads[[:space:]]*\)[[:space:]]*;' || {
	echo "src/driver/scanepg.cpp: handing a transponder over does not set the re-reads by scanpause::rereadsAtHandOver(chan_tpid == reading_tp, reading_disturbed, rereads)" >&2
	fail=1
}
h=`grep -n -F 'scanpause::rereadsAtHandOver(' "$tmp/backzap" | head -1 | cut -d: -f1`
r=`grep -n -E 'reading_tp[[:space:]]*=[[:space:]]*chan_tpid[[:space:]]*;' "$tmp/backzap" | head -1 | cut -d: -f1`
d=`grep -n -E 'reading_disturbed[[:space:]]*=[[:space:]]*false[[:space:]]*;' "$tmp/backzap" | head -1 | cut -d: -f1`
if [ -z "$h" ] || [ -z "$r" ] || [ -z "$d" ] || [ "$h" -gt "$r" ] || [ "$h" -gt "$d" ]; then
	echo "src/driver/scanepg.cpp: the hand-over has to count the re-reads before it replaces the transponder and its mark (count ${h:-none}, transponder ${r:-none}, mark ${d:-none})" >&2
	fail=1
fi
if grep -n -E 'rereads[[:space:]]*(\+\+|--|[-+*/]=)|(\+\+|--)[[:space:]]*rereads' "$tmp/scanepg" > "$tmp/hits"; then
	echo "src/driver/scanepg.cpp: the re-read count is moved outside scanpause::rereadsAtHandOver:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi
for f in 'void CEpgScan::Clear(' 'void CEpgScan::Stop('; do
	body "$f" < "$tmp/scanepg" | grep -q -F 'reading_tp = 0' || { echo "src/driver/scanepg.cpp: $f...) does not forget the read" >&2; fail=1; }
	body "$f" < "$tmp/scanepg" | grep -q -F 'rereads = 0' || { echo "src/driver/scanepg.cpp: $f...) does not forget the re-reads" >&2; fail=1; }
done
if body 'void CEpgScan::Next(' < "$tmp/scanepg" | grep -q -F 'reading_tp = 0'; then
	echo "src/driver/scanepg.cpp: CEpgScan::Next forgets the read, also when it only pauses" >&2
	fail=1
fi

# 6. A setup that published nothing is announced like the end of a stream.
strip "$SRC/src/driver/streamts.cpp" | body 'void CStreamManager::run(' > "$tmp/loop"
grep -q -F 'streamsetup::idleAfter(' "$tmp/loop" || { echo "CStreamManager::run does not announce a setup that published nothing (streamsetup::idleAfter)" >&2; fail=1; }
grep -q -E 'postMsg\(NeutrinoMessages::EVT_STREAM_STOP,[[:space:]]*added\.asked_hold[[:space:]]*\?[[:space:]]*1[[:space:]]*:[[:space:]]*0[[:space:]]*\)' "$tmp/loop" || {
	echo "CStreamManager::run does not say with the end of streams whether the setup asked zapit for a frontend" >&2
	fail=1
}

#    The end of the last stream is announced once, by the hang-up that
#    ended it, as CStreamManager::RemoveClient decides under the manager's
#    lock. A hang-up of a stream that is gone already - stopped for a
#    recording, which announces itself, or by another hang-up - announces
#    nothing, and the loop reads the streams only under that lock.
strip "$SRC/src/driver/streamts.cpp" | body 'bool CStreamManager::RemoveClient(' > "$tmp/remove"
[ -s "$tmp/remove" ] || { echo "check-epgscan-resume.sh: no bool CStreamManager::RemoveClient in src/driver/streamts.cpp" >&2; exit 1; }
awk '
	{ line = $0 }
	/ScopedLock<OpenThreads::Mutex>/ && !lockdepth { lockdepth = depth; inlock = 1 }
	{ o = gsub(/\{/, "{"); c = gsub(/\}/, "}"); depth += o - c }
	inlock { print "L " line } !inlock { print "U " line }
	inlock && depth < lockdepth { inlock = 0 }
' "$tmp/remove" > "$tmp/removelock"
grep -q -E '^L .*idle[[:space:]]*=[[:space:]]*streams\.empty\(\)[[:space:]]*&&[[:space:]]*gate\.pendingLocked\(\)[[:space:]]*==[[:space:]]*0[[:space:]]*;' "$tmp/removelock" &&
grep -q -E 'return[[:space:]]+idle[[:space:]]*;' "$tmp/remove" || {
	echo "src/driver/streamts.cpp: CStreamManager::RemoveClient does not answer, under the manager's lock, whether it ended the last stream" >&2
	fail=1
}
awk '/if[ \t]*\([ \t]*RemoveClient\(pfd\[i\]\.fd\)[ \t]*\)/ { f = NR } f && NR <= f + 3 && /postMsg\(NeutrinoMessages::EVT_STREAM_STOP,[ \t]*0[ \t]*\)/ { ok = 1 } END { exit !ok }' "$tmp/loop" &&
[ `grep -c -F 'RemoveClient(' "$tmp/loop"` -eq 1 ] || {
	echo "CStreamManager::run does not announce the end of streams by what RemoveClient answers" >&2
	fail=1
}
if [ `grep -c -F 'streams.empty()' "$tmp/loop"` -ne 1 ] || ! grep -F 'streams.empty()' "$tmp/loop" | grep -q -F 'streamsetup::idleAfter(' ||
   ! awk '/streams\.empty\(\)/ { exit !(prev ~ /^[ \t]*mutex\.lock\(\);[ \t]*$/) } { prev = $0 }' "$tmp/loop"; then
	echo "CStreamManager::run reads whether streams run elsewhere than for streamsetup::idleAfter, under the lock:" >&2
	grep -n -F 'streams.empty()' "$tmp/loop" | sed 's/^/  /' >&2
	fail=1
fi

# 7. Whether anything keeps zapit awake reads the recordings themselves: an
#    end of streams can come in after a recording started and before its
#    record mode is known.
for f in 'void CNeutrinoApp::standbyToStandby(' 'void CNeutrinoApp::tryDeferredDeepStandby('; do
	body "$f" < "$tmp/neutrino" | awk '/bool alive =/ { f = 1 } f { print } f && /;[ \t]*$/ { exit }' > "$tmp/alive"
	grep -q -F 'CRecordManager::getInstance()->RecordingStatus()' "$tmp/alive" || {
		echo "$f...): alive does not read CRecordManager::RecordingStatus()" >&2
		fail=1
	}
done

# 8. A retune of the frontend the scan reads makes the read not count.
#    Every tune and every switch of DiSEqC or voltage is counted.
strip "$SRC/src/zapit/frontend.cpp" > "$tmp/frontend"
body 'int CFrontend::setFrontend(' < "$tmp/frontend" | grep -q -E 'tune_count[[:space:]]*\+\+' || {
	echo "src/zapit/frontend.cpp: CFrontend::setFrontend does not count the tune" >&2
	fail=1
}
body 'void CFrontend::setInput(t_satellite_position' < "$tmp/frontend" > "$tmp/setinput"
u=`grep -n -F 'DISEQC_UNICABLE2' "$tmp/setinput" | head -1 | cut -d: -f1`
c=`grep -n -E 'tune_count[[:space:]]*\+\+' "$tmp/setinput" | head -1 | cut -d: -f1`
q=`grep -n -F 'setDiseqc' "$tmp/setinput" | head -1 | cut -d: -f1`
if [ -z "$u" ] || [ -z "$c" ] || [ -z "$q" ] || [ "$c" -lt "$u" ] || [ "$c" -gt "$q" ]; then
	echo "src/zapit/frontend.cpp: CFrontend::setInput has to count a switch of DiSEqC or voltage before it sends one (unicable ${u:-none}, count ${c:-none}, switch ${q:-none})" >&2
	fail=1
fi
#    zapit records the scan's own frontend, not the live one, right after
#    the scan's zap tuned it, and takes the record back when a zap starts.
strip "$SRC/src/zapit/zapit.cpp" | body 'bool CZapit::ZapForEpg(' > "$tmp/zapforepg"
[ -s "$tmp/zapforepg" ] || { echo "check-epgscan-resume.sh: no CZapit::ZapForEpg in src/zapit/zapit.cpp" >&2; exit 1; }
k=`grep -n -E 'epg_tune_fe[[:space:]]*=[[:space:]]*NULL' "$tmp/zapforepg" | head -1 | cut -d: -f1`
t=`grep -n -F 'TuneChannel(frontend' "$tmp/zapforepg" | head -1 | cut -d: -f1`
r=`grep -n -E 'epg_tune_fe[[:space:]]*=[[:space:]]*frontend[[:space:]]*;' "$tmp/zapforepg" | head -1 | cut -d: -f1`
n=`grep -n -E 'epg_tune_count[[:space:]]*=[[:space:]]*frontend->getTuneCount\(\)' "$tmp/zapforepg" | head -1 | cut -d: -f1`
if [ -z "$k" ] || [ -z "$t" ] || [ -z "$r" ] || [ -z "$n" ] || [ "$k" -gt "$t" ] || [ "$t" -gt "$r" ] || [ "$t" -gt "$n" ]; then
	echo "src/zapit/zapit.cpp: CZapit::ZapForEpg has to take its record back, tune, then record its own frontend and that one's count (back ${k:-none}, tune ${t:-none}, frontend ${r:-none}, count ${n:-none})" >&2
	fail=1
fi
if grep -n -F 'live_fe->getTuneCount' "$tmp/zapforepg" > "$tmp/hits"; then
	echo "src/zapit/zapit.cpp: CZapit::ZapForEpg records the live frontend's count:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi
#    The scan takes the record for the channel it hands over, after the
#    transponder, and never the live frontend.
grep -q -E 'GetEpgScanTune\([[:space:]]*next_chid[[:space:]]*,[[:space:]]*reading_fe[[:space:]]*,[[:space:]]*reading_tunes[[:space:]]*\)' "$tmp/backzap" || {
	echo "src/driver/scanepg.cpp: handing a transponder over does not take zapit's record of the scan's zap for that channel (GetEpgScanTune(next_chid, ...))" >&2
	fail=1
}
if grep -n -E 'GetLiveFrontend\(\)|getTuneCount' "$tmp/backzap" > "$tmp/hits"; then
	echo "src/driver/scanepg.cpp: the hand-over reads a frontend's count itself instead of zapit's record of the scan's zap:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi
#    The report compares the count before onEitComplete and hands it the
#    result.
x=`grep -n -F 'scanpause::readDisturbed(' "$tmp/eitcomplete" | head -1 | cut -d: -f1`
a=`grep -n -F 'scanpause::onEitComplete(' "$tmp/eitcomplete" | head -1 | cut -d: -f1`
if [ -z "$x" ] || [ -z "$a" ] || [ "$x" -gt "$a" ]; then
	echo "src/driver/scanepg.cpp: EVT_EIT_COMPLETE has to ask scanpause::readDisturbed before scanpause::onEitComplete (disturbed ${x:-none}, ask ${a:-none})" >&2
	fail=1
fi
awk '/scanpause::readDisturbed\(/ { f = 1 } f { print } f && /\)[ \t]*;/ { exit }' "$tmp/eitcomplete" | tr '\n' ' ' |
	grep -q -E 'readDisturbed\([[:space:]]*reading_disturbed[[:space:]]*,[[:space:]]*reading_fe[[:space:]]*!=[[:space:]]*NULL[[:space:]]*,[[:space:]]*reading_tunes[[:space:]]*,.*reading_fe->getTuneCount\(\)' || {
	echo "src/driver/scanepg.cpp: EVT_EIT_COMPLETE does not compare the scan frontend's count now with zapit's record" >&2
	fail=1
}
awk '/scanpause::onEitComplete\(/ { f = 1 } f { print } f && /\)[ \t]*;/ { exit }' "$tmp/eitcomplete" | tr '\n' ' ' | grep -q -E '[(,][[:space:]]*disturbed[[:space:]]*,[[:space:]]*busy' || {
	echo "src/driver/scanepg.cpp: EVT_EIT_COMPLETE does not hand scanpause::onEitComplete the comparison" >&2
	fail=1
}
for f in 'void CEpgScan::Clear(' 'void CEpgScan::Stop('; do
	body "$f" < "$tmp/scanepg" | grep -q -E 'reading_fe[[:space:]]*=[[:space:]]*NULL' || { echo "src/driver/scanepg.cpp: $f...) does not forget the frontend read" >&2; fail=1; }
done

exit $fail
