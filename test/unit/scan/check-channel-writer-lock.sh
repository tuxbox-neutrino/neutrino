#!/bin/sh
# The REST layer reads the bouquets and the channels under the channel lock on
# the web server's threads: a bouquet's two names and its hidden, locked and
# user flags, a channel's number, its lock flag and lock count, and its state
# flags. A copy of a string taken while another thread assigns a longer one to
# it reads a buffer that is freed under it, so every writer of those fields
# takes the same lock, and so do the walks that change them in bulk.
#
# What this holds the channel stack to:
#
# - the bouquet fields are written in a hold of the lock, in the files that
#   write them and in no others (for the plain Name and setName, too common a
#   spelling to look for elsewhere, only in the files read);
# - within the bouquet manager the channel numbers are handed out by
#   renumChannels alone, and only in a hold, and the renumbering holds the
#   lock in one hold from emptying the bouquet of the remaining channels to the
#   last number, through the forms that do not take it again;
# - a channel's lock flag and state flags are written in a hold by the bouquet
#   manager, the channel list and the bouquet editor's channel list; the scan
#   threads' marks are not read;
# - the channel daemon's rename and hide commands read the socket before they
#   take the lock, so a client that is slow to send holds up nobody.
#
# None of this can be linked into the test binary, so it is checked as text.
# check-channel-lock-reentry.sh holds the holds themselves to never taking the
# lock a second time.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-channel-writer-lock.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
BOUQUETS="$SRC/src/zapit/bouquets.cpp"
ZAPIT="$SRC/src/zapit/zapit.cpp"
EDITOR="$SRC/src/gui/bedit/bouqueteditor_bouquets.cpp"
CHEDITOR="$SRC/src/gui/bedit/bouqueteditor_channels.cpp"
LIST="$SRC/src/gui/channellist.cpp"
SERVICES="$SRC/src/zapit/getservices.cpp"
for f in "$BOUQUETS" "$ZAPIT" "$EDITOR" "$CHEDITOR" "$LIST" "$SERVICES" \
		"$HERE/strip-comments.awk" "$HERE/blank-if0.awk" "$HERE/channel-holds.awk"; do
	[ -r "$f" ] || { echo "check-channel-writer-lock.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
prepare() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1" | awk -f "$HERE/blank-if0.awk"
}
holds() {
	prepare "$1" | awk -f "$HERE/channel-holds.awk"
}
holds "$BOUQUETS" > "$tmp/bouquets"
holds "$ZAPIT" > "$tmp/zapit"
holds "$EDITOR" > "$tmp/editor"
holds "$CHEDITOR" > "$tmp/cheditor"
holds "$LIST" > "$tmp/list"
prepare "$SERVICES" > "$tmp/services"

bad=0

# Lines of a marked file that match the pattern and stand in no hold, leaving
# out the functions named in the third argument.
unheld() {
	awk -F '\t' -v pat="$2" -v skip="$3" '
		BEGIN { n = split(skip, s, " "); for (i = 1; i <= n; i++) leave[s[i]] = 1 }
		$2 == 0 && !($4 in leave) {
			text = $0
			sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text)
			if (text ~ pat)
				print "  " $1 ": " text
		}
	' "$1"
}

report() {
	if [ -n "$2" ]; then
		echo "check-channel-writer-lock.sh: $1" >&2
		printf '%s\n' "$2" >&2
		bad=1
	fi
}

# ------------------------------------------------------------ bouquet fields
#
# The overload of saveBouquets that takes a mode moves a scan's bouquets into
# the manager it is called on, the scan's own one, which nothing else reads.
FIELDS='(->|[.])(Name|bName|bHidden|bUser|bOther)[ \t]*=[^=]|setName[ \t]*[(]|bLockCount[ \t]*([+][+]|--|[+]=|-=)|([+][+]|--)[ \t]*[(]*[A-Za-z_]*[)]*[^;]*bLockCount|(->|[.])bLocked[ \t]*=[^=]'
report "these write a bouquet's names or flags, or a channel's lock count, outside a hold of the channel lock:" \
	"`unheld "$tmp/bouquets" "$FIELDS" 'saveBouquets'`"
report "the channel daemon writes a bouquet's names or flags outside a hold of the channel lock:" \
	"`unheld "$tmp/zapit" "$FIELDS" ''`"
report "the bouquet editor writes a bouquet's names or flags outside a hold of the channel lock:" \
	"`unheld "$tmp/editor" "$FIELDS" ''`"

# And no other file writes them. bName and the flags are the bouquet's own
# names; the plain Name and setName are too common a spelling to look for. A
# lock flag, a bouquet's or a channel's, may also be written by the two channel
# lists read below.
others=`cd "$SRC" && grep -rlE '(->|[.])(bName|bHidden|bUser|bOther|bLocked)[[:blank:]]*=[^=]|bLockCount[[:blank:]]*(\+\+|--|\+=|-=)' src lib 2>/dev/null | \
	grep -v -E '^src/zapit/(bouquets|zapit)[.]cpp$|^src/gui/bedit/bouqueteditor_(bouquets|channels)[.]cpp$|^src/gui/channellist[.]cpp$' || true`
report "these files write a bouquet's names or flags, or a lock flag or count, and are not read here:" \
	"`printf '%s\n' "$others" | sed '/^$/d; s/^/  /'`"

# ------------------------------------------------------------ channel numbers
#
# Handed out in one place in the bouquet manager, and that place reached only
# from a hold.
numbers=`awk -F '\t' '
	{ text = $0; sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text) }
	text ~ /->number[ \t]*=[^=]/ && $4 != "renumChannels" { print "  " $1 ": " text }
' "$tmp/bouquets"`
report "a channel number is written outside renumChannels:" "$numbers"
calls=`awk -F '\t' '
	{ text = $0; sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text) }
	text ~ /renumChannels[ \t]*[(]/ && text !~ /::renumChannels[ \t]*[(]/ && $2 == 0 { print "  " $1 ": " text }
' "$tmp/bouquets"`
report "renumChannels is called outside a hold of the channel lock:" "$calls"

remaining=`awk -F '\t' '$4 == "makeRemainingChannelsBouquet" {
	text = $0; sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text); print text
}' "$tmp/bouquets" | tr -d '[:space:]'`
# One hold, read off the marks rather than the order of the text: one guard in
# the function, and the zeroing and every numbering standing in a hold, which
# with one guard is that one.
oneHold=`awk -F '\t' '$4 == "makeRemainingChannelsBouquet" {
	text = $0; sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text)
	if (index(text, "CServiceManager::ChannelGuard") > 0) guards++
	if (index(text, "ResetChannelNumbersLocked(") > 0) { resets++; if ($2 == 0) loose++ }
	if (text ~ /renumChannels[ \t]*[(]/) { numbering++; if ($2 == 0) loose++ }
}
END { print ((guards == 1 && resets == 1 && numbering > 0 && loose == 0) ? "ok" : "guards " guards + 0 ", zeroings " resets + 0 ", numberings " numbering + 0 ", outside a hold " loose + 0) }
' "$tmp/bouquets"`
case "$remaining" in
	"")
		report "the renumbering has stopped matching:" "  no makeRemainingChannelsBouquet in $BOUQUETS"
		;;
	*)
		[ "$oneHold" = ok ] || report "the renumbering does not hold the channel lock in one hold from before the numbers are zeroed:" \
			"  makeRemainingChannelsBouquet: $oneHold"
		;;
esac
taking=`printf '%s' "$remaining" | grep -oE '(ResetChannelNumbers|addBouquet|->addService)[(]' || true`
report "the renumbering calls a form that takes the channel lock, under that lock:" \
	"`printf '%s\n' "$taking" | sed '/^$/d; s/^/  /'`"

reset=`awk '
	/^void CServiceManager::ResetChannelNumbersLocked[(]/ { want = "locked" }
	/^void CServiceManager::ResetChannelNumbers[(]/ { want = "taking" }
	want != "" {
		body[want] = body[want] $0
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") { depth++; seen = 1 }
			else if (c == "}") { depth--; if (seen && depth == 0) { want = ""; seen = 0 } }
		}
	}
	END {
		if (body["locked"] == "" || body["taking"] == "") { print "missing"; exit }
		if (index(body["locked"], "channels_mutex") > 0) { print "locked-takes"; exit }
		if (index(body["taking"], "channels_mutex") == 0 || index(body["taking"], "ResetChannelNumbersLocked(") == 0) { print "taking-wrong"; exit }
		print "ok"
	}
' "$tmp/services"`
[ "$reset" = ok ] || report "the two forms of ResetChannelNumbers are not what the renumbering relies on:" "  $reset"

# ------------------------------------------------------------ channel flags
FLAGS='(->|[.])bLocked[ \t]*=[^=]|(->|[.])flags[ \t]*[|&^]?=[^=]'
flagged=`for f in "$tmp/bouquets" "$tmp/list" "$tmp/cheditor"; do unheld "$f" "$FLAGS" ''; done`
report "a channel's lock flag or state flags are written outside a hold of the channel lock:" "$flagged"

# ------------------------------------------------------------ socket first
for case_ in 'case CZapitMessages::CMD_BQ_RENAME_BOUQUET:' 'case CZapitMessages::CMD_BQ_SET_HIDDENSTATE:'; do
	order=`awk -F '\t' -v want="$case_" '
		{ text = $0; sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text) }
		found == 0 && index(text, want) > 0 { found = 1 }
		found == 1 {
			print text
			n = length(text)
			for (i = 1; i <= n; i++) {
				c = substr(text, i, 1)
				if (c == "{") { depth++; seen = 1 }
				else if (c == "}") { depth--; if (seen && depth == 0) exit }
			}
		}
	' "$tmp/zapit" | tr -d '[:space:]'`
	case "$order" in
		"") report "the command has stopped matching:" "  $case_" ;;
		*"receive_"*"CServiceManager::ChannelGuard"*)
			case "$order" in
				*"CServiceManager::ChannelGuard"*"receive_"*)
					report "the command reads the socket under the channel lock:" "  $case_" ;;
			esac
			;;
		*) report "the command does not read the socket before it takes the channel lock:" "  $case_" ;;
	esac
done

exit $bad
