#!/bin/sh
# /control/config takes a change to the account only where login_bridge::mayChange
# says so and, under the account's lock, the transaction it then opens says so
# again (login_bridge::Transaction::admits). A request either of them refuses writes
# nothing: no configuration file, not the account, and it reloads nothing. The other
# keys of nhttpd.conf are written only once the account went through, inside the same
# transaction. The handler needs the whole application and cannot be linked into the
# test binary, which drives the rule itself (test_login_bridge.cpp); the decision and
# the order at the call site are held here, read as text. In ConfigCGI:
#
#  - login_bridge::mayChange is asked exactly once, negated, and the statement it
#    guards assigns error;
#  - one login_bridge::Transaction is made, after that ask, and its admits is asked
#    exactly once after that, negated, the statement it guards assigning error;
#  - a transaction whose admits refused is given up again before refused is set,
#    so a refused request holds the account's lock no longer than the look takes;
#  - refused is assigned exactly once, as !error.empty(), after both asks;
#  - all of that comes before ServerFileLock is taken, so no key derivation runs
#    under that lock and the account's lock is taken first, and before every write;
#  - every write (a configuration file saved, the account changed, the server's
#    configuration reloaded) stands in a block whose opening line tests !refused;
#  - the account is changed through the transaction before nhttpd.conf is saved,
#    and that save is guarded by error.empty(), so it follows only an account that
#    went through;
#  - after that change the new cookie of the session that asked is sent, once and
#    before the save: HookVarList["Set-Cookie"] set from
#    login_bridge::sessionCookie(account->session());
#  - the transaction is given up after that save and before the server's
#    configuration is reloaded, which reads nhttpd.conf under its lock.
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
		# the statement a refusing ask guards, on its own line or on the ask line
		if (want_error && $0 !~ /^[ \t]*\{?[ \t]*$/) {
			if ($0 !~ /^[ \t]*\{?[ \t]*error[ \t]*=[^=]/)
				fail("does not take the refusal of " want_error " into error")
			want_error = ""
		}
		if ($0 ~ /login_bridge::mayChange[ \t]*\(/) {
			asks++
			if ($0 !~ /![ \t]*login_bridge::mayChange[ \t]*\(/)
				fail("asks login_bridge::mayChange without refusing on its answer")
			else if ($0 !~ /\)[ \t]*error[ \t]*=[^=]/)
				want_error = "login_bridge::mayChange"
			if (locks || writes)
				fail("asks login_bridge::mayChange after it took the lock or wrote something")
		}
		if ($0 ~ /new[ \t]+login_bridge::Transaction/) {
			made++
			if (!asks || sets || locks || writes)
				fail("opens the transaction before it asks login_bridge::mayChange, or after it decided, took the lock or wrote something")
		}
		if ($0 ~ /->[ \t]*admits[ \t]*\(/) {
			admits++
			if ($0 !~ /![ \t]*[A-Za-z_][A-Za-z_0-9]*[ \t]*->[ \t]*admits[ \t]*\(/)
				fail("asks the transaction without refusing on its answer")
			else if ($0 !~ /\)[ \t]*error[ \t]*=[^=]/)
				want_error = "the transaction"
			if (!made || sets || locks || writes)
				fail("asks the transaction before it opened one, or after it decided, took the lock or wrote something")
		}
		if ($0 ~ /delete[ \t]+account[ \t]*;/ && admits && !sets)
			let_go++
		if ($0 ~ /(^|[^A-Za-z0-9_])refused[ \t]*=([^=]|$)/) {
			sets++
			if ($0 !~ /const[ \t]+bool[ \t]+refused[ \t]*=[ \t]*![ \t]*error\.empty[ \t]*\([ \t]*\)[ \t]*;/)
				fail("sets refused to something other than !error.empty()")
			if (!asks || !admits || locks || writes)
				fail("sets refused before it asks login_bridge::mayChange and the transaction, or after the lock or a write")
		}
		if ($0 ~ /new[ \t]+httpd::ServerFileLock/) {
			locks++
			if (!asks || !admits || !sets)
				fail("takes ServerFileLock before it has decided")
		}
		if ($0 ~ /account[ \t]*->[ \t]*change[ \t]*\(/) changes++
		# the comment blanker leaves a literal as one digit, so the header name is
		# read as the one literal a HookVarList index can be here
		if ($0 ~ /HookVarList[ \t]*\[/) {
			cookies++
			if ($0 !~ /HookVarList[ \t]*\[[ \t]*[0-9][ \t]*\][ \t]*=[ \t]*login_bridge::sessionCookie[ \t]*\([ \t]*account[ \t]*->[ \t]*session[ \t]*\([ \t]*\)[ \t]*\)/)
				fail("sends a cookie other than the one the transaction handed the session that asked: " $0)
			if (!changes || saves_server)
				fail("sends the cookie before the account changed or after nhttpd.conf was saved")
		}
		if ($0 ~ /httpd::saveServerFile[ \t]*\(/) {
			saves_server++
			if (!changes)
				fail("saves nhttpd.conf before it changed the account")
			if ($0 !~ /error\.empty[ \t]*\([ \t]*\)[ \t]*&&[ \t]*![ \t]*httpd::saveServerFile[ \t]*\(/)
				fail("saves nhttpd.conf without asking whether the account went through")
		}
		if ($0 ~ /delete[ \t]+account[ \t]*;/ && saves_server)
			released++
		if ($0 ~ /yhttpd_reload_config[ \t]*\(/) {
			reloads++
			if (!released)
				fail("reloads the server configuration while it holds the account transaction")
		}
		if ($0 ~ /saveConfig[ \t]*\(/) saves_config++
		if ($0 ~ /saveSetup[ \t]*\(/) saves_setup++
		if ($0 ~ /httpd::saveServerFile[ \t]*\(|account[ \t]*->[ \t]*change[ \t]*\(|yhttpd_reload_config[ \t]*\(|saveConfig[ \t]*\(|saveSetup[ \t]*\(/) {
			writes++
			if (!asks || !admits || !sets)
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
		if (made != 1)
			fail("opens the account transaction " made + 0 " times, not once")
		if (admits != 1)
			fail("asks the transaction " admits + 0 " times, not once")
		if (sets != 1)
			fail("sets refused " sets + 0 " times, not once")
		if (!let_go)
			fail("keeps the transaction its second look refused until the end of the handler")
		if (want_error != "")
			fail("ends before the statement " want_error " guards")
		if (cookies != 1)
			fail("sends the cookie of the session that asked " cookies + 0 " times, not once")
		if (!locks || !saves_server || !changes || !reloads || !saves_config || !saves_setup)
			fail("no longer locks, saves, changes the account and reloads where the scan looks")
		exit bad
	}
'

# The header the rule above sees only as a blanked literal, read off the text.
grep -qF 'hh->HookVarList["Set-Cookie"] = login_bridge::sessionCookie(account->session());' "$FILE" || {
	echo "check-control-account.sh: ConfigCGI no longer sends the new cookie as Set-Cookie" >&2
	exit 1
}
