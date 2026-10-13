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

# The box asks for a login since slice 3: reads pass from the lan, writes
# carry the session and its second token. The credentials default to the
# dev instance's (migrated out of its nhttpd.conf) and come from the
# environment for any other box.
fails=0
pass() { echo "PASS $1"; }
fail() { echo "FAIL $1"; fails=`expr $fails + 1`; }
AUTH_USER="${CHECKV1_USER:-root}"
AUTH_PASS="${CHECKV1_PASS:-root}"
curl -s -c "$T/jar" -D "$T/hl" -o "$T/bl" -H 'Content-Type: application/json' \
	-d "{\"user\":\"$AUTH_USER\",\"password\":\"$AUTH_PASS\"}" "$BASE/api/v1/login"
SID=`awk '$6 == "ni-web-session" {print $7}' "$T/jar" | tail -1`
CSRF_H=`python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(d.get("csrf_header",""))' "$T/bl" 2>/dev/null`
CSRF_V=`python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(d.get("csrf",""))' "$T/bl" 2>/dev/null`
if [ -n "$SID" ] && [ -n "$CSRF_H" ] && [ -n "$CSRF_V" ]; then
	pass "login: session and second token issued"
else
	fail "login: `head -1 $T/hl | tr -d '\r'` body `cat $T/bl`"
	echo "failures: $fails"; exit 1
fi
AUTH_C="Cookie: ni-web-session=$SID"
AUTH_X="$CSRF_H: $CSRF_V"
export CV1_SID="$SID" CV1_CSRF_H="$CSRF_H" CV1_CSRF_V="$CSRF_V"
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
curl -s -D "$T/h5" -o "$T/b5" -X PUT -H "$AUTH_C" -H "$AUTH_X" -H 'Content-Type: application/json' -d '{}' "$BASE/api/v1/bouquets/0/name"
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
for m in 'PUT /control/version' 'PUT /x' 'PATCH /control/version' 'TRACE /api/v1/system/info'; do
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
curl -s -H "$AUTH_C" -D "$T/h13" -o "$T/b13" "$BASE/api/v1/storage/file?path=$RF"
if [ "`status $T/h13`" = 200 ]; then
	cmp -s "$RF" "$T/b13" && [ "`hdr $T/h13 content-length`" = 100000 ] \
		&& pass "storage file served whole, bytes identical" \
		|| fail "storage file differs or wrong length (`hdr $T/h13 content-length`)"

	curl -s -H "$AUTH_C" -H 'Range: bytes=1000-1999' -D "$T/h13a" -o "$T/b13a" "$BASE/api/v1/storage/file?path=$RF"
	dd if="$RF" bs=1 skip=1000 count=1000 2>/dev/null > "$T/want13a"
	[ "`status $T/h13a`" = 206 ] && [ "`hdr $T/h13a content-range`" = "bytes 1000-1999/100000" ] \
		&& cmp -s "$T/b13a" "$T/want13a" \
		&& pass "range inside -> 206 with the stretch" \
		|| fail "range inside: `status $T/h13a` `hdr $T/h13a content-range`"

	curl -s -H "$AUTH_C" -H 'Range: bytes=99000-' -D "$T/h13b" -o "$T/b13b" "$BASE/api/v1/storage/file?path=$RF"
	[ "`status $T/h13b`" = 206 ] && [ "`hdr $T/h13b content-length`" = 1000 ] \
		&& [ "`hdr $T/h13b content-range`" = "bytes 99000-99999/100000" ] \
		&& pass "open-ended range -> 206 to the end" \
		|| fail "open-ended range: `status $T/h13b` `hdr $T/h13b content-range`"

	curl -s -H "$AUTH_C" -H 'Range: bytes=100000-' -D "$T/h13c" -o "$T/b13c" "$BASE/api/v1/storage/file?path=$RF"
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
s.sendall(("HEAD /api/v1/storage/file?path=" + path + " HTTP/1.0\r\nCookie: ni-web-session=" + __import__("os").environ.get("CV1_SID", "") + "\r\nRange: bytes=1000-1999\r\n\r\n").encode())
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
	import os
	code = subprocess.check_output(['curl', '-s', '-o', '/dev/null',
		'-w', '%{http_code}', '--max-time', '10', '-X', 'PUT',
		'-H', 'Cookie: ni-web-session=' + os.environ.get('CV1_SID', ''),
		'-H', os.environ.get('CV1_CSRF_H', 'X-CSRF-Token') + ': ' + os.environ.get('CV1_CSRF_V', ''),
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

# 15. legacy-usage answers with real counts: the known table stands at start
# in dispatcher order, a driven name counts what we drove, and a name nobody
# registered is remembered as itself. getmode is the probe because reading
# the box's mode changes nothing on it.
r15=`python3 - "$BASE" <<'PYEOF'
import json, subprocess, sys, time
base = sys.argv[1]
def usage():
	out = subprocess.check_output(['curl', '-s', '--max-time', '10',
	                               base + '/api/v1/system/legacy-usage'], timeout=15)
	return json.loads(out)['items']
def hit(name):
	subprocess.check_output(['curl', '-s', '-o', '/dev/null', '--max-time', '10',
	                         base + '/control/' + name], timeout=15)
fails = []
before = usage()
if not before:
	print('FAIL the list is empty, the dispatcher seeded nothing')
	sys.exit(0)
if before[0]['name'] != 'getservicesxml':
	fails.append('first name is %r, not the dispatch table head' % before[0]['name'])
counts = dict((i['name'], i['calls']) for i in before)
if 'getmode' not in counts:
	fails.append('getmode is not in the known table')
was = counts.get('getmode', 0)
hit('getmode'); hit('getmode')
# One reserved probe name, not a fresh one per run: the unknown list is
# bounded (200 seats, kept until restart), so a name minted every run would
# spend a seat each time and starve the box after enough runs. The delta
# keeps this honest across repeats and parallel runs alike.
stranger = 'check-v1-probe'
stranger_was = counts.get(stranger, 0)
hit(stranger)
after = dict((i['name'], i['calls']) for i in usage())
if after.get('getmode', 0) < was + 2:
	fails.append('getmode counted %s, expected at least %s' % (after.get('getmode'), was + 2))
if after.get(stranger, 0) < stranger_was + 1:
	if stranger not in after and len(after) >= len(counts) and stranger_was == 0:
		fails.append('the probe name found no seat (unknown list full? %d names)' % len(after))
	else:
		fails.append('the unknown name %s did not count up' % stranger)
print('PASS' if not fails else 'FAIL ' + '; '.join(fails))
PYEOF
`
case "$r15" in
	PASS*) pass "legacy-usage: seeded table, driven count, stranger remembered";;
	*) fail "legacy-usage: $r15";;
esac

# 16. the gate on the wire: a write with nothing is refused, a wrong login
# is refused the same way, a session mutation needs its second token, and
# a closed session is worth nothing any more
curl -s -D "$T/h16a" -o /dev/null -X PUT -H 'Content-Type: application/json' -d '{"percent":50}' "$BASE/api/v1/osd/volume"
[ "`status $T/h16a`" = 403 ] && pass "write without a credential -> 403" || fail "bare write status `status $T/h16a`"

curl -s -D "$T/h16b" -o /dev/null -H 'Content-Type: application/json' \
	-d "{\"user\":\"$AUTH_USER\",\"password\":\"definitely-wrong\"}" "$BASE/api/v1/login"
[ "`status $T/h16b`" = 401 ] && pass "wrong password -> 401" || fail "wrong password status `status $T/h16b`"

curl -s -D "$T/h16c" -o /dev/null -X PUT -H "$AUTH_C" -H 'Content-Type: application/json' -d '{"percent":50}' "$BASE/api/v1/osd/volume"
[ "`status $T/h16c`" = 403 ] && pass "cookie without the second token -> 403" || fail "csrf-less write status `status $T/h16c`"

curl -s -D "$T/h16d" -o "$T/b16d" -H "$AUTH_C" "$BASE/api/v1/session"
[ "`status $T/h16d`" = 200 ] && grep -q "\"user\":\"$AUTH_USER\"" "$T/b16d" && pass "session answers its owner" || fail "session: `status $T/h16d` `cat $T/b16d`"

# a second session, closed again, so the main one stays for the sections above
curl -s -c "$T/jar2" -o "$T/bl2" -H 'Content-Type: application/json' \
	-d "{\"user\":\"$AUTH_USER\",\"password\":\"$AUTH_PASS\"}" "$BASE/api/v1/login"
SID2=`awk '$6 == "ni-web-session" {print $7}' "$T/jar2" | tail -1`
C2H=`python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["csrf_header"])' "$T/bl2"`
C2V=`python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["csrf"])' "$T/bl2"`
curl -s -D "$T/h16e" -o /dev/null -X POST -H "Cookie: ni-web-session=$SID2" -H "$C2H: $C2V" "$BASE/api/v1/logout"
curl -s -D "$T/h16f" -o /dev/null -X PUT -H "Cookie: ni-web-session=$SID2" -H "$C2H: $C2V" -H 'Content-Type: application/json' -d '{"percent":50}' "$BASE/api/v1/osd/volume"
[ "`status $T/h16e`" = 204 ] && [ "`status $T/h16f`" = 403 ] && pass "logout ends the session (`status $T/h16e`, then `status $T/h16f`)" || fail "logout: `status $T/h16e` then `status $T/h16f`"

# 17. the scoped media token, end to end: minted over the session, read as
# the one credential in an address, and held to the media roots
r17=`python3 - "$BASE" <<'PYEOF'
import json, os, subprocess, sys, tempfile
base = sys.argv[1]
sid = os.environ.get('CV1_SID', '')
csrf_h = os.environ.get('CV1_CSRF_H', 'X-CSRF-Token')
csrf_v = os.environ.get('CV1_CSRF_V', '')
auth = ['-H', 'Cookie: ni-web-session=' + sid, '-H', csrf_h + ': ' + csrf_v]
def curl(args):
	return subprocess.check_output(['curl', '-s', '--max-time', '10'] + args, timeout=15)
def recordingdir():
	d = json.loads(curl([base + '/api/v1/settings/recording']))
	for item in d.get('items', []):
		if item.get('id') == 'network_nfs_recordingdir':
			return item.get('value', '')
	return ''
def point_at(path):
	code = subprocess.check_output(['curl', '-s', '-o', '/dev/null', '-w', '%{http_code}',
		'--max-time', '10', '-X', 'PATCH', '-H', 'Content-Type: application/json',
		'-d', json.dumps({'network_nfs_recordingdir': path}),
		base + '/api/v1/settings/recording'] + auth, timeout=15)
	return code.strip() in (b'200', b'202', b'204')
# The recording directory is read first and put back whatever happens: the
# scope admits it because somebody set it by hand, which is exactly what
# this section does, through the same surface a person would use.
before = recordingdir()
if not before:
	print('FAIL media token: the recording directory could not be read')
	sys.exit(0)
probe_dir = tempfile.mkdtemp(prefix='check-v1-media-')
name = os.path.join(probe_dir, 'probe.ts')
moved = False
try:
	with open(name, 'wb') as f:
		f.write(b'0123456789abcdef')
	if not point_at(probe_dir):
		print('FAIL media token: the recording directory would not move')
		sys.exit(0)
	moved = True
	minted = json.loads(curl(['-X', 'POST'] + auth + [base + '/api/v1/token/media']))
	token = minted.get('token', '')
	if not token:
		print('FAIL media token: nothing minted: %s' % minted)
		sys.exit(0)
	got = curl([base + '/api/v1/storage/file?path=' + name + '&token=' + token])
	if got != b'0123456789abcdef':
		print('FAIL media token: file read back %r' % got[:64])
		sys.exit(0)
	bare = subprocess.check_output(['curl', '-s', '-o', '/dev/null', '-w', '%{http_code}',
		'--max-time', '10', base + '/api/v1/storage/file?path=' + name], timeout=15)
	if bare.strip() != b'403':
		print('FAIL media token: tokenless read answered %s' % bare)
		sys.exit(0)
	print('PASS')
finally:
	if moved and not point_at(before):
		print('FAIL media token: the recording directory could not be put back to %s' % before)
	os.unlink(name)
	os.rmdir(probe_dir)
PYEOF
`
case "$r17" in
	PASS) pass "media token: minted, reads the file, tokenless is refused";;
	*) fail "media token: $r17";;
esac

# 18. the webserver table, and the one answer that moves the server: the
# write is answered on the old port with the address the caller reaches it
# under next, then the box really comes up there - and back again, so the
# instance is left where it was found. The session survives the move: the
# webserver thread is restarted, the program is not.
r18=`python3 - "$BASE" <<'PYEOF'
import json, os, subprocess, sys, time
base = sys.argv[1]
host = base.rsplit(':', 1)[0]
old_port = int(base.rsplit(':', 1)[1])
new_port = old_port + 1
sid = os.environ.get('CV1_SID', '')
csrf_h = os.environ.get('CV1_CSRF_H', 'X-CSRF-Token')
csrf_v = os.environ.get('CV1_CSRF_V', '')
auth = ['-H', 'Cookie: ni-web-session=' + sid, '-H', csrf_h + ': ' + csrf_v]
def curl(args):
	return subprocess.check_output(['curl', '-s', '--max-time', '10'] + args, timeout=15)
def webserver_on(port):
	return json.loads(curl(auth[:2] + ['%s:%d/api/v1/system/webserver' % (host, port)]))
def move(frm, to):
	answer = json.loads(curl(['-X', 'PUT', '-H', 'Content-Type: application/json',
		'-d', json.dumps({'port': to})] + auth + ['%s:%d/api/v1/system/webserver' % (host, frm)]))
	caller = answer.get('caller', {})
	if not caller.get('restarting') or not caller.get('port_moved'):
		return 'the answer does not say it is moving: %s' % caller
	if (':%d/' % to) not in caller.get('address', ''):
		return 'the address does not carry the new port: %r' % caller.get('address')
	for i in range(50):
		time.sleep(0.3)
		try:
			if webserver_on(to).get('port') == to:
				return ''
		except Exception:
			continue
	return 'nothing answers on %d' % to
fails = []
before = webserver_on(old_port)
if before.get('port') != old_port:
	fails.append('GET says port %s, expected %d' % (before.get('port'), old_port))
if before.get('has_password') is not True:
	fails.append('has_password is %s' % before.get('has_password'))
if 'password_hash' in json.dumps(before):
	fails.append('the stored form leaks')
if not fails:
	gone = move(old_port, new_port)
	if gone:
		fails.append('move out: ' + gone)
	else:
		back = move(new_port, old_port)
		if back:
			fails.append('MOVE BACK FAILED, instance may sit on %d: %s' % (new_port, back))
if not fails:
	# An address nhttpd cannot listen on as written (AF_INET only) is turned
	# down before anything is written, and nothing restarts.
	code = curl(['-o', '/dev/null', '-w', '%{http_code}', '-X', 'PUT', '-H', 'Content-Type: application/json',
		'-d', json.dumps({'bind': '::1'})] + auth + ['%s:%d/api/v1/system/webserver' % (host, old_port)])
	if code != b'400':
		fails.append('a v6 bind was answered %s, not 400' % code.decode())
	elif webserver_on(old_port).get('bind') != before.get('bind'):
		fails.append('a refused v6 bind moved the address')
if not fails:
	# A port something else holds: the answer promises the move, nothing comes
	# up there, and the server goes back to the port it answered on, its file
	# with it. Only where this runs on the box itself, the one place a port of
	# the box can be taken from here.
	import socket
	blocker = socket.socket()
	# The moves above leave that port in TIME-WAIT, which would refuse this
	# bind; a listening socket still refuses nhttpd's.
	blocker.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	try:
		blocker.bind((host.split('//', 1)[-1].strip('[]'), new_port))
	except OSError:
		blocker.close()
		blocker = None
		print('note: %s is not this machine, the taken port is not tried' % host, file=sys.stderr)
	if blocker is not None:
		blocker.listen(1)
		# A start count above the one the answer carried is the server after
		# the restart and not the one before it, however short the restart.
		def restarted_on(port, before):
			for i in range(100):
				time.sleep(0.2)
				try:
					answer = webserver_on(port)
				except Exception:
					continue
				if answer.get('restarts', -1) > before and answer.get('bound_port') == port:
					return True
			return False
		try:
			answer = json.loads(curl(['-X', 'PUT', '-H', 'Content-Type: application/json',
				'-d', json.dumps({'port': new_port})] + auth + ['%s:%d/api/v1/system/webserver' % (host, old_port)]))
			if not answer.get('caller', {}).get('restarting'):
				fails.append('the taken port was not even tried: %s' % answer.get('caller'))
			elif not restarted_on(old_port, answer.get('webserver', {}).get('restarts', 0)):
				fails.append('BOX DID NOT COME BACK on %d after %d was taken' % (old_port, new_port))
		finally:
			blocker.close()
		# The file says the old port again: a restart that changes nothing
		# about the port comes up there, the port now being free. The switch
		# reads back changed only once the restart read the files, so an
		# answer carrying it on the old port is the server after the restart.
		if not fails:
			logos = webserver_on(old_port).get('channel_logos')
			for value in (not logos, logos):
				curl(['-X', 'PUT', '-H', 'Content-Type: application/json',
					'-d', json.dumps({'channel_logos': value})] + auth + ['%s:%d/api/v1/system/webserver' % (host, old_port)])
				came = False
				for i in range(100):
					time.sleep(0.2)
					try:
						if webserver_on(old_port).get('channel_logos') == value:
							came = True
							break
					except Exception:
						continue
				if not came:
					fails.append('after the fallback a restart did not come up on %d, so the file '
						'does not say it' % old_port)
					break
print('PASS' if not fails else 'FAIL ' + '; '.join(fails))
PYEOF
`
case "$r18" in
	PASS*) pass "webserver table: truth, port moved and back, v6 bind refused, taken port falls back";;
	*) fail "webserver table: $r18";;
esac

echo "failures: $fails"
[ "$fails" -eq 0 ]
