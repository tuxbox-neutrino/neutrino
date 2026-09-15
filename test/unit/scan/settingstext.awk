# Every mention of a text field of the settings struct in one file, and what is
# done with it: written, had its address kept, or read without the accessor.
#
# Parsed rather than matched. A single pattern for "g_settings, then a field the
# header names, then whatever subscripts it carries" is an alternation of some
# eighty names wrapped in optional groups, and which of the possible matches an
# awk returns for that is not the same from one awk to the next: one answers with
# the subscript and one stops in front of it, and a scan that reads a shorter
# match than it meant to reports nothing and passes. So the name only anchors the
# search here, and the expression around it is walked a character at a time.
#
# Read a statement at a time and not a line at a time, because a call written
# across several lines reads as one to the compiler and has to read as one here,
# and because joining a whole file onto one line turns every search in it into a
# walk from the beginning.
#
# namefile: the field names, one per line. mode: write or read. file: the name to
# print a finding under.
BEGIN {
	while ((getline n < namefile) > 0)
		if (n != "") names[n] = 1
	close(namefile)
	buf = ""
	count = 0
}

function ident(s, i,    start) {
	start = i
	while (i <= length(s) && substr(s, i, 1) ~ /[A-Za-z_0-9]/) i++
	return i
}

# past [i], [mt][nr] and the like. Nothing the struct declares is subscripted by
# an expression carrying a bracket of its own.
function subscripts(s, i,    d) {
	while (substr(s, i, 1) == "[") {
		d = index(substr(s, i + 1), "]")
		if (d == 0) return i
		i = i + 1 + d
	}
	return i
}

function nonspace_back(s, i) {
	while (i > 0 && substr(s, i, 1) == " ") i--
	return i
}

function scan(s,    p, at, i, j, k, m, n1, n2, field, tail, head, q) {
	at = 1
	while (1) {
		p = index(substr(s, at), "g_settings.")
		if (p == 0) return
		p = at + p - 1
		at = p + 11
		# not the tail of a longer name
		if (p > 1 && substr(s, p - 1, 1) ~ /[A-Za-z_0-9]/) continue

		i = p + 11
		j = ident(s, i)
		if (j == i) continue
		n1 = substr(s, i, j - i)
		j = subscripts(s, j)

		field = n1
		if (substr(s, j, 1) == "." || substr(s, j, 2) == "->") {
			k = j + (substr(s, j, 1) == "." ? 1 : 2)
			m = ident(s, k)
			if (m > k) {
				n2 = substr(s, k, m - k)
				if (n2 in names) {
					field = n2
					j = subscripts(s, m)
				}
			}
		}

		if (!(field in names)) continue
		count++
		if (mode == "count") continue

		if (mode == "read") {
			head = substr(s, 1, p - 1)
			sub(/ +$/, "", head)
			if (head !~ /(set|append|clear)?[sS]ettingsText\($/)
				print "B\t" file "\t" substr(s, p, j - p)
			continue
		}

		tail = substr(s, j)
		sub(/^ +/, "", tail)
		if (tail ~ /^\+=/ || tail ~ /^=[^=]/ ||
		    tail ~ /^\.(clear|assign|append|insert|erase|resize|swap|push_back)[ \t]*\(/)
			print "W\t" file "\t" substr(s, p, j - p)

		# an address handed to a widget as an argument is how a screen lets one
		# edit a path, and the widget publishes it under the lock. An address
		# kept anywhere else is a writer this scan cannot follow.
		q = nonspace_back(s, p - 1)
		if (q > 0 && substr(s, q, 1) == "&" && substr(s, nonspace_back(s, q - 1), 1) != "&") {
			q = nonspace_back(s, q - 1)
			if (q == 0 || (substr(s, q, 1) != "(" && substr(s, q, 1) != ","))
				print "S\t" file "\t" substr(s, p, j - p)
		}
	}
}

{
	buf = (buf == "" ? $0 : buf " " $0)
	if ($0 ~ /[;{}]/) {
		gsub(/[\t]+/, " ", buf)
		scan(buf)
		buf = ""
	}
}

END {
	if (buf != "") { gsub(/[\t]+/, " ", buf); scan(buf) }
	print "#\t" count
}
