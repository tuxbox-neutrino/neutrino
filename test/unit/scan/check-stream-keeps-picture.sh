#!/bin/sh
# A stream changes what the box shows only through one door, and only where
# the setting streaming_switch_picture allows it: the stream server asks the
# GUI with EVT_STREAM_SWITCH, the GUI checks again and asks zapit
# (CMD_STREAM_SWITCH), and zapit's SwitchForStream zaps the ordinary way with
# the frontends of streams and PiPs kept busy, then stops the PiP slots showing
# the channel. Everything else holds as follows.
# The stream server takes the
# frontend zapit holds for it, on zapit's thread, or none: it does not zap,
# stop a PiP, allocate, lock or unlock a frontend itself, and it does not
# reach into the frontend manager at all. zapit's hold for a stream does not
# zap the picture, stop a PiP, allocate a frontend the way a zap or a
# recording does, set the live frontend or announce anything: it binds the
# frontend the admission chose, or the picture's own without retuning it.
# zapit's yield for a recording does none of these either: it takes stream
# holds back, and the recording does the rest. Only zapit's thread enters,
# collects, takes back and closes the hold register; any other thread only
# adopts and drops.
#
# The unit cases drive the hold and the setup over models; this holds the
# program's code to staying inside them. Neither the stream server nor zapit
# can be linked into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-stream-keeps-picture.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/driver/streamts.cpp src/zapit/zapit.cpp src/zapit/streamholdcmd.cpp src/zapit/streamyield.cpp src/zapit/femanager.cpp src/neutrino.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-stream-keeps-picture.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-stream-keeps-picture.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
strip() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1"
}

# 1. The stream server
strip "$SRC/src/driver/streamts.cpp" > "$tmp/streamts"
for word in 'zapTo_' 'stopPip' 'allocateFE' 'lockFrontend' 'unlockFrontend' 'CFEManager::getInstance'; do
	if grep -n -F -- "$word" "$tmp/streamts" > "$tmp/hits"; then
		echo "src/driver/streamts.cpp uses $word; a stream takes the frontend zapit holds for it:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done

# 2. zapit's hold and yield: the commands, the steps they run over and the
#    order they run them in
strip "$SRC/src/zapit/zapit.cpp" | awk '
	/^class CZapit::StreamHoldOps/ { inside = 1 }
	/^void CZapit::HoldForStream\(/ { inside = 1 }
	/^class CZapit::StreamYieldOps/ { inside = 1 }
	/^void CZapit::YieldStreams\(/ { inside = 1 }
	inside { print }
	inside && /^};?$/ { inside = 0 }
' > "$tmp/hold"
strip "$SRC/src/zapit/streamholdcmd.cpp" >> "$tmp/hold"
strip "$SRC/src/zapit/streamyield.cpp" >> "$tmp/hold"
for what in 'class CZapit::StreamHoldOps' 'void CZapit::HoldForStream(' 'class CZapit::StreamYieldOps' 'void CZapit::YieldStreams('; do
	grep -q -F "$what" "$tmp/hold" || { echo "check-stream-keeps-picture.sh: no $what in src/zapit/zapit.cpp" >&2; exit 1; }
done
for word in 'allocateFE' 'StopPip' 'ZapIt(' 'setLiveFE' 'SendEvent'; do
	if grep -n -F -- "$word" "$tmp/hold" > "$tmp/hits"; then
		echo "zapit's hold or yield for a stream uses $word; it may only bind the frontend the admission chose, or take holds back:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done

# ... and the hold of another channel on the live transponder reads PAT and
# PMT off the live frontend as it stands: nothing in it tunes.
awk '
	/^class CZapit::StreamHoldOps/ { inside = 1 }
	inside && /^};/ { inside = 0 }
	inside && /bool readPids\(/ { f = 1 }
	inside && f { print }
	f && /^\t\t}/ { exit }
' "$tmp/hold" > "$tmp/readpids"
grep -q -F 'ParsePatPmt(' "$tmp/readpids" || { echo "check-stream-keeps-picture.sh: no StreamHoldOps::readPids reading PAT and PMT in src/zapit/zapit.cpp" >&2; exit 1; }
for word in 'TuneChannel' 'tuneChannel' 'setInput' 'driveToSatellitePosition' 'tune('; do
	if grep -n -F -- "$word" "$tmp/readpids" > "$tmp/hits"; then
		echo "zapit's StreamHoldOps::readPids uses $word; the live frontend is not retuned for a stream:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done

# ... through the demux the hold bound: holdLiveForStream gives the channel
# the binding of the live frontend as its record demux, which ParsePatPmt
# reads, and as its stream demux, which the stream server reads.
strip "$SRC/src/zapit/femanager.cpp" | awk '
	/^void CFEManager::holdLiveForStream\(/ { f = 1 }
	f && /^#else/ { exit }
	f { print }
	f && /^}/ { exit }
' > "$tmp/holdlive"
grep -q -F 'void CFEManager::holdLiveForStream(' "$tmp/holdlive" || { echo "check-stream-keeps-picture.sh: no CFEManager::holdLiveForStream in src/zapit/femanager.cpp" >&2; exit 1; }
for what in 'b = fesearch::binding(live->fenumber)' 'channel->setRecordDemux(b.demux)' 'channel->setStreamDemux(b.demux)' 'cDemux::SetSource(b.demux, b.source)'; do
	if ! grep -q -F -- "$what" "$tmp/holdlive"; then
		echo "src/zapit/femanager.cpp: holdLiveForStream does not do $what; the stream's PAT, PMT and packets are read through the live frontend's binding" >&2
		fail=1
	fi
done
strip "$SRC/src/zapit/zapit.cpp" | awk '
	/^bool CZapit::ParsePatPmt\(/ { f = 1 }
	f { print }
	f && /^}/ { exit }
' | grep -q -F 'getRecordDemux()' || { echo "src/zapit/zapit.cpp: CZapit::ParsePatPmt no longer reads through the channel's record demux, which holdLiveForStream binds for a shared stream" >&2; fail=1; }

# 4. The one door. The stream server asks the GUI, behind the setting, and
#    never zapit itself; the GUI asks zapit behind the setting and does not zap
#    itself; zapit's switch zaps through ZapTo with the kept frontends and
#    nothing of its own; the prediction leaves the frontends as it found them.
grep -q -F 'EVT_STREAM_SWITCH' "$tmp/streamts" || { echo "check-stream-keeps-picture.sh: src/driver/streamts.cpp posts no EVT_STREAM_SWITCH" >&2; exit 1; }
grep -q -F 'bool settingOn() { return g_settings.streaming_switch_picture != 0; }' "$tmp/streamts" || { echo "src/driver/streamts.cpp: the hold's steps do not ask for a switch exactly where streaming_switch_picture is on" >&2; fail=1; }
grep -q -F 'bool clientGone() { return streamsetup::peerGone(fd); }' "$tmp/streamts" || { echo "src/driver/streamts.cpp: the hold's steps do not look whether the client hung up before a switch" >&2; fail=1; }
# ... asks only while its setup may still publish, and every stop that takes
# setups back cancels a request still pending as well.
awk '
	/^int CStreamManager::RequestSwitch\(/ { f = 1 }
	f { print }
	f && /^}/ { exit }
' "$tmp/streamts" | grep -q -F 'beginSwitchLocked(' || { echo "src/driver/streamts.cpp: RequestSwitch asks without holding the request to its setup (beginSwitchLocked)" >&2; fail=1; }
awk '
	/^[a-z].* CStreamManager::[A-Za-z]+\(/ { name = $0; cancels = 0; drops = 0; inside = 1 }
	inside && /gate\.cancel(Channel|Frontend)Locked\(/ { cancels = 1 }
	inside && /switch_wait\.cancel(For|Pending)\(/ { drops = 1 }
	inside && /^}/ { if (cancels && !drops) print name; inside = 0 }
' "$tmp/streamts" > "$tmp/hits"
if [ -s "$tmp/hits" ]; then
	echo "src/driver/streamts.cpp takes setups back without cancelling a pending switch request in:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi
for word in 'switchForStream' 'CMD_STREAM_SWITCH'; do
	if grep -n -F -- "$word" "$tmp/streamts" > "$tmp/hits"; then
		echo "src/driver/streamts.cpp uses $word; the stream server asks the GUI, which asks zapit:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done
strip "$SRC/src/neutrino.cpp" | awk '
	/^int CNeutrinoApp::switchPictureForStream\(/ { f = 1 }
	f { print }
	f && /^}/ { exit }
' > "$tmp/gui"
grep -q -F 'switchForStream(' "$tmp/gui" || { echo "check-stream-keeps-picture.sh: no CNeutrinoApp::switchPictureForStream asking zapit in src/neutrino.cpp" >&2; exit 1; }
grep -q -F 'g_settings.streaming_switch_picture' "$tmp/gui" || { echo "src/neutrino.cpp: switchPictureForStream does not look at streaming_switch_picture" >&2; fail=1; }
for word in 'zapTo_' 'zapTo(' 'stopPip' 'StopPip'; do
	if grep -n -F -- "$word" "$tmp/gui" > "$tmp/hits"; then
		echo "src/neutrino.cpp: switchPictureForStream uses $word; zapit switches the picture for a stream:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done
# Neither the GUI nor the stream server switches to a channel the channel
# list would ask the PIN for (streamsetup::pinNeeded, by its rule), nor away
# from a picture whose channel would need it; the GUI does not switch a
# picture it holds back.
pin='s.pin_needed = streamsetup::pinNeeded(g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_NEVER,'
shownpin='s.shown_pin_needed = !shown ||'
for what in "$pin" "$shownpin" 'FindChannel(g_RemoteControl->current_channel_id)' 's.picture_held = !g_RemoteControl->is_video_started;'; do
	grep -q -F -- "$what" "$tmp/gui" || { echo "src/neutrino.cpp: switchPictureForStream does not do $what; no switch where the GUI would ask the PIN or holds the picture back" >&2; fail=1; }
done
awk '
	/^bool CStreamManager::SwitchEligible\(/ { f = 1 }
	f { print }
	f && /^}/ { exit }
' "$tmp/streamts" > "$tmp/eligible"
for what in "$pin" "$shownpin" 'FindChannel(CZapit::getInstance()->GetCurrentChannelID())'; do
	grep -q -F -- "$what" "$tmp/eligible" || { echo "src/driver/streamts.cpp: SwitchEligible does not do $what; no switch is asked where the GUI would ask the PIN" >&2; fail=1; }
done
# ... and only for a request the setup still waits for: the GUI's message
# goes through ServeSwitch, which claims it first, and nothing else calls the
# switch.
strip "$SRC/src/neutrino.cpp" | awk '
	/msg == NeutrinoMessages::EVT_STREAM_SWITCH\)/ { f = 1 }
	f { print }
	f && /return messages_return::handled;/ { exit }
' > "$tmp/guimsg"
grep -q -F 'ServeSwitch(asked.seq, asked.channel_id, for_stream)' "$tmp/guimsg" || { echo "src/neutrino.cpp: the EVT_STREAM_SWITCH branch does not switch through ServeSwitch, which claims the request first" >&2; fail=1; }
strip "$SRC/src/neutrino.cpp" | grep -n -F 'switchPictureForStream(' | grep -v -F 'int CNeutrinoApp::switchPictureForStream(' > "$tmp/hits" || true
if [ "`sed 's/^[0-9]*://; s/^[ \t]*//' "$tmp/hits"`" != "int switchPicture(uint64_t channel) { return app.switchPictureForStream(channel); }" ]; then
	echo "src/neutrino.cpp: switchPictureForStream is called besides the SwitchPicture ServeSwitch carries out:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi
# The switch's own steps; the rest of the class hands on to the hold's.
strip "$SRC/src/zapit/zapit.cpp" | awk '
	/^class CZapit::StreamSwitchOps/ { inside = 1 }
	inside && /void stopPipsShowing\(/ { f = 1 }
	inside && /bool answerSwitch\(/ { exit }
	f { print }
' > "$tmp/switch"
grep -q -F 'void stopPipsShowing(' "$tmp/switch" || { echo "check-stream-keeps-picture.sh: no StreamSwitchOps::stopPipsShowing in src/zapit/zapit.cpp" >&2; exit 1; }
grep -q -F 'StopPip(i)' "$tmp/switch" || { echo "src/zapit/zapit.cpp: StreamSwitchOps does not stop a PiP slot by itself" >&2; fail=1; }
for what in 'ZapTo(' 'keepForZap(' 'releaseKept(' 'zap_keeps_pip = true'; do
	grep -q -F -- "$what" "$tmp/switch" || { echo "src/zapit/zapit.cpp: StreamSwitchOps does not do $what; the switch zaps the ordinary way with the kept frontends" >&2; fail=1; }
done
for word in 'allocateFE' 'setLiveFE' 'lockFrontend' 'unlockFrontend' 'ZapIt(' 'tune(' 'StreamKeep('; do
	if grep -n -F -- "$word" "$tmp/switch" > "$tmp/hits"; then
		echo "src/zapit/zapit.cpp: StreamSwitchOps uses $word; it zaps through ZapTo with the set the prediction kept and nothing of its own:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done
strip "$SRC/src/zapit/zapit.cpp" | grep -q -E 'if \(fe == NULL && !zap_keeps_pip\)' || { echo "src/zapit/zapit.cpp: ZapIt stops a PiP to find a frontend also for a stream's switch" >&2; fail=1; }
strip "$SRC/src/zapit/femanager.cpp" | awk '
	/^CFrontend \* CFEManager::zapFrontendFor\(/ { f = 1 }
	f { print }
	f && /^}/ { exit }
' > "$tmp/predict"
for what in 'return fesearch::pickKept(frontends(), useCounts(keep), walk);' 'return fem->getFrontend(channel);'; do
	grep -q -F -- "$what" "$tmp/predict" || { echo "src/zapit/femanager.cpp: zapFrontendFor does not do $what; the prediction is getFrontend with the kept frontends counted busy (fesearch::pickKept)" >&2; fail=1; }
done
for word in 'lockFrontend' 'unlockFrontend' 'fbcTie' 'fbcUntie' 'setLiveFE' 'SetSource'; do
	if grep -n -F -- "$word" "$tmp/predict" > "$tmp/hits"; then
		echo "src/zapit/femanager.cpp: zapFrontendFor uses $word; the prediction changes nothing:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done

# ... the dish the zap may turn, read off the dish's own frontend, and the
# kept counts raised and released by the helpers the unit cases drive.
fnbody() {
	strip "$SRC/src/zapit/femanager.cpp" | awk -v head="$1" '
		index($0, head) == 1 { f = 1 }
		f { print }
		f && /^}/ { exit }
	'
}
fnbody 'bool CFEManager::mayTurnDish(' > "$tmp/dish"
for what in 'fesearch::dishOf(fe, frontends(), CFrontend::linked(fe->getMode()))' 'return fesearch::turnsDish(dish->have_rotor, dish->getConfig().use_usals, listed,' 'sit->second.motor_position' 'sit->second.use_usals'; do
	grep -q -F -- "$what" "$tmp/dish" || { echo "src/zapit/femanager.cpp: mayTurnDish does not do $what; a switch turns no dish" >&2; fail=1; }
done
fnbody 'void CFEManager::keepForZap(' | grep -q -F 'if (fesearch::raiseKept(useCounts(keep)))' || { echo "src/zapit/femanager.cpp: keepForZap does not raise the kept counts through fesearch::raiseKept" >&2; fail=1; }
fnbody 'void CFEManager::releaseKept(' | grep -q -F 'have_locked = fesearch::releaseKept(useCounts(keep),' || { echo "src/zapit/femanager.cpp: releaseKept does not work out have_locked again through fesearch::releaseKept" >&2; fail=1; }

# 3. Only zapit's thread writes the register's places.
( cd "$SRC" && grep -r -l -E 'streamhold::(offer|collect|shutdown|revoke)[ \t]*\(' src --include='*.cpp' --include='*.h' ) | sort > "$tmp/writers" || true
if [ "`cat "$tmp/writers"`" != "src/zapit/zapit.cpp" ]; then
	echo "streamhold::offer, collect, revoke and shutdown belong to zapit's thread; called from:" >&2
	sed 's/^/  /' "$tmp/writers" >&2
	fail=1
fi

exit $fail
