#!/bin/sh
# /control/config takes a change to the account only where login_bridge::mayChange
# says so, and a request it refuses writes nothing: no configuration file, not the
# account, and it reloads nothing. The handler needs the whole application and
# cannot be linked into the test binary, which drives the rule itself
# (test_login_bridge.cpp); the decision and the order at the call site are held
# here, read as text. In ConfigCGI:
#
#  - login_bridge::mayChange is asked exactly once, negated, and the statement it
#    guards assigns error;
#  - refused is assigned exactly once, as !error.empty(), after that ask;
#  - both come before ServerFileLock is taken, so no key derivation runs under
#    that lock, and before every write;
#  - every write (a configuration file saved, the account changed, the server's
#    configuration reloaded) stands in a block whose opening line tests !refused.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
FILE="$SRC/src/nhttpd/tuxboxapi/controlapi.cpp"
[ -n "$SRC" ] && [ -r "$FILE" ] || {
	echo "usage: check-control-account.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-control-account.sh: cannot read $STRIP" >&2; exit 1; }

awk -v keepstrings=0 -f "$STRIP" "$FILE" | awk '
	function fail(what) { printf "check-control-account.sh: ConfigCGI %s\n", what > "/dev/stderr"; bad = 1 }
	!inside && !done && /^void[ \t]+CControlAPI::ConfigCGI[ \t]*\(/ { inside = 1 }
	inside {
		# the statement the refusing ask guards, on its own line or on the ask line
		if (want_error && $0 !~ /^[ \t]*\{?[ \t]*$/) {
			if ($0 !~ /^[ \t]*\{?[ \t]*error[ \t]*=[^=]/)
				fail("does not take the refusal of login_bridge::mayChange into error")
			want_error = 0
		}
		if ($0 ~ /login_bridge::mayChange[ \t]*\(/) {
			asks++
			if ($0 !~ /![ \t]*login_bridge::mayChange[ \t]*\(/)
				fail("asks login_bridge::mayChange without refusing on its answer")
			else if ($0 !~ /\)[ \t]*error[ \t]*=[^=]/)
				want_error = 1
			if (locks || writes)
				fail("asks login_bridge::mayChange after it took the lock or wrote something")
		}
		if ($0 ~ /(^|[^A-Za-z0-9_])refused[ \t]*=([^=]|$)/) {
			sets++
			if ($0 !~ /const[ \t]+bool[ \t]+refused[ \t]*=[ \t]*![ \t]*error\.empty[ \t]*\([ \t]*\)[ \t]*;/)
				fail("sets refused to something other than !error.empty()")
			if (!asks || locks || writes)
				fail("sets refused before it asks login_bridge::mayChange, or after the lock or a write")
		}
		if ($0 ~ /new[ \t]+httpd::ServerFileLock/) {
			locks++
			if (!asks || !sets)
				fail("takes ServerFileLock before it has decided")
		}
		if ($0 ~ /httpd::saveServerFile[ \t]*\(/) saves_server++
		if ($0 ~ /login_bridge::change[ \t]*\(/) changes++
		if ($0 ~ /yhttpd_reload_config[ \t]*\(/) reloads++
		if ($0 ~ /saveConfig[ \t]*\(/) saves_config++
		if ($0 ~ /saveSetup[ \t]*\(/) saves_setup++
		if ($0 ~ /httpd::saveServerFile[ \t]*\(|login_bridge::change[ \t]*\(|yhttpd_reload_config[ \t]*\(|saveConfig[ \t]*\(|saveSetup[ \t]*\(/) {
			writes++
			if (!asks || !sets)
				fail("writes before it has decided: " $0)
			if (!guarded)
				fail("writes outside the block for a request that was not refused: " $0)
		}
		opens = gsub(/\{/, "{")
		closes = gsub(/\}/, "}")
		if (!guarded && opens > 0 && $0 ~ /![ \t]*refused/) {
			guarded = 1
			guard_depth = depth
		}
		depth += opens - closes
		if (opens > 0)
			body = 1
		if (guarded && depth <= guard_depth)
			guarded = 0
		if (body && depth == 0) {
			inside = 0
			done = 1
		}
	}
	END {
		if (!done)
			fail("was not found, so the scan has stopped matching")
		if (asks != 1)
			fail("asks login_bridge::mayChange " asks + 0 " times, not once")
		if (sets != 1)
			fail("sets refused " sets + 0 " times, not once")
		if (want_error)
			fail("ends before the statement login_bridge::mayChange guards")
		if (!locks || !saves_server || !changes || !reloads || !saves_config || !saves_setup)
			fail("no longer locks, saves, changes the account and reloads where the scan looks")
		exit bad
	}
'
