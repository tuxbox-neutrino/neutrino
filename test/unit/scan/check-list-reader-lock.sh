#!/bin/sh
# Two more lists the main loop throws away and builds again while the web
# server's threads are walking them.
#
# The plugin list. CPlugins::loadPlugins empties the vector and fills it from
# five directories, and every accessor on it hands back a pointer into a string
# the vector owns. GET /api/v1/plugins walks it by index on another thread, and
# POST /api/v1/plugins/reload is what sets the rebuild going, so one client
# asking and another reloading is the whole of it.
#
# The channel list behind a logo. CPictureViewer::GetLogoName used to look a
# channel up through CNeutrinoApp::channelList, which is a vector of borrowed
# pointers that channelsInit deletes and creates again. GET
# /api/v1/channels/{id}/logo reaches that from a web thread, and the page that
# shows what is playing asks for it whenever the channel changes.
#
# The locale catalogue. Loading a language frees the block every entry in the
# table points into and writes the table again, and the settings document names
# a label out of it on a web thread. What that thread may ask for is the copying
# accessor, taken under the same lock the load holds.
#
# None of these can be linked into the test binary, so all are checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-list-reader-lock.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
PLUGINS="$SRC/src/gui/plugins.cpp"
NEUTRINO="$SRC/src/neutrino.cpp"
PICTURE="$SRC/src/driver/pictureviewer/pictureviewer.cpp"
LOCALIZE="$SRC/src/system/localize.cpp"
LOCALESRC="$SRC/src/coreapi/box/localesource_real.cpp"
for f in "$STRIP" "$BLANK" "$PLUGINS" "$NEUTRINO" "$PICTURE" "$LOCALIZE" "$LOCALESRC"; do
	[ -r "$f" ] || { echo "check-list-reader-lock.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

prepare() {
	awk -v keepstrings=0 -f "$STRIP" "$1" | awk -f "$BLANK"
}

prepare "$PLUGINS" > "$tmp/plugins"
prepare "$NEUTRINO" > "$tmp/neutrino"
prepare "$PICTURE" > "$tmp/picture"
prepare "$LOCALIZE" > "$tmp/localize"
prepare "$LOCALESRC" > "$tmp/localesrc"

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

# One line: whether the guard stands in front of the anchor, and how many braces
# deep it stands counted from the function's own opening brace. Depth one is the
# only reading accepted, anything else being a path through the function that
# can miss it.
reading_of() {
	tr '\n' ' ' < "$1" | tr -s '[:space:]' ' ' \
		| awk -v guard="$2" -v anchor="$3" '
		{
			s = $0
			g = index(s, guard)
			a = index(s, anchor)
			if (a == 0) { print "noanchor\t0"; exit }
			if (g == 0) { print "noguard\t0"; exit }
			depth = 0
			for (i = 1; i < g; i++) {
				c = substr(s, i, 1)
				if (c == "{") depth++
				else if (c == "}") depth--
			}
			print (g < a ? "ok" : "late") "\t" depth
		}
	'
}

verdict() {
	state=`printf '%s' "$1" | cut -f1`
	depth=`printf '%s' "$1" | cut -f2`
	what="$2"
	case "$state" in
		noanchor)
			echo "check-list-reader-lock.sh: $what no longer reads what this scan" >&2
			echo "  was written for, so the scan has stopped matching" >&2
			exit 1
			;;
		noguard)
			echo "check-list-reader-lock.sh: $what reads a list the box rebuilds" >&2
			echo "  without taking the lock that rebuild holds" >&2
			exit 1
			;;
		late)
			echo "check-list-reader-lock.sh: the guard in $what stands behind the" >&2
			echo "  read it is there to hold" >&2
			exit 1
			;;
	esac
	if [ "$depth" != 1 ]; then
		echo "check-list-reader-lock.sh: the guard in $what sits $depth braces" >&2
		echo "  deep, so a path through it can miss it" >&2
		exit 1
	fi
}

# ------------------------------------------------------------- the plugin list

body_of "$tmp/plugins" '^void CPlugins::loadPlugins[(]' > "$tmp/load"
[ -s "$tmp/load" ] || {
	echo "check-list-reader-lock.sh: no CPlugins::loadPlugins definition in" >&2
	echo "  $PLUGINS" >&2
	exit 1
}
verdict "`reading_of "$tmp/load" 'PluginGuard' 'plugin_list.clear()'`" \
	"CPlugins::loadPlugins"

body_of "$tmp/neutrino" '^coreapi::Status coreapi::boxPlugins[(]' > "$tmp/box"
[ -s "$tmp/box" ] || {
	echo "check-list-reader-lock.sh: no coreapi::boxPlugins definition in" >&2
	echo "  $NEUTRINO" >&2
	exit 1
}
verdict "`reading_of "$tmp/box" 'PluginGuard' 'getNumberOfPlugins()'`" \
	"coreapi::boxPlugins"

# ---------------------------------------------------------- the logo lookup

body_of "$tmp/picture" '^bool CPictureViewer::GetLogoName[(]' > "$tmp/logo"
[ -s "$tmp/logo" ] || {
	echo "check-list-reader-lock.sh: no CPictureViewer::GetLogoName definition in" >&2
	echo "  $PICTURE" >&2
	exit 1
}

if grep -q 'channelList' "$tmp/logo"; then
	echo "check-list-reader-lock.sh: CPictureViewer::GetLogoName reaches the" >&2
	echo "  application's channel list, which is a vector of borrowed pointers" >&2
	echo "  the main loop deletes, and this runs on the web server's threads" >&2
	exit 1
fi

# What it has to use instead: the copy the service manager takes under its own
# lock. Without this the check above would pass on a version that looks up no
# channel at all.
if ! grep -q 'CServiceManager::getInstance()->CopyChannel' "$tmp/logo"; then
	echo "check-list-reader-lock.sh: CPictureViewer::GetLogoName no longer takes" >&2
	echo "  its channel as a copy from the service manager, so the scan has" >&2
	echo "  stopped matching" >&2
	exit 1
fi

# The write back at the end of it goes to where the channel really sits, and it
# has to hold the channel lock over the lookup and the write.
WRITEBACK=`tr '\n' ' ' < "$tmp/logo" | tr -s '[:space:]' ' ' | awk '
	{
		s = $0
		w = index(s, "setAlternateLogo(")
		if (w == 0) { print "nowrite"; exit }
		g = index(s, "CServiceManager::ChannelGuard")
		if (g == 0 || g > w) { print "unheld"; exit }
		print "ok"
	}
'`

case "$WRITEBACK" in
	nowrite)
		echo "check-list-reader-lock.sh: CPictureViewer::GetLogoName no longer" >&2
		echo "  writes an alternate logo back, so the scan has stopped matching" >&2
		exit 1
		;;
	unheld)
		echo "check-list-reader-lock.sh: CPictureViewer::GetLogoName writes back" >&2
		echo "  into a channel without CServiceManager::ChannelGuard in front" >&2
		echo "  of the lookup" >&2
		exit 1
		;;
esac

# ------------------------------------------------------------ the catalogue

body_of "$tmp/localize" '^CLocaleManager::loadLocale_ret_t CLocaleManager::loadLocale[(]' > "$tmp/load2"
[ -s "$tmp/load2" ] || {
	echo "check-list-reader-lock.sh: no CLocaleManager::loadLocale definition in" >&2
	echo "  $LOCALIZE" >&2
	exit 1
}
verdict "`reading_of "$tmp/load2" 'catalog_mutex' 'free'`" \
	"CLocaleManager::loadLocale"

body_of "$tmp/localize" '^std::string CLocaleManager::getString[(]' > "$tmp/getstring"
[ -s "$tmp/getstring" ] || {
	echo "check-list-reader-lock.sh: no CLocaleManager::getString definition in" >&2
	echo "  $LOCALIZE" >&2
	exit 1
}
verdict "`reading_of "$tmp/getstring" 'catalog_mutex' 'localeData['`" \
	"CLocaleManager::getString"

# The pointer returning accessor outlives no lock, so the adapter that answers a
# label on a request's own thread must not use it.
if grep -q 'getText(' "$tmp/localesrc"; then
	echo "check-list-reader-lock.sh: the locale adapter hands on a pointer from" >&2
	echo "  getText, which a language being loaded frees under it" >&2
	exit 1
fi
if ! grep -q 'getString(' "$tmp/localesrc"; then
	echo "check-list-reader-lock.sh: the locale adapter no longer reads through" >&2
	echo "  getString, so the scan has stopped matching" >&2
	exit 1
fi

exit 0
