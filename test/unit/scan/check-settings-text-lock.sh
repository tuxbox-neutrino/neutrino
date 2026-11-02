#!/bin/sh
# A page left open on a phone and a page left open on a desk ask the box the same
# questions over and over, and the box answers them on the web server's threads
# while its own loop is somewhere else entirely. The settings struct is full of
# std::string, and assigning to one frees the buffer the other side is copying.
# That is a fault in the reader, it leaves no trace in the writer, and neither
# half can see the other coming.
#
# So every text field of SNeutrinoSettings is written through setSettingsText,
# appendSettingsText or clearSettingsText, and read off the box's own loop
# through settingsText. All four take one lock, declared beside the struct.
#
# Every field and not only the ones a route answers today. Which of them the API
# reads is a property of the moment and not of the code: it moves with the next
# route, and a rule that moves with it is no rule.
#
# Five properties:
#
#  - the four accessors take the lock, or the rest of this checks nothing;
#  - nothing in the tree assigns to a text field, appends to one or clears one
#    without them. The names are read out of the header, so a field added there
#    is covered the day it is added and not the day somebody remembers;
#  - a pointer into the struct may be handed to a widget as an argument, which is
#    how the setup screens edit a path, but it may not be stored anywhere else:
#    the widget publishes what it edits under the same lock, and a pointer kept
#    in a member is one this scan cannot follow;
#  - the readers that are not on the box's own loop go through the accessor.
#    A file whose every read can run off the loop is named whole - the web
#    server's layers, the threads that drive the front display, the stream
#    server's Lua calls and the icon lookup - and in a file whose other reads
#    run only on the loop the reads off it are pinned one by one; a read on
#    the loop itself races with nobody and stays as it is;
#  - and two fields whose every mention goes through the accessor, on the loop
#    too: the city and the coordinates of the weather, held to it as a
#    precaution, as settings.h asks; only the loop writes them, and the fetch
#    the display threads run works on copies of its own.
#
# What this does not cover, said here because the scan reads as if it did: the
# three strings of a usermenu entry are held to the same rule as the rest, but
# they hang off a vector of pointers the loop deletes and rebuilds, and the
# pointer is an exposure no lock on the string behind it answers. The same goes
# for the four std::list members of the struct: this scan is about text fields
# and says nothing about those.
#
# None of this can be linked into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-settings-text-lock.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
HEADER="$SRC/src/system/settings.h"
FIELD="$SRC/src/coreapi/settings/settingsfield.h"
for f in "$STRIP" "$BLANK" "$HEADER" "$FIELD"; do
	[ -r "$f" ] || { echo "check-settings-text-lock.sh: cannot read $f" >&2; exit 1; }
done

# Below these the scan is not measuring the tree any more. The header held 78
# text field names and the tree 874 mentions of one when this was written. The
# widgets published 21 of them then, and 11 once the input dialogs kept a buffer
# of their own.
FLOOR_NAMES=70
FLOOR_REFS=700
FLOOR_PUBLISHED=8
# The weather's two were mentioned 15 times.
FLOOR_THREADWRITTEN=12

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# Comments blanked, then #if 0 behind them, so the scan reads what the compiler
# reads. Literals are blanked too: a path written out in one is not a field.
prepare() {
	awk -v keepstrings=0 -f "$STRIP" "$1" | awk -f "$BLANK"
}

# ------------------------------------------------- the names, out of the header
#
# Every std::string the struct declares, the nested ones included: an entry of
# network_nfs is as much a text setting as a member beside it.
prepare "$HEADER" | awk '
	found == 0 && /^struct SNeutrinoSettings[ \t]*$/ { found = 1 }
	found == 1 {
		if ($0 ~ /^[ \t]+std::string[ \t]+[A-Za-z_]/) {
			s = $0
			sub(/^[ \t]+std::string[ \t]+/, "", s)
			sub(/[^A-Za-z_0-9].*$/, "", s)
			if (s != "") print s
		}
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") depth++
			else if (c == "}") { depth--; if (depth == 0) exit }
		}
	}
' | sort -u > "$tmp/names"

nnames=`awk 'END { print NR }' "$tmp/names"`
if [ "$nnames" -lt "$FLOOR_NAMES" ]; then
	echo "check-settings-text-lock.sh: $nnames text field names read out of" >&2
	echo "  $HEADER, the scan has stopped matching" >&2
	exit 1
fi

SCAN="$HERE/settingstext.awk"
[ -r "$SCAN" ] || { echo "check-settings-text-lock.sh: cannot read $SCAN" >&2; exit 1; }

# ------------------------------------------------------------- the accessors
#
# Each of the four, and the guard inside it. Without this the rest of the scan
# holds the tree to a function that locks nothing.
for fn in setSettingsText appendSettingsText clearSettingsText settingsText; do
	got=`prepare "$HEADER" | awk -v fn="$fn" '
		index($0, "inline") > 0 && index($0, fn "(") > 0 { inside = 1; next }
		inside == 1 && index($0, "CSettingsTextGuard") > 0 { held = 1 }
		inside == 1 && $0 ~ /^}/ { inside = 0 }
		END { print (held ? "held" : "bare") }
	'`
	if [ "$got" != "held" ]; then
		echo "check-settings-text-lock.sh: $fn in $HEADER does not take" >&2
		echo "  CSettingsTextGuard, so nothing below it is held to anything" >&2
		exit 1
	fi
done

# The pair the settings adapter reaches a text field through. It runs on a
# request's own thread at one end and on the box's loop at the other.
prepare "$FIELD" | tr '\n' ' ' | tr -s '[:space:]' ' ' > "$tmp/field"
grep -q 'struct TextField' "$tmp/field" || {
	echo "check-settings-text-lock.sh: no TextField in $FIELD, the scan has" >&2
	echo "  stopped matching" >&2
	exit 1
}
for call in 'out = settingsText(s.\*M)' 'setSettingsText(s.\*M, v)'; do
	grep -q "$call" "$tmp/field" || {
		echo "check-settings-text-lock.sh: TextField in $FIELD no longer reaches" >&2
		echo "  the member through the accessor: $call" >&2
		exit 1
	}
done

# ------------------------------------------------------- nobody writes one bare
#
# Candidates first, so the scan reads a hundred files rather than a thousand.
( cd "$SRC" && grep -rl 'g_settings\.' src lib --include='*.cpp' --include='*.h' --include='*.c' 2>/dev/null ) \
	| grep -v '^src/system/settings\.h$' | sort -u > "$tmp/candidates"

[ -s "$tmp/candidates" ] || {
	echo "check-settings-text-lock.sh: no file in the tree mentions g_settings," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
}

: > "$tmp/found"
while read -r f; do
	prepare "$SRC/$f" \
		| awk -v namefile="$tmp/names" -v mode=write -v file="$f" -f "$SCAN" >> "$tmp/found"
done < "$tmp/candidates"

refs=`awk -F'\t' '$1 == "#" { n += $2 } END { print n + 0 }' "$tmp/found"`
if [ "$refs" -lt "$FLOOR_REFS" ]; then
	echo "check-settings-text-lock.sh: $refs mentions of a text field found in" >&2
	echo "  the tree, below the floor of $FLOOR_REFS: the scan has stopped matching" >&2
	exit 1
fi

if grep -q '^W	' "$tmp/found"; then
	echo "check-settings-text-lock.sh: these write a text setting without" >&2
	echo "  setSettingsText, appendSettingsText or clearSettingsText, which is" >&2
	echo "  the lock a request reading the same member holds:" >&2
	grep '^W	' "$tmp/found" | cut -f2,3 | sed 's/^/  /' >&2
	exit 1
fi

# The one that keeps such a pointer rather than passing it: the package manager
# holds it in a member and writes through it under the lock in the same file.
grep '^S	' "$tmp/found" | grep -v '	src/gui/opkg_manager\.cpp	' > "$tmp/stored" || true
if [ -s "$tmp/stored" ]; then
	echo "check-settings-text-lock.sh: these keep a pointer into a text setting" >&2
	echo "  rather than handing it to a widget as an argument, so what writes" >&2
	echo "  through it is out of this scan's reach:" >&2
	cut -f2,3 "$tmp/stored" | sed 's/^/  /' >&2
	exit 1
fi

# ------------------------------------- the two held to the accessor everywhere
#
# The city and the coordinates of the weather go through the accessor on the
# loop too, a precaution settings.h asks for: only the loop writes them, and the
# fetch the display threads run works on copies of its own.
printf 'weather_city\nweather_location\n' > "$tmp/threadnames"
: > "$tmp/threadwritten"
while read -r f; do
	prepare "$SRC/$f" \
		| awk -v namefile="$tmp/threadnames" -v mode=read -v file="$f" -f "$SCAN" >> "$tmp/threadwritten"
done < "$tmp/candidates"

twrefs=`awk -F'\t' '$1 == "#" { n += $2 } END { print n + 0 }' "$tmp/threadwritten"`
if [ "$twrefs" -lt "$FLOOR_THREADWRITTEN" ]; then
	echo "check-settings-text-lock.sh: $twrefs mentions of the weather's two" >&2
	echo "  fields, below the floor of $FLOOR_THREADWRITTEN: the scan has stopped" >&2
	echo "  matching" >&2
	exit 1
fi

if grep -q '^B	' "$tmp/threadwritten"; then
	echo "check-settings-text-lock.sh: these read the weather's city or" >&2
	echo "  coordinates without the accessor, which settings.h asks of every read:" >&2
	grep '^B	' "$tmp/threadwritten" | cut -f2,3 | sed 's/^/  /' >&2
	exit 1
fi

# ------------------------------------------------------------- the widgets
#
# What a screen hands a pointer to. The widget edits the caller's string as the
# user types, so the write is here and not at the screen. The input dialogs
# edit a buffer of their own and put it into the caller's string through the
# accessors, on save and when they write back the value they started with; the
# other widgets change it through the accessors as well, under the lock its
# readers take. A bare write through one of the pointers below is refused. The
# structured input reads its target through getTarget() and writes only through
# csd_value; the PIN change writes through value.
WIDGETS='src/gui/widget/stringinput.cpp src/gui/widget/stringinput_ext.cpp src/gui/widget/keyboard_input.cpp src/gui/widget/menue.cpp src/gui/components/cc_input_dialog.cpp src/gui/components/cc_numeric_input_dialog.cpp src/gui/components/cc_structured_input_dialog.cpp src/gui/filebrowser.cpp src/gui/opkg_manager.cpp'
RAW='\*(valueString|optionValuePtr|local_dir|cid_value|csd_value|value)[ ]*=[^=]|(valueString|optionValuePtr|local_dir|cid_value|csd_value|value)->(append|assign|insert|erase|resize|swap|push_back|clear)[ ]*\(|(^|[^A-Za-z_.>])setting_dir[ ]*=[^=]|setting_dir\.(append|assign|insert|erase|resize|swap|push_back|clear)[ ]*\(|\*data[ ]*=[^=]'
published=0
for f in $WIDGETS; do
	[ -r "$SRC/$f" ] || {
		echo "check-settings-text-lock.sh: cannot read $SRC/$f" >&2
		exit 1
	}
	prepare "$SRC/$f" > "$tmp/widget"
	if grep -qE "$RAW" "$tmp/widget"; then
		echo "check-settings-text-lock.sh: $f changes the string it was handed" >&2
		echo "  without the lock its readers take:" >&2
		grep -nE "$RAW" "$tmp/widget" | sed 's/^/  /' >&2
		exit 1
	fi
	published=`expr $published + \`grep -c 'setSettingsText(' "$tmp/widget" || true\``
done

if [ "$published" -lt "$FLOOR_PUBLISHED" ]; then
	echo "check-settings-text-lock.sh: the widgets publish $published changes," >&2
	echo "  below the floor of $FLOOR_PUBLISHED: the scan has stopped matching" >&2
	exit 1
fi

# ------------------------------------------------------- the readers off the loop
#
# The compatibility layer answers on the web server's own threads, and four more
# are reached from those threads: the two download helpers, one behind the
# redirect a timer follows and one behind everything else that fetches a URL; the
# logo lookup the logo adapter calls; and the plugin scan, which a request for a
# reload runs outright. nhttpd's language lookup answers on those threads as
# well, the stream server's thread runs the Lua calls that fetch a URL, and the
# icon lookup is asked from threads that paint. Each of the rest is reached from a
# thread of its own that paints a front display or the icons over it. Every
# mention of a text field in these has to stand inside one of the four accessors.
READERS='src/nhttpd/tuxboxapi/controlapi.cpp src/nhttpd/yhttpd_core/ylanguage.cpp src/system/httptool.cpp src/system/helpers.cpp src/gui/plugins.cpp src/gui/lua/lua_curl.cpp src/gui/lua/luainstance.cpp src/driver/fb_generic.cpp src/driver/pictureviewer/pictureviewer.cpp src/driver/lcdd.cpp src/driver/vfd.cpp src/driver/simple_display.cpp src/driver/glcd/glcd.cpp src/driver/glcd/simpleclock.cpp src/driver/glcd/weather.cpp src/driver/lcd4l.cpp src/gui/screensaver.cpp'
for f in $READERS; do
	[ -r "$SRC/$f" ] || {
		echo "check-settings-text-lock.sh: cannot read $SRC/$f" >&2
		exit 1
	}
	prepare "$SRC/$f" \
		| awk -v namefile="$tmp/names" -v mode=read -v file="$f" -f "$SCAN" > "$tmp/reader"

	n=`awk -F'\t' '$1 == "#" { print $2 + 0 }' "$tmp/reader"`
	if [ "${n:-0}" -lt 1 ]; then
		echo "check-settings-text-lock.sh: $f no longer mentions a text setting," >&2
		echo "  so this scan has stopped matching" >&2
		exit 1
	fi
	if grep -q '^B	' "$tmp/reader"; then
		echo "check-settings-text-lock.sh: $f reads a text setting without the" >&2
		echo "  accessor, and it is not on the box's own loop:" >&2
		grep '^B	' "$tmp/reader" | cut -f3 | sed 's/^/  /' >&2
		exit 1
	fi
done

# ----------------------------------- the reads off the loop in a file of both
#
# Files whose other reads run only on the loop, where a bare read races with nobody, so
# only the statements another thread runs are held: the online update check on
# its own thread, the movie player's choice of language and subtitles on the
# player's thread, and the remote box address the web server's threads look up.
body() {	# body <file> <pattern of the function's first line>
	prepare "$SRC/$1" | awk -v pat="$2" '
		found == 0 && $0 ~ pat { found = 1 }
		found == 1 { print; if ($0 ~ /^}/) exit }
	' | tr '\n' ' ' | tr -s '[:space:]' ' '
}
pinned() {	# pinned <file> <function pattern> <what has to stand in it>
	if ! body "$1" "$2" | grep -qF "$3"; then
		echo "check-settings-text-lock.sh: $1 no longer reads off the loop through" >&2
		echo "  the accessor where it did: $3" >&2
		exit 1
	fi
}
pinned src/gui/update.cpp '::checkOnlineVersion[(]' 'settingsText(g_settings.softupdate_url_file)'
pinned src/gui/movieplayer.cpp 'void CMoviePlayerGui::selectAutoLang[(]' 'settingsText(g_settings.subs_charset)'
pinned src/gui/movieplayer.cpp 'void CMoviePlayerGui::selectAutoLang[(]' 'settingsText(g_settings.pref_lang[i])'
pinned src/gui/movieplayer.cpp 'void CMoviePlayerGui::selectAutoLang[(]' 'settingsText(g_settings.pref_subs[i])'
# The guard first and the walk of the list after it, inside one function.
RBIP=`body src/nhttpd/tuxboxapi/neutrinoapi.cpp 'CNeutrinoAPI::GetRemoteBoxIP[(]' | awk '
	{
		g = index($0, "CSettingsTextGuard")
		w = index($0, "timer_remotebox_ip.begin()")
		print (g > 0 && w > g) ? "held" : "bare"
	}
'`
if [ "$RBIP" != "held" ]; then
	echo "check-settings-text-lock.sh: CNeutrinoAPI::GetRemoteBoxIP walks the remote" >&2
	echo "  boxes without CSettingsTextGuard in front of the walk" >&2
	exit 1
fi

exit 0
