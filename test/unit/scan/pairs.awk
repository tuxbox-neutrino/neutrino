# One load site at a time: the field it assigns, the key it reads and the
# default it falls back to. The default is the second argument and it carries
# parentheses, commas and string literals of its own, so it is read by counting
# depth from the opening parenthesis rather than by a pattern.
#
# Two spellings of the same load, because a text field is not assigned to: it is
# written through setSettingsText, under the lock its readers take. Everything
# after the opening parenthesis of the read is the same in both, so only the
# head differs.
#
# And a third, for a text field whose value is worked on before it is
# published: it is read into a local first and handed to setSettingsText from
# there, so the key and the default stand where the local is declared and the
# field where it is published. The local's name joins the two until it is
# published: a local is forgotten once it is published, at the end of the
# function it is declared in, and when another such read declares it again.
#
# Reads the source with its literals intact, because both the key and a string
# default are literals.

# The arguments of the read whose opening parenthesis ends just before rest, one
# per element of a, trimmed; na is how many there are. Answers where in rest the
# read ends.
function readargs(rest, a,    depth, n, i, c, k)
{
	depth = 1
	na = 1
	split("", a)
	a[1] = ""
	n = length(rest)
	i = 1
	while (i <= n) {
		c = substr(rest, i, 1)
		# a literal is copied whole, so a comma or a parenthesis inside one
		# neither splits an argument nor closes the call
		if (c == "\"") {
			a[na] = a[na] c
			i++
			while (i <= n) {
				c = substr(rest, i, 1)
				a[na] = a[na] c
				i++
				if (c == "\\") { a[na] = a[na] substr(rest, i, 1); i++; continue }
				if (c == "\"") break
			}
			continue
		}
		if (c == "(") depth++
		else if (c == ")") { depth--; if (depth == 0) break }
		if (depth == 1 && c == ",") { na++; a[na] = ""; i++; continue }
		a[na] = a[na] c
		i++
	}

	for (k = 1; k <= na; k++) {
		gsub(/[ \t]+/, " ", a[k])
		sub(/^ /, "", a[k])
		sub(/ $/, "", a[k])
	}
	return i
}

# a key the program builds rather than writes is not one a row can name
function literal(arg)
{
	return arg ~ /^"[^"]*"$/
}

function unquote(arg)
{
	sub(/^"/, "", arg)
	sub(/"$/, "", arg)
	return arg
}

{
	line = $0

	# the third spelling: the local first ...
	if (match(line, /std::string[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]*=[ \t]*(configfile\.|tconfig->)get[A-Za-z0-9_]*\(/)) {
		head = substr(line, RSTART, RLENGTH)
		name = head
		sub(/^std::string[ \t]+/, "", name)
		sub(/[ \t]*=.*/, "", name)
		# declared again, it is a new local, even when its key is built
		delete localkey[name]
		delete localdefault[name]
		readargs(substr(line, RSTART + RLENGTH), a)
		if (literal(a[1])) {
			localkey[name] = unquote(a[1])
			localdefault[name] = (na >= 2 ? a[2] : "@none")
		}
	}

	# ... then where it is published
	if (match(line, /setSettingsText\([ \t]*g_settings\.[A-Za-z_][A-Za-z_0-9]*[ \t]*,[ \t]*[A-Za-z_][A-Za-z_0-9]*[ \t]*\)/)) {
		published = substr(line, RSTART, RLENGTH)
		field = published
		sub(/^setSettingsText\([ \t]*g_settings\./, "", field)
		sub(/[ \t]*,.*/, "", field)
		name = published
		sub(/^[^,]*,[ \t]*/, "", name)
		sub(/[ \t]*\)$/, "", name)
		if (name in localkey) {
			print localkey[name] "\t" field "\t" localdefault[name]
			delete localkey[name]
			delete localdefault[name]
		}
	}

	while (match(line, /(g_settings\.[A-Za-z_][A-Za-z_0-9]*[ \t]*=|setSettingsText\([ \t]*g_settings\.[A-Za-z_][A-Za-z_0-9]*[ \t]*,)[ \t]*(configfile\.|tconfig->)get[A-Za-z0-9_]*\(/)) {
		head = substr(line, RSTART, RLENGTH)
		rest = substr(line, RSTART + RLENGTH)

		field = head
		sub(/^setSettingsText\([ \t]*/, "", field)
		sub(/^g_settings\./, "", field)
		sub(/[ \t]*[=,].*/, "", field)

		i = readargs(rest, a)

		if (literal(a[1])) {
			# a read with no fallback is not the same as one whose fallback is
			# empty, so the two are told apart rather than both printed as none
			print unquote(a[1]) "\t" field "\t" (na >= 2 ? a[2] : "@none")
		}

		line = substr(rest, i + 1)
	}
}

# a local is forgotten at the end of the function it is declared in
/^}/ { split("", localkey); split("", localdefault) }
