#!/bin/sh
# Every bouquet change the web layer makes is sent to the channel daemon over a
# socket, and the client method that sends it answers whether it got there. A
# command that was never written is not a change: with the daemon's socket gone the
# box goes on running, and an adapter that read nothing back answered that a bouquet
# had been made while the list was untouched.
#
# The adapter cannot be linked into the test binary, because it needs the whole
# application. What it does with a command that did not land is checked here as
# text, which is why check-bouquet-rename.sh reads the same file.
#
# The rule is one line: every one of these calls stands in a refusal of its own and
# answers Internal, the same answer the file gives for a channel stack that is not
# there at all.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-zapit-write-checked.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
ADAPTER="$SRC/src/coreapi/box/channelsource_real.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$ADAPTER" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-zapit-write-checked.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$ADAPTER" | awk -f "$BLANK" > "$tmp/adapter"
[ -s "$tmp/adapter" ] || {
	echo "check-zapit-write-checked.sh: nothing left of $ADAPTER after stripping" >&2
	exit 1
}

# The ten the channel stack is asked to change something with. getMode is not
# among them on purpose: the adapter sends it to wait for the commands before
# it, and it changes nothing, so what it answers is not a change that did or
# did not happen.
WRITES="addBouquet renameBouquet moveBouquet deleteBouquet setBouquetHidden \
	setBouquetLock addChannelToBouquet removeChannelFromBouquet \
	saveBouquets renumChannellist"

awk -v names="$WRITES" '
	BEGIN {
		wanted = split(names, want, /[ \t\n]+/)
		for (i = 1; i <= wanted; i++)
			seen[want[i]] = 0
	}
	{ line[NR] = $0 }
	END {
		bad = 0
		for (i = 1; i <= NR; i++) {
			for (j = 1; j <= wanted; j++) {
				name = want[j]
				if (name == "")
					continue
				if (index(line[i], "g_Zapit->" name "(") == 0)
					continue
				seen[name]++
				if (line[i] !~ /if \(!g_Zapit->/) {
					printf "  %d: %s\n", i, line[i]
					print "    sends " name " without reading whether it landed"
					bad++
					continue
				}
				if (line[i + 1] !~ /return Status::Internal;/) {
					printf "  %d: %s\n", i, line[i]
					print "    refuses " name " with something other than Internal"
					bad++
				}
			}
		}
		# A rule with nothing to hold is a rule that cannot fail, so a name the
		# adapter stopped sending is as much a finding as one it sends blind.
		for (j = 1; j <= wanted; j++) {
			name = want[j]
			if (name == "")
				continue
			if (seen[name] == 0) {
				print "    the adapter never sends " name
				bad++
			}
		}
		if (bad > 0)
			exit 1
		exit 0
	}
' "$tmp/adapter" > "$tmp/findings" || {
	echo "check-zapit-write-checked.sh: a bouquet command is sent without being checked" >&2
	cat "$tmp/findings" >&2
	exit 1
}

exit 0
