#!/bin/sh
# check-remotebox.sh - the remote-timer client of another box, driven against
# a running neutrino's /api/v1 over the wire
#
# usage: check-remotebox.sh <base-url> <remotebox_client>
#   base-url         e.g. http://127.0.0.1:31344
#   remotebox_client the driver built in test/unit (make check builds it),
#                    which wraps the client code another box speaks with
#
# What another box does when it files a recording here: it reads the timer
# list and the two margins without a session, asks for a channel's name,
# and for every write is refused once, logs in, and tries again with the
# cookie and the second token the login answered. This script drives that
# client through a whole round trip - list, margins, channel, add, change,
# remove - and reads the box back over curl after every write, so the
# client's view and the box's own are compared rather than one of them
# trusted. Every read is made with no credential at all, so a box that
# asked its readers to log in would fail here rather than be logged into;
# every write is first made the same way and must be refused, so a box
# that let anyone write would fail too, and only then made with the
# credential.
#
# Prints one line per check, PASS or FAIL, and exits non-zero on any FAIL;
# a box this script could not put back the way it found it counts as one.
# Needs curl and python3. The credentials default to the dev instance's
# and come from the environment for any other box (CHECKV1_USER/_PASS,
# the same two check-v1.sh reads).

BASE="$1"; CLIENT="$2"
[ -n "$BASE" ] && [ -n "$CLIENT" ] || { echo "usage: $0 <base-url> <remotebox_client>" >&2; exit 2; }
T=`mktemp -d` || { echo "no temporary directory" >&2; exit 2; }
# every call carries a deadline: this script changes the box's state, and a
# box that stalls must not leave the changes in place because the script hung
CURL="curl -s --max-time 10"

AUTH_USER="${CHECKV1_USER:-root}"
AUTH_PASS="${CHECKV1_PASS:-root}"
HOSTPORT=`echo "$BASE" | sed 's,^[a-z]*://,,; s,/$,,'`
BOX="$AUTH_USER:$AUTH_PASS@$HOSTPORT"
# the same box with nothing to present. A call this client makes without a
# credential is either answered, or refused and followed by a login with no
# name, which the box turns down with 400 and the client reports as REFUSED.
ANON="$HOSTPORT"

fails=0
pass() { echo "PASS $1"; }
fail() { echo "FAIL $1"; fails=`expr $fails + 1`; }
status() { head -1 "$1" | tr -d '\r' | awk '{print $2}'; }
# the first line the driver prints: result=<word> http=<code>
result() { sed -n '1s/^result=\([A-Z_]*\).*/\1/p' "$1"; }
httpcode() { sed -n '1s/.*http=\([0-9]*\).*/\1/p' "$1"; }
# one field off a driver data line: field <file> <line-prefix> <key>
field() { grep "^$2 " "$1" | head -1 | sed -n "s/.* $3=\([^ ]*\).*/\1/p"; }
jq1() { python3 -c 'import json,sys
d=json.load(open(sys.argv[1]))
for k in sys.argv[2].split("."):
	d=d[int(k)] if k.isdigit() else d[k]
print(d)' "$1" "$2" 2>/dev/null; }
# the box offers no read of one timer, only the list; so a timer is looked up
# in the list by its id: tfield <list-file> <id> <key>
tfield() { python3 -c 'import json,sys
d=json.load(open(sys.argv[1]))
for t in d.get("items",[]):
	if str(t.get("id"))==sys.argv[2]:
		print(t.get(sys.argv[3],"")); break' "$1" "$2" "$3" 2>/dev/null; }
# the ids a later list holds that an earlier one did not: newids <before> <after>
newids() { python3 -c 'import json,sys
ids=lambda p:{str(t["id"]) for t in json.load(open(p)).get("items",[]) if "id" in t}
print(" ".join(sorted(ids(sys.argv[2])-ids(sys.argv[1]), key=int)))' "$1" "$2" 2>/dev/null; }
# the two margins as the box states them, "before after"
margins() { python3 -c 'import json,sys
d=json.load(open(sys.argv[1])); v={i["id"]:str(i["value"]) for i in d.get("items",[])}
print(v.get("record_safety_time_before",""), v.get("record_safety_time_after",""))' "$1" 2>/dev/null; }

# Putting the box back is part of the result: a margin that stayed moved or
# a timer that stayed filed is a FAIL, not a warning. finish does it once,
# on the normal way out and on a signal alike, and only then says how many
# checks failed; the EXIT trap is the net under an exit this script did not
# write. (An EXIT trap alone does not run when sh is interrupted.)
MARGINS_BEFORE=""; WANT_BACK=""; MADE_ID=""; PENDING_ADD=""; SID=""; CLEANED=""
cleanup() {
	[ -n "$CLEANED" ] && return
	CLEANED=1
	# an add that was sent but whose timer this script had not yet told apart
	# from the box's own: the same difference of lists, made now
	if [ -z "$MADE_ID" ] && [ -n "$PENDING_ADD" ] && [ -n "$SID" ]; then
		$CURL -o "$T/tlz" "$BASE/api/v1/timers"
		set -- `newids "$PENDING_ADD" "$T/tlz"`
		if [ $# -eq 1 ]; then
			MADE_ID="$1"
		elif [ $# -gt 1 ]; then
			fail "cleanup: several timers appeared since the add ($*), none removed; check them by hand"
		fi
	fi
	if [ -n "$MADE_ID" ] && [ -n "$SID" ]; then
		c=`$CURL -o /dev/null -w '%{http_code}' -X DELETE -H "$AUTH_C" -H "$AUTH_X" "$BASE/api/v1/timers/$MADE_ID"`
		case "$c" in 204|404) ;; *) fail "cleanup: timer $MADE_ID not removed ($c); DELETE $BASE/api/v1/timers/$MADE_ID by hand";; esac
	fi
	if [ -n "$MARGINS_BEFORE" ] && [ -n "$SID" ]; then
		$CURL -o /dev/null -X PATCH -H "$AUTH_C" -H "$AUTH_X" -H 'Content-Type: application/json' \
			-d "$MARGINS_BEFORE" "$BASE/api/v1/settings/recording"
		$CURL -o "$T/recz" "$BASE/api/v1/settings/recording"
		[ "`margins "$T/recz"`" = "$WANT_BACK" ] \
			|| fail "cleanup: margins read back as \"`margins "$T/recz"`\", wanted \"$WANT_BACK\"; PATCH $MARGINS_BEFORE to $BASE/api/v1/settings/recording by hand"
	fi
	rm -rf "$T"
}
finish() {
	cleanup
	echo "failures: $fails"
	[ "$fails" -eq 0 ] && exit 0
	exit "${1:-1}"
}
trap 'fail "interrupted"; finish 130' INT TERM HUP
trap cleanup EXIT

# 0. preconditions: the box answers, the driver exists, the reader is there
$CURL -D "$T/h0" -o /dev/null "$BASE/api/v1/system/info"
[ "`status "$T/h0"`" = 200 ] || { fail "box: system/info answered `status "$T/h0"`"; finish 1; }
[ -x "$CLIENT" ] || { fail "driver: $CLIENT is not an executable"; finish 1; }
command -v python3 >/dev/null 2>&1 || { fail "needs python3 to read the box's answers"; finish 1; }

# the script's own session, for the settings it moves and the reads that
# compare the box's view with the client's; the client under test never
# sees these credentials other than in its address
LOGIN_BODY=`python3 -c 'import json,sys; print(json.dumps({"user":sys.argv[1],"password":sys.argv[2]}))' "$AUTH_USER" "$AUTH_PASS"`
$CURL -c "$T/jar" -o "$T/bl" -H 'Content-Type: application/json' -d "$LOGIN_BODY" "$BASE/api/v1/login"
SID=`awk '$6 == "ni-web-session" {print $7}' "$T/jar" | tail -1`
CSRF_H=`jq1 "$T/bl" csrf_header`; CSRF_V=`jq1 "$T/bl" csrf`
[ -n "$SID" ] && [ -n "$CSRF_H" ] && [ -n "$CSRF_V" ] || { SID=""; fail "script session: no login (`cat "$T/bl"`)"; finish 1; }
AUTH_C="Cookie: ni-web-session=$SID"
AUTH_X="$CSRF_H: $CSRF_V"

# 1. margins the client can see: the box is set to 3 and 7 minutes for the
# length of this run, so that "widened by the box's own margins" is a number
# and not a nought; what it was is read first and put back at the end
$CURL -o "$T/rec0" "$BASE/api/v1/settings/recording"
WANT_BACK=`margins "$T/rec0"`
B0=`python3 -c 'import json,sys
d=json.load(open(sys.argv[1])); v={i["id"]:i["value"] for i in d["items"]}
print(json.dumps({"record_safety_time_before":v["record_safety_time_before"],"record_safety_time_after":v["record_safety_time_after"]}))' "$T/rec0"`
[ -n "$B0" ] && [ "$WANT_BACK" != " " ] || { fail "margins: the recording section could not be read"; finish 1; }
MARGINS_BEFORE="$B0"
$CURL -D "$T/h1" -o /dev/null -X PATCH -H "$AUTH_C" -H "$AUTH_X" -H 'Content-Type: application/json' \
	-d '{"record_safety_time_before":3,"record_safety_time_after":7}' "$BASE/api/v1/settings/recording"
$CURL -o "$T/rec1" "$BASE/api/v1/settings/recording"
[ "`margins "$T/rec1"`" = "3 7" ] && pass "margins set to 3/7 minutes for this run (were $WANT_BACK)" \
	|| { fail "margins: PATCH answered `status "$T/h1"`, box now says \"`margins "$T/rec1"`\""; finish 1; }

# 2. the client reads the margins with no credential, in seconds
"$CLIENT" "$ANON" safety > "$T/o2" 2> "$T/e2"
[ "`result "$T/o2"`" = OK ] && [ "`field "$T/o2" safety pre`" = 180 ] && [ "`field "$T/o2" safety post`" = 420 ] \
	&& pass "client: margins read with no credential, 180/420 s" \
	|| fail "client safety: `head -2 "$T/o2" | tr '\n' ' '` (stderr in $T/e2)"

# 3. the client reads the timer list with no credential
"$CLIENT" "$ANON" timers > "$T/o3" 2> "$T/e3"
N0=`grep -c '^timer ' "$T/o3"`
[ "`result "$T/o3"`" = OK ] && pass "client: timer list read with no credential ($N0 recording timers)" \
	|| fail "client timers: `head -1 "$T/o3"`"

# 4. a channel the box holds, by the name the box itself gives it
$CURL -o "$T/ch" "$BASE/api/v1/channels"
CH_ID=`jq1 "$T/ch" items.0.id`; CH_NAME=`jq1 "$T/ch" items.0.name`
[ -n "$CH_ID" ] || { fail "channels: the box lists none"; finish 1; }
"$CLIENT" "$ANON" channel "$CH_ID" > "$T/o4" 2> "$T/e4"
GOT_NAME=`sed -n 's/^name=//p' "$T/o4"`
[ "`result "$T/o4"`" = OK ] && [ "$GOT_NAME" = "$CH_NAME" ] && pass "client: channel $CH_ID is \"$CH_NAME\" on both sides" \
	|| fail "client channel: `head -1 "$T/o4"` name=\"$GOT_NAME\" box says \"$CH_NAME\""

# 5. a channel the box does not hold is an answer, not a fault
"$CLIENT" "$ANON" channel 1 > "$T/o5" 2> "$T/e5"
[ "`result "$T/o5"`" = NOT_THERE ] && [ "`httpcode "$T/o5"`" = 404 ] && pass "client: unknown channel -> NOT_THERE (404)" \
	|| fail "client unknown channel: `head -1 "$T/o5"`"

# 6. a recording filed from the other side. First with nothing to present:
# the box must refuse the write, and the client, driven into a login it has
# no name for, must come back with a refusal and nothing filed. Then with
# the credential: refused once, logged in, filed - and the box widens it by
# its own margins, which is what the client asked for. The timer is the
# script's by the difference of the box's list before and after, not by the
# times it expects.
NOW=`date +%s`
START=`expr $NOW + 7200`; STOP=`expr $START + 1800`; ANNOUNCE=`expr $START - 60`
$CURL -o "$T/tl0" "$BASE/api/v1/timers"
"$CLIENT" "$ANON" add "$CH_ID" "$START" "$STOP" "$ANNOUNCE" > "$T/o6a" 2> "$T/e6a"
$CURL -o "$T/tl0b" "$BASE/api/v1/timers"
[ "`result "$T/o6a"`" != OK ] && [ -n "`jq1 "$T/tl0b" items`" ] && [ -z "`newids "$T/tl0" "$T/tl0b"`" ] \
	&& pass "client: a write with no credential is refused (`result "$T/o6a"`, `httpcode "$T/o6a"`), nothing filed" \
	|| fail "client add with no credential: `head -1 "$T/o6a"`; new ids: `newids "$T/tl0" "$T/tl0b"`"
# from here until the new timer is told apart, an interruption has the
# cleanup make the same difference of lists against this snapshot
PENDING_ADD="$T/tl0b"
"$CLIENT" "$BOX" add "$CH_ID" "$START" "$STOP" "$ANNOUNCE" > "$T/o6" 2> "$T/e6"
[ "`result "$T/o6"`" = OK ] && [ "`httpcode "$T/o6"`" = 201 ] && pass "client: the same write with the credential is filed (201), so the refused-login-retry path was walked" \
	|| fail "client add: `head -1 "$T/o6"`"
$CURL -o "$T/tl" "$BASE/api/v1/timers"
NEW=`newids "$T/tl0b" "$T/tl"`
case `echo $NEW | wc -w` in
	1) MADE_ID="$NEW";;
	0) fail "box: the add was answered 201 but the list shows no new timer";;
	*)
		# more than one appeared: the box may have filed one of its own
		# meanwhile; take this script's if it is the only new one on its channel
		MINE=""
		for i in $NEW; do [ "`tfield "$T/tl" "$i" channel_id`" = "$CH_ID" ] && MINE="$MINE $i"; done
		set -- $MINE
		if [ $# -eq 1 ]; then MADE_ID="$1"; else fail "box: several new timers appeared ($NEW), cannot tell which one is this script's; check them by hand"; fi;;
esac
PENDING_ADD=""
if [ -n "$MADE_ID" ]; then
	[ "`tfield "$T/tl" "$MADE_ID" start`" = "`expr $START - 180`" ] && [ "`tfield "$T/tl" "$MADE_ID" stop`" = "`expr $STOP + 420`" ] && [ "`tfield "$T/tl" "$MADE_ID" announce`" = "`expr $ANNOUNCE - 180`" ] \
		&& pass "box: timer $MADE_ID holds start-180, stop+420, announce-180" \
		|| fail "box: timer $MADE_ID times `tfield "$T/tl" "$MADE_ID" start` `tfield "$T/tl" "$MADE_ID" stop` `tfield "$T/tl" "$MADE_ID" announce` (wanted `expr $START - 180` `expr $STOP + 420` `expr $ANNOUNCE - 180`)"
fi

# 7. the client's list shows the same timer, stamped with the box's margins
"$CLIENT" "$ANON" timers > "$T/o7" 2> "$T/e7"
N1=`grep -c '^timer ' "$T/o7"`
[ "`result "$T/o7"`" = OK ] && [ -n "$MADE_ID" ] && grep -q "^timer id=$MADE_ID .*channel_id=$CH_ID .*rem_pre=180 rem_post=420" "$T/o7" \
	&& pass "client: row $MADE_ID is in the client's list with the box's margins (list $N0 -> $N1)" \
	|| fail "client timers after add: `result "$T/o7"` $N0 -> $N1; `grep "^timer id=$MADE_ID " "$T/o7"`"

# 8. a change: refused with nothing to present and the times untouched,
# then over PATCH with the credential, and the box holds the new times as sent
START2=`expr $START + 600`; STOP2=`expr $STOP + 600`; ANNOUNCE2=`expr $ANNOUNCE + 600`
if [ -n "$MADE_ID" ]; then
	"$CLIENT" "$ANON" modify "$MADE_ID" "$START2" "$STOP2" "$ANNOUNCE2" > "$T/o8a" 2> "$T/e8a"
	$CURL -o "$T/t8a" "$BASE/api/v1/timers"
	[ "`result "$T/o8a"`" != OK ] && [ "`tfield "$T/t8a" "$MADE_ID" start`" = "`expr $START - 180`" ] \
		&& pass "client: a change with no credential is refused (`result "$T/o8a"`), times untouched" \
		|| fail "client modify with no credential: `head -1 "$T/o8a"` box start `tfield "$T/t8a" "$MADE_ID" start`"
	"$CLIENT" "$BOX" modify "$MADE_ID" "$START2" "$STOP2" "$ANNOUNCE2" > "$T/o8" 2> "$T/e8"
	$CURL -o "$T/t8" "$BASE/api/v1/timers"
	[ "`result "$T/o8"`" = OK ] && [ "`tfield "$T/t8" "$MADE_ID" start`" = "$START2" ] && [ "`tfield "$T/t8" "$MADE_ID" stop`" = "$STOP2" ] && [ "`tfield "$T/t8" "$MADE_ID" announce`" = "$ANNOUNCE2" ] \
		&& pass "client: change over PATCH with the credential, box holds the new times unwidened" \
		|| fail "client modify: `head -1 "$T/o8"` box `tfield "$T/t8" "$MADE_ID" start` `tfield "$T/t8" "$MADE_ID" stop` `tfield "$T/t8" "$MADE_ID" announce`"
else
	fail "client modify: nothing to change"
fi

# 9. a wrong password is the refusal the caller is told about, no credential
# is refused as well, and the timer stays both times
if [ -n "$MADE_ID" ]; then
	"$CLIENT" "$AUTH_USER:definitely-wrong@$HOSTPORT" remove "$MADE_ID" > "$T/o9" 2> "$T/e9"
	$CURL -o "$T/t9" "$BASE/api/v1/timers"
	[ "`result "$T/o9"`" = NOT_PERMITTED ] && [ "`httpcode "$T/o9"`" = 401 ] && [ "`tfield "$T/t9" "$MADE_ID" id`" = "$MADE_ID" ] \
		&& pass "client: wrong password -> NOT_PERMITTED (401), timer untouched" \
		|| fail "client wrong password: `head -1 "$T/o9"` timer still there: `tfield "$T/t9" "$MADE_ID" id`"
	"$CLIENT" "$ANON" remove "$MADE_ID" > "$T/o9b" 2> "$T/e9b"
	$CURL -o "$T/t9b" "$BASE/api/v1/timers"
	[ "`result "$T/o9b"`" != OK ] && [ "`tfield "$T/t9b" "$MADE_ID" id`" = "$MADE_ID" ] \
		&& pass "client: a removal with no credential is refused (`result "$T/o9b"`), timer untouched" \
		|| fail "client remove with no credential: `head -1 "$T/o9b"` timer still there: `tfield "$T/t9b" "$MADE_ID" id`"
fi

# 10. removing a timer the box does not hold, behind a login
"$CLIENT" "$BOX" remove 999999999 > "$T/o10" 2> "$T/e10"
[ "`result "$T/o10"`" = NOT_THERE ] && [ "`httpcode "$T/o10"`" = 404 ] && pass "client: unknown timer -> NOT_THERE (404) after the login" \
	|| fail "client remove unknown: `head -1 "$T/o10"`"

# 11. the timer is taken back and the box agrees; the box's list has to be
# readable before an id that is not in it counts as removed
if [ -n "$MADE_ID" ]; then
	"$CLIENT" "$BOX" remove "$MADE_ID" > "$T/o11" 2> "$T/e11"
	$CURL -o "$T/t11" "$BASE/api/v1/timers"
	"$CLIENT" "$ANON" timers > "$T/o11b" 2>/dev/null
	N2=`grep -c '^timer ' "$T/o11b"`
	if [ "`result "$T/o11"`" = OK ] && [ "`httpcode "$T/o11"`" = 204 ] && [ -n "`jq1 "$T/t11" items`" ] && [ -z "`tfield "$T/t11" "$MADE_ID" id`" ] \
		&& [ "`result "$T/o11b"`" = OK ] && ! grep -q "^timer id=$MADE_ID " "$T/o11b"; then
		pass "client: timer removed (204), gone from the box's list and from the client's (list $N1 -> $N2)"
		MADE_ID=""
	else
		fail "client remove: `head -1 "$T/o11"` box list readable: `jq1 "$T/t11" items | cut -c1-20`; box still lists: `tfield "$T/t11" "$MADE_ID" id`; client list `result "$T/o11b"` $N2"
	fi
fi

finish
