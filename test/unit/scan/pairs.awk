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
# Reads the source with its literals intact, because both the key and a string
# default are literals.
{
	line = $0
	while (match(line, /(g_settings\.[A-Za-z_][A-Za-z_0-9]*[ \t]*=|setSettingsText\([ \t]*g_settings\.[A-Za-z_][A-Za-z_0-9]*[ \t]*,)[ \t]*(configfile\.|tconfig->)get[A-Za-z0-9_]*\(/)) {
		head = substr(line, RSTART, RLENGTH)
		rest = substr(line, RSTART + RLENGTH)

		field = head
		sub(/^setSettingsText\([ \t]*/, "", field)
		sub(/^g_settings\./, "", field)
		sub(/[ \t]*[=,].*/, "", field)

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

		# a key the program builds rather than writes is not one a row can name
		if (a[1] ~ /^"[^"]*"$/) {
			key = a[1]
			sub(/^"/, "", key)
			sub(/"$/, "", key)
			# a read with no fallback is not the same as one whose fallback is
			# empty, so the two are told apart rather than both printed as none
			print key "\t" field "\t" (na >= 2 ? a[2] : "@none")
		}

		line = substr(rest, i + 1)
	}
}
