#!/bin/sh
# A stream never changes what the box shows. The stream server takes the
# frontend zapit holds for it, on zapit's thread, or none: it does not zap,
# stop a PiP, allocate, lock or unlock a frontend itself, and it does not
# reach into the frontend manager at all. zapit's hold for a stream does not
# zap the picture, stop a PiP, allocate a frontend the way a zap or a
# recording does, set the live frontend or announce anything: it binds the
# frontend the admission chose, or the picture's own without retuning it.
# zapit's yield for a recording does none of these either: it takes stream
# holds back, and the recording does the rest. Only zapit's thread enters,
# collects, takes back and closes the hold register; any other thread only
# adopts and drops.
#
# The unit cases drive the hold and the setup over models; this holds the
# program's code to staying inside them. Neither the stream server nor zapit
# can be linked into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-stream-keeps-picture.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/driver/streamts.cpp src/zapit/zapit.cpp src/zapit/streamholdcmd.cpp src/zapit/streamyield.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-stream-keeps-picture.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-stream-keeps-picture.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
strip() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1"
}

# 1. The stream server
strip "$SRC/src/driver/streamts.cpp" > "$tmp/streamts"
for word in 'zapTo_' 'stopPip' 'allocateFE' 'lockFrontend' 'unlockFrontend' 'CFEManager::getInstance'; do
	if grep -n -F -- "$word" "$tmp/streamts" > "$tmp/hits"; then
		echo "src/driver/streamts.cpp uses $word; a stream takes the frontend zapit holds for it:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done

# 2. zapit's hold and yield: the commands, the steps they run over and the
#    order they run them in
strip "$SRC/src/zapit/zapit.cpp" | awk '
	/^class CZapit::StreamHoldOps/ { inside = 1 }
	/^void CZapit::HoldForStream\(/ { inside = 1 }
	/^class CZapit::StreamYieldOps/ { inside = 1 }
	/^void CZapit::YieldStreams\(/ { inside = 1 }
	inside { print }
	inside && /^};?$/ { inside = 0 }
' > "$tmp/hold"
strip "$SRC/src/zapit/streamholdcmd.cpp" >> "$tmp/hold"
strip "$SRC/src/zapit/streamyield.cpp" >> "$tmp/hold"
for what in 'class CZapit::StreamHoldOps' 'void CZapit::HoldForStream(' 'class CZapit::StreamYieldOps' 'void CZapit::YieldStreams('; do
	grep -q -F "$what" "$tmp/hold" || { echo "check-stream-keeps-picture.sh: no $what in src/zapit/zapit.cpp" >&2; exit 1; }
done
for word in 'allocateFE' 'StopPip' 'ZapIt(' 'setLiveFE' 'SendEvent'; do
	if grep -n -F -- "$word" "$tmp/hold" > "$tmp/hits"; then
		echo "zapit's hold or yield for a stream uses $word; it may only bind the frontend the admission chose, or take holds back:" >&2
		sed 's/^/  /' "$tmp/hits" >&2
		fail=1
	fi
done

# 3. Only zapit's thread writes the register's places.
( cd "$SRC" && grep -r -l -E 'streamhold::(offer|collect|shutdown|revoke)[ \t]*\(' src --include='*.cpp' --include='*.h' ) | sort > "$tmp/writers" || true
if [ "`cat "$tmp/writers"`" != "src/zapit/zapit.cpp" ]; then
	echo "streamhold::offer, collect, revoke and shutdown belong to zapit's thread; called from:" >&2
	sed 's/^/  /' "$tmp/writers" >&2
	fail=1
fi

exit $fail
