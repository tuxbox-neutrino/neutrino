# The label a screen states beside a setting, where the setting is offered as neither a
# range nor a table of choices. Those two are read by the scans beside this; a row
# offered by anything else has its label compared against nothing at all.
#
# Two shapes state such a pair. A widget constructor takes the label first and the
# setting in the one argument it carries it in, and a table of items writes the same
# thing as an initialiser whose first element is the label.
#
# Which argument that is has to be known per widget rather than taken as any of them: a
# forwarder takes whether it is active in the argument before the one it shows, and a
# screen greys one item out on the value of another, so reading the whole call pairs a
# label with a setting it only asks about.
#
# One site at a time, because it spans lines and carries parentheses, braces and commas
# of its own. Depth is counted from the opening bracket and an element is what lies
# between commas at depth one. Reads the source with its literals already blanked.
# The argument each takes the setting in, from the declarations in src/gui/widget: the
# forwarder shows it as its option, and the rest edit it through the pointer that
# follows the label. Reading a colour needs four.
BEGIN {
	n = split("CMenuForwarder:3 CMenuOptionStringChooser:2 CStringInput:2 CKeyboardInput:2 CIntInput:2 CPINChangeWidget:2 CColorChooser:2-5", w, " ")
	for (i = 1; i <= n; i++) {
		split(w[i], part, ":")
		call[i] = part[1] "("
		slot[i] = part[2]
	}
	ncall = n
	collecting = 0
}

# the name of the file the lines after it came from, and a boundary no site
# spans
mark != "" && index($0, mark) == 1 { where = substr($0, length(mark) + 1); collecting = 0; fline = FNR; next }

{
	# a line break inside the site is a separator like any other space
	line = $0 " "
	n = length(line)
	pos = 1
	while (pos <= n) {
		if (!collecting) {
			best = 0
			blen = 0
			want = ""
			for (i = 1; i <= ncall; i++) {
				p = index(substr(line, pos), call[i])
				if (p > 0 && (best == 0 || p < best)) { best = p; blen = length(call[i]); want = slot[i] }
			}
			# an initialiser that opens with a label, which is what a table of
			# items writes each of its rows as
			q = pos
			while (1) {
				p = index(substr(line, q), "{")
				if (p == 0) break
				q = q + p - 1
				if (substr(line, q + 1) ~ /^[ \t]*LOCALE_/) {
					p = q - pos + 1
					# a table of items states nothing but the pair, so every
					# element after the label answers for the setting
					if (best == 0 || p < best) { best = p; blen = 1; want = "2-99" }
					break
				}
				q++
			}
			if (best == 0) break
			pos = pos + best - 1 + blen
			collecting = 1
			depth = 1
			na = 1
			split("", args)
			args[1] = ""
			startline = FNR - fline
			continue
		}
		ch = substr(line, pos, 1)
		pos++
		if (ch == "(" || ch == "[" || ch == "{") { depth++; args[na] = args[na] ch; continue }
		if (ch == ")" || ch == "]" || ch == "}") {
			depth--
			if (depth == 0) { emit(); collecting = 0; continue }
			args[na] = args[na] ch
			continue
		}
		if (depth == 1 && ch == ",") { na++; args[na] = ""; continue }
		args[na] = args[na] ch
	}
}

function emit(   i, lab, rest, seen, f, lo, hi, r) {
	if (na < 2) return
	lab = args[1]
	gsub(/[ \t]+/, "", lab)
	# the declaration of the widget itself takes a type here rather than a name
	if (lab !~ /^LOCALE_[A-Z0-9_]+$/) return

	split(want, r, "-")
	lo = r[1] + 0
	hi = (r[2] == "") ? lo : r[2] + 0
	rest = ""
	for (i = lo; i <= hi && i <= na; i++) rest = rest args[i] " "

	split("", seen)
	while (1) {
		f = member(rest)
		if (f == "") break
		sub(/g_settings\./, "", rest)
		# reached through an index or through a member of its own, so not a
		# setting a single key names
		if (f == "?") continue
		if (f in seen) continue
		seen[f] = 1
		print f "\t" lab "\t" where ":" startline
	}
}

# The next g_settings member the text names.
function member(s,   p, t, i, c, name) {
	p = index(s, "g_settings.")
	if (p == 0) return ""
	t = substr(s, p + 11)
	name = ""
	for (i = 1; i <= length(t); i++) {
		c = substr(t, i, 1)
		if (c ~ /[A-Za-z_0-9]/) { name = name c; continue }
		break
	}
	if (name == "" || name ~ /^[0-9]/) return "?"
	c = substr(t, length(name) + 1, 1)
	if (c == "." || c == "[") return "?"
	return name
}
