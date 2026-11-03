#!/bin/sh
# How the stream server sends to its clients. Every client's socket gets the
# same send buffer, the one that starts a stream and each one that joins a
# stream already running: the reader does not wait for a client, so the
# buffer is all the room a client has. What goes out stays on the packet
# grid: both readers hand on whole packets only, through a collector, and
# every client is sent to through tsgrid, which keeps the rest of a packet a
# client took part of and otherwise drops whole packets. That state lives
# under the stream's lock, which the send holds over all clients, and a
# client that is removed takes its state with it, so a descriptor that is
# closed and handed to a new client never gets the old one's bytes.
#
# The unit cases drive the helper on a socket and tsgrid over a scripted
# send; this holds the program's code to using them. The stream server
# cannot be linked into the test binary, so it is checked as text.
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

# body: a function defined at the top level, up to its closing brace.
body() {
	awk -v start="$1" 'index($0, start) == 1 { f = 1 } f { print } f && /^}/ { exit }'
}

# 2. Every client is sent to through tsgrid, under the stream's lock held
#    over all of them, and nothing is printed while it is held.
body 'bool CStreamInstance::Send(' < "$tmp/streamts" > "$tmp/send"
[ -s "$tmp/send" ] || { echo "check-stream-send.sh: no CStreamInstance::Send in src/driver/streamts.cpp" >&2; exit 1; }
awk '
	{ line = $0 }
	/ScopedLock<OpenThreads::Mutex>/ && !lockdepth { lockdepth = depth; inlock = 1 }
	{ o = gsub(/\{/, "{"); c = gsub(/\}/, "}"); depth += o - c }
	inlock { print "L " line } !inlock { print "U " line }
	inlock && depth < lockdepth { inlock = 0 }
' "$tmp/send" > "$tmp/sendlock"
grep -q -E '^L .*clients\.send\([[:space:]]*fds[[:space:]]*,' "$tmp/sendlock" || {
	echo "src/driver/streamts.cpp: CStreamInstance::Send does not send to the clients through tsgrid with the stream's lock held" >&2
	fail=1
}
if grep -n -E '^L .*(printf|perror|fprintf|dprintf)' "$tmp/sendlock" > "$tmp/hits"; then
	echo "src/driver/streamts.cpp: CStreamInstance::Send prints while it holds the stream's lock:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi
if grep -n -E '(^|[^.>[:alnum:]_])send[[:space:]]*\(|mutex\.(un)?lock\(|cfds' "$tmp/send" > "$tmp/hits"; then
	echo "src/driver/streamts.cpp: CStreamInstance::Send sends past tsgrid or copies the clients out of the lock:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi

# 3. A client that is removed takes its state with it before its
#    descriptor is closed; a stream that closes all its clients drops all.
body 'void CStreamInstance::RemoveClient(' < "$tmp/streamts" > "$tmp/remove"
f=`grep -n -F 'clients.forget(clientfd)' "$tmp/remove" | head -1 | cut -d: -f1`
c=`grep -n -F 'close(clientfd)' "$tmp/remove" | head -1 | cut -d: -f1`
if [ -z "$f" ] || [ -z "$c" ] || [ "$f" -gt "$c" ]; then
	echo "src/driver/streamts.cpp: CStreamInstance::RemoveClient has to forget the client's state before it closes the descriptor (forget ${f:-none}, close ${c:-none})" >&2
	fail=1
fi
body 'void CStreamInstance::Close(' < "$tmp/streamts" | grep -q -F 'clients.clear()' || {
	echo "src/driver/streamts.cpp: CStreamInstance::Close closes the clients and keeps their state" >&2
	fail=1
}

# 4. The demux reader reads behind the part of a packet it holds and hands
#    on the whole packets the collector counts.
body 'void CStreamInstance::run(' < "$tmp/streamts" > "$tmp/run"
grep -q -E 'dmx->Read\([[:space:]]*buf[[:space:]]*\+[[:space:]]*grid\.carry\(\)[[:space:]]*,[[:space:]]*IN_SIZE[[:space:]]*-[[:space:]]*grid\.carry\(\)' "$tmp/run" || {
	echo "src/driver/streamts.cpp: CStreamInstance::run does not read behind the part of a packet it holds" >&2
	fail=1
}
grep -q -E 'whole[[:space:]]*=[[:space:]]*grid\.add\([[:space:]]*r[[:space:]]*\)' "$tmp/run" &&
grep -q -E 'Send\([[:space:]]*whole[[:space:]]*\)' "$tmp/run" &&
grep -q -E 'grid\.next\([[:space:]]*buf[[:space:]]*\)' "$tmp/run" || {
	echo "src/driver/streamts.cpp: CStreamInstance::run does not hand on the whole packets the collector counts" >&2
	fail=1
}
if grep -n -E 'Send\([[:space:]]*r[[:space:]]*\)' "$tmp/run" > "$tmp/hits"; then
	echo "src/driver/streamts.cpp: CStreamInstance::run sends a read as it came:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi

# 5. ffmpeg's output goes through the collector with its own buffer; the
#    avio buffer itself is not sent from.
awk '/^int CStreamStream::write_packet\(/ { f = 1 } f { print } f && /^}/ { exit }' "$tmp/streamts" > "$tmp/write"
[ -s "$tmp/write" ] || { echo "check-stream-send.sh: no CStreamStream::write_packet in src/driver/streamts.cpp" >&2; exit 1; }
grep -q -E 'collector\.feed\(' "$tmp/write" || {
	echo "src/driver/streamts.cpp: CStreamStream::write_packet does not go through the collector" >&2
	fail=1
}
if grep -n -F 'Send(' "$tmp/write" > "$tmp/hits"; then
	echo "src/driver/streamts.cpp: CStreamStream::write_packet sends what ffmpeg wrote as it came:" >&2
	sed 's/^/  /' "$tmp/hits" >&2
	fail=1
fi

exit $fail
