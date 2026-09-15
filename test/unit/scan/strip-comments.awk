# C comments removed, so that a scan of the source reads what the compiler
# reads. One pass rather than a pass per form, because whichever of a comment
# opener and a quote comes first swallows the other, and two passes get that
# backwards.
#
# keepstrings=1 leaves literals as they stand, for a scan that reads the text of
# one. keepstrings=0 replaces each with a single digit, which keeps a comma or a
# parenthesis inside a literal out of an argument scan without joining the
# tokens on either side of it.
#
# mark is what a scan reading a whole tree in one pass needs: the name of each
# file ahead of it, on a line of its own behind that prefix. Thousands of files
# read by one process rather than by several apiece halves what such a scan
# costs, and the state a scan carries from line to line has to be dropped at
# each boundary for that to read the same as a pass per file.
BEGIN { sq = sprintf("%c", 39) }
# neither a comment nor a literal spans a file, so a scan handed several of them
# starts each one clean
FNR == 1 { inblock = 0; instr = 0; if (mark != "") print mark FILENAME }
{
	line = $0; out = ""; n = length(line); i = 1
	while (i <= n) {
		c = substr(line, i, 1)
		d = substr(line, i, 2)
		if (inblock) {
			if (d == "*/") { inblock = 0; i += 2 } else i++
			continue
		}
		if (instr) {
			if (c == "\\") { i += 2; continue }
			if (c == q) { instr = 0; out = out (keepstrings ? c : "0") }
			else if (keepstrings) out = out c
			i++
			continue
		}
		if (d == "//") break
		if (d == "/*") { inblock = 1; i += 2; continue }
		if (c == "\"" || c == sq) { instr = 1; q = c; if (keepstrings) out = out c; i++; continue }
		out = out c
		i++
	}
	# a literal does not span lines, a block comment does
	instr = 0
	print out
}
