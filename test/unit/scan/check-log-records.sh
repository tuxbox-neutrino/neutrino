#!/bin/sh
# check-log-records.sh - a part of a request reaches a log line only through
# the helpers that make it safe to stand there
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# The helpers in src/nhttpd/yhttpd_core/ylogging.h (namespace ylog) escape
# control characters, so one request is one record, leave the values of a
# query out, and put [redacted] in place of a credential. A log line is a
# printf whose arguments the compiler cannot tell the origin of, so the rule
# is held here, read as text with comments, literals and #if 0 blocks out:
#
#  - in every source of src/nhttpd, a call that writes a log line names none
#    of the request's own data (UrlData, HeaderList, ParameterList, ParamList,
#    GetHeader), none of the names its pieces are held under where they are
#    parsed and handed on, and not the answer (yresult), other than inside a
#    ylog:: call;
#  - inside a ylog:: call the helper fits what it is handed: text(), which
#    escapes and redacts nothing, is handed no value of a parameter or a
#    header and no raw line of a request (second, value, var_value,
#    ParameterList, ParamList, HeaderList, tmp_line); those go through
#    paramValue(), headerValue() or withheld();
#  - the values the parser and the control dispatcher list are withheld
#    (ylog::withheld): a query's values stay out of the log whatever they
#    are called;
#  - a log line names none of the configuration's own names (ConfigList,
#    the SSL files, the bind address, the group, the plugin folders, the
#    answer's MIME type) other than inside a ylog:: call;
#  - mod_weblog.cpp, whose whole work is records of requests, names the
#    request's own data only inside a ylog:: call, wherever it does;
#  - the call sites the rule was written for are still found, so a renamed
#    macro or a moved file does not leave a check on nothing.
#
# usage: check-log-records.sh <top_srcdir>
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC/src/nhttpd" ] || {
	echo "usage: check-log-records.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
IF0="$HERE/blank-if0.awk"
for f in "$STRIP" "$IF0" "$SRC/src/nhttpd/yhttpd_core/ylogging.h" "$SRC/src/nhttpd/yhttpd_mods/mod_weblog.cpp"; do
	[ -r "$f" ] || { echo "check-log-records.sh: cannot read $f" >&2; exit 1; }
done

grep -q '^namespace ylog' "$SRC/src/nhttpd/yhttpd_core/ylogging.h" || {
	echo "check-log-records.sh: ylogging.h no longer declares namespace ylog" >&2
	exit 1
}

tmp=`mktemp -d "${TMPDIR:-/tmp}/check-log-records.XXXXXX"`
trap 'rm -rf "$tmp"' EXIT

fail=0
seen="$tmp/seen"
: > "$seen"
for f in `cd "$SRC" && find src/nhttpd -name '*.cpp' | sort`; do
	awk -v keepstrings=0 -f "$STRIP" "$SRC/$f" | awk -f "$IF0" | awk -v file="$f" -v seen="$seen" '
		# the request, by the names the server keeps it under
		function own(s) {
			return s ~ /(^|[^A-Za-z0-9_])(UrlData|HeaderList|ParameterList|ParamList|GetHeader)([^A-Za-z0-9_]|$)/
		}
		# and the names its pieces are held under where they are parsed and
		# handed on: the request line, the header, a parameter, a name built
		# from the URL, a range, an uploaded or a script file name, the
		# function a /control/ call names, a key rcem was asked for, an
		# entry of a list walked by its iterator, and the answer itself
		function piece(s) {
			return s ~ /(^|[^A-Za-z0-9_])(rawbuffer|start_line|tmp_line|sheader|raw_header|post_header|param_string|header|name|value|var_name|var_value|url|target|filename|fullfilename|Filename|upload_filename|script|range|cookie|authorization|func_req|_keyname|first|second|yresult)([^A-Za-z0-9_]|$)/
		}
		# what the configuration names, which a box file or a request to
		# /control/config can have carry anything
		function configured(s) {
			return s ~ /(^|[^A-Za-z0-9_])(ConfigList|SSL_CA_file|SSL_pemfile|bindAddress|bind|groupname|PLUGIN_DIRS|ResponseMimeType)([^A-Za-z0-9_]|$)/
		}
		# a ylog::text() call handed a value or a raw line, which text()
		# would print as it is apart from its escapes
		function textOfValue(s,   at, j, d, c, arg) {
			while (match(s, /ylog::text[ \t]*\(/)) {
				at = RSTART + RLENGTH - 1
				d = 0
				for (j = at; j <= length(s); j++) {
					c = substr(s, j, 1)
					if (c == "(") d++
					else if (c == ")") { d--; if (d == 0) break }
				}
				arg = substr(s, at + 1, j - at - 1)
				if (arg ~ /(^|[^A-Za-z0-9_])(second|value|var_value|ParameterList|ParamList|HeaderList|tmp_line)([^A-Za-z0-9_]|$)/)
					return 1
				s = substr(s, j + 1)
			}
			return 0
		}
		# s with every ylog:: call, arguments and all, taken out
		function withoutYlog(s,   at, j, d, c) {
			while (match(s, /ylog::[A-Za-z_]+[ \t]*\(/)) {
				at = RSTART + RLENGTH - 1
				d = 0
				for (j = at; j <= length(s); j++) {
					c = substr(s, j, 1)
					if (c == "(") d++
					else if (c == ")") { d--; if (d == 0) break }
				}
				s = substr(s, 1, RSTART - 1) "Y" substr(s, j + 1)
			}
			return s
		}
		function balanced(s,   o, c) {
			o = gsub(/\(/, "(", s)
			c = gsub(/\)/, ")", s)
			return c >= o
		}
		function judge(stmt, ln,   rest) {
			calls++
			if (stmt ~ /ylog::/) helped++
			rest = withoutYlog(stmt)
			if (own(rest) || piece(rest)) {
				printf "check-log-records.sh: %s:%d writes a part of the request to a log without ylog::\n", file, ln > "/dev/stderr"
				bad = 1
			}
			if (configured(rest)) {
				printf "check-log-records.sh: %s:%d writes what the configuration names to a log without ylog::\n", file, ln > "/dev/stderr"
				bad = 1
			}
			if (textOfValue(stmt)) {
				printf "check-log-records.sh: %s:%d hands a value or a raw line to ylog::text(); paramValue, headerValue or withheld say it\n", file, ln > "/dev/stderr"
				bad = 1
			}
		}
		{
			line = $0
			if (file ~ /mod_weblog\.cpp$/ && own(withoutYlog(line))) {
				printf "check-log-records.sh: %s:%d builds a record from the request without ylog::\n", file, NR > "/dev/stderr"
				bad = 1
			}
			# a member printf (hh->printf) writes the answer, not a log
			if (!active && match(line, /(^|[^A-Za-z0-9_>.])(log_level_printfX?|log_printfX|aprintf|dprintf|printf|fprintf|puts|fputs|syslog)[ \t]*\(/)) {
				active = 1
				stmt = substr(line, RSTART)
				startln = NR
			} else if (active)
				stmt = stmt " " line
			if (active && balanced(stmt)) {
				judge(stmt, startln)
				active = 0
			}
		}
		END {
			printf "%s %d %d\n", file, calls, helped >> seen
			exit bad
		}
	' || fail=1
done

# The places the helpers were brought in for, each still a log call that takes
# a part of the request through them.
for f in src/nhttpd/yhttpd_core/yrequest.cpp src/nhttpd/yhttpd_core/yconnection.cpp \
	 src/nhttpd/yhttpd_core/yhook.cpp src/nhttpd/yhttpd_mods/mod_sendfile.cpp \
	 src/nhttpd/tuxboxapi/controlapi.cpp; do
	helped=`awk -v f="$f" '$1 == f { print $3 }' "$seen"`
	if [ -z "$helped" ] || [ "$helped" -lt 1 ]; then
		echo "check-log-records.sh: no log call in $f takes the request through ylog::;" >&2
		echo "  the scan no longer finds what it was written for" >&2
		fail=1
	fi
done
# The values the parser and the control dispatcher list are withheld.
withholds() {
	awk -v keepstrings=0 -f "$STRIP" "$SRC/$1" | awk -f "$IF0" | grep -q -E "$2" || {
		echo "check-log-records.sh: $1 lists $3 without ylog::withheld" >&2
		fail=1
	}
}
withholds src/nhttpd/yhttpd_core/yrequest.cpp 'ParseParams.*ylog::withheld\(value\)|^[[:blank:]]*ylog::withheld\(value\)\.c_str\(\)\);' "the parameters it parses"
withholds src/nhttpd/tuxboxapi/controlapi.cpp 'Parameter.*ylog::withheld\(it->second\)|^[[:blank:]]*ylog::withheld\(it->second\)\.c_str\(\)\);' "the parameters of a call"
for helper in 'ylog::startLine(' 'ylog::url('; do
	grep -qF "$helper" "$SRC/src/nhttpd/yhttpd_mods/mod_weblog.cpp" || {
		echo "check-log-records.sh: mod_weblog.cpp builds its records without $helper" >&2
		fail=1
	}
done

exit $fail
