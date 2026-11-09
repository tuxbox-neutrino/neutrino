#!/bin/sh
# check-file-answers.sh - a file nhttpd's own handlers send by a name somebody
# else can steer goes out of a descriptor that was checked
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# A handler can answer with a file by name (hh->SendFile), and the transport
# then opens that name itself; or it can open the file, check what it opened
# and hand the descriptor on (hh->SendOpenedFile). None of the files that hold
# who may reach the box may go out either way, whatever the name is: a fixed
# name can be a link or a second name of one of them. The transport and the
# control API's handlers need the whole program and cannot all be linked into
# the test binary, so the rule is held here, read as text with comments,
# literals and #if 0 blocks out:
#
#  - the transport opens a file a hook named through ystatic::openToSend,
#    before the answer's header, and sends that descriptor; it opens no file
#    by name anywhere else;
#  - LogoCGI, whose file a channel list names, opens it through
#    ystatic::openToSend and sends that descriptor with SendOpenedFile;
#  - the server reads each of its three web folders through
#    ystatic::rootRefused before it serves anything from them.
#
# usage: check-file-answers.sh <top_srcdir>
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC/src/nhttpd" ] || {
	echo "usage: check-file-answers.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
IF0="$HERE/blank-if0.awk"
CONTROL="$SRC/src/nhttpd/tuxboxapi/controlapi.cpp"
for f in "$STRIP" "$IF0" "$CONTROL"; do
	[ -r "$f" ] || { echo "check-file-answers.sh: cannot read $f" >&2; exit 1; }
done

RESPONSE="$SRC/src/nhttpd/yhttpd_core/yresponse.cpp"
[ -r "$RESPONSE" ] || { echo "check-file-answers.sh: cannot read $RESPONSE" >&2; exit 1; }
fail=0
# The transport's answer from a file: from the test of HANDLED_SENDFILE that
# sends it to the end of that block.
awk -v keepstrings=0 -f "$STRIP" "$RESPONSE" | awk -f "$IF0" | awk '
	!inside && !done && prev ~ /status[ \t]*==[ \t]*HANDLED_SENDFILE/ && /RequestCanceled/ { inside = 1 }
	inside {
		if ($0 ~ /ystatic::openToSend[ \t]*\(/ && !header) opens++
		if ($0 ~ /BuildHeader[ \t]*\(/) header = 1
		if ($0 ~ /SendFile[ \t]*\(/ && $0 ~ /SendFileFd|filed/) sends++
		o = gsub(/\{/, "{")
		c = gsub(/\}/, "}")
		depth += o - c
		if (o > 0) body = 1
		if (body && depth == 0) { inside = 0; done = 1 }
	}
	# no file opened by its name anywhere else in the transport
	!inside && /(^|[^A-Za-z0-9_:.>])(open|fopen)[ \t]*\(/ { byname++ }
	/CWebserverResponse::Sendfile[ \t]*\(/ { byname++ }
	{ prev = $0 }
	END {
		if (!done) { print "check-file-answers.sh: the transport'"'"'s answer from a file was not found, so the scan has stopped matching" > "/dev/stderr"; exit 1 }
		if (opens != 1) { print "check-file-answers.sh: the transport does not open a file a hook named through ystatic::openToSend before its header" > "/dev/stderr"; exit 1 }
		if (sends < 1) { print "check-file-answers.sh: the transport does not send the descriptor it opened" > "/dev/stderr"; exit 1 }
		if (byname) { print "check-file-answers.sh: the transport opens a file by its name past ystatic::openToSend" > "/dev/stderr"; exit 1 }
	}
' || fail=1

# The server reads its web folders through ystatic::rootRefused, all three of
# them, before anything is served from them.
YHTTPD="$SRC/src/nhttpd/yhttpd.cpp"
[ -r "$YHTTPD" ] || { echo "check-file-answers.sh: cannot read $YHTTPD" >&2; exit 1; }
awk -v keepstrings=1 -f "$STRIP" "$YHTTPD" | awk -f "$IF0" > "${TMPDIR:-/tmp}/check-file-answers.$$"
for key in WebsiteMain.directory WebsiteMain.override_directory WebsiteMain.hosted_directory; do
	grep -q -F "{ \"$key\"," "${TMPDIR:-/tmp}/check-file-answers.$$" || {
		echo "check-file-answers.sh: yhttpd.cpp does not hold $key to ystatic::rootRefused" >&2
		fail=1
	}
done
grep -q -E 'ystatic::rootRefused[[:blank:]]*\([[:blank:]]*ConfigList\[roots\[i\]\[0\]\]' "${TMPDIR:-/tmp}/check-file-answers.$$" || {
	echo "check-file-answers.sh: yhttpd.cpp reads its web folders without ystatic::rootRefused" >&2
	fail=1
}
rm -f "${TMPDIR:-/tmp}/check-file-answers.$$"

# LogoCGI, from its opening line to the end of its body
awk -v keepstrings=0 -f "$STRIP" "$CONTROL" | awk -f "$IF0" | awk '
	!inside && !done && /^void[ \t]+CControlAPI::LogoCGI[ \t]*\(/ { inside = 1 }
	inside {
		if ($0 ~ /ystatic::openToSend[ \t]*\(/) opens++
		if ($0 ~ /->SendOpenedFile[ \t]*\(/) sends++
		if ($0 ~ /(^|[^A-Za-z0-9_:])(open|fopen|stat)[ \t]*\(/) byname++
		o = gsub(/\{/, "{")
		c = gsub(/\}/, "}")
		depth += o - c
		if (o > 0) body = 1
		if (body && depth == 0) { inside = 0; done = 1 }
	}
	END {
		if (!done) { print "check-file-answers.sh: LogoCGI was not found, so the scan has stopped matching" > "/dev/stderr"; exit 1 }
		if (opens != 1) { print "check-file-answers.sh: LogoCGI does not open its file through ystatic::openToSend" > "/dev/stderr"; exit 1 }
		if (sends != 1) { print "check-file-answers.sh: LogoCGI does not send the descriptor it opened (SendOpenedFile)" > "/dev/stderr"; exit 1 }
		if (byname) { print "check-file-answers.sh: LogoCGI looks at or opens its file by name besides ystatic::openToSend" > "/dev/stderr"; exit 1 }
	}
' || fail=1

exit $fail
