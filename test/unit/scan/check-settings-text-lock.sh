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
#  - the readers that are not on the box's own loop go through the accessor. The
#    web server's compatibility layer and the threads that drive the front
#    display are named, because those are the readers that exist; a read on the
#    loop itself races with nobody and stays as it is;
#  - except for the two fields a thread writes. The city and the coordinates the
#    weather fetch resolves are written wherever that fetch runs, and two display
#    threads run it, so a read of those on the loop races with a writer like any
#    other and every mention of them goes through the accessor.
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
# text field names and the tree 874 mentions of one when this was written, and
# the widgets published 21 of them.
FLOOR_NAMES=70
FLOOR_REFS=700
FLOOR_PUBLISHED=15
# The two the weather fetch writes were mentioned 15 times.
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

# ------------------------------------------ the two a thread writes as well
#
# The weather fetch resolves a city and its coordinates and stores both, and the
# display threads run that fetch, so these two have no reader that races with
# nobody. Every mention of them anywhere goes through the accessor.
printf 'weather_city\nweather_location\n' > "$tmp/threadnames"
: > "$tmp/threadwritten"
while read -r f; do
	prepare "$SRC/$f" \
		| awk -v namefile="$tmp/threadnames" -v mode=read -v file="$f" -f "$SCAN" >> "$tmp/threadwritten"
done < "$tmp/candidates"

twrefs=`awk -F'\t' '$1 == "#" { n += $2 } END { print n + 0 }' "$tmp/threadwritten"`
if [ "$twrefs" -lt "$FLOOR_THREADWRITTEN" ]; then
	echo "check-settings-text-lock.sh: $twrefs mentions of the two fields the" >&2
	echo "  weather fetch writes, below the floor of $FLOOR_THREADWRITTEN: the scan has" >&2
	echo "  stopped matching" >&2
	exit 1
fi

if grep -q '^B	' "$tmp/threadwritten"; then
	echo "check-settings-text-lock.sh: these read a field the weather fetch writes" >&2
	echo "  from its own thread, without the accessor:" >&2
	grep '^B	' "$tmp/threadwritten" | cut -f2,3 | sed 's/^/  /' >&2
	exit 1
fi

# ------------------------------------------------------------- the widgets
#
# What a screen hands a pointer to. The widget edits the caller's string as the
# user types, so the write is here and not at the screen. Every change that can
# move the buffer goes through the accessor; a character written in place cannot
# move it and is left as it stands.
WIDGETS='src/gui/widget/stringinput.cpp src/gui/widget/keyboard_input.cpp src/gui/widget/menue.cpp src/gui/opkg_manager.cpp'
RAW='\*(valueString|optionValuePtr|local_dir)[ ]*=[^=]|(valueString|optionValuePtr|local_dir)->(append|assign|insert|erase|resize|swap|push_back|clear)[ ]*\('
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
# reload runs outright. Each of the rest is reached from a thread of its own that
# paints a front display or the icons over it. Every mention of a text field in
# these has to stand inside one of the four accessors.
READERS='src/httpd/compat/controlapi.cpp src/system/httptool.cpp src/system/helpers.cpp src/gui/plugins.cpp src/driver/pictureviewer/pictureviewer.cpp src/driver/lcdd.cpp src/driver/vfd.cpp src/driver/simple_display.cpp src/driver/glcd/glcd.cpp src/driver/glcd/simpleclock.cpp src/driver/glcd/weather.cpp src/driver/lcd4l.cpp src/gui/screensaver.cpp src/gui/infoicons.cpp'
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

exit 0
