#!/bin/sh
# check-v1.sh - HTTP-level checks of the REST bridge against a running neutrino
#
# usage: check-v1.sh <base-url> <openapi.json from openapi_dump> [before-dir]
#   base-url    e.g. http://127.0.0.1:31344
#   openapi.json the document the dump tool wrote from the same build
#   before-dir  optional: directory with legacy answers captured before the
#               change (control-getbouquets.txt, control-getversion.txt,
#               page.txt) to compare header+body byte for byte
#
# Prints one line per check, PASS or FAIL, and exits non-zero on any FAIL.
# Needs curl, gzip, cmp.

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

# 5. PUT with JSON body is dispatched (router answers, not 501)
curl -s -D "$T/h5" -o "$T/b5" -X PUT -H 'Content-Type: application/json' -d '{}' "$BASE/api/v1/bouquets/0/name"
s5=`status $T/h5`; [ "$s5" != 501 ] && [ "$s5" != 404 ] && pass "PUT reaches the router ($s5)" || fail "PUT status $s5"

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

# 10. half a body meets the deadline (about 10 s), not a hang
start=`date +%s`
printf 'POST /api/v1/system/standby HTTP/1.0\r\nContent-Length: 100\r\nContent-Type: application/json\r\n\r\n{"on"' | timeout 30 nc -q 25 `echo "$BASE" | sed 's#https\?://##; s#:.*##'` `echo "$BASE" | sed 's#.*:##'` > "$T/h10" 2>/dev/null
took=`expr \`date +%s\` - $start`
grep -q ' 408 ' "$T/h10" && [ "$took" -lt 20 ] && pass "half body -> 408 after ${took}s" || fail "half body: `head -1 $T/h10 | tr -d '\r'` after ${took}s"

# 11. legacy untouched
curl -s -D "$T/h11" -o /dev/null -X PUT "$BASE/control/standby"
[ "`status $T/h11`" = 501 ] && pass "PUT /control/standby -> 501 as before" || fail "PUT /control status `status $T/h11`"
curl -s -D "$T/h12" -o /dev/null -X PUT "$BASE/x"
[ "`status $T/h12`" = 501 ] && pass "PUT /x -> 501 as before" || fail "PUT /x status `status $T/h12`"
curl -s -D "$T/h13" -o /dev/null -X TRACE "$BASE/api/v1/system/info"
[ "`status $T/h13`" = 501 ] && pass "TRACE -> 501" || fail "TRACE status `status $T/h13`"
curl -s -o "$T/gv" "$BASE/control/getversion"; [ -s "$T/gv" ] && pass "/control/getversion answers" || fail "/control/getversion empty"
if [ -n "$BEFORE" ] && [ -d "$BEFORE" ]; then
	for f in control-getbouquets control-getversion page; do
		[ -f "$BEFORE/$f.url" ] || continue
		curl -s -D "$T/$f.h" -o "$T/$f.b" "$BASE`cat $BEFORE/$f.url`"
		grep -vi '^date:' "$T/$f.h" > "$T/$f.hh"; grep -vi '^date:' "$BEFORE/$f.h" > "$T/$f.hb"
		cmp -s "$T/$f.hh" "$T/$f.hb" && cmp -s "$T/$f.b" "$BEFORE/$f.b" && pass "legacy $f byte-identical" || fail "legacy $f differs"
	done
fi

echo "failures: $fails"
[ "$fails" -eq 0 ]
