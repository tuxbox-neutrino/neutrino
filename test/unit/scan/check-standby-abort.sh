#!/bin/sh
# Putting the box into standby is not one act. It writes the guide out in the
# middle of it, and that call answers the remote control for up to two minutes.
# A zap timer or a mode change arriving in there writes the mode through tvMode,
# radioMode or an assignment of its own, none of which is the call that leaves
# standby and none of which lockStandbyCall keeps out, so the box comes back out
# of standby while the function that was putting it in is still inside the guide.
#
# Everything the function does after that call is the rest of the standby state,
# and the last of it blanks the framebuffer. Applied to a box that is running
# again, that is a front display saying standby, an OSD that drops every paint
# and a screenshot drawn half way. So none of it may be reached without the mode
# being looked at again first.
#
# Checked as text because CNeutrinoApp is the application and stands in no LDADD:
# a case that could run standbyMode would have to bring the whole box up with it.
#
# Three things, and the first is the one that matters: nothing at all may stand
# between the end of the guide call and the test of the mode, so a line added
# later cannot slip in front of it. Then the test has to leave the function, or
# it decides nothing. And the blanking has to stand behind it, which is what
# pins the direction the test is written in.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-standby-abort.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$APP" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-standby-abort.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$APP" | awk -f "$BLANK" > "$tmp/app"
[ -s "$tmp/app" ] || {
	echo "check-standby-abort.sh: nothing left of $APP after stripping" >&2
	exit 1
}

# The body of standbyMode, brace balanced off its own signature, then flattened
# onto one line so a statement split across lines reads as one written on a
# single line. The parentheses of the signature are a set rather than an escape,
# because the pattern goes into awk through -v, which reads the assignment for
# escapes of its own first.
BODY=`awk -v pat='^void CNeutrinoApp::standbyMode[(]bool bOnOff, bool fromDeepStandby[)][ \t]*$' '
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
	echo "check-standby-abort.sh: no CNeutrinoApp::standbyMode definition on a line of its own in $APP" >&2
	exit 1
}

SAVE='saveEpg(NeutrinoModes::mode_standby);'
TEST='if (mode != NeutrinoModes::mode_standby)'
BLANKING='frameBuffer->setActive(false);'

bad=0

# Positions in the flattened body, one line each: where the name starts, and
# what stands between the end of the guide call and the test.
read_places()
{
	printf '%s' "$BODY" | awk -v save="$SAVE" -v test="$TEST" -v blanking="$BLANKING" '
		{
			s = $0
			psave = index(s, save)
			ptest = index(s, test)
			pblank = index(s, blanking)
			print psave
			print ptest
			print pblank
			if (psave > 0 && ptest > psave) {
				between = substr(s, psave + length(save), ptest - psave - length(save))
				gsub(/[}{ \t]/, "", between)
				print between
			} else {
				print ""
			}
		}
	'
}

places=`read_places`
PSAVE=`printf '%s\n' "$places" | sed -n 1p`
PTEST=`printf '%s\n' "$places" | sed -n 2p`
PBLANK=`printf '%s\n' "$places" | sed -n 3p`
BETWEEN=`printf '%s\n' "$places" | sed -n 4p`

[ "$PSAVE" -gt 0 ] || {
	echo "  nothing in CNeutrinoApp::standbyMode writes the guide out, which is what this is about" >&2
	exit 1
}

if [ "$PTEST" -le "$PSAVE" ]; then
	echo "  CNeutrinoApp::standbyMode does not look at the mode again after writing the guide out:" >&2
	echo "    wanted, after the call: $TEST" >&2
	bad=1
else
	if [ -n "$BETWEEN" ]; then
		echo "  something in CNeutrinoApp::standbyMode stands between the guide being written out and the mode being looked at again:" >&2
		echo "    $BETWEEN" >&2
		bad=1
	fi

	# What the test controls, up to the end of its own braced part, has to leave
	# the function: a test that falls through decides nothing.
	CONTROLLED=`printf '%s' "$BODY" | awk -v test="$TEST" '
		{
			s = $0
			p = index(s, test)
			if (p == 0) exit
			n = length(s)
			d = 0
			for (i = p + length(test); i <= n; i++) {
				c = substr(s, i, 1)
				if (c == "{") d++
				else if (c == "}") { d--; if (d == 0) { print substr(s, p, i - p + 1); exit } }
			}
		}
	'`
	case "$CONTROLLED" in
		*"return;"*) ;;
		*)
			echo "  the mode is looked at in CNeutrinoApp::standbyMode and the standby is applied anyway:" >&2
			echo "    $CONTROLLED" >&2
			bad=1
			;;
	esac

	if [ "$PBLANK" -le "$PTEST" ]; then
		echo "  CNeutrinoApp::standbyMode blanks the framebuffer before it looks at the mode again" >&2
		bad=1
	fi
fi

[ "$bad" -eq 0 ] || {
	echo "check-standby-abort.sh: the rest of the standby state is applied to a box that may be running again" >&2
	exit 1
}

exit 0
