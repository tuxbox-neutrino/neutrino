#!/bin/sh
# A channel's own stream state is not the channel map, and CServiceManager's lock
# was only ever taken for the map. What a channel holds beside its identity is a
# list of audio tracks made of pointers, a raw program map table on the heap and
# two conditional access containers, and CPmt::ParseInternal throws all of them
# away and builds them again on every tune.
#
# GET /api/v1/channels/current answers the audio track list, and it is served on
# one of the web server's own threads. So the parse and the read run at once, and
# what the read walks is a vector being cleared and pointers being freed. Leaving
# standby is where the two meet: the event that says the box is waking goes out
# before the loop acts on it, which sends every open page to ask what is playing
# exactly while the forced re-zap reparses it.
#
# The rule this holds the tree to: everything that rewrites a channel's stream
# state stands under CServiceManager::ChannelGuard, which is the lock the readers
# already hold, and everything that writes the channel maps holds the same lock.
# The map matters here because one writer of it is not a reload anybody asked
# for: the thread that watches the service tables finds a changed transponder
# now and then on a box nobody has touched, and adds and renames entries in the
# map a channel listing is walking.
#
# None of these functions can be linked into the test binary, so they are checked
# as text.
#
# Two properties, and the second is why this is not a written down list of call
# sites: the set of files allowed to write that state is pinned, so a new writer
# anywhere else fails here rather than being added without the lock.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-channel-content-lock.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
SCANPMT="$SRC/src/zapit/scanpmt.cpp"
ZAPIT="$SRC/src/zapit/zapit.cpp"
SOURCE="$SRC/src/coreapi/box/channelsource_real.cpp"
SERVICES="$SRC/src/zapit/getservices.cpp"
for f in "$STRIP" "$BLANK" "$SCANPMT" "$ZAPIT" "$SOURCE" "$SERVICES"; do
	[ -r "$f" ] || { echo "check-channel-content-lock.sh: cannot read $f" >&2; exit 1; }
done

GUARD='CServiceManager::ChannelGuard'

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# Comments blanked and then #if 0 behind them, so the scan reads what the
# compiler reads.
prepare() {
	awk -v keepstrings=0 -f "$STRIP" "$1" | awk -f "$BLANK"
}

prepare "$SCANPMT" > "$tmp/scanpmt"
prepare "$ZAPIT" > "$tmp/zapit"
prepare "$SOURCE" > "$tmp/source"
prepare "$SERVICES" > "$tmp/services"

# The body of the one function whose signature the pattern matches, brace
# balanced from that signature.
body_of() {
	awk -v pat="$2" '
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
	' "$1"
}

# ---------------------------------------------------------------- the writers

body_of "$tmp/scanpmt" '^bool CPmt::ParseInternal[(]' > "$tmp/parseinternal"
[ -s "$tmp/parseinternal" ] || {
	echo "check-channel-content-lock.sh: no CPmt::ParseInternal definition in" >&2
	echo "  $SCANPMT" >&2
	exit 1
}

# The guard stands at the function's own depth and in front of the first write,
# or a path through the parse runs without it. Joined onto one line first, so a
# declaration reflowed across lines reads the same.
FLAT=`tr '\n' ' ' < "$tmp/parseinternal" | tr -s '[:space:]' ' '`

READING=`printf '%s' "$FLAT" | awk -v guard="$GUARD" -v first="channel->resetPids()" '
	{
		s = $0
		g = index(s, guard)
		w = index(s, first)
		if (g == 0) { print "noguard\t0"; exit }
		if (w == 0) { print "nowrite\t0"; exit }
		depth = 0
		for (i = 1; i < g; i++) {
			c = substr(s, i, 1)
			if (c == "{") depth++
			else if (c == "}") depth--
		}
		print (g < w ? "ok" : "late") "\t" depth
	}
'`

STATE=`printf '%s' "$READING" | cut -f1`
DEPTH=`printf '%s' "$READING" | cut -f2`

case "$STATE" in
	noguard)
		echo "check-channel-content-lock.sh: CPmt::ParseInternal rewrites the tracks," >&2
		echo "  the table and the CA containers of a channel without taking" >&2
		echo "  $GUARD, which is the lock its readers hold" >&2
		exit 1
		;;
	nowrite)
		echo "check-channel-content-lock.sh: CPmt::ParseInternal no longer calls" >&2
		echo "  channel->resetPids(), so this scan has stopped matching" >&2
		exit 1
		;;
	late)
		echo "check-channel-content-lock.sh: the guard in CPmt::ParseInternal stands" >&2
		echo "  behind the first write, which leaves that write unheld" >&2
		exit 1
		;;
esac

if [ "$DEPTH" != 1 ]; then
	echo "check-channel-content-lock.sh: the guard in CPmt::ParseInternal sits" >&2
	echo "  $DEPTH braces deep, so a path through the function can miss it" >&2
	exit 1
fi

# The other writer: the channel that was playing has its pids dropped when the
# box tunes away from it. Every resetPids in the channel daemon has to stand in
# a block that opens with the guard.
awk -v guard="$GUARD" '
	/resetPids/ {
		found++
		# The guard has to be the statement before it inside the same block,
		# which is how the one call site is written.
		if (held != 1) bad++
	}
	index($0, guard) > 0 { held = 1; next }
	/[{}]/ { held = 0 }
	END { print found "\t" bad+0 }
' "$tmp/zapit" > "$tmp/zapitreading"

FOUND=`cut -f1 "$tmp/zapitreading"`
BAD=`cut -f2 "$tmp/zapitreading"`

if [ "${FOUND:-0}" -lt 1 ]; then
	echo "check-channel-content-lock.sh: no resetPids call in $ZAPIT," >&2
	echo "  so this scan has stopped matching" >&2
	exit 1
fi

if [ "$BAD" -gt 0 ]; then
	echo "check-channel-content-lock.sh: $BAD resetPids call(s) in $ZAPIT" >&2
	echo "  stand outside a block opened with $GUARD" >&2
	exit 1
fi

# ------------------------------------------------- nobody else writes it at all
#
# The set of files allowed to rewrite a channel's stream state. channel.cpp and
# its header own the members, and the two above are the only writers; a write
# from anywhere else would be one this scan has never looked at.
WRITE='resetPids\(\)|setRawPmt\(|addAudioChannel\(|->camap[ \t]*=[^=]|->capids[ \t]*=[^=]'

# Each candidate read again with its comments blanked, because a write that is
# commented out is not a write and one of them is.
WRITERS=
for f in `cd "$SRC" && grep -rlE "$WRITE" src lib | sort -u`; do
	case "$f" in
		src/zapit/channel.cpp|src/zapit/channel.h) continue ;;
		src/zapit/scanpmt.cpp|src/zapit/zapit.cpp) continue ;;
	esac
	if prepare "$SRC/$f" | grep -qE "$WRITE"; then
		WRITERS="$WRITERS $f"
	fi
done

if [ -n "$WRITERS" ]; then
	echo "check-channel-content-lock.sh: a channel's stream state is written in" >&2
	echo "  files this check does not hold to the channel lock:" >&2
	printf '  %s\n' $WRITERS >&2
	exit 1
fi

# ------------------------------------------------------------ the maps
#
# Every member of the service manager whose body writes one of the three channel
# maps, and whether it holds the lock. The names are read out of the file, so one
# added without the lock fails here.
#
# ParseChannels is the one left out, and by name: it is private, its only caller
# is LoadServices, and that one holds the lock across the whole parse.
awk -v m="channels_mutex" '
	function flush() {
		if (name == "") return
		if (writes && name != "CServiceManager::ParseChannels")
			print (held ? "held" : "bare") "\t" name
		name = ""; writes = 0; held = 0
	}
	depth == 0 && /^[A-Za-z_].*CServiceManager::[A-Za-z_0-9]+[(]/ {
		flush()
		s = $0
		sub(/[(].*$/, "", s)
		sub(/^.*[ \t*&]/, "", s)
		name = s
	}
	{
		if (name != "") {
			if ($0 ~ /(allchans|curchans|nvodchannels)\.(insert|erase|clear)[(]/) writes = 1
			if ($0 ~ /second\.setName[(]/) writes = 1
			if ($0 ~ /second\.flags = /) writes = 1
			if (index($0, m) > 0) held = 1
		}
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") depth++
			else if (c == "}") { depth--; if (depth == 0 && name != "") flush() }
		}
	}
	END { flush() }
' "$tmp/services" > "$tmp/mapwriters"

mapwriters=`awk 'END { print NR }' "$tmp/mapwriters"`
# Six members wrote a map when this was written.
if [ "$mapwriters" -lt 6 ]; then
	echo "check-channel-content-lock.sh: $mapwriters map writers read out of" >&2
	echo "  $SERVICES, the scan has stopped matching" >&2
	exit 1
fi

bare=`grep '^bare	' "$tmp/mapwriters" | cut -f2 || true`
if [ -n "$bare" ]; then
	echo "check-channel-content-lock.sh: these write a channel map without" >&2
	echo "  taking channels_mutex:" >&2
	printf '  %s\n' $bare >&2
	exit 1
fi

# ---------------------------------------------------------------- the reader
#
# The other half. Without it the guard above is held against nobody.
body_of "$tmp/source" 'Status channelStreams[(]' > "$tmp/streams"
[ -s "$tmp/streams" ] || {
	echo "check-channel-content-lock.sh: no channelStreams definition in $SOURCE" >&2
	exit 1
}

STREAMS=`tr '\n' ' ' < "$tmp/streams" | tr -s '[:space:]' ' '`
printf '%s' "$STREAMS" | awk -v guard="$GUARD" '
	{
		g = index($0, guard)
		r = index($0, "fillStreams")
		if (g == 0 || r == 0 || g > r) exit 1
	}
' || {
	echo "check-channel-content-lock.sh: channelStreams reads a channel's tracks" >&2
	echo "  without $GUARD in front of the read" >&2
	exit 1
}

body_of "$tmp/source" 'Status currentCaids[(]' > "$tmp/caids"
[ -s "$tmp/caids" ] || {
	echo "check-channel-content-lock.sh: no currentCaids definition in $SOURCE" >&2
	exit 1
}

CAIDS=`tr '\n' ' ' < "$tmp/caids" | tr -s '[:space:]' ' '`
printf '%s' "$CAIDS" | awk -v guard="$GUARD" '
	{
		g = index($0, guard)
		r = index($0, "->camap")
		if (g == 0 || r == 0 || g > r) exit 1
	}
' || {
	echo "check-channel-content-lock.sh: currentCaids reads a channel's CA table" >&2
	echo "  without $GUARD in front of the read" >&2
	exit 1
}

exit 0
