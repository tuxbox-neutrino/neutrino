#!/bin/sh
# What the front display says has to be read at the moment it is written, not at
# the moment the function that writes it was entered.
#
# Both writers below sit at the end of something that runs for a long time and
# answers the remote control while it runs: the guide is written out with a
# hundred and twenty second wait around handleMsg, and the fast scan reads a
# transponder. A key can take the box out of standby inside either of them, so
# the standby the caller named on the way in is not the state the box is in on
# the way out. Written from that stale argument, the box came back with a front
# display saying standby while it was running, and nothing on the console.
#
# Checked here as text because neither function can be linked into the test
# binary: CNeutrinoApp is the application, and a case that could run saveEpg
# would have to bring up the whole box with it.
#
# What is checked is the one statement in each that puts the display into
# standby: it has to decide on the live mode, and it may not mention either name
# the state was carried in on, saveEpg's _mode or CheckFastScan's standby. Which
# leaves what the argument is still right for untouched: it says which caller
# this is, which is why saveEpg still reads it to decide whether keys may be
# answered at all.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-standby-display.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$APP" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-standby-display.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$APP" | awk -f "$BLANK" > "$tmp/app"
[ -s "$tmp/app" ] || {
	echo "check-standby-display.sh: nothing left of $APP after stripping" >&2
	exit 1
}

# The body of the one function the pattern names, brace balanced off its own
# signature, the same way check-hook.sh isolates CNeutrinoApp::run.
body_of()
{
	awk -v pat="$1" '
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
	' "$tmp/app"
}

# The statement of that body carrying the call, runs of whitespace collapsed to
# one space so a call split across lines reads as one written on a single line,
# and cut at the semicolons so what comes back is the call and the branch it
# hangs on rather than the whole function.
statement_with()
{
	printf '%s\n' "$1" | tr '\n' ' ' | tr -s '[:space:]' ' ' | tr ';' '\n' |
		grep -F "$2" | head -1
}

# The mode by name rather than the whole call, so that a site writing it through
# a conditional expression is read as the site it is rather than as a site that
# has gone missing.
WRITE='CVFD::MODE_STANDBY'
LIVE='(^|[^_A-Za-z0-9])mode == NeutrinoModes::mode_standby'

bad=0

# Each pair is the function's signature and the name the state travelled in
# under, and every name here is one this refuses to see beside the write. The
# second is written out per function because they are not the same word, and a
# scan looking for one of them only would walk straight past the other.
check_site()
{
	what="$1"; pat="$2"; carried="$3"

	body=`body_of "$pat"`
	[ -n "$body" ] || {
		echo "  no $what definition on a line of its own in $APP" >&2
		bad=1
		return
	}

	st=`statement_with "$body" "$WRITE"`
	[ -n "$st" ] || {
		echo "  nothing in $what puts the front display into standby" >&2
		bad=1
		return
	}

	case "$st" in
		*"setMode("*) ;;
		*)
			echo "  $what names the standby display mode without setting it:" >&2
			echo "    $st" >&2
			bad=1
			return
			;;
	esac

	if ! printf '%s\n' "$st" | grep -Eq "$LIVE"; then
		echo "  $what does not read the live mode where it puts the front display into standby:" >&2
		echo "    $st" >&2
		bad=1
	fi

	if printf '%s\n' "$st" | grep -Eq "(^|[^_A-Za-z0-9])$carried([^_A-Za-z0-9]|\$)"; then
		echo "  $what decides the front display from $carried, which is the state it was entered with:" >&2
		echo "    $st" >&2
		bad=1
	fi

	return 0
}

# The parentheses of each signature are written as sets rather than escaped: the
# pattern travels into awk through -v, which reads the assignment for escapes of
# its own first and would hand the regex a bare parenthesis.
check_site "CNeutrinoApp::saveEpg" \
	'^void CNeutrinoApp::saveEpg[(]int _mode[)][ \t]*$' '_mode'
check_site "CNeutrinoApp::CheckFastScan" \
	'^void CNeutrinoApp::CheckFastScan[(]bool standby, bool reload[)][ \t]*$' 'standby'

[ "$bad" -eq 0 ] || {
	echo "check-standby-display.sh: the front display is set from a state that may be two minutes old" >&2
	exit 1
}

exit 0
