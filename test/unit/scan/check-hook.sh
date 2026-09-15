#!/bin/sh
# The publish hook lives in an object that cannot be linked into the test
# binary, so no case can reach it. Its position is checked as text instead:
# every branch below it returns early or frees the payload, so a later edit
# that moves the call down would drop events with nothing failing.
set -e
FILE="$1"

HERE=`dirname "$0"`
BLANK="$HERE/blank-if0.awk"
[ -r "$BLANK" ] || { echo "check-hook.sh: cannot read $BLANK" >&2; exit 1; }

WANT="coreapi::publishFromMessage(_msg, data);"

# Read once, here, and every check below reads what this leaves rather than the
# file itself: comments blanked (one scan rather than a pass per form, because
# whichever of a comment opener and a quote comes first swallows the other, and
# two passes get that backwards), then #if 0 blanked behind them, both keeping
# one output line per input line so the numbers still match.
uncommented() {
	awk '
		BEGIN { sq = sprintf("%c", 39) }
		{
			line = $0; out = ""; n = length(line); i = 1
			while (i <= n) {
				c = substr(line, i, 1)
				d = substr(line, i, 2)
				if (inblock) {
					if (d == "*/") { inblock = 0; i += 2 } else { i++ }
					continue
				}
				if (instr) {
					if (c == "\\") { i += 2; continue }
					if (c == q) instr = 0
					i++
					continue
				}
				if (d == "//") break
				if (d == "/*") { inblock = 1; i += 2; continue }
				if (c == "\"" || c == sq) { instr = 1; q = c; i++; continue }
				out = out c
				i++
			}
			# a literal does not span lines, a block comment does
			instr = 0
			print out
		}
	' "$1"
}

CODE=$(uncommented "$FILE" | awk -f "$HERE/blank-if0.awk")

FIRST=$(printf '%s\n' "$CODE" | awk '
	/^int CNeutrinoApp::handleMsg\(.*\)[ \t]*$/ { body = 1; next }
	!body { next }
	{ line = $0; sub(/^[ \t]+/, "", line); sub(/[ \t]+$/, "", line) }
	line == "" || line == "{" { next }
	{ print line; exit }
')

[ -n "$FIRST" ] || { echo "no CNeutrinoApp::handleMsg definition on a line of its own in $FILE" >&2; exit 1; }

if [ "$FIRST" != "$WANT" ]; then
	echo "first statement of CNeutrinoApp::handleMsg is not the publish hook:" >&2
	echo "  want: $WANT" >&2
	echo "  got:  $FIRST" >&2
	exit 1
fi

# The install calls and the registration below them cannot be linked either, and they
# have to run before the threads below them, so their order is checked as text.
#
# The registration is here and not left to a case because nothing at run time can miss
# it: a section with no applier is a setting nothing has to be told about, so a
# forgotten registration and a section that has nothing to apply are the same silence.
#
# Presence and line order alone used to be all that was checked, which a dead wrapper
# around any of these calls satisfies word for word. What is checked below is not a
# line number but a position inside the one function each call belongs to
# (CNeutrinoApp::run or CNeutrinoApp::ExitRun, brace balanced off its own signature the
# same way check-legacy-bodygate.sh isolates serve()): nothing may stand between that
# function's own opening brace and a call except a bare, always-run block. A for, a
# while, a switch with no matching case, a do, a catch, a lambda body nothing calls or
# a block behind a label all read, to a scan that only looks for the call's own text,
# exactly like a bare statement, and all are refused. Scoping the scan to one function
# this way also catches a call moved into a function neither of these two reaches.
#
# Because none of the calls below are ever meant to sit inside a branch of anything,
# not even an if is let through here the way check-legacy-bodygate.sh lets one through
# around its own gate, which is why "if (0)" around a call here is caught. What this
# still cannot see is a call folded into an operand a boolean short-circuit never
# evaluates, or a ternary whose condition never takes the arm naming the call: neither
# needs an enclosing construct this is built on. Catching that would need a case that
# runs CNeutrinoApp::run or CNeutrinoApp::ExitRun for real, and neither can be linked
# into this test binary, which is the reason this file exists at all.
#
# Every install call is named here by hand, so a seam added below with nothing in this
# list is a wiring this scan walks straight past.
WIRING="coreapi::installRealChannelSource coreapi::installRealEpgSource coreapi::installRealTimerSource coreapi::installRealCommandSink coreapi::installRealSystemSource coreapi::installRealTunerSource coreapi::installRealInputDevice coreapi::installRealScreenshotSource coreapi::installRealLogoSource coreapi::installRealEventSink coreapi::installRealSettingsSource coreapi::installRealPluginSource coreapi::installRealLocaleSource coreapi::installRealRecordingSafetySource registerSettingsAppliers installVideoSettingChoices installOsdResolutionSource"
# The web server is started from the same object and is out of reach of a case for the
# same reason. What a case cannot see at all is a start that was deleted: the program
# would come up with no web server and nothing anywhere would be red, because every
# case here drives the layer directly. So its presence is checked as text, and its
# position with it: every seam above has to be installed in front of it.
#
# Whether it can answer before the seams above are installed is not this check's
# business: the call refuses to start a daemon while they are empty, which is a case in
# the suite.
WEB_START="httpd::start("
# Confined to CNeutrinoApp::ExitRun's own body the same way the calls above
# are confined to CNeutrinoApp::run, so a stop call moved to a function
# nothing tears the server down through is caught the same way a deleted one
# already was.
WEB_STOP="httpd::stop();"

# The call that leaves the box with a configuration is the same kind of thing
# and has the same hole: it carries an upgraded box's credential over and
# writes the shipped login where there is nothing to carry, and a deleted call
# would leave every box with no file answering nothing that needs a password,
# with no case anywhere going red. It has to stand in front of the read, since
# on the start where it writes, the file being read is the one it wrote.
WEB_MIGRATE="httpd::ensureConfigFile("
WEB_LOAD="httpd::load("

# The body of the one function named by the pattern given, brace balanced on
# $CODE (comments and #if 0 already gone, one line kept per input line), the
# same way check-legacy-bodygate.sh isolates serve() and
# check-server-credentials.sh isolates credentialsOf().
body_of() {
	printf '%s\n' "$CODE" | awk -v pat="$1" '
		found == 0 && $0 ~ pat { found = 1 }
		found == 1 {
			print
			n = length($0)
			for (i = 1; i <= n; i++) {
				c = substr($0, i, 1)
				if (c == "{") depth++
				else if (c == "}") { depth--; if (depth == 0) exit }
			}
		}
	'
}

RUNBODY=$(body_of '^int CNeutrinoApp::run\(.*\)[ \t]*$')
[ -n "$RUNBODY" ] || { echo "no CNeutrinoApp::run definition found in $FILE" >&2; exit 1; }

EXITBODY=$(body_of '^void CNeutrinoApp::ExitRun\(.*\)[ \t]*$')
[ -n "$EXITBODY" ] || { echo "no CNeutrinoApp::ExitRun definition found in $FILE" >&2; exit 1; }

# Runs of whitespace collapsed to one space and each body joined onto one
# line, so a call split across lines, or reflowed onto a different one, reads
# the same as one written on a single line. The preprocessor lines a body
# carries (#ifdef ENABLE_NI_WEB and friends) survive this and are read as
# plain words below: they hold neither a parenthesis nor a brace, so they
# cannot move a balance, and "ifdef"/"endif" are not "if" to anything that
# only matches a keyword whole.
flatten() { tr '\n' ' ' | tr -s '[:space:]' ' '; }

RUNFLAT=$(printf '%s\n' "$RUNBODY" | flatten)
EXITFLAT=$(printf '%s\n' "$EXITBODY" | flatten)

# For one function body and one name inside it, one line: the position of the name's
# first occurrence (0 if it is not there at all), a tab, and what stands between that
# function's own opening brace and the name that is not a bare, always-run block. Empty
# after the tab, with a nonzero position, is the only reading this accepts.
#
# The construct scan is the one check-legacy-bodygate.sh already runs against serve(),
# with one difference: there the assignment is meant to sit inside an if or its else,
# so those two are let through. None of the calls this file asks about are meant to sit
# inside anything at all, so an if counts here exactly like a for.
reachable() {
	flat="$1"; anchor="$2"
	printf '%s' "$flat" | awk -v anchor="$anchor" '
		function skipspace(i) { while (substr(s, i, 1) == " ") i++; return i }
		function backspace(i) { while (i >= 1 && substr(s, i, 1) == " ") i--; return i }

		# the identifier ending at i, empty where there is none
		function wordat(i,    j) {
			if (i < 1 || substr(s, i, 1) !~ /[A-Za-z0-9_]/) return ""
			j = i
			while (j >= 1 && substr(s, j, 1) ~ /[A-Za-z0-9_]/) j--
			return substr(s, j + 1, i - j)
		}

		function closeparen(i,    d, c) {
			d = 0
			while (i <= n) {
				c = substr(s, i, 1)
				if (c == "(") d++
				else if (c == ")") { d--; if (d == 0) return i }
				i++
			}
			return 0
		}

		function openparen(i,    d, c) {
			d = 0
			while (i >= 1) {
				c = substr(s, i, 1)
				if (c == ")") d++
				else if (c == "(") { d--; if (d == 0) return i }
				i--
			}
			return 0
		}

		function closebrace(i,    d, c) {
			d = 0
			while (i <= n) {
				c = substr(s, i, 1)
				if (c == "{") d++
				else if (c == "}") { d--; if (d == 0) return i }
				i++
			}
			return 0
		}

		# the end of the statement or block starting at i, one past its last
		# character
		function branchend(i,    e) {
			i = skipspace(i)
			if (substr(s, i, 1) == "{") { e = closebrace(i); return e ? e + 1 : n + 1 }
			while (i <= n && substr(s, i, 1) != ";") i++
			return i + 1
		}

		function add(kind, from, to) {
			count++
			ckind[count] = kind
			cstart[count] = from
			cend[count] = to
		}

		{
			s = $0; n = length(s)

			pos = index(s, anchor)
			if (pos == 0) { print "0\t"; exit }

			count = 0

			# Every keyword that heads a controlled part of its own.
			for (i = 1; i <= n; i++) {
				if (substr(s, i, 1) !~ /[A-Za-z_]/) continue
				# a word, not the tail of an identifier
				if (i > 1 && substr(s, i - 1, 1) ~ /[A-Za-z0-9_]/) continue
				j = i
				while (j <= n && substr(s, j, 1) ~ /[A-Za-z0-9_]/) j++
				w = substr(s, i, j - i)

				if (w == "do") { add("do", j, branchend(j)); continue }
				if (w != "if" && w != "for" && w != "while" && w != "switch" && w != "catch")
					continue

				k = skipspace(j)
				if (substr(s, k, 1) != "(") continue

				c = closeparen(k)
				if (c == 0) continue

				e = branchend(c + 1)
				if (w == "if") {
					f = skipspace(e)
					if (substr(s, f, 4) == "else" && substr(s, f + 4, 1) !~ /[A-Za-z0-9_]/)
						e = branchend(f + 4)
				}
				add(w, c + 1, e)
			}

			# Blocks the pass above did not account for: a lambda body, a
			# block handed to something as an argument, a braced initializer,
			# a block behind a label. A bare compound statement is left
			# alone, because it cannot decide whether what is inside it runs.
			first = index(s, "{")
			for (i = 1; i <= n; i++) {
				if (substr(s, i, 1) != "{") continue
				if (i == first) continue
				k = backspace(i - 1)
				if (k < 1) continue
				c = substr(s, k, 1)
				if (c == ")") {
					p = openparen(k)
					w = (p > 1) ? wordat(backspace(p - 1)) : ""
					if (w == "if" || w == "for" || w == "while" || w == "switch" || w == "catch")
						continue
				} else {
					w = wordat(k)
					if (w == "else" || w == "do" || w == "try") continue
					if (w == "" && (c == ";" || c == "{" || c == "}")) continue
				}
				e = closebrace(i)
				add("block", i, e ? e + 1 : n + 1)
			}

			bad = ""
			for (m = 1; m <= count; m++) {
				if (pos < cstart[m] || pos >= cend[m]) continue
				bad = bad (bad == "" ? "" : ",") ckind[m]
			}
			print pos "\t" bad
		}
	'
}

result=$(reachable "$RUNFLAT" "$WEB_START")
wline=$(printf '%s\n' "$result" | cut -f1)
wbad=$(printf '%s\n' "$result" | cut -f2)
[ "$wline" -ne 0 ] || { echo "nothing starts the web server in $FILE" >&2; exit 1; }
[ -z "$wbad" ] || {
	echo "the web server's start in $FILE is enclosed by a $wbad the source never enters: it is never started" >&2
	exit 1
}

for name in $WIRING; do
	result=$(reachable "$RUNFLAT" "$name(")
	iline=$(printf '%s\n' "$result" | cut -f1)
	ibad=$(printf '%s\n' "$result" | cut -f2)

	[ "$iline" -ne 0 ] || { echo "no $name(...); reachable in CNeutrinoApp::run in $FILE" >&2; exit 1; }
	[ -z "$ibad" ] || {
		echo "$name(...); in CNeutrinoApp::run in $FILE is enclosed by a $ibad the source never enters: the call stays in the file and never runs" >&2
		exit 1
	}

	[ "$iline" -lt "$wline" ] || {
		echo "$name(...); is reachable after the web server's start in $FILE, not before it" >&2
		exit 1
	}
done

result=$(reachable "$EXITFLAT" "$WEB_STOP")
xline=$(printf '%s\n' "$result" | cut -f1)
xbad=$(printf '%s\n' "$result" | cut -f2)
[ "$xline" -ne 0 ] || { echo "nothing stops the new web server in $FILE" >&2; exit 1; }
[ -z "$xbad" ] || {
	echo "the new web server's stop in $FILE is enclosed by a $xbad the source never enters: it is never stopped" >&2
	exit 1
}

result=$(reachable "$RUNFLAT" "$WEB_MIGRATE")
mline=$(printf '%s\n' "$result" | cut -f1)
mbad=$(printf '%s\n' "$result" | cut -f2)
[ "$mline" -ne 0 ] || { echo "nothing migrates the old server's configuration in $FILE" >&2; exit 1; }
[ -z "$mbad" ] || {
	echo "the migration in $FILE is enclosed by a $mbad the source never enters: it never runs" >&2
	exit 1
}

result=$(reachable "$RUNFLAT" "$WEB_LOAD")
lline=$(printf '%s\n' "$result" | cut -f1)
lbad=$(printf '%s\n' "$result" | cut -f2)
[ "$lline" -ne 0 ] || { echo "nothing reads the new web server's configuration in $FILE" >&2; exit 1; }
[ -z "$lbad" ] || {
	echo "the read of the new web server's configuration in $FILE is enclosed by a $lbad the source never enters: it never runs" >&2
	exit 1
}

[ "$mline" -lt "$lline" ] || {
	echo "the migration is reachable after the read in $FILE, not before it" >&2
	exit 1
}

exit 0
