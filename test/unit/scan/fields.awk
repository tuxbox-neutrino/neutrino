# Every member of SNeutrinoSettings, and whether a table row could carry it. Read after
# the comments have been taken out.
#
# The conditionals are ignored rather than followed, so every arm is read: a row may be
# written for a box this build is not for, and the count it is held to has to hold those
# as well.
#
# One line per member, tab between: the name, scalar or aggregate, whether it stands
# inside a conditional arm, and for an aggregate what makes it one.
BEGIN { depth = 0; body = 0; seen = 0; arm = 0 }
/^[ \t]*#[ \t]*(if|ifdef|ifndef)/ { if (body) arm++; next }
/^[ \t]*#[ \t]*endif/ { if (body && arm > 0) arm--; next }
/^[ \t]*#/ { next }
!body {
	# the definition and not a forward declaration, which ends before its brace
	if ($0 ~ /struct[ \t]+SNeutrinoSettings[ \t]*(\/|$)/) { found = 1 }
	if (!found) next
	if ($0 ~ /;/) { found = 0; next }
	if (index($0, "{") == 0) next
	body = 1
	sub(/^.*\{/, "", $0)
	depth = 1
}
{
	line = $0
	n = length(line)
	for (i = 1; i <= n; i++) {
		c = substr(line, i, 1)
		if (c == "{") {
			depth++
			# what stood before a nested block says whether the declarator
			# after it is a member or a type name
			if (depth == 2) prefix = buf
			buf = ""
			continue
		}
		if (c == "}") {
			depth--
			buf = ""
			continue
		}
		if (c == ";" && depth == 1) {
			emit(prefix, buf)
			prefix = ""
			buf = ""
			continue
		}
		if (depth == 1) buf = buf c
	}
	if (depth >= 1) { buf = buf " "; seen = 1 }
	if (depth <= 0 && seen) exit
}
END {
	if (!seen) {
		print "fields.awk: no SNeutrinoSettings body was read" > "/dev/stderr"
		exit 1
	}
	if (depth != 0) {
		print "fields.awk: the struct body does not close, depth " depth > "/dev/stderr"
		exit 1
	}
}

function emit(pre, decl,   name, type, why, bare)
{
	gsub(/[ \t]+/, " ", decl); sub(/^ /, "", decl); sub(/ $/, "", decl)
	gsub(/[ \t]+/, " ", pre); sub(/^ /, "", pre); sub(/ $/, "", pre)
	if (decl == "") return
	# a name after a nested block is a type this struct declares when the
	# block was a typedef, and a member of its own when it was not
	if (pre != "" && pre ~ /(^| )typedef( |$)/) return
	if (decl ~ /(^| )typedef( |$)/) return
	# the subscripts come off first, so that a space before one does not take
	# the name with it
	bare = decl
	gsub(/ *\[[^]]*\]/, "", bare)
	sub(/ $/, "", bare)
	if (bare !~ / [A-Za-z_][A-Za-z_0-9]*$/ && pre == "") {
		print "fields.awk: cannot read a declarator out of: " decl > "/dev/stderr"
		exit 1
	}
	name = bare
	sub(/^.* /, "", name)
	if (pre != "") {
		print name "\taggregate\t" where() "\tnested block"
		return
	}
	type = bare
	sub(/ [A-Za-z_][A-Za-z_0-9]*$/, "", type)
	if (decl ~ /\[/) {
		print name "\taggregate\t" where() "\tarray"
		return
	}
	if (decl ~ /,/) {
		print "fields.awk: two members in one declaration are not read: " decl > "/dev/stderr"
		exit 1
	}
	why = kind(type)
	if (why == "") {
		print "fields.awk: no verdict for the type " type " of " name > "/dev/stderr"
		exit 1
	}
	if (why == "scalar")
		print name "\tscalar\t" where()
	else
		print name "\taggregate\t" where() "\t" why
}

# A member behind a conditional exists on some builds and not on others, which
# is why a table naming one is compiled under every arm.
function where()
{
	return arm > 0 ? "conditional" : "plain"
}

# The types the struct holds, each answered once. A type nothing here names
# stops the scan rather than being guessed at, because guessing it either way
# moves the count this stands behind.
function kind(t)
{
	if (t == "int" || t == "bool" || t == "char" || t == "unsigned char") return "scalar"
	if (t == "std::string") return "scalar"
	# Sixty four bits wide, which no long on the box holds, so a row carries one
	# as the text a channel is named by rather than as a number. Carriable all
	# the same, and counted here as one: leaving it out would have put the two
	# start channels outside every set this compares.
	if (t == "t_channel_id") return "scalar"
	if (t == "SNeutrinoTheme" || t == "SNeutrinoGlcdTheme") return "a struct of its own"
	if (t ~ /^std::(list|vector|map|set)</) return "a container"
	return ""
}
