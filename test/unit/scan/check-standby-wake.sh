#!/bin/sh
# A box in standby has to answer its power button. Writing the guide out holds
# the message loop for up to two minutes, and the call that leaves standby is
# locked out for all of that time by lockStandbyCall, because the call that
# enters standby is the one still running. Answered inside that loop, a wake is
# therefore answered by nothing at all: the key is gone and the box sits there,
# which from the outside is a box that has died.
#
# So the loop puts a wake back on the queue and stops waiting. The caller
# finishes putting the box into standby, releases the lock, and the main loop
# finds the message a moment later and leaves standby the one whole way there is.
#
# Checked as text because CNeutrinoApp is the application and stands in no LDADD.
#
# Four things, all of them inside the stretch between the wake being named and
# the message being handed to handleMsg, which is what makes this an
# interception rather than something that runs after the fact:
#   both names, because either one alone leaves half the wakes swallowed
#   the message going back on the queue, or the wake is dropped rather than moved
#   the wait ending, or the box still sits there for the rest of the two minutes
#   the caller being asked, because the other caller is the shutdown: the process
#   ends after that one and a wait cut short there is a half written file
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-standby-wake.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$APP" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-standby-wake.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$APP" | awk -f "$BLANK" > "$tmp/app"
[ -s "$tmp/app" ] || {
	echo "check-standby-wake.sh: nothing left of $APP after stripping" >&2
	exit 1
}

# saveEpg's body, brace balanced off its own signature and flattened onto one
# line so a statement split across lines reads as one written on a single line.
# The parentheses of the signature are a set rather than an escape: the pattern
# goes into awk through -v, which reads the assignment for escapes of its own
# first.
BODY=`awk -v pat='^void CNeutrinoApp::saveEpg[(]int _mode[)][ \t]*$' '
	found == 0 && $0 ~ pat { found = 1 }
	found == 1 {
		print
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") depth++
			else if (c == "}") { depth--; if (depth == 0) exit }
		}
	}
' "$tmp/app" | tr '\n' ' ' | tr -s '[:space:]' ' '`

[ -n "$BODY" ] || {
	echo "check-standby-wake.sh: no CNeutrinoApp::saveEpg definition on a line of its own in $APP" >&2
	exit 1
}

# The stretch from the first of the two wake names to the call that hands a
# message on, which is where an interception has to stand to be one.
REGION=`printf '%s' "$BODY" | awk '
	{
		s = $0
		first = index(s, "NeutrinoMessages::STANDBY_OFF")
		toggle = index(s, "NeutrinoMessages::STANDBY_TOGGLE")
		if (first == 0 || (toggle > 0 && toggle < first))
			first = toggle
		hand = index(s, "handleMsg(msg, data)")
		if (first == 0 || hand == 0 || hand <= first)
			exit
		print substr(s, first, hand - first)
	}
'`

[ -n "$REGION" ] || {
	echo "  nothing in CNeutrinoApp::saveEpg takes a wake out of the loop before the message is handed on" >&2
	echo "check-standby-wake.sh: a box writing its guide out answers its power button with nothing" >&2
	exit 1
}

# The shorter stretch, ending where the message goes back on the queue rather
# than where it is handed on. The caller is asked inside this one, or the answer
# would be satisfied by the test that stands in front of handleMsg further down
# and says nothing about which caller the wake belongs to.
GATE=`printf '%s' "$REGION" | awk '
	{
		s = $0
		p = index(s, "postMsg(msg, data)")
		if (p == 0) exit
		print substr(s, 1, p)
	}
'`

bad=0

want()
{
	case "$REGION" in
		*"$1"*) ;;
		*)
			echo "  $2" >&2
			echo "    wanted, before the message is handed on: $1" >&2
			bad=1
			;;
	esac
}

want_gate()
{
	case "$GATE" in
		*"$1"*) ;;
		*)
			echo "  $2" >&2
			echo "    wanted, before the message goes back on the queue: $1" >&2
			bad=1
			;;
	esac
}

want 'NeutrinoMessages::STANDBY_OFF' \
	"CNeutrinoApp::saveEpg does not know one of the two ways a box is woken"
want 'NeutrinoMessages::STANDBY_TOGGLE' \
	"CNeutrinoApp::saveEpg does not know one of the two ways a box is woken"
want 'postMsg(msg, data)' \
	"CNeutrinoApp::saveEpg drops the wake rather than putting it back on the queue"
want 'break;' \
	"CNeutrinoApp::saveEpg keeps waiting after a wake, so the box stays unresponsive"
want_gate '_mode == NeutrinoModes::mode_standby' \
	"CNeutrinoApp::saveEpg does not ask which caller it is, so the shutdown stops waiting too"

[ "$bad" -eq 0 ] || {
	echo "check-standby-wake.sh: a box writing its guide out does not answer its power button" >&2
	exit 1
}

exit 0
