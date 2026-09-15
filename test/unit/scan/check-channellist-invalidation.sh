#!/bin/sh
# CServiceManager::allchans owns every CZapitChannel by value, and LoadServices
# destroys the whole map before it parses the new one. Every channel list the
# application holds is a vector of borrowed raw pointers into that map with no
# lifetime binding of its own, so at that moment each of them names freed objects
# until channelsInit() has built them again on the main loop.
#
# CZapit::PrepareChannels is where the destruction happens and where the lists are
# emptied. Neither that function nor CNeutrinoApp::invalidateChannelLists can be
# linked into the test binary, so both are checked as text.
#
# Two properties, and the second is why this file is not a written down list of names:
# the set of lists is read out of channelsInit's own deletes, so a list added there and
# forgotten here fails rather than staying behind holding dead pointers.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-channellist-invalidation.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
NEUTRINO="$SRC/src/neutrino.cpp"
ZAPIT="$SRC/src/zapit/zapit.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$NEUTRINO" "$ZAPIT" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-channellist-invalidation.sh: cannot read $f" >&2; exit 1; }
done

# Below this the delete scan is not reading channelsInit any more. The tree held
# thirteen lists when this was written: two CChannelList and eleven
# CBouquetList.
LIST_FLOOR=13

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# Comments blanked and then #if 0 behind them, so the scan reads what the
# compiler reads. Literals are replaced rather than kept: none of the text below
# lives inside one, and a brace or a parenthesis inside a string would otherwise
# move a balance.
prepare() {
	awk -v keepstrings=0 -f "$STRIP" "$1" | awk -f "$BLANK"
}

prepare "$NEUTRINO" > "$tmp/neutrino"
prepare "$ZAPIT" > "$tmp/zapit"

# The body of the one function whose signature the pattern matches, brace
# balanced from that signature, the same way check-hook.sh isolates
# CNeutrinoApp::run.
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

body_of "$tmp/neutrino" '^void CNeutrinoApp::channelsInit[(]' > "$tmp/channelsinit"
[ -s "$tmp/channelsinit" ] || {
	echo "check-channellist-invalidation.sh: no CNeutrinoApp::channelsInit definition in $NEUTRINO" >&2
	exit 1
}

body_of "$tmp/neutrino" '^void CNeutrinoApp::invalidateChannelLists[(]' > "$tmp/invalidate"
[ -s "$tmp/invalidate" ] || {
	echo "check-channellist-invalidation.sh: no CNeutrinoApp::invalidateChannelLists definition in $NEUTRINO" >&2
	exit 1
}

body_of "$tmp/zapit" '^bool CZapit::PrepareChannels[(]' > "$tmp/prepare"
[ -s "$tmp/prepare" ] || {
	echo "check-channellist-invalidation.sh: no CZapit::PrepareChannels definition in $ZAPIT" >&2
	exit 1
}

# What channelsInit destroys and builds again is exactly what has to be emptied
# when the channels under it die: a list it does not rebuild would stay empty,
# and a list it rebuilds without this emptying it keeps dead pointers.
grep -oE 'delete[ \t]+(TV|RADIO|All)[A-Za-z0-9_]*List' "$tmp/channelsinit" \
	| sed -E 's/^delete[ \t]+//' | sort -u > "$tmp/rebuilt"

lists=`awk 'END { print NR }' "$tmp/rebuilt"`
if [ "$lists" -lt "$LIST_FLOOR" ]; then
	echo "check-channellist-invalidation.sh: $lists lists read out of channelsInit," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

grep -oE '(TV|RADIO|All)[A-Za-z0-9_]*List' "$tmp/invalidate" | sort -u > "$tmp/emptied"

missing=`comm -23 "$tmp/rebuilt" "$tmp/emptied"`
if [ -n "$missing" ]; then
	echo "check-channellist-invalidation.sh: channelsInit rebuilds lists that" >&2
	echo "  invalidateChannelLists leaves holding freed channels:" >&2
	printf '  %s\n' $missing >&2
	exit 1
fi

# A name here that channelsInit never rebuilds is either a typo or a list whose
# lifetime nobody manages, and both are worth stopping for.
extra=`comm -13 "$tmp/rebuilt" "$tmp/emptied"`
if [ -n "$extra" ]; then
	echo "check-channellist-invalidation.sh: invalidateChannelLists names lists" >&2
	echo "  channelsInit does not rebuild:" >&2
	printf '  %s\n' $extra >&2
	exit 1
fi

# PrepareChannels has to reach the call on every path through it, so the
# position is checked and not only the presence. The body is joined onto one
# line first, so a call reflowed across lines reads the same as one written on
# a single line.
FLAT=`tr '\n' ' ' < "$tmp/prepare" | tr -s '[:space:]' ' '`

ANCHOR="CNeutrinoApp::getInstance()->invalidateChannelLists();"

# One line: the depth the anchor stands at counted from the function's own
# opening brace, a tab, and the character that ends the statement before it.
# Depth 1 and one of ; { } is the only reading this accepts. A for, a while, a
# switch, a do, a lambda body or a bare block around the call all raise the
# depth; a one line if, an else, a label, a ternary arm and an operand of a
# short circuit all leave the depth alone and put something other than the end
# of a statement in front of it. Either way the call is not reached on every
# path, and either way this refuses it.
READING=`printf '%s' "$FLAT" | awk -v anchor="$ANCHOR" '
	{
		s = $0; n = length(s)
		pos = index(s, anchor)
		if (pos == 0) { print "0\t"; exit }
		depth = 0
		for (i = 1; i < pos; i++) {
			c = substr(s, i, 1)
			if (c == "{") depth++
			else if (c == "}") depth--
		}
		i = pos - 1
		while (i >= 1 && substr(s, i, 1) == " ") i--
		print depth "\t" (i >= 1 ? substr(s, i, 1) : "")
	}
'`

DEPTH=`printf '%s' "$READING" | cut -f1`
PREV=`printf '%s' "$READING" | cut -f2`

if [ "$DEPTH" = 0 ]; then
	echo "check-channellist-invalidation.sh: CZapit::PrepareChannels does not call" >&2
	echo "  $ANCHOR" >&2
	exit 1
fi

if [ "$DEPTH" != 1 ]; then
	echo "check-channellist-invalidation.sh: the invalidateChannelLists call in" >&2
	echo "  CZapit::PrepareChannels sits $DEPTH braces deep, so a path through the" >&2
	echo "  function can miss it" >&2
	exit 1
fi

case "$PREV" in
	';'|'{'|'}') ;;
	*)
		echo "check-channellist-invalidation.sh: the invalidateChannelLists call in" >&2
		echo "  CZapit::PrepareChannels stands behind '$PREV' rather than at the end of" >&2
		echo "  the statement before it, so something decides whether it runs" >&2
		exit 1
		;;
esac

# The one list form this replaced. Emptying a single list from here reads as a
# fix and is the bug: the other twelve keep their dead pointers.
if grep -q 'ClearChannelList' "$tmp/prepare"; then
	echo "check-channellist-invalidation.sh: CZapit::PrepareChannels empties a list" >&2
	echo "  of its own beside the call, which leaves the rest of them behind" >&2
	exit 1
fi

exit 0
