#!/bin/sh
# The record manager is stopped at the end of the program, never deleted:
# the web server's, the stream server's and the front displays' threads ask
# it until the process exits. Every end - ExitRun(), the flash path in
# stop_daemons() and the handler of SIGTERM and SIGINT - calls
# CRecordManager::Shutdown(), which stops the recordings under the
# manager's lock and only tries that lock for a while: the thread that ends
# the program may hold it already. So:
#  1. nothing deletes the manager, and its destructor is private;
#  2. the three ends call Shutdown(), the handler after it marked the end as
#     running and before it stops the daemons;
#  3. Shutdown() tries the lock with a limit, never waits for it, and gives
#     it back.
# The program cannot be linked into the test binary, so it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-record-shutdown.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
F=src/neutrino.cpp
R=src/driver/record.cpp
H=src/driver/record.h
for f in $F $R $H; do
	[ -r "$SRC/$f" ] || { echo "check-record-shutdown.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-record-shutdown.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SRC/$F" > "$tmp/src"
awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SRC/$R" > "$tmp/record"
awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SRC/$H" > "$tmp/header"
body() {
	awk -v start="$2" 'index($0, start) == 1 && $0 !~ /;[[:blank:]]*$/ { f = 1 } f { print } f && /^}/ { exit }' "$1"
}
first() {
	grep -n -E -- "$2" "$1" | head -1 | cut -d: -f1
}

# 1. Nothing deletes the manager; the destructor is private.
if grep -r -n -E 'delete[[:blank:]]+CRecordManager::getInstance\(\)' "$SRC/src" > "$tmp/hits"; then
	echo "the record manager is deleted, under threads that ask it until the process exits:" >&2
	sed "s|^$SRC/||; s/^/  /" "$tmp/hits" >&2
	fail=1
fi
awk '/^class CRecordManager/ { f = 1 } f && /^[[:blank:]]*public:/ { exit } f { print }' "$tmp/header" |
	grep -q -E '^[[:blank:]]*~CRecordManager\(\);' ||
	{ echo "$H: the record manager's destructor is not private" >&2; fail=1; }

# 2. The three ends stop it.
call='CRecordManager::getInstance\(\)->Shutdown\(\);'
body "$tmp/src" 'void CNeutrinoApp::ExitRun(' | grep -q -E "$call" ||
	{ echo "$F: ExitRun does not stop the record manager" >&2; fail=1; }
body "$tmp/src" 'void stop_daemons(' | awk '/if[[:blank:]]*\([[:blank:]]*for_flash[[:blank:]]*\)[[:blank:]]*\{/ { f = 1 } f { print } f && /^\t}/ { exit }' |
	grep -q -E "$call" || { echo "$F: the flash path does not stop the record manager" >&2; fail=1; }
body "$tmp/src" 'void sighandler' > "$tmp/handler"
m=`first "$tmp/handler" 'shutdown_in_progress[[:blank:]]*=[[:blank:]]*1'`
s=`first "$tmp/handler" "$call"`
d=`first "$tmp/handler" 'stop_daemons[[:blank:]]*\('`
if [ -z "$m" ] || [ -z "$s" ] || [ -z "$d" ] || [ "$m" -gt "$s" ] || [ "$s" -gt "$d" ]; then
	echo "$F: the signal handler does not stop the record manager between marking the end and stopping the daemons" >&2
	fail=1
fi

# 3. The lock is tried with a limit, never waited for, and given back.
body "$tmp/record" 'bool CRecordManager::Shutdown(' > "$tmp/shutdown"
grep -q -E 'for[[:blank:]]*\([[:blank:]]*int i = 0; i < [0-9]+ && !locked; i\+\+\)' "$tmp/shutdown" &&
	grep -q -E 'locked = mutex\.trylock\(\) == 0;' "$tmp/shutdown" &&
	grep -q -E 'return false;' "$tmp/shutdown" &&
	! grep -q -E 'mutex\.lock\(\)' "$tmp/shutdown" &&
	[ `grep -c -E 'mutex\.unlock\(\)' "$tmp/shutdown"` -eq 1 ] ||
	{ echo "$R: CRecordManager::Shutdown waits for the lock or does not give it back" >&2; fail=1; }

[ $fail -eq 0 ] && echo "check-record-shutdown.sh: every end stops the record manager, trying its lock with a limit, and nothing deletes it"
exit $fail
