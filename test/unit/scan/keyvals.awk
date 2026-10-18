# One table of named choices at a time: the array's name and the entries it holds. A
# screen states what a setting offers here and the declaration states it again, and
# nothing but a scan of this can hold the two together.
#
# An entry is flagged when a preprocessor arm inside the array gates it, because a value
# only one box model is given is one the declaration may leave out. An entry under #if 0
# is left out altogether: no build compiles it, so no screen offers it.
#
# Reads the source with its literals kept. A keyval_ext entry can name its value in words
# of its own rather than by a locale, and those words are what the screen shows beside
# it; a comma or a brace inside a literal is passed over rather than counted.
BEGIN { collecting = 0; pending = 0; dead = 0; q = ""; sq = sprintf("%c", 39) }

# the name of the file the lines after it came from, and a boundary no table
# spans
mark != "" && index($0, mark) == 1 { where = substr($0, length(mark) + 1); collecting = 0; pending = 0; dead = 0; fline = FNR; next }

# An arm no build takes, followed to its end through the arms nested in it. An
# arm after it is one a build does take, and it is counted as gating what it
# holds like any other, which also keeps its #endif balanced against the count.
dead && /^[ \t]*#[ \t]*(if|ifdef|ifndef)/ { dead++; next }
dead && /^[ \t]*#[ \t]*(else|elif)/ { if (dead == 1) { dead = 0; if (collecting) ppdepth++ } next }
dead && /^[ \t]*#[ \t]*endif/ { dead--; next }
dead { next }
/^[ \t]*#[ \t]*if[ \t]+0[ \t]*$/ { dead = 1; next }

# The arms are counted rather than followed: which of them a build takes is not
# something this can know, and the flag says only that one gates the entry.
/^[ \t]*#[ \t]*(if|ifdef|ifndef)/ { if (collecting) ppdepth++; next }
/^[ \t]*#[ \t]*(else|elif)/ { if (collecting && ppdepth == 0) ppdepth++; next }
/^[ \t]*#[ \t]*endif/ { if (collecting && ppdepth > 0) ppdepth--; next }
/^[ \t]*#/ { next }

{
	line = $0
	if (!collecting && !pending) {
		# keyval, or keyval_ext with its third member. One whose declaration
		# ends before an initialiser starts is an array filled at run time,
		# which holds no table this can read.
		if (!match(line, /keyval(_ext)?[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]*\[/)) next
		head = substr(line, RSTART, RLENGTH)
		sub(/^keyval(_ext)?[ \t]+/, "", head)
		sub(/[ \t]*\[$/, "", head)
		name = head
		startline = FNR - fline
		ppdepth = 0
		rest = substr(line, RSTART + RLENGTH)
		p = index(rest, "{")
		if (ended(rest, p)) next
		if (p == 0) { pending = 1; next }
		start()
		scan(substr(rest, p + 1))
		next
	}
	if (pending) {
		p = index(line, "{")
		if (ended(line, p)) { pending = 0; next }
		if (p == 0) next
		pending = 0
		start()
		scan(substr(line, p + 1))
		next
	}
	scan(line)
}

function start() { collecting = 1; depth = 1; buf = ""; entrypp = 0; q = "" }

# whether a semicolon comes before the brace at p, or with no brace at all
function ended(s, p,   e) {
	e = index(s, ";")
	return e > 0 && (p == 0 || e < p)
}

function scan(s,   i, n, c) {
	n = length(s)
	for (i = 1; i <= n; i++) {
		c = substr(s, i, 1)
		if (q != "") { buf = buf c; if (c == q) q = ""; continue }
		if (c == "\"" || c == sq) { q = c; buf = buf c; continue }
		if (c == "{") { if (depth == 1) entrypp = ppdepth; depth++; buf = buf c; continue }
		if (c == "}") {
			depth--
			# the last entry carries no comma after it, so the closing brace is
			# what ends it
			if (depth == 0) { emit(); collecting = 0; return }
			buf = buf c
			continue
		}
		if (depth == 1 && c == ",") { emit(); continue }
		buf = buf c
	}
	# a literal does not span lines, and a line break inside the array is a
	# separator like any other space
	q = ""
	buf = buf " "
}

# The label is the locale an entry names, or the literal a keyval_ext entry
# names its value under instead, quotes and all, which is what tells the two
# apart further on. The screen shows the literal whenever there is one, and the
# locale only when the third member is null.
function emit(   e, m, n, k, v, t) {
	e = buf
	buf = ""
	sub(/^[ \t]+/, "", e)
	sub(/[ \t]+$/, "", e)
	if (e == "") return
	if (e !~ /^\{.*\}$/) { print name "\t?\t?\t" (entrypp > 0 ? 1 : 0) "\t" where ":" startline; return }
	sub(/^\{[ \t]*/, "", e)
	sub(/[ \t]*\}$/, "", e)
	n = members(e, m)
	if (n != 2 && n != 3) { print name "\t?\t?\t" (entrypp > 0 ? 1 : 0) "\t" where ":" startline; return }
	k = m[1]
	v = m[2]
	gsub(/[ \t]/, "", k)
	gsub(/[ \t]/, "", v)
	if (n == 3) {
		t = m[3]
		sub(/^[ \t]+/, "", t)
		sub(/[ \t]+$/, "", t)
		# words the files further on would split at, on the commas and the
		# signs that join an entry, are words this cannot hand on
		if (t ~ /^"[^"]*"$/)
			v = (t ~ /[,=\t]/) ? "?" : t
		else if (t != "NULL" && t != "0" && t != "nullptr")
			v = "?"
	}
	print name "\t" k "\t" v "\t" (entrypp > 0 ? 1 : 0) "\t" where ":" startline
}

# The members of one entry, split at the commas between them, which are not all
# the commas in it: a macro that takes arguments carries commas of its own, and a
# literal can hold any.
function members(e, m,   i, n, c, d, cnt, from, qq) {
	n = length(e)
	d = 0
	cnt = 0
	from = 1
	qq = ""
	for (i = 1; i <= n; i++) {
		c = substr(e, i, 1)
		if (qq != "") { if (c == qq) qq = ""; continue }
		if (c == "\"" || c == sq) { qq = c; continue }
		if (c == "(" || c == "[") { d++; continue }
		if (c == ")" || c == "]") { d--; continue }
		if (c == "," && d == 0) { m[++cnt] = substr(e, from, i - from); from = i + 1 }
	}
	m[++cnt] = substr(e, from)
	return cnt
}
