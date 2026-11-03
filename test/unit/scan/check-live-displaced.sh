#!/bin/sh
# A zap to the channel the box shows is skipped as one that changes
# nothing, in the channel list, in the program's zap message and in the old
# web interface's zap. A stream can hold the live frontend on another
# transponder, after a zap to the live channel found no frontend for it;
# then that zap is the way back to a picture, and each of the three lets it
# through. zapit says whether the live channel is displaced in this way
# through livetp, from the live frontend it zapped and the live channel's
# transponder.
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
for f in src/zapit/zapit.cpp src/gui/channellist.cpp src/neutrino.cpp src/nhttpd/tuxboxapi/neutrinoapi.cpp; do
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

exit $fail
