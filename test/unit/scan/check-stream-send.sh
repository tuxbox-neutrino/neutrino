#!/bin/sh
# How the stream server sends to its clients. Every client's socket gets the
# same send buffer, the one that starts a stream and each one that joins a
# stream already running: the reader does not wait for a client, so the
# buffer is all the room a client has.
#
# The unit cases drive the helper on a socket; this holds the program's code
# to calling it. The stream server cannot be linked into the test binary, so
# it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-stream-send.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
for f in src/driver/streamts.cpp; do
	[ -r "$SRC/$f" ] || { echo "check-stream-send.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-stream-send.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
strip() {
	awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$1"
}
# The block that starts at the first line holding the text given, up to the
# brace that closes it; for member functions written inside a class.
block() {
	awk -v start="$1" '
		!f && index($0, start) { f = 1 }
		f {
			print
			o = gsub(/\{/, "{"); c = gsub(/\}/, "}")
			depth += o - c
			if (o) opened = 1
			if (opened && depth <= 0) exit
		}
	'
}

strip "$SRC/src/driver/streamts.cpp" > "$tmp/streamts"

# 1. Both ways a client comes to a stream give its socket the send buffer:
#    the setup that builds a stream, and the join of a running one, before
#    the client is added and the reader sends to it.
block 'void *buildLocked(' < "$tmp/streamts" > "$tmp/build"
block 'void joinLocked(' < "$tmp/streamts" > "$tmp/join"
[ -s "$tmp/build" ] || { echo "check-stream-send.sh: no buildLocked in src/driver/streamts.cpp" >&2; exit 1; }
[ -s "$tmp/join" ] || { echo "check-stream-send.sh: no joinLocked in src/driver/streamts.cpp" >&2; exit 1; }
grep -q -E 'streamsetup::setSendBuffer\([[:space:]]*fd[[:space:]]*,[[:space:]]*SEND_BUFFER_SIZE[[:space:]]*\)' "$tmp/build" || {
	echo "src/driver/streamts.cpp: the setup that builds a stream does not give the client's socket the send buffer" >&2
	fail=1
}
s=`grep -n -E 'streamsetup::setSendBuffer\([[:space:]]*fd[[:space:]]*,[[:space:]]*SEND_BUFFER_SIZE[[:space:]]*\)' "$tmp/join" | head -1 | cut -d: -f1`
a=`grep -n -F 'AddClient(fd)' "$tmp/join" | head -1 | cut -d: -f1`
if [ -z "$s" ] || [ -z "$a" ] || [ "$s" -gt "$a" ]; then
	echo "src/driver/streamts.cpp: a client that joins a running stream has to get the send buffer before it is added (buffer ${s:-none}, add ${a:-none})" >&2
	fail=1
fi
if grep -n -F 'SO_SNDBUF' "$tmp/streamts" > "$tmp/hits"; then
	echo "src/driver/streamts.cpp sets SO_SNDBUF itself; streamsetup::setSendBuffer is the one place:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi

exit $fail
