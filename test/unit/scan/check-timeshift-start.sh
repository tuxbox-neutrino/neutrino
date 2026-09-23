#!/bin/sh
# A shift asked for over the wire has to be the same act as the timeshift key.
#
# The manager writes the file and the player shows it, and only the two
# together are a shift. Starting the file alone leaves the picture on live, so
# the viewer sees a recording begin and nothing else, and the box answers a
# second request with the shift it already keeps.
#
# The key handler a few hundred lines up is where this was taken from, so it is
# held here as well: if one of the two moves, they are about the same thing.
#
# Checked as text because CNeutrinoApp is the application and stands in no
# LDADD: a case that could run this would have to bring the whole box up.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-timeshift-start.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$APP" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-timeshift-start.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$APP" | awk -f "$BLANK" > "$tmp/app"
[ -s "$tmp/app" ] || {
	echo "check-timeshift-start.sh: nothing left of $APP after stripping" >&2
	exit 1
}

# One block, from the line that opens it to the line that closes it, brace
# balanced so a nested block cannot end it early, then flattened and squeezed
# so a statement split across lines reads as one and no spelling of the spacing
# decides the outcome.
arm() {
	awk -v want="$1" '
		found == 0 && index($0, want) > 0 { found = 1 }
		found == 1 {
			print
			n = length($0)
			for (i = 1; i <= n; i++) {
				c = substr($0, i, 1)
				if (c == "{") { depth++; seen = 1 }
				else if (c == "}") { depth--; if (seen && depth == 0) exit }
			}
		}
	' "$tmp/app" | tr -d '[:space:]'
}

WIRE=`arm 'msg == NeutrinoMessages::EVT_START_TIMESHIFT'`
[ -n "$WIRE" ] || {
	echo "check-timeshift-start.sh: no arm for EVT_START_TIMESHIFT in $APP" >&2
	exit 1
}

case "$WIRE" in
	*"StartTimeshift()"*) ;;
	*)
		echo "check-timeshift-start.sh: the EVT_START_TIMESHIFT arm does not start a shift" >&2
		echo "  Writing the file is a recording. The shift is the file and the" >&2
		echo "  player together, which is what the timeshift key does." >&2
		exit 1
		;;
esac

# The player cannot be opened on top of one that is already showing a file, and
# it cannot be opened at all with no live picture behind it. Both are what the
# fall back to writing the file alone is for.
case "$WIRE" in
	*"Playing()"*) ;;
	*)
		echo "check-timeshift-start.sh: the EVT_START_TIMESHIFT arm opens the player unguarded" >&2
		echo "  A second player would stack on the one already showing a file." >&2
		exit 1
		;;
esac

case "$WIRE" in
	*"is_video_started"*) ;;
	*)
		echo "check-timeshift-start.sh: the EVT_START_TIMESHIFT arm does not ask for a picture" >&2
		echo "  With nothing playing there is nothing to shift, and the player" >&2
		echo "  would be opened on a box that is not watching anything." >&2
		exit 1
		;;
esac

KEY=`arm 'g_settings.key_timeshift'`
[ -n "$KEY" ] || {
	echo "check-timeshift-start.sh: no handler for the timeshift key in $APP" >&2
	exit 1
}

case "$KEY" in
	*"StartTimeshift()"*) ;;
	*)
		echo "check-timeshift-start.sh: the timeshift key no longer starts a shift" >&2
		echo "  That handler is where the one next door was copied from. One of" >&2
		echo "  the two moved, and they are about the same thing." >&2
		exit 1
		;;
esac

echo "check-timeshift-start.sh: a shift asked for over the wire is the same act as the timeshift key"
