#!/bin/sh
# Two requests that arrive over the wire change what the screens draw from or
# open a screen of their own: a reload of the plugin list and the start of a
# shift. handleMsg is not only called by the main loop. Every screen with a
# loop of its own passes on what it does not handle, so an arm runs nested
# inside whatever screen is open: a list drawn from the plugins being reloaded
# under it, a player opened on top of the guide.
#
# So both arms ask whether this message came straight from the main loop, and
# when it did not, they note it and leave it to the main loop: back at the top
# of its loop, with every screen closed, it does what was asked. The shift is
# then still only started where a picture is shown.
#
# The question is only as good as its answer, so the answer is checked too:
# handleMsg takes the main loop's mark and clears it before anything else, so
# a screen opened below reads false, and the main loop sets the mark right
# before each of its own calls and takes it back right after.
#
# Checked as text: CNeutrinoApp is the application and stands in no LDADD.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-wire-arms-top-level.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
for f in "$APP" "$HERE/strip-comments.awk" "$HERE/blank-if0.awk"; do
	[ -r "$f" ] || { echo "check-wire-arms-top-level.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$APP" | awk -f "$HERE/blank-if0.awk" > "$tmp/app"

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

bad=0
for triple in 'msg == NeutrinoMessages::EVT_RELOAD_PLUGINS|plugins_reload_pending|if(g_Plugins)g_Plugins->loadPlugins();' \
		'msg == NeutrinoMessages::EVT_START_TIMESHIFT|timeshift_pending|startAskedTimeshift();'; do
	want=`echo "$triple" | cut -d'|' -f1`
	pending=`echo "$triple" | cut -d'|' -f2`
	action=`echo "$triple" | cut -d'|' -f3`
	a=`arm "$want"`
	if [ -z "$a" ]; then
		echo "check-wire-arms-top-level.sh: no arm for $want in $APP" >&2
		bad=1
		continue
	fi
	case "$a" in
		*at_top_level*) ;;
		*)
			echo "check-wire-arms-top-level.sh: the arm for $want does not ask" >&2
			echo "  whether it runs from the main loop or nested inside a screen" >&2
			bad=1
			;;
	esac
	# The nested case: the block of if(!at_top_level), braces counted, ends
	# by noting the request and returning - not inverted, not falling on.
	# What comes before the block must not do what was asked already, and
	# what comes after it must: that is the main loop's own case.
	parts=`printf '%s' "$a" | awk '{
		i = index($0, "if(!at_top_level){")
		if (i == 0) exit
		t = substr($0, i + 17); depth = 0
		for (j = 1; j <= length(t); j++) {
			c = substr(t, j, 1)
			if (c == "{") depth++
			else if (c == "}") { depth--; if (depth == 0) {
				print substr($0, 1, i - 1); print substr(t, 1, j); print substr(t, j + 1); exit } }
		}
	}'`
	before=`printf '%s\n' "$parts" | sed -n 1p`
	nested=`printf '%s\n' "$parts" | sed -n 2p`
	after=`printf '%s\n' "$parts" | sed -n 3p`
	case "$before" in
		*"$action"*)
			echo "check-wire-arms-top-level.sh: the arm for $want does what was asked" >&2
			echo "  before it asks whether it runs from the main loop" >&2
			bad=1
			;;
	esac
	case "$after" in
		*"$action"*) ;;
		*)
			echo "check-wire-arms-top-level.sh: the arm for $want does not do what was" >&2
			echo "  asked when it runs from the main loop ($action)" >&2
			bad=1
			;;
	esac
	case "$nested" in
		*"$pending=true;returnmessages_return::handled;}") ;;
		*)
			echo "check-wire-arms-top-level.sh: the arm for $want does not leave" >&2
			echo "  a nested request to the main loop: if (!at_top_level) must end" >&2
			echo "  with $pending = true and return" >&2
			bad=1
			;;
	esac
	# The main loop takes it up at the top of its loop, before the next
	# message, and only once.
	top=`awk '
		index($0, "void CNeutrinoApp::RealRun(") > 0 { inrun = 1 }
		inrun && index($0, "g_RCInput->getMsg(&msg, &data, 100") > 0 { exit }
		inrun { print }
	' "$tmp/app" | tr -d '[:space:]'`
	case "$top" in
		*"if($pending){$pending=false;$action}"*) ;;
		*)
			echo "check-wire-arms-top-level.sh: RealRun does not take up $pending" >&2
			echo "  at the top of its loop and do what was asked ($action)" >&2
			bad=1
			;;
	esac
done

# A stop of the shift calls off one still waiting.
stop=`arm 'msg == NeutrinoMessages::EVT_STOP_TIMESHIFT'`
case "$stop" in
	*"timeshift_pending=false;"*) ;;
	*)
		echo "check-wire-arms-top-level.sh: the EVT_STOP_TIMESHIFT arm leaves a waiting" >&2
		echo "  shift to be started after the stop" >&2
		bad=1
		;;
esac

# handleMsg takes the mark and clears it before anything else.
head=`awk '
	index($0, "int CNeutrinoApp::handleMsg(") > 0 { found = 1; next }
	found && NF > 0 { print; if (++n == 3) exit }
' "$tmp/app" | tr -d '[:space:]'`
case "$head" in
	"{constboolat_top_level=top_level_msg;top_level_msg=false;"*) ;;
	*)
		echo "check-wire-arms-top-level.sh: handleMsg does not take the main" >&2
		echo "  loop's mark and clear it first: $head" >&2
		bad=1
		;;
esac

# The main loop sets the mark right before each of its calls and takes it
# back right after; nothing else sets it.
wrapped=`awk '
	NF == 0 { next }
	{ line[++n] = $0; gsub(/[[:space:]]/, "", line[n]) }
	END {
		ok = 0; other = 0
		for (i = 1; i <= n; i++) {
			if (line[i] != "top_level_msg=true;")
				continue
			if (line[i + 1] == "handleMsg(msg,data);" && line[i + 2] == "top_level_msg=false;")
				ok++
			else
				other++
		}
		print ok " " other
	}
' "$tmp/app"`
set -- $wrapped
if [ "$1" -lt 2 ] || [ "$2" -ne 0 ]; then
	echo "check-wire-arms-top-level.sh: $1 of the main loop's handleMsg calls are" >&2
	echo "  marked and unmarked around the call (want both in RealRun), $2 marks" >&2
	echo "  stand anywhere else" >&2
	bad=1
fi

exit $bad
