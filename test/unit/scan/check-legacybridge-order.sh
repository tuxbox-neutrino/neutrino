#!/bin/sh
# realDispatch (compat/legacybridge.cpp) is the one function in this tree that
# calls CControlAPI::init() and CControlAPI::Execute(), and it cannot be linked
# into the test binary for the same reason Execute() itself cannot (see the comment
# in compat/mount.h). So no case here can prove what order these three statements
# run in, or even that all three are still there.
#
# Three orderings are checked, and all three fail silently instead of loudly if
# they slip:
#
#   - config is read from legacyWebsiteMainConfig(NI_WEB_OLD_SETTINGS_FILE).
#     config used to be checked only by the name it is assigned under, which is
#     what let its initializer slip from that call to a default-constructed
#     CStringList with no docroots in it at all: the assignment into hh is still
#     there, still in front of init(), and both orderings below still hold, so
#     nothing here said so. Only reading for the call itself tells them apart.
#     This still cannot prove what the call returns.
#
#   - the WebserverConfigList assignment before init(): CControlAPI::init()
#     reads WebserverConfigList["WebsiteMain.override_directory"] and
#     ["WebsiteMain.directory"] on the one call that still finds PLUGIN_DIRS[0]
#     empty. Without the assignment in front of it, hh carries an empty
#     WebserverConfigList, and init() turns that into
#     PLUGIN_DIRS[0] = PLUGIN_DIRS[2] = "" - yExecuteScript then searches the root
#     of the filesystem for whatever name a caller asked for. No case anywhere
#     would go red.
#
#   - init() before Execute(): the table Execute() dispatches through calls
#     handlers that read PLUGIN_DIRS themselves.
#
# Presence and line order used to be all that was checked here, and neither ever
# asked whether a line sits inside the one function three of these four belong to.
# A dead wrapper around any of the three statements satisfies both. This was weaker
# than check-legacy-mount.sh's own hole in the same round: that file at least
# confined its two calls to CNeutrinoApp::run's own body.
#
# What is checked below instead is a position inside realDispatch's own body,
# brace-balanced off its signature the same way check-hook.sh isolates
# CNeutrinoApp::run: nothing may stand between that function's own opening brace
# and one of the three statements except a bare, always-run block, the positive
# list check-legacy-bodygate.sh, check-server-credentials.sh and check-hook.sh
# already hold their own gates to. None of the three is ever meant to sit inside a
# branch of anything either, not even an if.
#
# The docroot read is not folded into that scan, and on purpose: it is the
# initializer of "static const CStringList config = ...;", not a statement of its
# own, and C++ scopes a declaration to the one statement controlling it. A
# "for (...) static const CStringList config = ...;" leaves "config" out of scope
# at the assignment below, which stops this from compiling rather than from
# running. So the read keeps the presence and order check it already had.
#
# What none of this can see is a call folded into an operand a boolean
# short-circuit never evaluates, or a ternary whose condition never takes the arm
# naming the call. Catching that would need a case that runs realDispatch for real.
#
# Comments are blanked first, keeping one output line per input line. One scan
# rather than a pass per form, because whichever of a comment opener and a quote
# comes first swallows the other.
#
# #if 0 is blanked after them, for the reason blank-if0.awk's own header gives: all
# three statements can stay exactly where they are and still never reach the
# binary.
set -e
FILE="$1"

HERE=`dirname "$0"`
BLANK="$HERE/blank-if0.awk"
[ -r "$BLANK" ] || { echo "check-legacybridge-order.sh: cannot read $BLANK" >&2; exit 1; }

READ="legacyWebsiteMainConfig(NI_WEB_OLD_SETTINGS_FILE)"
CONFIG="hh.WebserverConfigList = config;"
INIT="api.init(&hh);"
EXECUTE="api.Execute(&hh);"

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

# realDispatch's own body, brace-balanced off its signature the same way
# check-hook.sh isolates CNeutrinoApp::run: every statement checked below has
# to fall inside it, not merely somewhere ahead of another in the file, or a
# statement moved into a different function would still read as correctly
# ordered.
BODY=$(printf '%s\n' "$CODE" | awk '
	found == 0 && /^void realDispatch\(.*\)[ \t]*$/ { found = 1 }
	found == 1 {
		print
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") depth++
			else if (c == "}") { depth--; if (depth == 0) exit }
		}
	}
')
[ -n "$BODY" ] || { echo "no realDispatch definition found in $FILE" >&2; exit 1; }

# Runs of whitespace collapsed to one space and the whole body joined onto one
# line, so a statement split across lines, or reflowed onto a different one,
# reads the same as one written on a single line.
FLAT=$(printf '%s\n' "$BODY" | tr '\n' ' ' | tr -s '[:space:]' ' ')

# For one name inside realDispatch's flattened body, one line: the position
# of its first occurrence (0 if it is not there at all), a tab, and what
# encloses it that is not a bare, always-run block. Empty after the tab, with
# a nonzero position, is the only reading this accepts. The construct scan is
# the one check-hook.sh already runs for the same reason: none of the three
# statements this is used for is meant to sit inside anything at all, so an
# if counts here exactly like a for or a switch does.
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

# The read is not scanned for reachability the same way: see the file header
# for why a dead wrapper around its own declaration cannot pass here without
# already failing to compile. Presence and position are still read off the
# same flattened body, so its ordering against the assignment below compares
# like against like.
rpos=$(printf '%s' "$FLAT" | awk -v anchor="$READ" '{ print index($0, anchor) }')
[ "$rpos" -ne 0 ] || { echo "no $READ reachable in realDispatch in $FILE" >&2; exit 1; }

result=$(reachable "$FLAT" "$CONFIG")
cpos=$(printf '%s\n' "$result" | cut -f1)
cbad=$(printf '%s\n' "$result" | cut -f2)
[ "$cpos" -ne 0 ] || { echo "no $CONFIG reachable in realDispatch in $FILE" >&2; exit 1; }
[ -z "$cbad" ] || {
	echo "$CONFIG in realDispatch in $FILE is enclosed by a $cbad the source never enters: init() sees an empty WebserverConfigList" >&2
	exit 1
}

result=$(reachable "$FLAT" "$INIT")
ipos=$(printf '%s\n' "$result" | cut -f1)
ibad=$(printf '%s\n' "$result" | cut -f2)
[ "$ipos" -ne 0 ] || { echo "no $INIT reachable in realDispatch in $FILE" >&2; exit 1; }
[ -z "$ibad" ] || {
	echo "$INIT in realDispatch in $FILE is enclosed by a $ibad the source never enters: PLUGIN_DIRS is never filled in" >&2
	exit 1
}

result=$(reachable "$FLAT" "$EXECUTE")
epos=$(printf '%s\n' "$result" | cut -f1)
ebad=$(printf '%s\n' "$result" | cut -f2)
[ "$epos" -ne 0 ] || { echo "no $EXECUTE reachable in realDispatch in $FILE" >&2; exit 1; }
[ -z "$ebad" ] || {
	echo "$EXECUTE in realDispatch in $FILE is enclosed by a $ebad the source never enters: /control/ never answers a request" >&2
	exit 1
}

[ "$rpos" -le "$cpos" ] || {
	echo "$READ is reachable after $CONFIG in $FILE, not on or before it" >&2
	exit 1
}

[ "$cpos" -lt "$ipos" ] || {
	echo "$CONFIG is reachable after init() in $FILE, not before it" >&2
	exit 1
}

[ "$ipos" -lt "$epos" ] || {
	echo "init() is reachable after Execute() in $FILE, not before it" >&2
	exit 1
}

exit 0
