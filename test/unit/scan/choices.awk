# One call site at a time, because the call spans lines and its arguments carry
# parentheses and commas of their own. Depth is counted from the opening
# parenthesis and an argument is what lies between commas at depth one, so an
# argument that is itself a call or a condition is read whole.
#
# CMenuOptionChooser takes its label, the value it edits, the table of choices and
# how many of them, in that order (src/gui/widget/menue.h). The table is what binds
# a setting to the values a screen offers for it.
#
# Reads the source with its literals already blanked, which keeps a comma inside
# one out of the count.
BEGIN { call = "CMenuOptionChooser("; clen = length(call); collecting = 0 }

# the name of the file the lines after it came from, and a boundary no call
# spans
mark != "" && index($0, mark) == 1 { where = substr($0, length(mark) + 1); collecting = 0; fline = FNR; next }

{
	# a line break inside the call is a separator like any other space
	line = $0 " "
	n = length(line)
	pos = 1
	while (pos <= n) {
		if (!collecting) {
			p = index(substr(line, pos), call)
			if (p == 0) break
			pos = pos + p - 1 + clen
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
		if (ch == "(" || ch == "[") { depth++; args[na] = args[na] ch; continue }
		if (ch == ")" || ch == "]") {
			depth--
			if (depth == 0) { emit(); collecting = 0; continue }
			args[na] = args[na] ch
			continue
		}
		if (depth == 1 && ch == ",") { na++; args[na] = ""; continue }
		args[na] = args[na] ch
	}
}
function emit(   i, ptr, field, lab, arr) {
	# four arguments before the first one with a default of its own
	if (na < 4) return
	for (i = 1; i <= 4; i++) {
		gsub(/[ \t]+/, " ", args[i])
		sub(/^ /, "", args[i])
		sub(/ $/, "", args[i])
	}

	ptr = args[2]
	# the cast in front of the address is there where the field is a width the
	# chooser does not take, and says nothing about which field it is
	sub(/^\( *int *\* *\) */, "", ptr)
	# a plain field and not an element of an array, because a setting reached
	# through an index is not one a single key names
	if (ptr !~ /^& *g_settings\.[A-Za-z_][A-Za-z_0-9]*$/) return
	field = ptr
	sub(/^& *g_settings\./, "", field)

	lab = args[1]
	if (lab !~ /^LOCALE_[A-Z0-9_]+$/) lab = "?"

	# a table built at run time is not one a scan of the source can read
	arr = args[3]
	if (arr !~ /^[A-Za-z_][A-Za-z_0-9]*$/) arr = "?"

	print field "\t" lab "\t" arr "\t" where ":" startline
}
