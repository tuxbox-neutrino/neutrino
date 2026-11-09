#!/bin/sh
# A password checked against one account opens or changes nothing of another.
# The login route and the old server's change to the account read the account,
# check a credential against it, and act; a change to the account can land
# during the check, which is a key derivation run without a lock and with nothing
# a case can stop at. What makes such a request act on nothing is that it acts
# under the revision read before the check, and not under whatever is in effect
# afterwards. Those lines are held here as text.
#
# In login() of src/httpd/ep/ep_auth.cpp:
#  - the account is read once, as const Login cfg = loginInEffect(), before the
#    password is checked (verifySecret);
#  - the session is opened once, after that check, as
#    openSession(cfg.username, cfg.revision).
#
# In login_bridge::mayChange of src/httpd/login_bridge.cpp:
#  - the revision is read once, as
#    const unsigned long long revision = httpd::accountRevision(), before the
#    session (sessionAdmits) and the Basic pair (basicAdmits) are asked;
#  - that reading is what it hands on, as admitted->revision = revision.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
AUTH="$SRC/src/httpd/ep/ep_auth.cpp"
BRIDGE="$SRC/src/httpd/login_bridge.cpp"
[ -n "$SRC" ] && [ -r "$AUTH" ] && [ -r "$BRIDGE" ] || {
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

awk -v keepstrings=0 -f "$STRIP" "$BRIDGE" | awk '
	function fail(what) { printf "check-account-revision.sh: login_bridge::mayChange %s\n", what > "/dev/stderr"; bad = 1 }
	!inside && !done && /^bool[ \t]+mayChange[ \t]*\(/ { inside = 1 }
	inside {
		if ($0 ~ /accountRevision[ \t]*\(/) {
			reads++
			if ($0 !~ /const[ \t]+unsigned[ \t]+long[ \t]+long[ \t]+revision[ \t]*=[ \t]*httpd::accountRevision[ \t]*\([ \t]*\)[ \t]*;/)
				fail("reads the revision other than into const unsigned long long revision: " $0)
			if (asks)
				fail("reads the revision after it asked the session or the Basic pair")
		}
		if ($0 ~ /sessionAdmits[ \t]*\(|basicAdmits[ \t]*\(/)
			asks++
		if ($0 ~ /->[ \t]*revision[ \t]*=[^=]/) {
			hands++
			if ($0 !~ /admitted[ \t]*->[ \t]*revision[ \t]*=[ \t]*revision[ \t]*;/)
				fail("hands on another revision than the one it read: " $0)
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
			fail("reads the revision " reads + 0 " times, not once")
		if (!asks)
			fail("no longer asks the session or the Basic pair where the scan looks")
		if (hands != 1)
			fail("hands the revision on " hands + 0 " times, not once")
		exit bad
	}
'
