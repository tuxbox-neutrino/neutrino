#!/bin/sh
# Every function whose address is handed to the HTTP library is entered from C. A
# throw that leaves one of them unwinds into a frame built without exceptions, which
# on the target ends the process rather than the request, and the host this is
# developed on does not show it. So each of them has to hold a boundary.
#
# The list is derived and not written down here: a list written down drifts the day
# someone hands the library a seventh callback, and the new one would simply not be
# checked. What is scanned is every argument of the form &name inside a call to
# something whose name begins with MHD_, kept only where a function of that name is
# defined in the same directory.
#
# A boundary used to be read as "the substring try appears somewhere in the body",
# which a dead wrapper around the try/catch satisfies word for word: the try stays
# exactly where it was written and never runs. The same substring test also has no
# notion of a word: "retryCount" or a class "Entry" both carry the three letters.
#
# try cannot be either of those by accident: it is reserved in C++. So it is found
# here as a whole word, and it has to be reachable: nothing may enclose it except a
# bare, always-run block, the same positive list check-legacy-bodygate.sh,
# check-server-credentials.sh and check-hook.sh hold their own gates to. A for, a
# while, a switch with no matching case, a do, a catch, a lambda body nothing calls or
# a block behind a label all read, to a scan that only looks for the word itself,
# exactly like a bare try, and all are refused for standing in the way. None of the six
# callbacks has its boundary inside a branch of anything, not even an if, so an if
# counts here exactly like a for.
#
# What this still cannot see is the keyword folded into an operand a boolean
# short-circuit never evaluates, or a lambda whose own reachability this does not
# follow: neither needs an enclosing construct this is built on. Catching that would
# need a case that runs the callback for real, and none of the six can be driven by
# this suite.
#
# Usage: check-callback-boundary.sh SRCDIR
set -e

# Read as bytes and not as text. The name scan below pushes whole sources
# through tr before anything has stripped their comments, and a byte that is
# not a character in the caller's locale ends tr with "Illegal byte sequence"
# there. What the run then prints is that the scan found too few callbacks and
# is broken, which sends whoever reads it into the scan rather than to the one
# source file that carries the byte. Nothing here has any business reading a
# character anyway: every pattern below is ASCII and every source it is held to
# is C++.
LC_ALL=C
export LC_ALL
DIR="$1"
[ -d "$DIR" ] || { echo "check-callback-boundary: $DIR fehlt" >&2; exit 1; }

HERE=`dirname "$0"`
BLANK="$HERE/blank-if0.awk"
[ -r "$BLANK" ] || { echo "check-callback-boundary.sh: cannot read $BLANK" >&2; exit 1; }

FILES=$(find "$DIR" -name '*.cpp' | sort)
[ -n "$FILES" ] || { echo "check-callback-boundary: keine Quellen unter $DIR" >&2; exit 1; }

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

# Statements are joined before they are scanned, because a call to the library
# spans several lines and the callback is rarely on the one that names it.
# Read off the raw sources, same as before: a name that only exists inside a
# comment or a dead #if 0 still has to be looked up by grep -l below, which
# finds nothing to define it and drops it on the next line either way.
NAMES=$(cat $FILES | tr '\n' ' ' | sed 's/;/;\n/g' |
	grep 'MHD_' |
	grep -oE '[(,][[:space:]]*&[A-Za-z_][A-Za-z0-9_]*[[:space:]]*[,)]' |
	tr -d '(),& \t' | sort -u)

# For one flattened function body, whether a reachable "try" is in it: 1 or 0.
# The word is found bounded on both sides, not as a bare substring, and then
# read through the same construct scan check-hook.sh runs, with the same
# refusal of an if: reaching this function at all is what the case this file
# has none of would exercise, not a condition inside it.
has_reachable_try() {
	flat="$1"
	printf '%s' "$flat" | awk '
		function skipspace(i) { while (substr(s, i, 1) == " ") i++; return i }
		function backspace(i) { while (i >= 1 && substr(s, i, 1) == " ") i--; return i }

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

			# "try" bounded on both sides: it is reserved in C++, so a whole
			# word spelled that way is always the keyword and never a prefix
			# of "retryCount" or the tail of a class "Entry" whose own brace
			# follows it. The first one found is enough: none of the six
			# bodies this is used on carries more than one.
			pos = 0
			for (p = 1; p + 2 <= n; p++) {
				if (substr(s, p, 3) != "try") continue
				if (p > 1 && substr(s, p - 1, 1) ~ /[A-Za-z0-9_]/) continue
				if (substr(s, p + 3, 1) ~ /[A-Za-z0-9_]/) continue
				pos = p
				break
			}
			if (pos == 0) { print 0; exit }

			count = 0
			for (i = 1; i <= n; i++) {
				if (substr(s, i, 1) !~ /[A-Za-z_]/) continue
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

			for (m = 1; m <= count; m++) {
				if (pos < cstart[m] || pos >= cend[m]) continue
				print 0
				exit
			}
			print 1
		}
	'
}

FOUND=0
MISSING=""
for n in $NAMES; do
	# Where the name is defined as a function, if it is one at all.
	def=$(grep -l -E "^[A-Za-z_][A-Za-z0-9_:<>,\*& \t]*\\b$n[[:space:]]*\\(" $FILES || true)
	[ -n "$def" ] || continue
	for f in $def; do
		FOUND=$((FOUND + 1))

		CODE=$(uncommented "$f" | awk -f "$BLANK")

		# The function's own body, brace-balanced off the line that names it,
		# the same way check-hook.sh isolates a function among several: a
		# signature split across lines is handled the same as one that is
		# not, because the balance only starts moving once the first "{" of
		# the definition is actually read, whichever line it is on.
		BODY=$(printf '%s\n' "$CODE" | awk -v name="$n" '
			found == 0 && $0 ~ "^[A-Za-z_].*[^A-Za-z0-9_]" name "[[:space:]]*\\(" { found = 1 }
			found == 1 {
				print
				len = length($0)
				for (i = 1; i <= len; i++) {
					c = substr($0, i, 1)
					if (c == "{") depth++
					else if (c == "}") { depth--; if (depth == 0) exit }
				}
			}
		')
		[ -n "$BODY" ] || { echo "check-callback-boundary: cannot find $n's own body in $f" >&2; exit 1; }

		FLAT=$(printf '%s\n' "$BODY" | tr '\n' ' ' | tr -s '[:space:]' ' ')

		ok=$(has_reachable_try "$FLAT")
		[ "$ok" = "1" ] || MISSING="$MISSING $f:$n"
	done
done

# A scan that found nothing is a broken scan, not a clean tree. The floor is
# the number of callbacks this server had when the rule was written; it may
# grow and must not shrink without someone saying why here.
if [ "$FOUND" -lt 6 ]; then
	echo "check-callback-boundary: nur $FOUND Callbacks gefunden, erwartet mindestens 6" >&2
	echo "  Der Scan ist kaputt, nicht der Baum." >&2
	exit 1
fi

if [ -n "$MISSING" ]; then
	echo "check-callback-boundary: Callback ohne erreichbare Boundary:" >&2
	for m in $MISSING; do echo "  $m" >&2; done
	exit 1
fi

echo "callback boundaries: $FOUND geprueft, alle mit Boundary"
