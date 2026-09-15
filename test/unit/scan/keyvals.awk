# One table of named choices at a time: the array's name and the entries it holds. A
# screen states what a setting offers here and the declaration states it again, and
# nothing but a scan of this can hold the two together.
#
# An entry is flagged when a preprocessor arm inside the array gates it, because a value
# only one box model is given is one the declaration may leave out.
#
# Reads the source with its literals already blanked, which keeps a comma inside one out
# of the entry count.
BEGIN { collecting = 0; pending = 0 }

# the name of the file the lines after it came from, and a boundary no table
# spans
mark != "" && index($0, mark) == 1 { where = substr($0, length(mark) + 1); collecting = 0; pending = 0; fline = FNR; next }

# The arms are counted rather than followed: which of them a build takes is not
# something this can know, and the flag says only that one gates the entry.
/^[ \t]*#[ \t]*(if|ifdef|ifndef)/ { if (collecting) ppdepth++; next }
/^[ \t]*#[ \t]*(else|elif)/ { if (collecting && ppdepth == 0) ppdepth++; next }
/^[ \t]*#[ \t]*endif/ { if (collecting && ppdepth > 0) ppdepth--; next }
/^[ \t]*#/ { next }

{
	line = $0
	if (!collecting && !pending) {
		# keyval and not keyval_ext, which carries a third member this cannot
		# read; the space after the type is what tells the two apart.
		if (!match(line, /keyval[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]*\[/)) next
		head = substr(line, RSTART, RLENGTH)
		sub(/^keyval[ \t]+/, "", head)
		sub(/[ \t]*\[$/, "", head)
		name = head
		startline = FNR - fline
		ppdepth = 0
		rest = substr(line, RSTART + RLENGTH)
		p = index(rest, "{")
		if (p == 0) { pending = 1; next }
		start()
		scan(substr(rest, p + 1))
		next
	}
	if (pending) {
		p = index(line, "{")
		if (p == 0) next
		pending = 0
		start()
		scan(substr(line, p + 1))
		next
	}
	scan(line)
}

function start() { collecting = 1; depth = 1; buf = ""; entrypp = 0 }

function scan(s,   i, n, c) {
	n = length(s)
	for (i = 1; i <= n; i++) {
		c = substr(s, i, 1)
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
	# a line break inside the array is a separator like any other space
	buf = buf " "
}

function emit(   e, k, v, p) {
	e = buf
	buf = ""
	gsub(/[ \t]+/, " ", e)
	sub(/^ /, "", e)
	sub(/ $/, "", e)
	if (e == "") return
	if (e !~ /^\{.*\}$/) { print name "\t?\t?\t" (entrypp > 0 ? 1 : 0) "\t" where ":" startline; return }
	sub(/^\{ ?/, "", e)
	sub(/ ?\}$/, "", e)
	p = topcomma(e)
	if (p == 0) { print name "\t?\t?\t" (entrypp > 0 ? 1 : 0) "\t" where ":" startline; return }
	k = substr(e, 1, p - 1)
	v = substr(e, p + 1)
	gsub(/ /, "", k)
	gsub(/ /, "", v)
	print name "\t" k "\t" v "\t" (entrypp > 0 ? 1 : 0) "\t" where ":" startline
}

# The comma between the two members, which is not the first one when the value
# is written as a call: a macro that takes arguments carries commas of its own.
function topcomma(e,   i, n, c, d) {
	n = length(e)
	d = 0
	for (i = 1; i <= n; i++) {
		c = substr(e, i, 1)
		if (c == "(" || c == "[") { d++; continue }
		if (c == ")" || c == "]") { d--; continue }
		if (c == "," && d == 0) return i
	}
	return 0
}
