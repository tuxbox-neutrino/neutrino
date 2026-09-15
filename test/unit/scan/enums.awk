# The enumerators of one block at a time, with the values the standard fixes for
# them. A block is read only when every enumerator in it is a bare name or a
# plain integer; anything else, an expression or a preprocessor arm among them
# included, skips the whole block, because counting past what cannot be read
# would put a wrong number on every name after it.
#
# Reads the source with its literals already blanked.
BEGIN { inenum = 0; depth = 0 }

# a boundary no declaration spans
mark != "" && index($0, mark) == 1 { inenum = 0; head = ""; body = ""; next }

{
	line = $0
	n = length(line)
	for (i = 1; i <= n; i++) {
		c = substr(line, i, 1)
		if (!inenum) {
			head = head c
			if (head ~ /(^|[^A-Za-z_0-9])enum([ \t]+[A-Za-z_][A-Za-z_0-9]*)?([ \t]*:[ \t]*[A-Za-z_0-9 \t]+)?[ \t]*\{$/) {
				inenum = 1
				depth = 1
				body = ""
			}
			# the tail is all that can still complete the opening, and a whole
			# file held in one string is what this avoids
			if (length(head) > 200) head = substr(head, length(head) - 100)
			continue
		}
		if (c == "{") { depth++; body = body c; continue }
		if (c == "}") {
			depth--
			if (depth == 0) { flush(); inenum = 0; head = ""; continue }
			body = body c
			continue
		}
		body = body c
	}
	if (inenum) body = body " "; else head = head " "
}
function flush(   k, m, parts, e, p, nm, val, next_val, ok, out, cnt) {
	m = split(body, parts, ",")
	body = ""
	next_val = 0
	ok = 1
	cnt = 0
	out = ""
	for (k = 1; k <= m; k++) {
		e = parts[k]
		gsub(/[ \t]+/, " ", e)
		sub(/^ /, "", e)
		sub(/ $/, "", e)
		# the comma after the last enumerator is allowed and names nothing
		if (e == "") continue
		p = index(e, "=")
		if (p == 0)
			nm = e
		else {
			nm = substr(e, 1, p - 1)
			val = substr(e, p + 1)
			gsub(/ /, "", val)
			if (val ~ /^-?[0-9]+$/) next_val = val + 0
			else if (val ~ /^0[xX][0-9a-fA-F]+$/) next_val = hex(val)
			else { ok = 0; break }
		}
		gsub(/ /, "", nm)
		if (nm !~ /^[A-Za-z_][A-Za-z_0-9]*$/) { ok = 0; break }
		out = out nm "\t" next_val "\n"
		cnt++
		next_val = next_val + 1
	}
	if (ok && cnt > 0) printf "%s", out
}
function hex(s,   d, r, i, c) {
	d = "0123456789abcdef"
	r = 0
	s = tolower(substr(s, 3))
	for (i = 1; i <= length(s); i++) {
		c = substr(s, i, 1)
		r = r * 16 + index(d, c) - 1
	}
	return r
}
