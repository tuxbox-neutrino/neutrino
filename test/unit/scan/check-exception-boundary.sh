#!/bin/sh
# nhttpd is built without exceptions, the API layer under src/httpd with them.
# An exception that leaves the layer into an nhttpd frame ends the process, so
# every function nhttpd calls in the layer directly catches everything before
# it returns. Allocation is the case that matters: a failed new throws, and the
# layer allocates on nearly every path.
#
# Two properties:
#
#  - nhttpd calls into the layer through the names listed below and no others.
#    A new direct call fails here until its target is written with a barrier
#    and added to the list.
#  - a type of the layer nhttpd builds is a call as well, its constructor and
#    destructor running in an nhttpd frame. Only the types listed below may be
#    built there, each one whose constructor and destructor cannot throw.
#  - the hooks are reached through the Cyhook interface and carry their own
#    barriers, held to the same rule below. The one thing a hook may do ahead
#    of its try is ask claims() whether the request is its own: a lookup in
#    a map by a three-letter key, short enough never to allocate, and a call
#    into nhttpd. Hook_PrepareResponse does nothing else and has no barrier.
#  - each listed function that can allocate opens with try and has a
#    catch (...). setBoundPortFromBridge and boundPort only store and read
#    an int and registerPrefixes only calls back into nhttpd, so none of
#    them needs one.
#
# Read as text: the layer's entry points and nhttpd link into different
# objects, and the question is where the braces are, not what runs.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC/src/nhttpd" ] && [ -d "$SRC/src/httpd" ] || {
	echo "usage: check-exception-boundary.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-exception-boundary.sh: cannot read $f" >&2; exit 1; }
done

ALLOWED='httpd::compat::knowName httpd::compat::record httpd::setBoundPortFromBridge httpd::boundPort httpd::scheduleReloadAndRestart httpd::saveServerFile httpd::workingFile CApiBridge::routesOk CApiBridge::registerPrefixes CApiBridge::startStreams CApiBridge::stopStreams'

# name, file that defines it; each of these opens with try
GUARDED='knowName src/httpd/stubs/legacy_usage.cpp
record src/httpd/stubs/legacy_usage.cpp
scheduleReloadAndRestart src/httpd/stubs/server_nhttpd.cpp
reloadRunner src/httpd/stubs/server_nhttpd.cpp
saveServerFile src/httpd/webconfig.cpp
workingFile src/httpd/webconfig.cpp
CApiBridge::routesOk src/httpd/nhttpd_bridge.cpp
CApiBridge::startStreams src/httpd/nhttpd_bridge.cpp
CApiBridge::stopStreams src/httpd/nhttpd_bridge.cpp
CApiBridge::Hook_SendResponse src/httpd/nhttpd_bridge.cpp
CApiBridge::Hook_SendStream src/httpd/nhttpd_bridge.cpp'

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

prepare() {
	awk -v keepstrings=0 -f "$STRIP" "$1" | awk -f "$BLANK"
}

# ------------------------------------------------ the calls out of nhttpd
: > "$tmp/calls"
find "$SRC/src/nhttpd" \( -name '*.cpp' -o -name '*.h' \) | sort | while read -r f; do
	# a name of its own, not the tail of one: Cyhttpd:: is nhttpd's
	prepare "$f" | grep -o -E '(^|[^A-Za-z_0-9:])(httpd::[A-Za-z_:]*[A-Za-z_]|CApiBridge::[A-Za-z_]+)[ \t]*\(' \
		| sed -E 's/^[^A-Za-z_]//; s/[ \t]*\($//' >> "$tmp/calls" || true
done
sort -u "$tmp/calls" > "$tmp/called"

# httpd::ServerFileLock locks and unlocks a mutex that load() builds on the
# program's way up, before nhttpd runs, so nothing in it allocates.
TYPES='httpd::ServerFileLock'
: > "$tmp/types"
find "$SRC/src/nhttpd" \( -name '*.cpp' -o -name '*.h' \) | sort | while read -r f; do
	prepare "$f" | grep -o -E '(^|[^A-Za-z_0-9:])httpd::[A-Z][A-Za-z_0-9]*' \
		| sed -E 's/^[^A-Za-z_]//' >> "$tmp/types" || true
done
sort -u "$tmp/types" | while read -r name; do
	case " $TYPES " in
	*" $name "*) ;;
	*)
		echo "check-exception-boundary.sh: nhttpd builds $name, which is not a listed" >&2
		echo "  type whose constructor and destructor cannot throw" >&2
		exit 1
		;;
	esac
done || exit 1
if [ ! -s "$tmp/called" ]; then
	echo "check-exception-boundary.sh: no call from nhttpd into the layer found," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi
bad=0
while read -r name; do
	case " $ALLOWED " in
	*" $name "*) ;;
	*)
		echo "check-exception-boundary.sh: nhttpd calls $name directly, which is" >&2
		echo "  not a listed entry with an exception barrier" >&2
		bad=1
		;;
	esac
done < "$tmp/called"
[ "$bad" -eq 0 ] || exit 1

# ------------------------------------------------ the barriers
printf '%s\n' "$GUARDED" | while read -r name file; do
	[ -r "$SRC/$file" ] || { echo "check-exception-boundary.sh: cannot read $SRC/$file" >&2; exit 1; }
	prepare "$SRC/$file" | awk -v name="$name" -v file="$file" '
		BEGIN { pat = name; gsub(/:/, "[:]", pat) }
		!found && $0 ~ ("(^|[^A-Za-z_:])" pat "[ \t]*\\(") && $0 !~ /;[ \t]*$/ { found = 1 }
		found && !body {
			if (index($0, "{")) { body = 1; depth = 0 }
		}
		found && body {
			n = length($0)
			for (i = 1; i <= n; i++) {
				c = substr($0, i, 1)
				if (c == "{") depth++
				else if (c == "}") depth--
			}
			text = text " " $0
			if (depth == 0) {
				t = text
				sub(/^[^{]*\{[ \t]*/, "", t)
				sub(/^if[ \t]*\([ \t]*![ \t]*claims[ \t]*\([ \t]*hh[ \t]*\)[ \t]*\)[ \t]*return[ \t]+HANDLED_NONE[ \t]*;[ \t]*/, "", t)
				if (t !~ /^try[ \t]*\{/) {
					printf "check-exception-boundary.sh: %s in %s does not open with try\n", name, file > "/dev/stderr"
					exit 1
				}
				if (text !~ /catch[ \t]*\([ \t]*\.\.\.[ \t]*\)/) {
					printf "check-exception-boundary.sh: %s in %s has no catch (...)\n", name, file > "/dev/stderr"
					exit 1
				}
				exit 0
			}
		}
		END {
			if (!found) {
				printf "check-exception-boundary.sh: no definition of %s in %s\n", name, file > "/dev/stderr"
				exit 1
			}
		}
	' || exit 1
done

exit 0
