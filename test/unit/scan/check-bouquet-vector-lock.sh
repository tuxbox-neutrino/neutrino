#!/bin/sh
# CBouquetManager::Bouquets is a vector of pointers to heap objects, and a channel
# reload frees every one of them and empties it, on the channel daemon's thread.
# GET /api/v1/bouquets walks that same vector on one of the web server's threads
# and reads two strings off each entry, so a reload under a walk is a walk of
# freed objects. The two lists inside a bouquet are the same story one level
# down: adding a channel to one can move the whole list.
#
# The rule this holds the tree to: the channel manager's lock covers the bouquet
# vector and the two lists inside a bouquet as well as the channel map. Writers
# take it around the change; readers hold it for the whole of a walk, because
# what it holds off is the freeing of what they are reading.
#
# None of this can be linked into the test binary, so it is checked as text.
#
# Two properties, and the second is why this is not a written down list of
# writers: the functions that change the vector are read out of the file, so one
# added there without the lock fails here.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-bouquet-vector-lock.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
BOUQUETS="$SRC/src/zapit/bouquets.cpp"
SOURCE="$SRC/src/coreapi/box/channelsource_real.cpp"
for f in "$STRIP" "$BLANK" "$BOUQUETS" "$SOURCE"; do
	[ -r "$f" ] || { echo "check-bouquet-vector-lock.sh: cannot read $f" >&2; exit 1; }
done

GUARD='CServiceManager::ChannelGuard'

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

prepare() {
	awk -v keepstrings=0 -f "$STRIP" "$1" | awk -f "$BLANK"
}

prepare "$BOUQUETS" > "$tmp/bouquets"
prepare "$SOURCE" > "$tmp/source"

# Below this the writer scan is not reading the file any more. The tree held
# eleven such functions when this was written.
WRITER_FLOOR=11

# ------------------------------------------------------------ the writers
#
# Every function in the file, with the name it is defined under, whether its
# body changes the length of the bouquet vector or of a bouquet's two channel
# lists, and whether it takes the guard. A function is read from its definition
# line to the brace that closes it.
#
# Two names are left out, and both by name rather than by a pattern that happens
# not to match them. saveBouquets(bouquetMode, ...) is only ever called on the
# scan's own manager and never on the one behind g_bouquetManager, so nothing
# outside the scan can see what it changes. A name ending in Locked says the
# caller holds the lock, and the call sites of those are checked below instead.
scan_writers() {
	awk -v guard="$GUARD" '
		function flush() {
			if (name == "") return
			locked = (name ~ /Locked$/)
			if (writes && !locked && name != "CBouquetManager::saveBouquets")
				print (held ? "held" : "bare") "\t" name
			# A caller of a Locked form has to hold the lock itself, that being
			# the whole of what the suffix promises.
			if (calls && !locked)
				print (held ? "held" : "bare") "\t" name
			name = ""; writes = 0; held = 0; calls = 0
		}
		depth == 0 && /^[A-Za-z_].*(CBouquetManager|CZapitBouquet)::[A-Za-z_]+[(]/ {
			flush()
			s = $0
			sub(/[(].*$/, "", s)
			sub(/^.*[ \t*&]/, "", s)
			name = s
		}
		{
			if (name != "") {
				# What changes the length of one of the three vectors, or frees
				# what one of them holds.
				if ($0 ~ /Bouquets\.(clear|erase|insert|push_back)[(]/) writes = 1
				if ($0 ~ /^[ \t]*Bouquets = /) writes = 1
				if ($0 ~ /delete Bouquets\[/) writes = 1
				if ($0 ~ /^[ \t]*sort\(Bouquets\.begin/) writes = 1
				if ($0 ~ /(tvChannels|radioChannels)\.(clear|erase|insert|push_back)[(]/) writes = 1
				if ($0 ~ /^[ \t]*sort\((tv|radio)Channels\.begin/) writes = 1
				if ($0 ~ /channels->(erase|insert)[(]/) writes = 1
				if ($0 ~ /[(][*]channels[)]\.erase[(]/) writes = 1
				if ($0 ~ /[A-Za-z_]+Locked[(]/) calls = 1
				if (index($0, guard) > 0) held = 1
			}
			n = length($0)
			for (i = 1; i <= n; i++) {
				c = substr($0, i, 1)
				if (c == "{") depth++
				else if (c == "}") { depth--; if (depth == 0 && name != "") flush() }
			}
		}
		END { flush() }
	' "$1"
}

scan_writers "$tmp/bouquets" > "$tmp/writers"

writers=`awk 'END { print NR }' "$tmp/writers"`
if [ "$writers" -lt "$WRITER_FLOOR" ]; then
	echo "check-bouquet-vector-lock.sh: $writers writers read out of $BOUQUETS," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

bare=`grep '^bare	' "$tmp/writers" | cut -f2 || true`
if [ -n "$bare" ]; then
	echo "check-bouquet-vector-lock.sh: these change the bouquet vector or the" >&2
	echo "  channel lists inside a bouquet without taking $GUARD:" >&2
	printf '  %s\n' $bare >&2
	exit 1
fi

# ------------------------------------------------------------- the readers
#
# The other half. A read that names the vector has to stand in a function that
# holds the guard, or the writers above are held against nobody. The functions
# are read out of the file the same way, so a bouquet read added there and not
# held fails.
awk -v guard="$GUARD" '
	function flush() {
		if (name == "") return
		if (reads)
			print (held ? "held" : "bare") "\t" name
		name = ""; reads = 0; held = 0
	}
	# A member of the adapter class, which is what every bouquet read here is.
	# The depth it opens at is remembered rather than assumed, the class sitting
	# inside two namespaces.
	name == "" && /^[ \t]+(Status|void|bool)[ \t]+[A-Za-z_]+[(]/ {
		s = $0
		sub(/[(].*$/, "", s)
		sub(/^.*[ \t*&]/, "", s)
		name = s
		base = depth
	}
	{
		if (name != "") {
			if (index($0, "g_bouquetManager->Bouquets") > 0) reads = 1
			if (index($0, guard) > 0) held = 1
		}
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") { depth++; opened = 1 }
			else if (c == "}") { depth--; if (name != "" && opened && depth <= base) { flush(); opened = 0 } }
		}
	}
	END { flush() }
' "$tmp/source" > "$tmp/readers"

readers=`awk 'END { print NR }' "$tmp/readers"`
# Five functions named the vector when this was written.
if [ "$readers" -lt 5 ]; then
	echo "check-bouquet-vector-lock.sh: $readers bouquet readers read out of" >&2
	echo "  $SOURCE, the scan has stopped matching" >&2
	exit 1
fi

bare=`grep '^bare	' "$tmp/readers" | cut -f2 || true`
if [ -n "$bare" ]; then
	echo "check-bouquet-vector-lock.sh: these walk the bouquet vector without" >&2
	echo "  $GUARD:" >&2
	printf '  %s\n' $bare >&2
	exit 1
fi

# The one write this layer makes into a bouquet itself. It runs with the guard
# already held, so it has to use the form that does not take it again; the
# taking form would stop the thread on a lock it is holding.
if grep -q '\->moveService(' "$tmp/source"; then
	echo "check-bouquet-vector-lock.sh: channelsource_real.cpp calls moveService," >&2
	echo "  which takes the channel lock, from under that same lock" >&2
	exit 1
fi

exit 0
