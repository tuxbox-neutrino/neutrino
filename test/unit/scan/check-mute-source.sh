#!/bin/sh
# The mute the API answers with has to be the mute the API sets. Those are two ends
# of one chain, and the chain came apart in the middle: the command went to the
# box's own flag and the read went to the decoder's, which is a different flag that
# two ordinary boxes never write. A box muting over the television link leaves the
# decoder alone on purpose, and a build with no audio hardware has a decoder whose
# flag nothing writes at all, so both answered sound that is on while the box drew
# the mute icon.
#
# Checked here as text because none of the four links can be linked into the test
# binary: the adapter needs the whole application, and the other three are the
# application.
#
# The four links, in the order a mute travels:
#   the loop hands EVT_SET_MUTE to CAudioMute::AudioMute
#   AudioMute writes the box's flag through setCurrentMuted
#   the facade reads that same flag back through isMuted
#   the adapter answers the facade and does not ask the decoder
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-mute-source.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
ADAPTER="$SRC/src/coreapi/box/systemsource_real.cpp"
APP="$SRC/src/neutrino.cpp"
ICON="$SRC/src/gui/audiomute.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$ADAPTER" "$APP" "$ICON" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-mute-source.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

for pair in "adapter $ADAPTER" "app $APP" "icon $ICON"; do
	name=`echo "$pair" | cut -d' ' -f1`
	file=`echo "$pair" | cut -d' ' -f2-`
	awk -v keepstrings=0 -f "$STRIP" "$file" | awk -f "$BLANK" > "$tmp/$name"
	[ -s "$tmp/$name" ] || {
		echo "check-mute-source.sh: nothing left of $file after stripping" >&2
		exit 1
	}
done

# Read inside the function that owns it rather than anywhere in the file, or a
# name that moved to another function would still count.
body_has()
{
	awk -v opener="$2" -v wanted="$3" '
		index($0, opener) > 0 { inside = 1 }
		inside && index($0, wanted) > 0 { found = 1 }
		inside && /^[\t ]*}/ && index($0, opener) == 0 { inside = 0 }
		END { exit found ? 0 : 1 }
	' "$1"
}

bad=0

if ! body_has "$tmp/app" "EVT_SET_MUTE)" "AudioMute("; then
	echo "  the loop answers EVT_SET_MUTE with something other than AudioMute" >&2
	bad=1
fi

if ! body_has "$tmp/icon" "void CAudioMute::AudioMute(" "setCurrentMuted("; then
	echo "  AudioMute no longer writes the box's own mute flag" >&2
	bad=1
fi

if ! body_has "$tmp/app" "bool coreapi::audioMuted()" "isMuted()"; then
	echo "  coreapi::audioMuted does not read the box's own mute flag" >&2
	bad=1
fi

if ! grep -q "audioMuted()" "$tmp/adapter"; then
	echo "  the adapter answers the mute from somewhere other than the facade" >&2
	bad=1
fi

# The decoder's flag by name, because that is the one this came apart over and
# it says nothing about a box muting over the television link.
if grep -q "audioDecoder" "$tmp/adapter"; then
	echo "  the adapter asks the decoder, whose flag the mute command need not reach" >&2
	bad=1
fi

[ "$bad" -eq 0 ] || {
	echo "check-mute-source.sh: the mute that is read is not the mute that is set" >&2
	exit 1
}

exit 0
