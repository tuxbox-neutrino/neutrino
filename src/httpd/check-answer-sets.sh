#!/bin/sh
# A member of an answer that states the whole of what it can be, held to the
# code that writes it.
#
# A set in the document is a promise, and the reader it is written for is a
# generated client, which turns one into a closed type and turns down anything
# outside it. So a set the server may step outside of is worse for a caller
# than a member described as free text: the box answers truthfully and the
# client refuses the answer. Nothing at run time can see that happen, because
# the answer is right and the refusal is in somebody else's program.
#
# The two are in two places and neither reads the other. The row states the
# set; a naming function turns an enumeration into one of a handful of words,
# and the compiler holds that function to its enumeration and to nothing else.
# A word changed in the function, an enumerator added with a word of its own, or
# a value listed in a row that nothing writes: each of those is the row and the
# writer coming apart, and each is invisible until a client is generated.
#
# So they are compared here as text, both directions, the way the applier
# registrations and the sets of values a settings row offers already are.
#
# Which function writes which member is read out of the writer itself and not
# out of a list kept here: the member is written as a key and the value beside
# it, so the pair says which function answers for it. A third list would be a
# third thing to keep in step.
#
# One file may state a member name more than once, because two shapes in one
# module can each carry a member of that name with a set of its own: a channel's
# picture and one of its sound tracks are both answered under "codec". Where
# that happens the pairing is by set rather than by name: every writer of the
# name has to hold to one of the sets stated for it, and every set stated for it
# has to be held to by some writer. What that cannot catch is two same named
# members whose sets were swapped between them, which is the price of a scan
# that reads rows and functions and not the shapes that hold them.
#
# Comments are blanked first, so a word behind // or inside a block comment
# counts as absent in both.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
STRIP="$2"
[ -n "$SRC" ] && [ -d "$SRC" ] && [ -n "$STRIP" ] && [ -r "$STRIP" ] || {
	echo "usage: check-answer-sets.sh <src/httpd> <strip-comments.awk>" >&2
	exit 2
}

# Below this the scan is not reading the rows any more, and a comparison of two
# empty sets holds every time. The tree states thirteen of these, the newest being
# the scope a token is minted for; the floor is well under that on purpose, being a
# guard against the scan going blind rather than a count of what there is.
FLOOR=6

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=1 -v mark='#file ' -f "$STRIP" "$SRC"/*.cpp "$SRC"/ep/*.cpp "$SRC"/doc/*.cpp > "$tmp/blank"

awk -v floor="$FLOOR" '
# The nth string literal of a line, and empty when there is no nth.
function quoted(s, n,   i, out) {
	for (i = 0; i < n; i++) {
		if (!match(s, /"[^"]*"/))
			return ""
		out = substr(s, RSTART + 1, RLENGTH - 2)
		s = substr(s, RSTART + RLENGTH)
	}
	return out
}

# The function a value is written through: the first name that is called inside
# j.value(...) and is not reached through a dot or a pair of colons, so that a
# conversion wrapped round the call is stepped over and a method on the
# argument is not taken for the writer.
function writerOf(s,   rest, name, before) {
	if (!match(s, /j\.value\(/))
		return ""
	rest = substr(s, RSTART + RLENGTH)
	while (match(rest, /[A-Za-z_][A-Za-z0-9_]*\(/)) {
		name = substr(rest, RSTART, RLENGTH - 1)
		before = (RSTART > 1) ? substr(rest, RSTART - 1, 1) : " "
		rest = substr(rest, RSTART + RLENGTH)
		if (before == "." || before == ":" || before == ">")
			continue
		return name
	}
	return ""
}

# The words of a list, in order and each of them once. A naming function that
# answers one of its words for a value cast in from outside its enumeration
# names that word twice, and the set it is compared with names it once.
function sorted(list,   n, i, j, t, a, out) {
	n = split(list, a, " ")
	for (i = 1; i < n; i++) {
		for (j = i + 1; j <= n; j++) {
			if (a[j] < a[i]) { t = a[i]; a[i] = a[j]; a[j] = t }
		}
	}
	out = ""
	for (i = 1; i <= n; i++) {
		if (i > 1 && a[i] == a[i - 1])
			continue
		out = out (out != "" ? " " : "") a[i]
	}
	return out
}

# Neither a body nor a pair spans a file.
/^#file / { file = $2; body = ""; pending = ""; next }

# A set written once and named, so that a row and a declared parameter can
# state one set rather than two copies of one.
/^const char [A-Za-z_][A-Za-z0-9_]*\[\] *= *"/ {
	name = $3
	sub(/\[\].*/, "", name)
	named[file, name] = quoted($0, 1)
	next
}

/HTTPD_MEMBER_OF_SET\(/ {
	member = quoted($0, 1)
	rest = substr($0, index($0, "HTTPD_MEMBER_OF_SET(") + 20)
	sub(/^[^,]*, */, "", rest)
	if (substr(rest, 1, 1) == "\"")
		set = quoted(rest, 1)
	else {
		sub(/[^A-Za-z0-9_].*/, "", rest)
		set = "@" rest
	}
	# Kept as a list, because a file may state the name more than once.
	d = ++decls[file, member]
	declared[file, member, d] = set
	if (d == 1)
		names[++nn] = file SUBSEP member
	nm++
	next
}

# A key and the value beside it, which is where a member and the function that
# answers it are written down together.
/j\.key\("/ { pending = quoted($0, 1); next }
pending != "" {
	fn = writerOf($0)
	if (fn != "")
		wrote[file, pending] = wrote[file, pending] " " fn
	pending = ""
}

/^(const char \*|std::string )[A-Za-z_][A-Za-z0-9_]*\(/ {
	inbody = $0
	sub(/^(const char \*|std::string )/, "", inbody)
	sub(/\(.*/, "", inbody)
	body = inbody
	home = file
	next
}
body != "" {
	if ($0 ~ /^}/) { body = ""; next }
	# Every word on a line that answers, because one of these answers through a
	# question mark rather than through a case of its own.
	#
	# An empty word is kept and named rather than passed over. It is the one
	# answer that looks like no answer at all, it is what a naming function
	# reached for a value cast in from outside its enumeration used to give,
	# and a row cannot state it: a set with an empty value in it is refused
	# where shapes are checked. So it shows up here as something no row can
	# match rather than as nothing.
	if ($0 ~ /return/) {
		rest = $0
		while (match(rest, /"[^"]*"/)) {
			word = substr(rest, RSTART + 1, RLENGTH - 2)
			answers[home, body] = answers[home, body] " " (word == "" ? "<nothing>" : word)
			rest = substr(rest, RSTART + RLENGTH)
		}
	}
}

END {
	if (nm < floor) {
		print "check-answer-sets.sh: " nm " rows state a set, the scan has stopped matching" > "/dev/stderr"
		exit 1
	}

	fail = 0
	for (k = 1; k <= nn; k++) {
		split(names[k], p, SUBSEP)
		file = p[1]; member = p[2]
		nd = decls[file, member]

		bad = 0
		for (d = 1; d <= nd; d++) {
			set = declared[file, member, d]
			if (substr(set, 1, 1) == "@") {
				name = substr(set, 2)
				if (!((file, name) in named)) {
					print file ": " member " states a set under a name this file does not define: " name > "/dev/stderr"
					fail = 1
					bad = 1
					continue
				}
				set = named[file, name]
			}
			gsub(/,/, " ", set)
			want[d] = sorted(set)
			held[d] = 0
		}
		if (bad)
			continue

		fns = wrote[file, member]
		if (fns == "") {
			print file ": " member " states a set and nothing here is seen writing it" > "/dev/stderr"
			fail = 1
			continue
		}

		nf = split(fns, seen, " ")
		for (i = 1; i <= nf; i++) {
			fn = seen[i]
			# The writer is looked for beside the row first, because one name
			# answers for two different sets in two of these files.
			where = ((file, fn) in answers) ? file : ""
			if (where == "") {
				for (f in answers) {
					split(f, q, SUBSEP)
					if (q[2] == fn) { where = q[1]; break }
				}
			}
			if (where == "") {
				print file ": " member " is written through " fn ", which answers with no word this can read" > "/dev/stderr"
				fail = 1
				continue
			}
			got = sorted(answers[where, fn])
			hit = 0
			for (d = 1; d <= nd; d++) {
				if (got == want[d]) { held[d] = 1; hit = 1 }
			}
			if (!hit) {
				print file ": " member " states a set the writer does not hold to" > "/dev/stderr"
				for (d = 1; d <= nd; d++)
					print "  a row says:      " want[d] > "/dev/stderr"
				print "  " fn " answers: " got > "/dev/stderr"
				fail = 1
			}
			checked++
		}

		# And the other way, which is what a member stated twice needs: a set
		# nothing writes is a set a generated client offers and the box never
		# gives.
		for (d = 1; d <= nd; d++) {
			if (!held[d]) {
				print file ": " member " states a set no writer here holds to: " want[d] > "/dev/stderr"
				fail = 1
			}
		}
	}
	if (fail)
		exit 1
	print "answer sets held to their writer                    " nm
}
' "$tmp/blank"
exit 0
