#!/bin/sh
# A channel asked for while the box is in standby has to leave standby the whole
# way before anything writes the mode.
#
# The web interface is the one place that can ask. The handler for ZAPTO then
# calls tvMode or radioMode, and both of those write the mode; in standby the
# mode is the standby one, so one of them always ran. That left the box saying
# it was watching television while the picture, the screen and the front display
# were still as standby had left them. A television told over the wire that the
# box had woken showed whatever the decoder still held, and the message that
# leaves standby found a mode that was no longer the standby one and did nothing,
# so the only way back was the mains.
#
# The announce of a zap timer, a few lines below in the same function, has always
# done it correctly, which is what this holds the other one to.
#
# Checked as text because CNeutrinoApp is the application and stands in no LDADD:
# a case that could run this would have to bring the whole box up with it.
#
# Two things. The handler has to leave standby, and it has to do so BEFORE the
# first call that writes the mode, because after it the mode is no longer the
# standby one and the test would decide nothing.
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
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$APP" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-zap-standby.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$APP" | awk -f "$BLANK" > "$tmp/app"
[ -s "$tmp/app" ] || {
	echo "check-zap-standby.sh: nothing left of $APP after stripping" >&2
	exit 1
}

# The ZAPTO arm, from the line that opens it to the line that closes it, brace
# balanced so a nested block cannot end it early, then flattened onto one line
# so a statement split across lines reads as one.
ARM=`awk '
	found == 0 && /msg == NeutrinoMessages::ZAPTO/ { found = 1 }
	found == 1 {
		print
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") { depth++; seen = 1 }
			else if (c == "}") { depth--; if (seen && depth == 0) exit }
		}
	}
' "$tmp/app" | tr '\n' ' ' | tr -s '[:space:]' ' '`

[ -n "$ARM" ] || {
	echo "check-zap-standby.sh: no arm for NeutrinoMessages::ZAPTO in $APP" >&2
	exit 1
}

# Compared without any space at all, because the call is written both as
# standbyMode(false) and as standbyMode( false ) in this file and neither
# spelling is the rule.
ARM=`printf '%s' "$ARM" | tr -d '[:space:]'`

case "$ARM" in
	*"standbyMode(false)"*) ;;
	*)
		echo "check-zap-standby.sh: the ZAPTO arm does not leave standby" >&2
		echo "  A channel asked for while the box is in standby has to leave it" >&2
		echo "  the whole way. Writing the mode alone leaves the picture, the" >&2
		echo "  screen and the front display as standby left them, and the" >&2
		echo "  message that leaves standby then finds a mode that is no longer" >&2
		echo "  the standby one and does nothing." >&2
		exit 1
		;;
esac

# And it has to come first. Everything up to the first mode writing call must
# already carry the leaving; if it does not, the leaving stands behind one.
BEFORE=`printf '%s' "$ARM" | awk '
	{
		i = index($0, "tvMode(")
		j = index($0, "radioMode(")
		if (i == 0 || (j > 0 && j < i)) i = j
		if (i == 0) print $0
		else print substr($0, 1, i - 1)
	}'`

case "$BEFORE" in
	*"standbyMode(false)"*) ;;
	*)
		echo "check-zap-standby.sh: the ZAPTO arm leaves standby too late" >&2
		echo "  It stands behind a call that writes the mode, and after that the" >&2
		echo "  mode is no longer the standby one, so the test decides nothing." >&2
		exit 1
		;;
esac

# The arm this one was written against, so that a rule moved there is noticed
# here rather than silently diverging.
ANN=`awk '
	found == 0 && /msg == NeutrinoMessages::ANNOUNCE_ZAPTO/ { found = 1 }
	found == 1 {
		print
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") { depth++; seen = 1 }
			else if (c == "}") { depth--; if (seen && depth == 0) exit }
		}
	}
' "$tmp/app" | tr -d '[:space:]'`

case "$ANN" in
	*"standbyMode(false)"*) ;;
	*)
		echo "check-zap-standby.sh: the ANNOUNCE_ZAPTO arm no longer leaves standby" >&2
		echo "  That arm is where the rule next door was copied from. One of the" >&2
		echo "  two moved, and they are about the same thing." >&2
		exit 1
		;;
esac

echo "check-zap-standby.sh: a channel asked for in standby leaves it before the mode is written, as the zap timer's announce does"
