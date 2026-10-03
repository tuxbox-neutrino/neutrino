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
# added there without the lock fails here. A third holds the one write the core
# API makes into a bouquet itself, its reorder, to a write that keeps the
# length: the channel stack's walkers of these lists do not all take the lock,
# and one at the end of a list that shrinks under it steps past it.
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

# The one write this layer makes into a bouquet itself, its reorder. Not
# through the channel stack's move or its add and remove, which take a channel
# out and put it back and so change the list's length, and of which the forms
# that take the lock would stop the thread on the lock it holds. The adapter
# changes what a bouquet holds only through the channel stack's commands.
if grep -qE '(move|add|remove)Service(Locked)?[[:blank:]]*[(]' "$tmp/source"; then
	echo "check-bouquet-vector-lock.sh: channelsource_real.cpp changes a bouquet through" >&2
	echo "  the channel stack's move, add or remove, which change the list's length" >&2
	exit 1
fi

# Nor does it change the length of a bouquet's list any other way, or the
# block the list keeps its entries in: not directly, not through a reference or
# a pointer it took to the list, whatever type that spells and whether or not
# the pointer itself is const, or to such a name in turn, through the name or
# through the pointer taken back, not by assigning over the list, not by a swap
# with it on either side, and not by asking it for a block of another size.
# What is read is text: a name is one when its declaration or its first value
# says so, and else, do, return and throw before a star are no type. A
# reference or a pointer to a const list cannot change it, and a copy made to
# work on, or a pointer to one of its entries, is not the list; none of these
# is looked at.
lengths=`awk '
	function word(n) { return "(^|[^A-Za-z_0-9])" n "([^A-Za-z_0-9]|$)" }
	{ line[NR] = $0 }
	END {
		grow = "(erase|insert|push_back|pop_back|clear|resize|assign|swap|emplace|emplace_back|reserve|shrink_to_fit)"
		names["tvChannels"] = 1
		names["radioChannels"] = 1
		# A name given to a list: a reference or a pointer declared with the
		# list type, or set to a list or to a name found already, until no new
		# one turns up. Set to a list means named whole, not an entry of it
		# nor what a call on it returns. A const or volatile after the star
		# qualifies the pointer and may stand before the name.
		do {
			added = 0
			for (i = 1; i <= NR; i++) {
				rest = line[i]
				while (match(rest, /[&*][ \t]*((const|volatile)[ \t]+)*[A-Za-z_][A-Za-z_0-9]*[ \t]*(=[^=]|[(]|[{])[^;]*/)) {
					decl = substr(rest, RSTART, RLENGTH)
					lead = substr(rest, 1, RSTART - 1)
					rest = substr(rest, RSTART + RLENGTH)
					# The list is const only by a const in the type before
					# the first star or ampersand of this declaration; one
					# after that makes a pointer const and not the list.
					base = lead
					sub(/^.*[,;(){}]/, "", base)
					sub(/[&*].*$/, "", base)
					if (base ~ /(^|[^A-Za-z_0-9])const([^A-Za-z_0-9]|$)/)
						continue
					name = decl
					sub(/^[&*][ \t]*((const|volatile)[ \t]+)*/, "", name)
					sub(/[^A-Za-z_0-9].*$/, "", name)
					if (name in names)
						continue
					init = decl
					sub(/^[&*][ \t]*((const|volatile)[ \t]+)*[A-Za-z_][A-Za-z_0-9]*[ \t]*/, "", init)
					hit = (lead ~ /ZapitChannelList[ \t]*$/)
					for (n in names)
						if (init ~ ("(^|[^A-Za-z_0-9])" n "[ \t]*([^A-Za-z_0-9 \t.([-]|$)"))
							hit = 1
					if (hit) {
						names[name] = 1
						added = 1
					}
				}
			}
		} while (added)
		for (i = 1; i <= NR; i++) {
			t = line[i]
			bad = (t ~ /std::swap[ \t]*[(]/)
			for (n in names) {
				if (t ~ ("(^|[^A-Za-z_0-9])" n "[ \t]*([.]|->)[ \t]*" grow "[ \t]*[(]"))
					bad = 1
				if (t ~ ("[(][ \t]*[*][ \t]*" n "[ \t]*[)][ \t]*([.]|->)[ \t]*" grow "[ \t]*[(]"))
					bad = 1
				# An assignment over it, directly or through the pointer taken
				# back, which its own declaration, a type before the name, is not.
				# A statement word before the star is no type: else *p = x
				# assigns.
				d = t
				gsub("(^|[^A-Za-z_0-9])(else|do|return|throw)[ \t]*[&*]", ";*", d)
				gsub("[A-Za-z_0-9>][ \t]*[&*][ \t]*((const|volatile)[ \t]+)*" n "[ \t]*=", "@", d)
				if (d ~ ("(^|[^A-Za-z_0-9])" n "[ \t]*=[^=]"))
					bad = 1
				if (t ~ ("[(][ \t]*[*][ \t]*" n "[ \t]*[)][ \t]*=[^=]"))
					bad = 1
				s = t
				while (match(s, /swap[ \t]*[(][^)]*/)) {
					arg = substr(s, RSTART, RLENGTH)
					s = substr(s, RSTART + RLENGTH)
					sub(/^swap[ \t]*[(]/, "", arg)
					if (arg ~ word(n))
						bad = 1
				}
			}
			if (bad)
				print i ": " t
		}
	}
' "$tmp/source"`
if [ -n "$lengths" ]; then
	echo "check-bouquet-vector-lock.sh: channelsource_real.cpp changes the length or the block" >&2
	echo "  of a bouquet's list:" >&2
	printf '%s\n' "$lengths" | sed 's/^/  /' >&2
	exit 1
fi

# And the reorder writes the new order back inside the block of the guard it
# took, so a reader that holds the lock sees the old order or the new one.
reorder=`awk -v pat='Status setBouquetChannels[(]' '
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
' "$tmp/source" | tr '\n' ' '`
placed=`printf '%s' "$reorder" | awk -v guard="$GUARD" '{
	w = index($0, "std::copy(")
	if (w == 0) { print "nocopy"; exit }
	g = 0; pos = 0
	while (1) {
		k = index(substr($0, pos + 1), guard)
		if (k == 0 || pos + k >= w)
			break
		g = pos + k
		pos = g
	}
	if (g == 0) { print "noguard"; exit }
	depth = 0
	for (j = g; j < w; j++) {
		c = substr($0, j, 1)
		if (c == "{") depth++
		else if (c == "}") { depth--; if (depth < 0) { print "closed"; exit } }
	}
	print "ok"
}'`
if [ "$placed" != ok ]; then
	echo "check-bouquet-vector-lock.sh: setBouquetChannels does not write the new order back" >&2
	echo "  with std::copy inside the block of $GUARD ($placed)" >&2
	exit 1
fi

exit 0
