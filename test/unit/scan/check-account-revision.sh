#!/bin/sh
# A password checked against one account opens nothing of another. The login
# route reads the account in effect, derives the key for the password it was
# sent, and opens a session; a change to the account can land during the
# derivation, which runs without a lock and with nothing a case can stop at.
# What makes such a login open nothing is one line: the session is opened under
# the revision read together with the stored form that was checked, and not under
# whatever is in effect afterwards. It is held here as text. In login() of
# src/httpd/ep/ep_auth.cpp:
#
#  - the account is read once, as const Login cfg = loginInEffect(), before the
#    password is checked (verifySecret);
#  - the session is opened once, after that check, as
#    openSession(cfg.username, cfg.revision).
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
AUTH="$SRC/src/httpd/ep/ep_auth.cpp"
[ -n "$SRC" ] && [ -r "$AUTH" ] || {
	echo "usage: check-account-revision.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-account-revision.sh: cannot read $STRIP" >&2; exit 1; }

awk -v keepstrings=0 -f "$STRIP" "$AUTH" | awk '
	function fail(what) { printf "check-account-revision.sh: login() %s\n", what > "/dev/stderr"; bad = 1 }
	!inside && !done && /^Response[ \t]+login[ \t]*\([ \t]*const[ \t]+Request[ \t]*&/ { inside = 1 }
	inside {
		if ($0 ~ /loginInEffect[ \t]*\(/) {
			reads++
			if ($0 !~ /const[ \t]+Login[ \t]+cfg[ \t]*=[ \t]*loginInEffect[ \t]*\([ \t]*\)[ \t]*;/)
				fail("reads the account other than into const Login cfg")
			if (checks)
				fail("reads the account after it checked the password")
		}
		if ($0 ~ /verifySecret[ \t]*\(/) {
			checks++
			if ($0 !~ /cfg\.password_hash/)
				fail("checks the password against something other than cfg")
		}
		if ($0 ~ /openSession[ \t]*\(/) {
			opens++
			if ($0 !~ /openSession[ \t]*\([ \t]*cfg\.username[ \t]*,[ \t]*cfg\.revision[ \t]*\)/)
				fail("opens the session under something other than the account it checked: " $0)
			if (!checks)
				fail("opens the session before it checked the password")
		}
		opens_b = gsub(/\{/, "{")
		closes_b = gsub(/\}/, "}")
		depth += opens_b - closes_b
		if (opens_b > 0)
			body = 1
		if (body && depth == 0) {
			inside = 0
			done = 1
		}
	}
	END {
		if (!done)
			fail("was not found, so the scan has stopped matching")
		if (reads != 1)
			fail("reads the account " reads + 0 " times, not once")
		if (checks != 1)
			fail("checks the password " checks + 0 " times, not once")
		if (opens != 1)
			fail("opens a session " opens + 0 " times, not once")
		exit bad
	}
'
