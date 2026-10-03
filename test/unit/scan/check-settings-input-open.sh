#!/bin/sh
# A string input edits the setting it was handed in place: it pads the value
# once to its width and indexes it up to that width, and the address inputs
# keep pointers into the string. A value put into such a setting from
# elsewhere while the input is open ends neutrino at the next key. The keyboard
# input and the text input dialog edit a copy, but put it back into the setting
# when they close, on cancel too, over any value put there meanwhile. Two
# messages put values into the settings from elsewhere, and either can arrive
# inside an open input, which passes on what it does not handle: a write of
# the settings over the wire (APPLY_SETTINGS) and a reload of the settings
# file (RELOAD_SETUP, which the REST layer's reload-setup route sends too).
#
# So the inputs count themselves while they are open, and both arms note what
# was asked and leave it waiting while one is. What waited goes through one
# function, carryWaitingSettings, which takes up the reload first and then the
# write, so that the reload reads the file before a carried write is saved over
# it. It is called by both arms when no input is open; when the message the
# last input posts arrives, in the screen the input was opened from, asking for
# it again when another input opened meanwhile; before neutrino.conf is saved,
# so that no save puts the values the box runs on over a file a reload was asked
# to read; by the exit and the restart before their saves, as they never return
# to an input; and at the top of the main loop, before luaclient scripts are
# let in.
#
# Checked as text: CNeutrinoApp is the application and stands in no LDADD,
# and the inputs are screens.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-settings-input-open.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
INPUT="$SRC/src/gui/widget/stringinput.cpp"
EXT="$SRC/src/gui/widget/stringinput_ext.cpp"
KEYBOARD="$SRC/src/gui/widget/keyboard_input.cpp"
DIALOG="$SRC/src/gui/components/cc_input_dialog.cpp"
for f in "$APP" "$INPUT" "$EXT" "$KEYBOARD" "$DIALOG" "$HERE/strip-comments.awk" "$HERE/blank-if0.awk"; do
	[ -r "$f" ] || { echo "check-settings-input-open.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
prepare() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1" | awk -f "$HERE/blank-if0.awk"
}
prepare "$APP" > "$tmp/app"
prepare "$INPUT" > "$tmp/input"
prepare "$EXT" > "$tmp/ext"
prepare "$KEYBOARD" > "$tmp/keyboard"
prepare "$DIALOG" > "$tmp/dialog"

# The block that opens on the first line naming $2 in $1, braces counted,
# without white space.
block() {
	awk -v want="$2" '
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
	' "$1" | tr -d '[:space:]'
}

bad=0
fail() {
	for line in "$@"; do
		echo "check-settings-input-open.sh: $line" >&2
	done
	bad=1
}

# The five inputs count themselves first, for as long as their exec runs, and
# the last to close posts the message when asked to.
for spec in "$tmp/input|int CStringInput::exec(" "$tmp/input|int CPINInput::exec(" \
		"$tmp/ext|int CExtendedInput::exec(" "$tmp/keyboard|int CKeyboardInput::exec(" \
		"$tmp/dialog|int CCTextInputDialog::exec("; do
	file=`echo "$spec" | cut -d'|' -f1`
	want=`echo "$spec" | cut -d'|' -f2`
	b=`block "$file" "$want"`
	case "$b" in
		"") fail "no $want" ;;
		*"&){CStringInputOpencounted;"*) ;;
		*) fail "$want does not count itself as an open input before anything else" ;;
	esac
done
closed=`block "$tmp/input" 'CStringInputOpen::~CStringInputOpen('`
case "$closed" in
	*"if(--string_inputs_open>0)return;"*"postMsg(NeutrinoMessages::STRING_INPUTS_CLOSED"*) ;;
	*) fail "the last string input to close does not post STRING_INPUTS_CLOSED" ;;
esac

# Each arm notes what was asked, and with an input open asks to be told when the
# last one closes; otherwise it takes up what waits, its own request included.
for pair in 'msg == NeutrinoMessages::APPLY_SETTINGS|settings_apply_pending' \
		'msg == NeutrinoMessages::RELOAD_SETUP|reload_setup_pending'; do
	want=`echo "$pair" | cut -d'|' -f1`
	pending=`echo "$pair" | cut -d'|' -f2`
	a=`block "$tmp/app" "$want"`
	case "$a" in
		"") fail "no arm for $want in $APP" ;;
		*"){$pending=true;if(CStringInputOpen::any())CStringInputOpen::notifyWhenClosed();elsecarryWaitingSettings();returnmessages_return::handled;}") ;;
		*) fail "the arm for $want does not leave the request waiting while a string" \
			"  input is open and take up what waits otherwise" ;;
	esac
done

# What waits is taken up in one place: the reload first, then the write.
carry=`block "$tmp/app" 'void CNeutrinoApp::carryWaitingSettings('`
case "$carry" in
	*"if(reload_setup_pending){reload_setup_pending=false;reloadSetup();}if(settings_apply_pending){settings_apply_pending=false;coreapi::applyPendingSettings();}}") ;;
	*) fail "carryWaitingSettings does not take up a waiting reload and then a waiting" \
		"  settings write, each once" ;;
esac

# The message the last input posts: asked again while another input is open,
# with the second look after asking, and taken up otherwise.
c=`block "$tmp/app" 'msg == NeutrinoMessages::STRING_INPUTS_CLOSED'`
case "$c" in
	*"if(CStringInputOpen::any()){CStringInputOpen::notifyWhenClosed();if(CStringInputOpen::any())returnmessages_return::handled;}carryWaitingSettings();returnmessages_return::handled;}") ;;
	*) fail "the arm for STRING_INPUTS_CLOSED does not ask again while another input is" \
		"  open and take up what waits otherwise" ;;
esac

# Before neutrino.conf is written, with no input open.
save=`block "$tmp/app" 'void CNeutrinoApp::saveSetup('`
case "$save" in
	*"if(strcmp(fname,NEUTRINO_SETTINGS_FILE)==0&&!CStringInputOpen::any())carryWaitingSettings();"*"configfile.saveConfig("*) ;;
	*) fail "saveSetup writes neutrino.conf without taking up what waits for the inputs first" ;;
esac

# At the top of the main loop, first thing, and before UnBlock lets luaclient
# scripts in.
top=`awk '
	index($0, "void CNeutrinoApp::RealRun(") > 0 { inrun = 1 }
	inrun && index($0, "g_RCInput->getMsg(&msg, &data, 100") > 0 { exit }
	inrun { print }
' "$tmp/app" | tr -d '[:space:]'`
case "$top" in
	*"while(true){if(!CStringInputOpen::any())carryWaitingSettings();"*"luaServer->UnBlock();#endif") ;;
	*) fail "RealRun does not take up what waits for the inputs first in its loop, with" \
		"  no input open, ahead of luaServer->UnBlock()" ;;
esac

# The two ways out that do not come back to the main loop: the exit once it is
# sure to leave and before the daemons stop, the restart right before its save.
exitrun=`awk '
	index($0, "void CNeutrinoApp::ExitRun(") > 0 { found = 1 }
	found && index($0, "stop_daemons(true);") > 0 { print; exit }
	found { print }
' "$tmp/app" | tr -d '[:space:]'`
case "$exitrun" in
	*"if(!do_exiting)return;"*"carryWaitingSettings();"*"stop_daemons(true);") ;;
	*) fail "ExitRun does not take up what waits once it is sure to leave and before" \
		"  the daemons stop" ;;
esac
restart=`awk '
	index($0, "OnBeforeRestart();") > 0 { found = 1 }
	found && index($0, "saveSetup(NEUTRINO_SETTINGS_FILE);") > 0 { print; exit }
	found { print }
' "$tmp/app" | tr -d '[:space:]'`
case "$restart" in
	*"carryWaitingSettings();saveSetup(NEUTRINO_SETTINGS_FILE);") ;;
	*) fail "the restart saves without taking up what waits for the inputs" ;;
esac

exit $bad
