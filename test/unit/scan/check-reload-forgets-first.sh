#!/bin/sh
# A full reload of the services destroys every CZapitChannel the map owns.
# The bouquets and the application's channel lists hold borrowed pointers into
# that map, and readers on other threads walk them under the channel lock. If
# the lists are emptied only after the reload gave the lock back, a reader that
# takes it in between walks freed channels.
#
# So LoadServices empties everything that borrows from the map through the hook
# its caller hands it, under the lock it holds, and before the map is cleared;
# and the caller that reloads everything hands it one. That hook empties both
# kinds of borrower, the bouquets and the application's lists, and runs under
# a lock that is not recursive: neither it nor what it calls may take the lock
# again, except where the caller says it is not held.
#
# Checked as text: neither file links into the test binary.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-reload-forgets-first.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
SERVICES="$SRC/src/zapit/getservices.cpp"
ZAPIT="$SRC/src/zapit/zapit.cpp"
for f in "$SERVICES" "$ZAPIT" "$HERE/strip-comments.awk" "$HERE/blank-if0.awk"; do
	[ -r "$f" ] || { echo "check-reload-forgets-first.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
prepare() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1" | awk -f "$HERE/blank-if0.awk"
}

# In LoadServices: the line numbers of the lock, the hook and the clear.
prepare "$SERVICES" | awk '
	/^bool CServiceManager::LoadServices[(]/ { inside = 1 }
	inside && /ScopedLock<OpenThreads::Mutex>[ \t]+lock[(]channels_mutex[)]/ && !lock { lock = NR }
	inside && /forget_borrowed[ \t]*[(][ \t]*[)]/ && !hook { hook = NR }
	inside && /allchans[.]clear[(][)]/ && !clear { clear = NR }
	END { printf "%d %d %d\n", lock, hook, clear }
' > "$tmp/lines"
read lock hook clear < "$tmp/lines"
if [ "$lock" -eq 0 ] || [ "$clear" -eq 0 ]; then
	echo "check-reload-forgets-first.sh: LoadServices no longer reads as expected" >&2
	echo "  (lock at $lock, clear at $clear): the scan has stopped matching" >&2
	exit 1
fi
if [ "$hook" -eq 0 ] || [ "$hook" -lt "$lock" ] || [ "$hook" -gt "$clear" ]; then
	echo "check-reload-forgets-first.sh: LoadServices does not empty what borrows" >&2
	echo "  from the map under its lock before it clears the map" >&2
	echo "  (lock at $lock, hook at $hook, clear at $clear)" >&2
	exit 1
fi

# The full reload hands it the hook.
hookname=`prepare "$ZAPIT" | tr '\n' ' ' | grep -o -E 'LoadServices[(][ \t]*false[ \t]*,[ \t]*&?[A-Za-z_]+[ \t]*[)]' \
	| head -n 1 | sed -E 's/.*,[ \t]*&?([A-Za-z_]+)[ \t]*[)]$/\1/'`
if [ -z "$hookname" ]; then
	echo "check-reload-forgets-first.sh: the full reload in zapit.cpp hands" >&2
	echo "  LoadServices no hook to empty what borrows from the map" >&2
	exit 1
fi

# The body of a function defined in a file, braces counted, one line.
body() {
	prepare "$1" | awk -v want="$2" '
		!found && index($0, want "(") > 0 && $0 !~ /;[ \t]*$/ { found = 1 }
		found {
			print
			n = length($0)
			for (i = 1; i <= n; i++) {
				c = substr($0, i, 1)
				if (c == "{") { depth++; seen = 1 }
				else if (c == "}") { depth--; if (seen && depth == 0) exit }
			}
		}
	' | tr -d '[:space:]'
}

relocks() {
	case "$1" in
		*LockChannels*|*ChannelGuard*|*channels_mutex*) return 0 ;;
	esac
	return 1
}

bad=0
hook=`body "$ZAPIT" "$hookname"`
case "$hook" in
	*"forgetChannelsLocked()"*) ;;
	*) echo "check-reload-forgets-first.sh: $hookname does not empty the bouquets" >&2; bad=1 ;;
esac
case "$hook" in
	*"invalidateChannelLists(true)"*) ;;
	*) echo "check-reload-forgets-first.sh: $hookname does not empty the application's" >&2
	   echo "  lists, or does not say that the lock is held" >&2; bad=1 ;;
esac
if relocks "$hook"; then
	echo "check-reload-forgets-first.sh: $hookname takes the channel lock it runs under" >&2
	bad=1
fi
forget=`body "$SRC/src/zapit/bouquets.cpp" "void CBouquetManager::forgetChannelsLocked"`
if [ -z "$forget" ] || relocks "$forget"; then
	echo "check-reload-forgets-first.sh: forgetChannelsLocked is missing or takes the lock" >&2
	bad=1
fi
case "$forget" in
	*"tvChannels.clear();"*"radioChannels.clear();"*) ;;
	*)
		echo "check-reload-forgets-first.sh: forgetChannelsLocked does not empty the" >&2
		echo "  bouquets' channel lists" >&2
		bad=1
		;;
esac
# invalidateChannelLists takes the lock only where the caller did not.
lists=`body "$SRC/src/neutrino.cpp" "void CNeutrinoApp::invalidateChannelLists"`
rest=`printf '%s' "$lists" | sed -e 's/if(!lock_held)CServiceManager::getInstance()->LockChannels();//g' \
	-e 's/if(!lock_held)CServiceManager::getInstance()->UnlockChannels();//g'`
if [ -z "$lists" ] || relocks "$rest"; then
	echo "check-reload-forgets-first.sh: invalidateChannelLists is missing or takes the" >&2
	echo "  lock although its caller holds it" >&2
	bad=1
fi

exit $bad
