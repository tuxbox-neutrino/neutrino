#!/bin/sh
# A page over the network holds the bouquet list and learns that it changed
# from bouquets-changed on the event stream. The core API's own writes say it,
# and so must every screen that changes the bouquets past those writes:
#
# - the channel list, whose edit mode and bouquet list change the bouquets in
#   place and write them out on the way back through the bouquet manager,
#   past the channel daemon; it says it after the last write and the rebuild,
#   and without waiting, nothing of the daemon's being involved;
# - the bouquet editor, which saves through the daemon or throws its changes
#   away by having the daemon read the files back; it says it after either;
# - the favourites, which add a channel and save through the daemon.
#
# Checked as text: none of these screens can be linked into the test binary.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-channellist-announce.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
APP="$SRC/src/neutrino.cpp"
EDITOR="$SRC/src/gui/bedit/bouqueteditor_bouquets.cpp"
FAV="$SRC/src/gui/favorites.cpp"
for f in "$APP" "$EDITOR" "$FAV" "$HERE/strip-comments.awk" "$HERE/blank-if0.awk"; do
	[ -r "$f" ] || { echo "check-channellist-announce.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
prepare() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1" | awk -f "$HERE/blank-if0.awk"
}
prepare "$APP" > "$tmp/app"
prepare "$EDITOR" > "$tmp/editor"
prepare "$FAV" > "$tmp/fav"

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

# The channel list's save block, inside showChannelList.
list=`block "$tmp/app" 'int CNeutrinoApp::showChannelList('`
save=`printf '%s' "$list" | awk '{
	i = index($0, "if(channels_changed||favorites_changed||bouquets_changed||channels_init){")
	if (i == 0) exit
	t = substr($0, i); depth = 0
	for (j = 1; j <= length(t); j++) {
		c = substr(t, j, 1)
		if (c == "{") depth++
		else if (c == "}") { depth--; if (depth == 0) { print substr(t, 1, j); exit } }
	}
}'`
case "$save" in
	"")
		echo "check-channellist-announce.sh: no save block in showChannelList" >&2
		bad=1
		;;
	*"g_bouquetManager->saveBouquets()"*"channelsInit("*"}coreapi::channels::publishBouquetsChanged();"*) ;;
	*)
		echo "check-channellist-announce.sh: showChannelList writes the bouquets out and" >&2
		echo "  does not say so on the event bus after the last write and the rebuild" >&2
		bad=1
		;;
esac

# The bouquet editor, after a save and after throwing its changes away.
for spec in 'void CBEBouquetWidget::saveChanges(|g_Zapit->saveBouquets()' \
		'void CBEBouquetWidget::discardChanges(|g_Zapit->restoreBouquets();'; do
	want=`echo "$spec" | cut -d'|' -f1`
	after=`echo "$spec" | cut -d'|' -f2`
	b=`block "$tmp/editor" "$want"`
	case "$b" in
		*"$after"*"coreapi::channels::announceBouquetsChanged();"*) ;;
		*)
			echo "check-channellist-announce.sh: $want changes the bouquets through the" >&2
			echo "  channel daemon and does not say so afterwards" >&2
			bad=1
			;;
	esac
done

# The favourites, after the save.
b=`block "$tmp/fav" 'int CFavorites::addChannelToFavorites('`
case "$b" in
	*"g_Zapit->saveBouquets()"*"coreapi::channels::announceBouquetsChanged();"*) ;;
	*)
		echo "check-channellist-announce.sh: CFavorites::addChannelToFavorites saves the" >&2
		echo "  bouquets and does not say so afterwards" >&2
		bad=1
		;;
esac

exit $bad
