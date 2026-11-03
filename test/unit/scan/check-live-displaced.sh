#!/bin/sh
# A zap to the channel the box shows is skipped as one that changes
# nothing, in the channel list, in the program's zap message and in the old
# web interface's zap. A stream can hold the live frontend on another
# transponder, after a zap to the live channel found no frontend for it;
# then that zap is the way back to a picture, and each of the three lets it
# through. zapit says whether the live channel is displaced in this way
# through livetp, from the live frontend it zapped and the live channel's
# transponder. The end of the streams brings the picture back by itself:
# when the box is awake in TV or radio mode and the live channel is
# displaced, zapit gives the ended streams' holds back within a deadline
# that ends before anything else is asked, and the box zaps the way a wake
# does it, to the channel it was last asked to show when the list holds
# it, unless a stream still runs or this displacement was tried already.
# While a zap the program sent is pending, the remote control would drop
# this one: the try is not marked, and a timer has the check made again
# once that zap is over. The mark of a try is forgotten when a stream
# starts, when the live channel's zap completes, and on the way into and
# out of standby.
#
# The unit cases drive livetp over values; this holds the program's code to
# asking it. Neither zapit, the channel list nor the program can be linked
# into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-live-displaced.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/zapit/zapit.cpp src/gui/channellist.cpp src/neutrino.cpp src/nhttpd/tuxboxapi/neutrinoapi.cpp src/daemonc/remotecontrol.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-live-displaced.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-live-displaced.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
strip() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1"
}
body() {
	awk -v start="$1" 'index($0, start) == 1 { f = 1 } f { print } f && /^}/ { exit }'
}

# 1. zapit asks livetp with the live channel's kind, the live frontend's
#    state and transponder, the channel's transponder and whether it is NVOD.
strip "$SRC/src/zapit/zapit.cpp" | body 'bool CZapit::LiveDisplaced(' > "$tmp/displaced"
[ -s "$tmp/displaced" ] || { echo "check-live-displaced.sh: no CZapit::LiveDisplaced in src/zapit/zapit.cpp" >&2; exit 1; }
tr '\n' ' ' < "$tmp/displaced" | grep -q -E 'livetp::displaced\([[:space:]]*IS_WEBCHAN\([^)]*\)[[:space:]]*,[^,]*,[^,]*->tuned[^,]*,[^,]*getTsidOnid\(\)[^,]*,[[:space:]]*channel_tp[[:space:]]*,[[:space:]]*current_is_nvod[[:space:]]*\)' || {
	echo "src/zapit/zapit.cpp: CZapit::LiveDisplaced does not ask livetp::displaced with the live channel, the live frontend and the channel's transponder:" >&2
	sed 's/^/  /' "$tmp/displaced" >&2
	fail=1
}
grep -q -F 'live_fe' "$tmp/displaced" || { echo "src/zapit/zapit.cpp: CZapit::LiveDisplaced does not read the live frontend" >&2; fail=1; }
#    The channel and its transponder are read under the channel lock, which
#    a reload frees the channels under.
guarded=`awk '
	/CServiceManager::ChannelGuard[ \t]/ && !g { g = depth + 1 }
	/FindChannel\(/ { f = (g && depth + 1 >= g) ? "in" : "out" }
	/channel_tp[ \t]*=[ \t]*channel->getTransponderId\(\)/ { t = (g && depth + 1 >= g) ? "in" : "out" }
	{ o = gsub(/\{/, "{"); c = gsub(/\}/, "}"); depth += o - c; if (g && depth + 1 < g) g = 0 }
	END { print f "/" t }
' "$tmp/displaced"`
if [ "$guarded" != "in/in" ]; then
	echo "src/zapit/zapit.cpp: CZapit::LiveDisplaced reads the channel and its transponder without the channel lock (lookup/transponder: $guarded)" >&2
	fail=1
fi

# 2. The channel list zaps to the channel it shows when the live channel is
#    displaced. It asks last, in the condition itself: LiveDisplaced takes
#    the channel lock, and a forced zap can come under a recording's lock,
#    which a web thread takes inside the channel lock.
strip "$SRC/src/gui/channellist.cpp" | body 'void CChannelList::zapToChannel(' > "$tmp/list"
[ -s "$tmp/list" ] || { echo "check-live-displaced.sh: no CChannelList::zapToChannel in src/gui/channellist.cpp" >&2; exit 1; }
grep -q -E '^[[:space:]]*if[[:space:]]*\([[:space:]]*force[[:space:]]*\|\|[[:space:]]*\([[:space:]]*selected_chid[[:space:]]*!=[[:space:]]*channel->getChannelID\(\)[[:space:]]*\)[[:space:]]*\|\|[[:space:]]*CZapit::getInstance\(\)->LiveDisplaced\(\)[[:space:]]*\)' "$tmp/list" || {
	echo "src/gui/channellist.cpp: CChannelList::zapToChannel does not let a zap to the channel it shows through when the live channel is displaced, asked last (force || selected_chid != id || LiveDisplaced())" >&2
	fail=1
}
n=`grep -c -F 'LiveDisplaced()' "$tmp/list" || true`
if [ "$n" != 1 ]; then
	echo "src/gui/channellist.cpp: CChannelList::zapToChannel asks LiveDisplaced() $n times; once, last in the condition, so a forced zap does not take the channel lock" >&2
	fail=1
fi

# 3. The program's zap message, and through it every zap of the web
#    interface's API, likewise.
strip "$SRC/src/neutrino.cpp" | awk '/msg == NeutrinoMessages::ZAPTO\)/ { f = 1 } f { print } f && /return messages_return::handled;/ { exit }' > "$tmp/zapto"
[ -s "$tmp/zapto" ] || { echo "check-live-displaced.sh: no ZAPTO handler in src/neutrino.cpp" >&2; exit 1; }
tr '\n' ' ' < "$tmp/zapto" | grep -q -E 'if[[:space:]]*\([[:space:]]*!woken[[:space:]]*&&[[:space:]]*\([[:space:]]*channel_id[[:space:]]*!=[[:space:]]*CZapit::getInstance\(\)->GetCurrentChannelID\(\)[[:space:]]*\|\|[[:space:]]*CZapit::getInstance\(\)->LiveDisplaced\(\)[[:space:]]*\)[[:space:]]*\)' || {
	echo "src/neutrino.cpp: the ZAPTO handler does not let a zap to the live channel through when it is displaced, asked in the condition after the channel test" >&2
	fail=1
}

# 4. The old web interface's zap likewise.
strip "$SRC/src/nhttpd/tuxboxapi/neutrinoapi.cpp" | body 'void CNeutrinoAPI::ZapToChannelId(' > "$tmp/api"
[ -s "$tmp/api" ] || { echo "check-live-displaced.sh: no CNeutrinoAPI::ZapToChannelId in src/nhttpd/tuxboxapi/neutrinoapi.cpp" >&2; exit 1; }
tr '\n' ' ' < "$tmp/api" | grep -q -E 'if[[:space:]]*\([[:space:]]*channel_id[[:space:]]*==[[:space:]]*Zapit->getCurrentServiceID\(\)[[:space:]]*&&[[:space:]]*!CZapit::getInstance\(\)->LiveDisplaced\(\)[[:space:]]*\)' || {
	echo "src/nhttpd/tuxboxapi/neutrinoapi.cpp: CNeutrinoAPI::ZapToChannelId does not let a zap to the live channel through when it is displaced, asked in the condition after the channel test" >&2
	fail=1
}

# 5. The end of the streams brings the picture back.
strip "$SRC/src/neutrino.cpp" > "$tmp/neutrino"
awk '/msg == NeutrinoMessages::EVT_STREAM_STOP\)/ { f = 1 } f { print } f && /return messages_return::handled;/ { exit }' "$tmp/neutrino" > "$tmp/stop"
[ -s "$tmp/stop" ] || { echo "check-live-displaced.sh: no EVT_STREAM_STOP handler in src/neutrino.cpp" >&2; exit 1; }
grep -q -E 'rezapAfterStreams\([[:space:]]*mode[[:space:]]*,[[:space:]]*channelList[[:space:]]*\)' "$tmp/stop" || {
	echo "src/neutrino.cpp: the EVT_STREAM_STOP handler does not bring the live channel back (rezapAfterStreams)" >&2
	fail=1
}
body 'static void rezapAfterStreams(' < "$tmp/neutrino" > "$tmp/rezap"
[ -s "$tmp/rezap" ] || { echo "check-live-displaced.sh: no rezapAfterStreams in src/neutrino.cpp" >&2; exit 1; }
w=`grep -n -F 'livetp::rezapWanted(' "$tmp/rezap" | head -1 | cut -d: -f1`
y=`grep -n -F 'yieldStreams(0' "$tmp/rezap" | head -1 | cut -d: -f1`
t=`grep -n -F 'livetp::rezapTarget(' "$tmp/rezap" | head -1 | cut -d: -f1`
d=`grep -n -F 'live_rezap.next(' "$tmp/rezap" | head -1 | cut -d: -f1`
z=`grep -n -E 'zapTo_ChannelID\([[:space:]]*target[[:space:]]*,[[:space:]]*true[[:space:]]*\)' "$tmp/rezap" | head -1 | cut -d: -f1`
if [ -z "$w" ] || [ -z "$y" ] || [ -z "$t" ] || [ -z "$d" ] || [ -z "$z" ] || [ "$w" -gt "$y" ] || [ "$y" -gt "$t" ] || [ "$t" -gt "$d" ] || [ "$d" -gt "$z" ]; then
	echo "src/neutrino.cpp: rezapAfterStreams has to ask livetp::rezapWanted, have zapit collect, choose the target, ask live_rezap.next and then force the zap to the target, in that order (ask ${w:-none}, collect ${y:-none}, target ${t:-none}, next ${d:-none}, zap ${z:-none})" >&2
	fail=1
fi
#    The deadline bounds the exchanges of the thread's zapit calls while it
#    lives, so its block has ended where the state is read again and the
#    zap goes out.
scope=`awk '
	/CBasicClient::Deadline[ \t]/ && !dl { dl = depth + 1 }
	/live_rezap\.next\(/ && !n { n = 1; print (dl && depth + 1 >= dl) ? "inside" : "outside"; exit }
	{ o = gsub(/\{/, "{"); c = gsub(/\}/, "}"); depth += o - c }
' "$tmp/rezap"`
if [ "$scope" != outside ]; then
	echo "src/neutrino.cpp: rezapAfterStreams reads the state again and zaps while the zapit deadline is still in scope (${scope:-no next found})" >&2
	fail=1
fi
grep -q -E 'CBasicClient::Deadline[[:space:]]' "$tmp/rezap" || { echo "src/neutrino.cpp: rezapAfterStreams has no deadline for zapit's collect" >&2; fail=1; }
#    After the collect: still displaced, no stream, no zap of the program
#    pending, for the target and the live frontend's transponder.
awk '/live_rezap\.next\(/ { f = 1 } f { print } f && /\)\)[ \t]*\{?[ \t]*$/ { exit }' "$tmp/rezap" | tr '\n' ' ' |
	grep -q -E 'LiveDisplaced\(\)[[:space:]]*,[[:space:]]*CStreamManager::getInstance\(\)->StreamStatus\(\)[[:space:]]*,[[:space:]]*g_RemoteControl->zapPending\(\)[[:space:]]*,[[:space:]]*target[[:space:]]*,' || {
	echo "src/neutrino.cpp: rezapAfterStreams does not read, after the collect, whether the live channel is still displaced, whether a stream runs and whether a zap of the program is pending, for the target" >&2
	fail=1
}
#    The target is the channel the remote control was last asked for when
#    the list holds it, zapit's live channel otherwise.
awk '/livetp::rezapTarget\(/ { f = 1 } f { print } f && /;[ \t]*$/ { exit }' "$tmp/rezap" | tr '\n' ' ' |
	grep -q -E 'rezapTarget\([[:space:]]*asked[[:space:]]*,[[:space:]]*IS_WEBCHAN\([[:space:]]*asked[[:space:]]*\)[[:space:]]*,[^,]*hasChannelID\([[:space:]]*asked[[:space:]]*\)[^,]*,[[:space:]]*CZapit::getInstance\(\)->GetCurrentChannelID\(\)' &&
grep -q -E 'asked[[:space:]]*=[[:space:]]*g_RemoteControl->current_channel_id' "$tmp/rezap" || {
	echo "src/neutrino.cpp: rezapAfterStreams does not choose the channel last asked for (g_RemoteControl->current_channel_id) when the list holds it" >&2
	fail=1
}
#    A pending zap arms the timer, and the timer makes the check again.
awk '/case[ \t]+livetp::Rezap::Later:/ { f = 1 } f { print } f && /return;/ { exit }' "$tmp/rezap" | grep -q -E 'rezap_timer[[:space:]]*=[[:space:]]*g_RCInput->addTimer\(' || {
	echo "src/neutrino.cpp: rezapAfterStreams does not arm the timer for a check once the pending zap is over" >&2
	fail=1
}
#    The branch sits in the timer block of CNeutrinoApp::handleMsg, where
#    every screen that passes messages on hands the timer to.
body 'int CNeutrinoApp::handleMsg(' < "$tmp/neutrino" > "$tmp/handlemsg"
[ -s "$tmp/handlemsg" ] || { echo "check-live-displaced.sh: no CNeutrinoApp::handleMsg in src/neutrino.cpp" >&2; exit 1; }
awk '/^\tif[ \t]*\(\(msg == NeutrinoMessages::EVT_TIMER\)\)[ \t]*\{/ { f = 1 } f { print } f && /data == scrambled_timer/ { exit }' "$tmp/handlemsg" | tr '\n' ' ' |
	grep -q -E 'data[[:space:]]*==[[:space:]]*rezap_timer[[:space:]]*\)[[:space:]]*\{[[:space:]]*rezap_timer[[:space:]]*=[[:space:]]*0[[:space:]]*;[[:space:]]*rezapAfterStreams\([[:space:]]*mode[[:space:]]*,[[:space:]]*channelList[[:space:]]*\)' || {
	echo "src/neutrino.cpp: the timer of a pending zap does not make the check again (EVT_TIMER, rezap_timer)" >&2
	fail=1
}
#    Pending is the remote control's guard: a zap goes out only when it has
#    run out.
strip "$SRC/src/daemonc/remotecontrol.cpp" | body 'bool CRemoteControl::zapPending(' | tr '\n' ' ' | grep -q -E 'now[[:space:]]*=[[:space:]]*time_monotonic_ms\(\)[[:space:]]*;[[:space:]]*return[[:space:]]+zap_completion_timeout[[:space:]]*>=[[:space:]]*now[[:space:]]*;' || {
	echo "src/daemonc/remotecontrol.cpp: CRemoteControl::zapPending does not read the zap guard (zap_completion_timeout >= now)" >&2
	fail=1
}
strip "$SRC/src/daemonc/remotecontrol.cpp" | body 'void CRemoteControl::zapTo_ChannelID(' | grep -q -E 'if[[:space:]]*\([[:space:]]*zap_completion_timeout[[:space:]]*<[[:space:]]*now[[:space:]]*\)' || {
	echo "src/daemonc/remotecontrol.cpp: CRemoteControl::zapTo_ChannelID does not send by the guard zapPending reads" >&2
	fail=1
}

# 6. The points where the mark of a try is forgotten.
awk '/msg == NeutrinoMessages::EVT_STREAM_START\)/ { f = 1 } f { print } f && /return messages_return::handled;/ { exit }' "$tmp/neutrino" | grep -q -F 'live_rezap.reset()' || {
	echo "src/neutrino.cpp: the start of a stream does not forget the mark of the zap back" >&2
	fail=1
}
awk '/if[ \t]*\(msg == NeutrinoMessages::EVT_ZAP_COMPLETE\)/ { f = 1 } f { print } f && /^\t}/ { exit }' "$tmp/neutrino" | tr '\n' ' ' |
	grep -q -E 'GetCurrentChannelID\(\)[[:space:]]*\)[[:space:]]*live_rezap\.reset\(\)' || {
	echo "src/neutrino.cpp: the completed zap of the live channel does not forget the mark of the zap back" >&2
	fail=1
}
body 'void CNeutrinoApp::standbyMode(' < "$tmp/neutrino" | awk '/live_rezap\.reset\(\)/ { r = NR } /if[ \t]*\([ \t]*bOnOff[ \t]*\)/ && !b { b = NR } END { exit !(r && b && r < b) }' || {
	echo "src/neutrino.cpp: CNeutrinoApp::standbyMode does not forget the mark of the zap back on both ways, before it branches" >&2
	fail=1
}

exit $fail
