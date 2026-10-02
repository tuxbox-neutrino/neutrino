#!/bin/sh
# What a channel or a mode asked for while the box is in standby does to it.
#
# In standby a zap from the web API or the zap timer is a wake. The ZAPTO arm
# has to wake before the current channel test, which in standby names whatever
# the guide scan left there, and before any mode write; wake straight onto the
# channel asked for; end a deferred deep standby; and refuse by the same rule
# as the API. A mode change carrying wakeup leaves standby before the mode is
# written.
#
# Checked as text because CNeutrinoApp is the application and stands in no LDADD:
# a case that could run this would have to bring the whole box up with it.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-zap-standby.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
REAL="$SRC/src/coreapi/box/channelsource_real.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$APP" "$REAL" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-zap-standby.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$APP" | awk -f "$BLANK" > "$tmp/app"
awk -v keepstrings=0 -f "$STRIP" "$REAL" | awk -f "$BLANK" > "$tmp/real"
[ -s "$tmp/app" ] && [ -s "$tmp/real" ] || {
	echo "check-zap-standby.sh: nothing left of $APP or $REAL after stripping" >&2
	exit 1
}

# From the first line matching the pattern to the brace closing what it opened,
# without spaces: the calls are spelled both standbyMode(false) and
# standbyMode( false ).
block()
{
	awk -v pat="$1" '
		found == 0 && $0 ~ pat { found = 1 }
		found == 1 {
			print
			n = length($0)
			for (i = 1; i <= n; i++) {
				c = substr($0, i, 1)
				if (c == "{") { depth++; seen = 1 }
				else if (c == "}") { depth--; if (seen && depth == 0) exit }
			}
		}
	' "$2" | tr -d '[:space:]'
}

# Position of the earlier of the two names, or 0.
first()
{
	printf '%s' "$1" | awk -v a="$2" -v b="$3" '
		{
			i = index($0, a)
			j = (b == "") ? 0 : index($0, b)
			if (i == 0 || (j > 0 && j < i)) i = j
			print i
		}'
}

bad=0

# Both positions found, the first ahead of the second.
before()
{
	if [ "$2" -eq 0 ] || [ "$3" -eq 0 ] || [ "$2" -ge "$3" ]; then
		echo "  $1" >&2
		bad=1
	fi
}

has()
{
	case "$2" in
		*"$3"*) ;;
		*) echo "  $1" >&2; echo "    wanted: $3" >&2; bad=1 ;;
	esac
}

lacks()
{
	case "$2" in
		*"$3"*) echo "  $1" >&2; echo "    found: $3" >&2; bad=1 ;;
		*) ;;
	esac
}

ZAP=`block 'msg == NeutrinoMessages::ZAPTO[)]' "$tmp/app"`
WAKE=`block '^bool CNeutrinoApp::wakeOnto[(]' "$tmp/app"`
CANCEL=`block '^void CNeutrinoApp::cancelDeferredDeepStandby[(]' "$tmp/app"`
RULE=`block '^bool CNeutrinoApp::zapPossible[(]' "$tmp/app"`
FACADE=`block '^bool coreapi::applicationCanZap[(]' "$tmp/app"`
MODE=`block 'msg == NeutrinoMessages::CHANGEMODE [)]' "$tmp/app"`
ANN=`block 'msg == NeutrinoMessages::ANNOUNCE_ZAPTO[)]' "$tmp/app"`
SOURCE=`block 'Status canZap[(]ChannelId' "$tmp/real"`

for part in ZAP WAKE CANCEL RULE FACADE MODE ANN SOURCE; do
	eval "v=\$$part"
	[ -n "$v" ] || {
		echo "check-zap-standby.sh: found nothing for $part" >&2
		exit 1
	}
done

before "the ZAPTO arm wakes the box before asking whether a recording allows the zap" \
	`first "$ZAP" 'zapPossible('` `first "$ZAP" 'wakeOnto('`
before "the ZAPTO arm tests the current channel before it wakes the box, so a zap to the channel zapit holds in standby does nothing" \
	`first "$ZAP" 'wakeOnto('` `first "$ZAP" 'GetCurrentChannelID()'`
before "the ZAPTO arm writes the mode before it wakes the box, which leaves it half awake" \
	`first "$ZAP" 'wakeOnto('` `first "$ZAP" 'tvMode(' 'radioMode('`
lacks "the ZAPTO arm carries a rule of its own beside the one the web API refuses by" "$ZAP" 'SameTP('
lacks "the ZAPTO arm carries a rule of its own beside the one the web API refuses by" "$ZAP" 'TimeshiftOnly('

before "wakeOnto leaves standby before it points the wake at the channel asked for, so the old channel plays first" \
	`first "$WAKE" 'standby_channel_id=channel_id'` `first "$WAKE" 'standbyMode(false)'`
before "wakeOnto leaves standby before it ends a deferred deep standby" \
	`first "$WAKE" 'cancelDeferredDeepStandby()'` `first "$WAKE" 'standbyMode(false)'`
has "cancelDeferredDeepStandby leaves the deferred deep standby in place" "$CANCEL" 'deferred_deepstandby=false'
has "cancelDeferredDeepStandby leaves the recheck timer running" "$CANCEL" 'killTimer(deferred_recheck_timer)'

has "the rule the arm applies no longer asks about the tuners" "$RULE" 'SameTP(channel_id)'
has "the rule the web API asks is not the one the arm applies" "$FACADE" '->zapPossible('
has "the channel source answers canZap without asking the application" "$SOURCE" 'applicationCanZap(id)'

before "the CHANGEMODE arm writes the mode before a mode change that may wake has left standby" \
	`first "$MODE" 'NeutrinoModes::wakeup'` `first "$MODE" 'standbyMode(false)'`
before "the CHANGEMODE arm writes the mode before a mode change that may wake has left standby" \
	`first "$MODE" 'standbyMode(false)'` `first "$MODE" 'tvMode(' 'radioMode('`
has "a mode change that wakes the box leaves a deferred deep standby in place" "$MODE" 'cancelDeferredDeepStandby()'

has "the ANNOUNCE_ZAPTO arm no longer leaves standby" "$ANN" 'standbyMode(false)'

[ "$bad" -eq 0 ] || {
	echo "check-zap-standby.sh: a channel or a mode asked for in standby does not leave it whole" >&2
	exit 1
}

echo "check-zap-standby.sh: a zap or a mode change in standby leaves it whole, in one zap, by the rule the web API refuses by"
