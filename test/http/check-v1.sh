#!/bin/sh
# check-v1.sh - HTTP-level checks of the REST bridge against a running neutrino
#
# usage: check-v1.sh <base-url> <openapi.json from openapi_dump> [before-dir]
#   base-url    e.g. http://127.0.0.1:31344
#   openapi.json the document the dump tool wrote from the same build
#   before-dir  optional: directory with legacy answers captured before the
#               change (control-getbouquets.txt, control-getversion.txt,
#               page.txt) to compare header+body byte for byte, the
#               three time headers excepted
#
# Prints one line per check, PASS or FAIL, and exits non-zero on any FAIL.
# Needs curl, gzip, cmp and python3 (for the raw-socket cases).

BASE="$1"; DOC="$2"; BEFORE="$3"
[ -n "$BASE" ] && [ -f "$DOC" ] || { echo "usage: $0 <base-url> <openapi.json> [before-dir]" >&2; exit 2; }
T=`mktemp -d`; trap 'rm -rf "$T"' EXIT
fails=0
pass() { echo "PASS $1"; }
fail() { echo "FAIL $1"; fails=`expr $fails + 1`; }
hdr() { grep -i "^$2:" "$1" | tr -d '\r' | sed 's/^[^:]*: *//'; }
status() { head -1 "$1" | tr -d '\r' | awk '{print $2}'; }
count() { grep -ic "^$2:" "$1"; }

# 1. system/info: JSON, api headers, exact length
curl -s -D "$T/h1" -o "$T/b1" "$BASE/api/v1/system/info"
[ "`status $T/h1`" = 200 ] && pass "GET system/info 200" || fail "GET system/info status `status $T/h1`"
echo "`hdr $T/h1 content-type`" | grep -q '^application/json' && pass "system/info content-type" || fail "system/info content-type `hdr $T/h1 content-type`"
[ "`count $T/h1 cache-control`" = 1 ] && [ "`hdr $T/h1 cache-control`" = "no-store" ] && pass "exactly one Cache-Control: no-store" || fail "Cache-Control lines: `count $T/h1 cache-control` (`hdr $T/h1 cache-control`)"
[ "`hdr $T/h1 x-content-type-options`" = "nosniff" ] && pass "nosniff" || fail "nosniff missing"
[ "`hdr $T/h1 content-length`" = "`wc -c < $T/b1`" ] && pass "Content-Length == body bytes" || fail "Content-Length `hdr $T/h1 content-length` vs `wc -c < $T/b1`"
head -1 "$T/h1" | grep -q '^HTTP/1.0 ' && pass "HTTP/1.0 status line" || fail "status line `head -1 $T/h1`"

# 2. HEAD: same headers, no body
curl -s -I -o "$T/h2" "$BASE/api/v1/system/info"
[ "`hdr $T/h2 content-length`" = "`hdr $T/h1 content-length`" ] && pass "HEAD states the GET length" || fail "HEAD length `hdr $T/h2 content-length`"

# 3. 404 problem with body and true length
curl -s -D "$T/h3" -o "$T/b3" "$BASE/api/v1/nothing"
[ "`status $T/h3`" = 404 ] && grep -q '"status":404' "$T/b3" && pass "404 problem+json body" || fail "404 body"
[ "`hdr $T/h3 content-length`" = "`wc -c < $T/b3`" ] && [ "`wc -c < $T/b3`" -gt 0 ] && pass "404 Content-Length is the body's" || fail "404 Content-Length `hdr $T/h3 content-length` vs `wc -c < $T/b3`"

# 4. 405 with Allow
curl -s -D "$T/h4" -o /dev/null -X DELETE "$BASE/api/v1/system/info"
[ "`status $T/h4`" = 405 ] && echo "`hdr $T/h4 allow`" | grep -q GET && pass "DELETE -> 405 with Allow" || fail "DELETE status `status $T/h4` allow `hdr $T/h4 allow`"

# 5. PUT with an empty JSON body is dispatched: the router refuses the
#    missing member with 400, which no legacy path and no 501 would say
curl -s -D "$T/h5" -o "$T/b5" -X PUT -H 'Content-Type: application/json' -d '{}' "$BASE/api/v1/bouquets/0/name"
[ "`status $T/h5`" = 400 ] && grep -q '"status":400' "$T/b5" && pass "PUT reaches the router (400 problem)" || fail "PUT status `status $T/h5`"

# 6. raw query reaches the router
curl -s -D "$T/h6" -o "$T/b6" "$BASE/api/v1/channels?x=%ZZ"
[ "`status $T/h6`" = 400 ] && grep -qi escape "$T/b6" && pass "bad escape -> 400" || fail "bad escape status `status $T/h6`"

# 7. trailing slash is not a route
curl -s -D "$T/h7" -o /dev/null "$BASE/api/v1/channels/"
[ "`status $T/h7`" = 404 ] && pass "trailing segment -> 404" || fail "trailing segment status `status $T/h7`"

# 8. document == dump, gzip on request
curl -s -o "$T/doc" "$BASE/api/openapi.json"
cmp -s "$T/doc" "$DOC" && pass "openapi.json == openapi_dump" || fail "openapi.json differs from dump"
curl -s -D "$T/h8" -o "$T/doc.gz" -H 'Accept-Encoding: gzip' "$BASE/api/openapi.json"
[ "`hdr $T/h8 content-encoding`" = gzip ] && gzip -dc "$T/doc.gz" | cmp -s - "$DOC" && pass "gzip document unpacks to the same bytes" || fail "gzip document"
[ "`wc -c < $T/doc.gz`" -lt "`wc -c < $T/doc`" ] && pass "gzip is smaller" || fail "gzip not smaller"

# 9. 2 MiB JSON POST -> 413, connection closed
head -c 2097152 /dev/zero | tr '\0' 'a' > "$T/big"
curl -s -D "$T/h9" -o "$T/b9" -X POST -H 'Content-Type: application/json' -H 'Expect:' --data-binary "@$T/big" "$BASE/api/v1/system/standby" 2>/dev/null
[ "`status $T/h9`" = 413 ] && pass "2 MiB body -> 413" || fail "2 MiB body status `status $T/h9`"

# raw-socket helper: send bytes, print the status line and how the peer
# ended the exchange (EOF, RESET, TIMEOUT) and the seconds it took
raw() {
	python3 - "$BASE" "$1" "$2" <<'PYEOF'
import socket, sys, time
base, req, wait = sys.argv[1], sys.argv[2].encode().decode('unicode_escape').encode(), float(sys.argv[3])
host = base.split('://', 1)[-1].split(':')[0]; port = int(base.rsplit(':', 1)[-1])
t0 = time.time()
s = socket.create_connection((host, port), timeout=wait)
s.sendall(req)
data, note = b'', 'EOF'
try:
	while True:
		c = s.recv(4096)
		if not c: break
		data += c
except ConnectionResetError: note = 'RESET'
except socket.timeout: note = 'TIMEOUT'
s.close()
print((data.split(b'\r\n', 1)[0].decode(errors='replace') if data else '(nothing)') + ' ' + note + ' %ds' % int(time.time() - t0))
PYEOF
}

# 10. half a body meets the deadline (about 10 s), not a hang
r10=`raw 'POST /api/v1/system/standby HTTP/1.0\r\nContent-Length: 100\r\nContent-Type: application/json\r\n\r\n{"on"' 30`
echo "$r10" | grep -q ' 408 ' && [ "`echo "$r10" | awk '{print $NF}' | tr -d s`" -lt 20 ] && pass "half body -> 408 ($r10)" || fail "half body: $r10"

# 11. legacy untouched: PUT/DELETE/TRACE outside the API are refused before
#     any hook runs. nhttpd has always answered these with a 501 line and an
#     immediate close without reading the rest of the request, which every
#     client sees as a reset; so the check is "a 501 line or a reset, and
#     never an answer from a hook".
for m in 'PUT /control/version' 'PUT /x' 'TRACE /api/v1/system/info'; do
	r=`raw "$m HTTP/1.0\r\nHost: box\r\nAccept: */*\r\n\r\n" 10`
	case "$r" in
		*" 501 "*|"(nothing) RESET"*) pass "$m refused before any hook ($r)";;
		*) fail "$m: $r";;
	esac
done
# (measured on the unchanged code: even a bare request line ends in a reset,
#  because the empty line after it is never read before the close)
# (/control/version serves a file the PC runtime does not have, so its body
#  is empty there; getmode always answers with one)
curl -s -o "$T/gm" "$BASE/control/getmode"; [ -s "$T/gm" ] && pass "/control/getmode answers" || fail "/control/getmode empty"
if [ -n "$BEFORE" ] && [ -d "$BEFORE" ]; then
	for f in control-getbouquets control-getmode page; do
		[ -f "$BEFORE/$f.url" ] || continue
		curl -s -D "$T/$f.h" -o "$T/$f.b" "$BASE`cat $BEFORE/$f.url`"
		# Date is now; Last-Modified is now for a dynamic answer; Expires is now
		# plus a day for a cached page. Everything else has to match.
		grep -viE '^(date|last-modified|expires):' "$T/$f.h" > "$T/$f.hh"; grep -viE '^(date|last-modified|expires):' "$BEFORE/$f.h" > "$T/$f.hb"
		cmp -s "$T/$f.hh" "$T/$f.hb" && cmp -s "$T/$f.b" "$BEFORE/$f.b" && pass "legacy $f byte-identical" || fail "legacy $f differs"
	done
fi

# 12. one read route per group slice 2a added: each answers JSON with the
#     API headers, or a problem the router built (netfs and config take a
#     name, so a missing one has to come back as the router's 404/400 and
#     never as anything nhttpd made up). daemons has no read route.
for r in epg?channel=0 settings/schema storage/mounts timers osd/volume tuner/signal plugins recordings 'storage/netfs/none' 'config/none'; do
	curl -s -D "$T/h12" -o "$T/b12" "$BASE/api/v1/$r"
	st=`status $T/h12`
	ct=`hdr $T/h12 content-type`
	case "$st:$ct" in
		200:application/json*|4*:application/problem+json*) pass "/api/v1/$r -> $st $ct";;
		*) fail "/api/v1/$r -> $st $ct";;
	esac
	[ "`count $T/h12 cache-control`" = 1 ] || fail "/api/v1/$r cache-control count"
done

# 13. a file answer comes out of its descriptor, one stretch of it where the
#     caller asked for one. /tmp is one of the shipped storage roots, so a
#     file written here is one the box will serve; random bytes, so the same
#     run also proves the descriptor path is binary-safe end to end.
RF="/tmp/check-v1-range.$$"
python3 -c "import sys,os; sys.stdout.buffer.write(os.urandom(100000))" > "$RF"
curl -s -D "$T/h13" -o "$T/b13" "$BASE/api/v1/storage/file?path=$RF"
if [ "`status $T/h13`" = 200 ]; then
	cmp -s "$RF" "$T/b13" && [ "`hdr $T/h13 content-length`" = 100000 ] \
		&& pass "storage file served whole, bytes identical" \
		|| fail "storage file differs or wrong length (`hdr $T/h13 content-length`)"

	curl -s -H 'Range: bytes=1000-1999' -D "$T/h13a" -o "$T/b13a" "$BASE/api/v1/storage/file?path=$RF"
	dd if="$RF" bs=1 skip=1000 count=1000 2>/dev/null > "$T/want13a"
	[ "`status $T/h13a`" = 206 ] && [ "`hdr $T/h13a content-range`" = "bytes 1000-1999/100000" ] \
		&& cmp -s "$T/b13a" "$T/want13a" \
		&& pass "range inside -> 206 with the stretch" \
		|| fail "range inside: `status $T/h13a` `hdr $T/h13a content-range`"

	curl -s -H 'Range: bytes=99000-' -D "$T/h13b" -o "$T/b13b" "$BASE/api/v1/storage/file?path=$RF"
	[ "`status $T/h13b`" = 206 ] && [ "`hdr $T/h13b content-length`" = 1000 ] \
		&& [ "`hdr $T/h13b content-range`" = "bytes 99000-99999/100000" ] \
		&& pass "open-ended range -> 206 to the end" \
		|| fail "open-ended range: `status $T/h13b` `hdr $T/h13b content-range`"

	curl -s -H 'Range: bytes=100000-' -D "$T/h13c" -o "$T/b13c" "$BASE/api/v1/storage/file?path=$RF"
	[ "`status $T/h13c`" = 416 ] && [ "`hdr $T/h13c content-range`" = "bytes */100000" ] \
		&& grep -q '"status":416' "$T/b13c" \
		&& pass "range past the end -> 416 problem with the length" \
		|| fail "range past end: `status $T/h13c` `hdr $T/h13c content-range`"

	# at the socket, so a body after the blank line cannot hide from the check
	r13d=`python3 - "$BASE" "$RF" <<'PYEOF'
import socket, sys
base, path = sys.argv[1], sys.argv[2]
host = base.split('://', 1)[-1].split(':')[0]; port = int(base.rsplit(':', 1)[-1])
s = socket.create_connection((host, port), timeout=15)
s.sendall(("HEAD /api/v1/storage/file?path=%s HTTP/1.0\r\nRange: bytes=1000-1999\r\n\r\n" % path).encode())
data = b''
while True:
	c = s.recv(4096)
	if not c: break
	data += c
s.close()
head, sep, rest = data.partition(b'\r\n\r\n')
status = data.split(b'\r\n', 1)[0].decode(errors='replace')
# the last header line's \r\n is part of the separator; put one back
ok = sep and not rest and b' 206 ' in data.split(b'\r\n', 1)[0] and b'Content-Length: 1000\r\n' in head + b'\r\n'
print(('PASS' if ok else 'FAIL') + ' ' + status + ' body=%d' % len(rest))
PYEOF
`
	case "$r13d" in
		PASS*) pass "HEAD with range: the stretch's headers, no body ($r13d)";;
		*) fail "HEAD with range: $r13d";;
	esac
else
	# the instance runs in a container whose /tmp is not this one
	fail "storage file not served (status `status $T/h13`), range checks did not run"
fi
rm -f "$RF"

# 14. the event stream: header without a length, the retry frame and its
#     spread, one real event reaching two streams at once (the volume goes
#     over the box's own loop: route -> wish -> applied -> announced), and
#     the seats coming back after a hard close. The volume is put back.
r14=`python3 - "$BASE" <<'PYEOF'
import json, socket, subprocess, sys, time
base = sys.argv[1]
host = base.split('://', 1)[-1].split(':')[0]; port = int(base.rsplit(':', 1)[-1])
def open_stream():
	s = socket.create_connection((host, port), timeout=10)
	s.sendall(b"GET /api/v1/events HTTP/1.0\r\nHost: box\r\n\r\n")
	buf = b''
	while b'\r\n\r\n' not in buf:
		c = s.recv(4096)
		if not c: break
		buf += c
	head, _, body = buf.partition(b'\r\n\r\n')
	return s, head, body
def read_until(s, needle, body, deadline):
	s.settimeout(0.5)
	while needle not in body and time.time() < deadline:
		try:
			c = s.recv(4096)
		except socket.timeout:
			continue
		except OSError:
			break
		if not c: break
		body += c
	return body
def get_volume():
	out = subprocess.check_output(['curl', '-s', '--max-time', '10',
	                               base + '/api/v1/osd/volume'], timeout=15)
	return json.loads(out)['percent']
def put_volume(value):
	code = subprocess.check_output(['curl', '-s', '-o', '/dev/null',
		'-w', '%{http_code}', '--max-time', '10', '-X', 'PUT',
		'-H', 'Content-Type: application/json',
		'-d', '{"percent":%d}' % value, base + '/api/v1/osd/volume'],
		timeout=15)
	return code.strip() == b'202'
fails = []
# The volume is read first and put back whatever happens after. Without the
# pre-read nothing is written at all: a probe must not leave the box changed.
try:
	percent = get_volume()
except Exception:
	print('FAIL could not read the volume, section skipped before writing anything')
	sys.exit(0)
wrote = False
try:
	a, ah, abody = open_stream()
	b, bh, bbody = open_stream()
	if b' 200 ' not in ah.split(b'\r\n', 1)[0]:
		fails.append('status ' + ah.split(b'\r\n', 1)[0].decode(errors='replace'))
	hl = ah.lower()
	if b'content-type: text/event-stream' not in hl: fails.append('content-type')
	if b'content-length:' in hl: fails.append('has content-length')
	if b'transfer-encoding:' in hl: fails.append('has transfer-encoding')
	abody = read_until(a, b'retry:', abody, time.time() + 2)
	bbody = read_until(b, b'retry:', bbody, time.time() + 2)
	if b'retry:' not in abody: fails.append('no retry frame within 2s')
	ra = abody.split(b'retry:')[1].split(b'\n')[0] if b'retry:' in abody else b''
	rb = bbody.split(b'retry:')[1].split(b'\n')[0] if b'retry:' in bbody else b''
	if ra == rb: fails.append('both told the same retry')
	want = 37 if percent != 37 else 38
	wrote = True
	if not put_volume(want):
		fails.append('the volume PUT was not accepted')
	# the event, with the value that was asked, on both streams
	needle = ('"value":%d' % want).encode()
	deadline = time.time() + 3
	abody = read_until(a, needle, abody, deadline)
	bbody = read_until(b, needle, bbody, deadline)
	if b'event: volume' not in abody or needle not in abody:
		fails.append('stream a missed the volume event with its value')
	if b'event: volume' not in bbody or needle not in bbody:
		fails.append('stream b missed the volume event with its value')
	a.close(); b.close()
	# The seats really come back: fill to the ceiling, hard-close everything,
	# then the same number opens again. Two extra opens would pass beside a
	# leak; the full count cannot.
	time.sleep(1.0)
	held = []
	for i in range(12):
		si, hi, bi = open_stream()
		if b' 200 ' in hi.split(b'\r\n', 1)[0]:
			held.append(si)
		else:
			si.close()
			break
	ceiling = len(held)
	if ceiling == 0 or ceiling >= 12:
		fails.append('no ceiling found within 12 streams (got %d)' % ceiling)
	for si in held:
		si.close()
	time.sleep(1.0)
	reopened = []
	for i in range(ceiling):
		si, hi, bi = open_stream()
		if b' 200 ' in hi.split(b'\r\n', 1)[0]:
			reopened.append(si)
		else:
			si.close()
	if len(reopened) != ceiling:
		fails.append('only %d of %d seats came back' % (len(reopened), ceiling))
	for si in reopened:
		si.close()
finally:
	if wrote:
		try:
			if not put_volume(percent):
				fails.append('the volume could not be put back to %d' % percent)
			elif get_volume() != percent:
				fails.append('the volume reads back wrong after the restore')
		except Exception as e:
			fails.append('restore failed: %s' % e)
print('PASS' if not fails else 'FAIL ' + '; '.join(fails))
PYEOF
`
case "$r14" in
	PASS*) pass "event stream: header, retry spread, one event on two streams, reap";;
	*) fail "event stream: $r14";;
esac

echo "failures: $fails"
[ "$fails" -eq 0 ]
