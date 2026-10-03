#!/bin/sh
# The channel lock is not one a thread may take twice: a hold of it that calls
# a function which takes it again stops that thread for good. The renumbering
# holds it for a whole walk and runs at every bouquet load, start-up included,
# so such a call there would stop the box from starting.
#
# What this holds the tree to: no hold of the channel lock takes it again, by a
# second guard or by calling a function that takes the lock itself, and no hold
# waits on another thread - no call to the
# channel daemon's client, no join of a thread, no exchange over a socket - for
# a thread that waits on this lock while the hold waits on it would be the same
# stop by two threads. And the other way round: a function whose name ends in
# Locked, which the scans trust to be called with the lock held, is called only
# from a hold, or from one of the two functions named below whose callers hold
# it.
#
# The functions that take the lock are read out of the files that define them,
# members and free functions alike, so one that starts taking it is held to
# this without being written down here; their Locked forms, for a caller
# holding the lock already, do not count. A function there that calls one of
# them takes the lock as well, and so on, so a hold calling it is found too.
# Calls through another file than those, or through a pointer, are not seen.
#
# None of this can be linked into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-channel-lock-reentry.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
for f in "$HERE/strip-comments.awk" "$HERE/blank-if0.awk" "$HERE/channel-holds.awk"; do
	[ -r "$f" ] || { echo "check-channel-lock-reentry.sh: cannot read $f" >&2; exit 1; }
done

# Where the lock-taking functions are defined, and where the lock is held.
DEFINERS="src/zapit/getservices.cpp src/zapit/bouquets.cpp src/zapit/zapit.cpp \
	src/zapit/scanpmt.cpp src/driver/pictureviewer/pictureviewer.cpp"
HOLDERS="src/zapit/getservices.cpp src/zapit/bouquets.cpp src/zapit/zapit.cpp \
	src/zapit/scanpmt.cpp src/zapit/scan.cpp src/zapit/scansdt.cpp src/zapit/fastscan.cpp \
	src/driver/scanepg.cpp src/driver/pictureviewer/pictureviewer.cpp \
	src/gui/channellist.cpp src/gui/bedit/bouqueteditor_bouquets.cpp \
	src/gui/bedit/bouqueteditor_channels.cpp src/coreapi/box/channelsource_real.cpp \
	src/neutrino.cpp"

# Functions that call a Locked form outside a hold of their own, because every
# way to them holds the lock: ParseChannels is reached only from the service
# load's hold, through FindTransponder and ParseTransponders, and
# forgetBorrowedChannels is handed to that load, which calls it in its hold.
CONTEXT_HELD="src/zapit/getservices.cpp:ParseChannels src/zapit/zapit.cpp:forgetBorrowedChannels"
for f in $DEFINERS $HOLDERS; do
	[ -r "$SRC/$f" ] || { echo "check-channel-lock-reentry.sh: cannot read $SRC/$f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
holds() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1" | awk -f "$HERE/blank-if0.awk" | \
		awk -f "$HERE/channel-holds.awk"
}

# Every function whose body takes the lock, by the name it is called under, and
# every one that calls such a function, until nothing more is added.
for f in $DEFINERS; do
	holds "$SRC/$f"
done | awk -F '\t' '
	$4 != "" {
		fn = $4
		text = $0
		sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text)
		if (fn !~ /Locked$/ &&
		    (index(text, "CServiceManager::ChannelGuard") > 0 || text ~ /(^|[^A-Za-z_0-9])LockChannels[ \t]*\(/ ||
		     (index(text, "channels_mutex") > 0 && (index(text, "ScopedLock") > 0 || index(text, ".lock(") > 0))))
			taking[fn] = 1
		if (text ~ /^[A-Za-z_~]/ && text !~ /;[ \t]*$/)
			next
		rest = text
		while (match(rest, /[A-Za-z_][A-Za-z_0-9]*[ \t]*\(/)) {
			name = substr(rest, RSTART, RLENGTH)
			before = (RSTART > 1) ? substr(rest, RSTART - 1, 1) : ""
			rest = substr(rest, RSTART + RLENGTH)
			sub(/[ \t]*\($/, "", name)
			if (before !~ /[A-Za-z_0-9]/ && name != fn)
				calls[fn, name] = 1
		}
	}
	END {
		do {
			added = 0
			for (pair in calls) {
				split(pair, p, SUBSEP)
				if ((p[2] in taking) && !(p[1] in taking) && p[1] !~ /Locked$/) {
					taking[p[1]] = 1
					added = 1
				}
			}
		} while (added)
		for (fn in taking)
			print fn
	}
' | sort -u > "$tmp/taking"

# The tree held more than twenty of them when this was written; fewer means the
# reading has stopped matching.
n=`awk 'END { print NR }' "$tmp/taking"`
if [ "$n" -lt 20 ]; then
	echo "check-channel-lock-reentry.sh: only $n functions that take the channel lock" >&2
	echo "  were read out of the source, so the scan has stopped matching" >&2
	exit 1
fi
for want in addService removeService addBouquet deleteBouquet ResetChannelNumbers setBouquetLock \
		toggleBouquetLock renumServices; do
	grep -qx "$want" "$tmp/taking" || {
		echo "check-channel-lock-reentry.sh: $want is not among the functions read as taking" >&2
		echo "  the channel lock, so the scan has stopped matching" >&2
		exit 1
	}
done

bad=0
for f in $HOLDERS; do
	holds "$SRC/$f" > "$tmp/marked"
	awk -F '\t' -v file="$f" -v takers="$tmp/taking" -v context="$CONTEXT_HELD" '
		BEGIN {
			while ((getline name < takers) > 0) taking[name] = 1
			n = split(context, c, " ")
			for (i = 1; i <= n; i++) held_by_caller[c[i]] = 1
		}
		$3 == 1 && (index($0, "CServiceManager::ChannelGuard") > 0 || $0 ~ /(^|[^A-Za-z_0-9])LockChannels[ \t]*\(/ ||
		            (index($0, "channels_mutex") > 0 && (index($0, "ScopedLock") > 0 || index($0, ".lock(") > 0))) {
			text = $0
			sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text)
			print "  " file ":" $1 ": takes the lock again inside a hold of it: " text
		}
		# A Locked form called on a line no hold covers, outside the functions
		# whose callers hold the lock. A definition line names, it does not call.
		$2 == 0 && !((file ":" $4) in held_by_caller) {
			text = $0
			sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text)
			if (text ~ /^[A-Za-z_~]/ && text !~ /;[ \t]*$/)
				next
			rest = text
			while (match(rest, /[A-Za-z_][A-Za-z_0-9]*Locked[ \t]*\(/)) {
				name = substr(rest, RSTART, RLENGTH)
				before = (RSTART > 1) ? substr(rest, RSTART - 1, 1) : ""
				rest = substr(rest, RSTART + RLENGTH)
				sub(/[ \t]*\($/, "", name)
				if (before !~ /[A-Za-z_0-9]/)
					print "  " file ":" $1 ": calls " name " outside a hold of the lock: " text
			}
		}
		$2 == 1 {
			text = $0
			sub(/^[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t/, "", text)
			# A definition line names the function, it does not call it.
			if (text ~ /^[A-Za-z_~]/ && text !~ /;[ \t]*$/)
				next
			rest = text
			while (match(rest, /[A-Za-z_][A-Za-z_0-9]*[ \t]*\(/)) {
				name = substr(rest, RSTART, RLENGTH)
				before = (RSTART > 1) ? substr(rest, RSTART - 1, 1) : ""
				rest = substr(rest, RSTART + RLENGTH)
				sub(/[ \t]*\($/, "", name)
				if (before ~ /[A-Za-z_0-9]/)
					continue
				if (name in taking)
					print "  " file ":" $1 ": calls " name " under the lock: " text
			}
			if (text ~ /g_Zapit->|Zapit->|(^|[^A-Za-z_0-9]|pthread_)join[ \t]*\(|LogoStop[ \t]*\(|send_data[ \t]*\(|receive_[a-z]+[ \t]*\(/)
				print "  " file ":" $1 ": waits on another thread under the lock: " text
		}
	' "$tmp/marked" >> "$tmp/findings"
done

if [ -s "$tmp/findings" ]; then
	echo "check-channel-lock-reentry.sh: a hold of the channel lock calls what takes it again" >&2
	echo "  or waits on another thread, or a Locked form is called outside a hold:" >&2
	cat "$tmp/findings" >&2
	bad=1
fi

exit $bad
