#!/bin/sh
# The real legacy dispatcher lives in legacybridge.o, which cannot be linked into
# the test binary (see the comment in httpd/compat/mount.h: naming
# CControlAPI::Execute at all pulls in a table of seventy-four handlers and the
# real Neutrino singletons behind them). So installDispatch's one real caller is
# out of reach of every case in test_compat_mount.cpp. If the install call were
# deleted, /control/ would silently fall back to mount.cpp's own "nobody answers
# for it" and no case anywhere would go red.
#
# Presence and line order used to be all that was checked, which a dead wrapper
# around either call satisfies word for word: the line stays exactly where it was
# found and never runs.
#
# What is checked below is not a line number but a position inside
# CNeutrinoApp::run's own body, brace-balanced off its signature the same way
# check-hook.sh isolates CNeutrinoApp::run and CNeutrinoApp::ExitRun: nothing may
# stand between that function's own opening brace and a call except a bare,
# always-run block, the same positive list check-legacy-bodygate.sh,
# check-server-credentials.sh and check-hook.sh already hold their own gates to. A
# for, a while, a switch with no matching case, a do, a catch, a lambda body
# nothing calls or a block behind a label all read, to a scan that only looks for
# the call's own text, exactly like a bare statement, and all are refused. Scoping
# the scan to one function this way also catches a call moved into a function
# neither of these two reaches.
#
# Neither call is ever meant to sit inside a branch of anything, not even an if,
# which is why "if (0)" around a call here is caught. What this still cannot see is
# a call folded into an operand a boolean short-circuit never evaluates, or a
# ternary whose condition never takes the arm naming the call. Catching that would
# need a case that runs CNeutrinoApp::run for real, and it cannot be linked into
# this test binary, which is the reason this file exists at all.
set -e
FILE="$1"

HERE=`dirname "$0"`
BLANK="$HERE/blank-if0.awk"
[ -r "$BLANK" ] || { echo "check-legacy-mount.sh: cannot read $BLANK" >&2; exit 1; }

INSTALL="httpd::compat::installDispatch(&httpd::compat::realDispatch);"
SERVER="httpd::start("

# Comments are blanked first, keeping one output line per input line so the
# numbers still match: without that a call behind // or inside a block
# comment satisfies this. One scan rather than a pass per form, because
# whichever of a comment opener and a quote comes first swallows the other,
# and two passes get that backwards.
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

# CNeutrinoApp::run's own body, brace-balanced off its signature the same way
# check-hook.sh isolates it: both calls have to fall inside it, not merely
# somewhere ahead of each other in the file, or a call moved into a function
# nobody reaches would still read as correctly ordered.
RUNBODY=$(printf '%s\n' "$CODE" | awk '
	found == 0 && /^int CNeutrinoApp::run\(.*\)[ \t]*$/ { found = 1 }
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
[ -n "$RUNBODY" ] || { echo "no CNeutrinoApp::run definition found in $FILE" >&2; exit 1; }

# Preprocessor lines blanked before the join below, not just left standing:
# CNeutrinoApp::run wraps both calls this checks for in a #ifdef or #ifndef of
# their own, so their own #endif is routinely the line right in front of whatever
# follows, and a bare, always-run block placed there would butt its opening brace
# up against the word "endif" or a macro name once joined. That reads to the
# construct scan below exactly like a block introduced by an identifier, which is
# the one shape it refuses on sight. Blanking costs nothing: these lines hold
# neither a parenthesis nor a brace.
RUNFLAT=$(printf '%s\n' "$RUNBODY" | awk '/^[ \t]*#/ { print ""; next } { print }' | tr '\n' ' ' | tr -s '[:space:]' ' ')

# For one name inside CNeutrinoApp::run's flattened body, one line: the
# position of its first occurrence (0 if it is not there at all), a tab, and
# what encloses it that is not a bare, always-run block. Empty after the tab,
# with a nonzero position, is the only reading this accepts. The construct
# scan is the one check-hook.sh already runs for the same reason: neither
# call here is meant to sit inside anything at all, so an if counts exactly
# like a for or a switch does.
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

result=$(reachable "$RUNFLAT" "$INSTALL")
iline=$(printf '%s\n' "$result" | cut -f1)
ibad=$(printf '%s\n' "$result" | cut -f2)
[ "$iline" -ne 0 ] || { echo "no $INSTALL reachable in CNeutrinoApp::run in $FILE" >&2; exit 1; }
[ -z "$ibad" ] || {
	echo "installDispatch in $FILE is enclosed by a $ibad the source never enters: /control/ falls back to mount.cpp's own refusal and nothing is wired" >&2
	exit 1
}

result=$(reachable "$RUNFLAT" "$SERVER")
sline=$(printf '%s\n' "$result" | cut -f1)
sbad=$(printf '%s\n' "$result" | cut -f2)
[ "$sline" -ne 0 ] || { echo "nothing starts the new web server in $FILE" >&2; exit 1; }
[ -z "$sbad" ] || {
	echo "the new web server's start in $FILE is enclosed by a $sbad the source never enters: it is never started" >&2
	exit 1
}

[ "$iline" -lt "$sline" ] || {
	echo "installDispatch is reachable after the web server start in $FILE, not before it" >&2
	exit 1
}

exit 0
