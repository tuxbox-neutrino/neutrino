#!/bin/sh
# serve() decides, on the head alone and before a single body byte is accepted,
# whether this box will pay for the body of the request it is holding
# (server.cpp). Two assignments say yes, one per surface: the router's own,
# inside the branch allowed() let through, and the legacy prefix's, inside the
# branch wouldDispatch() let through.
#
# WHAT A CASE HOLDS AND WHAT THIS HOLDS, MEASURED RATHER THAN ASSUMED.
#
# Both assignments are held by cases for the direction that says the body is
# kept, and for any spelling that stops the line running at all.
# test_serverbridge.cpp "nothing of a body is kept for a request nothing will
# answer" posts to a route that answers, reads the count of body bytes taken in,
# and requires it to have moved by the length of what it sent.
# test_compat_mount.cpp does the same for the legacy prefix over a real socket. A
# dead wrapper around either assignment is red in those cases because the counter
# never moves.
#
# The router's assignment is held by a case for the other direction too: the same
# serverbridge case then posts the same body with no credential, is answered 403,
# and requires the counter not to have moved.
#
# One half of one gate is left over, and it is the whole reason this script
# exists: whether the legacy assignment is decided by wouldDispatch() at all.
# Taking that call out of the condition was measured against the whole suite, and
# every case stayed green. legacyAllows() lets a loopback peer through in its
# first line, and every socket the suite opens is a loopback one, so
# wouldDispatch() cannot answer false for anything a case sends.
#
# What the unheld half costs is measured in serve()'s own comment: sixty
# connections to a route that answered every one of them 403 held seventy
# megabytes of a box that has a hundred and twenty eight.
#
# So that half is checked as text. What is checked is not the presence of a call,
# which "(void) wouldDispatch(...);" followed by an unconditional assignment
# satisfies word for word, but that each assignment lies inside the part of an if
# that condition controls: conditions are balanced on parentheses and their
# branches on braces, so a rewritten condition, a renamed local or a reflowed line
# all read the same.
#
# And that nothing else lies in between. That part is a whitelist: every construct
# enclosing an assignment has to be an if or its else, so a for, a while, a switch,
# a do, a catch, a lambda body, a labelled block or anything else that heads a
# controlled part of its own is refused for being there. A never-entered one of
# those around a correctly gated assignment used to read here as correctly gated,
# which is worse than reading nothing at all.
#
# THE ONE SHAPE THIS CANNOT SEE. Conditions are not evaluated here and cannot be,
# so "if (0)" around an assignment reads as a gate. That shape is caught by the
# cases named at the top.
#
# The method is part of the legacy half and not a separate check, because
# compat::answer() reads a body for POST and for nothing else.
#
# Usage: check-legacy-bodygate.sh <server.cpp>
set -e
FILE="$1"
[ -n "$FILE" ] && [ -r "$FILE" ] || {
	echo "usage: check-legacy-bodygate.sh <server.cpp>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-legacy-bodygate.sh: cannot read $STRIP" >&2; exit 1; }
BLANK="$HERE/blank-if0.awk"
[ -r "$BLANK" ] || { echo "check-legacy-bodygate.sh: cannot read $BLANK" >&2; exit 1; }

ASSIGN="st->keep_body = true"

# serve()'s own body, braces balanced on the comment- and string-stripped
# source (keepstrings=0, so a brace inside a literal cannot end the body early
# or late) with #if 0 blanked behind the comments, the same technique
# check-server-credentials.sh uses to isolate credentialsOf().
BODY=`awk -v keepstrings=0 -f "$STRIP" "$FILE" | awk -f "$BLANK" | awk '
	found == 0 && /^MHD_Result serve\(/ { found = 1 }
	found == 1 {
		print
		n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1)
			if (c == "{") depth++
			else if (c == "}") { depth--; if (depth == 0) exit }
		}
	}
'`

[ -n "$BODY" ] || { echo "no serve() definition found in $FILE" >&2; exit 1; }

# Runs of whitespace collapsed to one space and the whole body joined onto one
# line, so a condition split across lines reads the same as one written on a
# single line. The preprocessor lines the body carries (#ifndef/#endif around
# the legacy branch) survive this and are left alone: they hold neither a
# parenthesis nor a brace, so they cannot move either balance, and the word
# scan below reads "ifndef" and "endif" whole rather than finding an "if"
# inside them.
FLAT=`printf '%s\n' "$BODY" | tr '\n' ' ' | tr -s '[:space:]' ' '`

# For every "$ASSIGN" in FLAT, one line holding two things: what encloses it that
# is not an if, and the conditions of the ifs that do, innermost last. The two are
# separated by the first tab, the conditions from each other by a tab as well,
# which nothing can carry because every run of whitespace above became a single
# space.
#
# An assignment under no condition at all prints a line with nothing before and
# nothing after that tab, which is the shape that has to fail below.
#
# A condition's controlled part is both of its branches. Which of the two an
# assignment sits in does not matter here; what matters is that some condition
# decides whether it runs at all.
ENCLOSING=`printf '%s' "$FLAT" | awk -v assign="$ASSIGN" '
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

	function add(kind, cond, from, to) {
		count++
		ckind[count] = kind
		ctext[count] = cond
		cstart[count] = from
		cend[count] = to
	}

	{
		s = $0; n = length(s); count = 0

		# Every keyword that heads a controlled part of its own, the ifs this
		# wants to read and the rest so that they can be refused rather than
		# missed.
		for (i = 1; i <= n; i++) {
			if (substr(s, i, 1) !~ /[A-Za-z_]/) continue
			# a word, not the tail of an identifier
			if (i > 1 && substr(s, i - 1, 1) ~ /[A-Za-z0-9_]/) continue
			j = i
			while (j <= n && substr(s, j, 1) ~ /[A-Za-z0-9_]/) j++
			w = substr(s, i, j - i)

			if (w == "do") { add("do", "", j, branchend(j)); continue }
			if (w != "if" && w != "for" && w != "while" && w != "switch" && w != "catch")
				continue

			k = skipspace(j)
			if (substr(s, k, 1) != "(") continue

			# the condition, parentheses balanced
			c = closeparen(k)
			if (c == 0) continue

			# both branches, so that an assignment in the else of a refusal is
			# held to the same condition as one in the if
			e = branchend(c + 1)
			if (w == "if") {
				f = skipspace(e)
				if (substr(s, f, 4) == "else" && substr(s, f + 4, 1) !~ /[A-Za-z0-9_]/)
					e = branchend(f + 4)
			}
			add(w, substr(s, k + 1, c - k - 1), c + 1, e)
		}

		# Blocks the pass above did not account for, read off what heads them:
		# a lambda body, a block handed to something as an argument, a braced
		# initializer, a block behind a label. A bare compound statement is
		# left alone, because it cannot decide whether what is inside it runs.
		# serve()s own body is the first brace here and is what all of this is
		# inside of.
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
			add("block", "", i, e ? e + 1 : n + 1)
		}

		len = length(assign)
		for (pos = 1; pos <= n; pos++) {
			if (substr(s, pos, len) != assign) continue
			out = ""; bad = ""
			for (m = 1; m <= count; m++) {
				if (pos < cstart[m] || pos >= cend[m]) continue
				if (ckind[m] == "if") out = out ctext[m] "\t"
				else bad = bad (bad == "" ? "" : ", ") ckind[m]
			}
			print bad "\t" out
		}
	}
'`

[ -n "$ENCLOSING" ] || { echo "no \"$ASSIGN\" in serve() in $FILE" >&2; exit 1; }

STATUS=0
COUNT=`printf '%s\n' "$ENCLOSING" | wc -l | tr -d ' '`
[ "$COUNT" -eq 2 ] || {
	echo "serve() in $FILE has $COUNT \"$ASSIGN\", not the two this checks for (one per surface)" >&2
	STATUS=1
}

ROUTED=0
LEGACY=0
UNGATED=0
while IFS= read -r line; do
	# the first tab separates what is not an if from the conditions of the ifs
	intruders=`printf '%s\n' "$line" | cut -f1`
	conds=`printf '%s\n' "$line" | cut -f2-`

	[ -z "$intruders" ] || {
		echo "serve() in $FILE has a \"$ASSIGN\" enclosed by a $intruders as well as by its gate: whether that line runs is then decided by something other than the gate, and a never-entered one of those holds no body at all" >&2
		STATUS=1
	}

	case "$conds" in
	*"wouldDispatch("*)
		LEGACY=`expr $LEGACY + 1`
		case "$conds" in
		*Post*) ;;
		*)
			echo "serve() in $FILE keeps a legacy body for every method: no condition around \"$ASSIGN\" names Post, and compat::answer() reads a body for no other one" >&2
			STATUS=1
			;;
		esac
		;;
	*"allowed("*)
		ROUTED=`expr $ROUTED + 1`
		;;
	*)
		UNGATED=`expr $UNGATED + 1`
		;;
	esac
done <<EOF
$ENCLOSING
EOF

[ "$UNGATED" -eq 0 ] || {
	echo "serve() in $FILE has $UNGATED \"$ASSIGN\" that no condition reading allowed() or wouldDispatch() decides: a caller this box is going to turn away can make it hold a body first" >&2
	STATUS=1
}

[ "$ROUTED" -eq 1 ] || {
	echo "serve() in $FILE has $ROUTED \"$ASSIGN\" gated on allowed(), not the one the router's own branch needs" >&2
	STATUS=1
}

[ "$LEGACY" -eq 1 ] || {
	echo "serve() in $FILE has $LEGACY \"$ASSIGN\" gated on wouldDispatch(), not the one the legacy prefix needs" >&2
	STATUS=1
}

exit $STATUS
