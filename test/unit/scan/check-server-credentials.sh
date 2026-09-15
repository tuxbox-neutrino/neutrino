#!/bin/sh
# credentialsOf() is what serve() calls to turn a connection into what a request
# carried (server.cpp), and it is the only place either credential reader is
# called: bearerToken for the Bearer scheme, basicCredential for Basic, both fed
# by one lookup of the Authorization header into authz. None of the three has a
# case that goes red if it is broken.
#
# A deleted bearerToken(authz) fails an API token nobody sees fail: granted()
# falls back to whatever the address alone is worth, and third-party clients and
# plugins holding one stop working with no case here noticing.
#
# A deleted basicCredential(authz) fails the same way for Basic, and the caller it
# breaks is two boxes syncing timers over /control/ (timerlist.cpp:815): every
# socket this suite can open is loopback, and loopback reaches every level
# regardless of what credentialsOf() found.
#
# A quieter way to the same failure than deleting either call: keep it, and drop
# what it returns on the floor. "(void) basicCredential(authz);" calls the reader,
# satisfies a check that only greps for the call, and leaves c.basic exactly as
# default-constructed. So the two calls are read for below in the one shape that
# reaches credentialsOf()'s caller, the assignment into the field granted() goes on
# to read, not the bare call.
#
# A third way: point authz at a header that is not Authorization. Nine existing
# Bearer cases happen to catch that today, but a check that never reads what feeds
# the calls guards a narrower thing than its own name says.
#
# A fourth way survives even the assignment check above: leave the one if this body
# already opens with alone, and nest a second, always-false condition around just
# one assignment inside it, for example
#   if (authz != NULL)
#   {
#       c.bearer_token = bearerToken(authz);
#       if (authz != NULL && false)
#           c.basic = basicCredential(authz);
#   }
# Both required strings are still there and neither line ever runs. Nothing here
# requires the second gate to be another "if": a "for (;false;)", a "while
# (false)", a "switch" whose only case does not match, a "do" around a "while (0)",
# a "catch" behind a "try" that throws nothing and a lambda body nothing calls all
# hide the same assignment behind the same kind of dead branch. Writing that list
# out shows it is the wrong way round: what belongs in that span is short and
# known, so it is what belongs that is read for and everything else that is
# refused. c.bearer_token's reachability already has a live case (test_auth.cpp's
# "a bearer token off the wire is worth what it was minted at"); c.basic has none,
# because every socket here is loopback, so its reachability can only be shown as
# text: nothing may sit between the if this body opens with and either assignment.
#
# A fifth way needs no second condition at all: wrap the assignment in #if 0. So
# #if 0 is blanked before any of this is read (blank-if0.awk).
#
# All five fail silently at run time and none is provable red by a case.
set -e
FILE="$1"
[ -n "$FILE" ] && [ -r "$FILE" ] || {
	echo "usage: check-server-credentials.sh <server.cpp>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-server-credentials.sh: cannot read $STRIP" >&2; exit 1; }
BLANK="$HERE/blank-if0.awk"
[ -r "$BLANK" ] || { echo "check-server-credentials.sh: cannot read $BLANK" >&2; exit 1; }

# credentialsOf()'s own body, braces balanced on the comment- and
# string-stripped source (keepstrings=0), the same technique
# check-legacy-levels.sh uses to isolate one function among several. #if 0 is
# blanked behind the comments and not before them, so a "#if 0" written inside
# a comment cannot blank live code below it.
BODY=`awk -v keepstrings=0 -f "$STRIP" "$FILE" | awk -f "$BLANK" | awk '
	found == 0 && /^Credentials credentialsOf\(/ { found = 1 }
	found == 1 {
		print
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") depth++
			else if (c == "}") { depth--; if (depth == 0) exit }
		}
	}
'`

[ -n "$BODY" ] || { echo "no credentialsOf() definition found in $FILE" >&2; exit 1; }

# Runs of whitespace collapsed to one space and the whole body joined onto
# one line, so a declaration split across two lines - the shape it is
# written in today - reads the same as one written on a single line.
FLAT=`printf '%s\n' "$BODY" | tr '\n' ' ' | tr -s '[:space:]' ' '`

STATUS=0

LOOKUP="authz = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_AUTHORIZATION)"
BEARER="c.bearer_token = bearerToken(authz)"
BASIC="c.basic = basicCredential(authz)"

printf '%s' "$FLAT" | grep -q -F "$LOOKUP" || {
	echo "credentialsOf() in $FILE no longer reads authz from MHD_HTTP_HEADER_AUTHORIZATION" >&2
	STATUS=1
}

# Against FLAT rather than BODY, the same reason the header lookup above
# already is: an assignment split across two lines (the field on one, the
# call on the next) is the same assignment as one written on a single line,
# and collapsing runs of whitespace first is what makes the two read alike.
printf '%s' "$FLAT" | grep -q -F "$BEARER" || {
	echo "credentialsOf() in $FILE no longer assigns c.bearer_token = bearerToken(authz)" >&2
	STATUS=1
}

printf '%s' "$FLAT" | grep -q -F "$BASIC" || {
	echo "credentialsOf() in $FILE no longer assigns c.basic = basicCredential(authz)" >&2
	STATUS=1
}

# The fourth way (see the file comment above): a second, always-false gate nested
# between the header lookup and an assignment it is meant to reach directly leaves
# the assignment's own text untouched.
#
# What belongs in that span is short and known: the one if that guards both
# assignments, the brace it opens, the other of the two assignments, and the
# semicolons and spaces between them. So that is what is taken out, and whatever is
# left over is the finding. Read this way round rather than as a list of the
# spellings a dead branch could be written in, because such a list leaves one over
# every time it is drawn up: "if (", "for (", "while (" and "switch (" were the
# four it once held, and a "do" around a "while (0)", a "catch" behind a "try" that
# throws nothing, a lambda body nothing calls and a labelled block all hide the
# same assignment while being none of them.
#
# The if is taken out whatever its condition says, parentheses balanced so one
# holding a call of its own reads the same as one holding none. A second one is
# left exactly where it stands, which is the shape being looked for.
leftover() {
	printf '%s' "$FLAT" | awk -v anchor="$LOOKUP" -v target="$1" -v sibling="$2" '
	{
		s = $0
		a = index(s, anchor)
		t = index(s, target)
		if (a == 0 || t == 0) { print "?"; exit }
		start = a + length(anchor)
		span = (t > start) ? substr(s, start, t - start) : ""
		n = length(span)

		# the first "if" written as a word, and the parentheses behind it
		i = 0
		for (p = 1; p < n; p++) {
			if (substr(span, p, 2) != "if") continue
			if (p > 1 && substr(span, p - 1, 1) ~ /[A-Za-z0-9_]/) continue
			if (substr(span, p + 2, 1) ~ /[A-Za-z0-9_]/) continue
			q = p + 2
			while (substr(span, q, 1) == " ") q++
			if (substr(span, q, 1) != "(") continue
			i = p
			break
		}
		if (i > 0) {
			d = 0
			while (q <= n) {
				c = substr(span, q, 1)
				if (c == "(") d++
				else if (c == ")") { d--; if (d == 0) break }
				q++
			}
			if (q <= n) span = substr(span, 1, i - 1) substr(span, q + 1)
		}

		p = index(span, sibling)
		if (p > 0) span = substr(span, 1, p - 1) substr(span, p + length(sibling))

		sub(/\{/, "", span)
		gsub(/[ \t;]/, "", span)
		print span
	}'
}

# A "?" means the anchor or the assignment is not in this body at all, and
# each of those three strings is read for on its own above, so leaving it
# without a line of its own here passes nothing over in silence.
check_reachable() {
	rest=`leftover "$1" "$2"`
	[ "$rest" = "?" ] && return 0
	[ -n "$rest" ] || return 0
	echo "credentialsOf() in $FILE has \"$rest\" standing between the header lookup and $1: only the one if that guards it, the brace it opens and the other assignment belong in that gap, and whatever else is written into it can keep the line from ever running" >&2
	STATUS=1
}

check_reachable "$BEARER" "$BASIC"
check_reachable "$BASIC" "$BEARER"

exit $STATUS
