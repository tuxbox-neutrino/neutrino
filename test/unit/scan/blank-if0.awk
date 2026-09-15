# #if 0 ... #endif blanked to blank lines, so a scan of the source reads what the
# compiler reads. #if 0 is the one spelling that leaves a line exactly where it is
# and still keeps it out of the binary, so every guard here that checks a call is
# present, or that two calls run in a given order, has to blank it.
#
# One output line per input line, so the numbers still match whatever produced the
# input. Meant to run behind strip-comments.awk, so a "#if 0" written inside a
# comment cannot blank live code below it.
#
# Only a literal "#if 0" opens a blanked block. "#if SOMETHING", "#ifdef" and
# "#ifndef" are left standing: those are how this tree is built for more than one
# box (DISABLE_LEGACY_API among them), and blanking them would make a guard unable
# to find the call it is looking for. Nesting is tracked.
#
# #else and #elif at the depth a tracked "#if 0" opened flip that depth back to
# live, because that branch is exactly the one a false "0" always takes. Once live,
# a depth stays live through any further #elif/#else at the same level: an #elif's
# condition is not evaluated here, so the blanking has to err toward not hiding
# code.
#
# neither a directive nor a block spans a file, so a scan handed several of them
# starts each one clean
FNR == 1 { depth = 0; zero = 0 }
{
	line = $0; stripped = line
	sub(/^[ \t]*/, "", stripped)
	if (stripped ~ /^#[ \t]*if/) {
		depth++
		if (zero == 0 && stripped ~ /^#[ \t]*if[ \t]+0([ \t]|$)/) zero = depth
		print line
		next
	}
	if (stripped ~ /^#[ \t]*el(se|if)/) {
		if (zero != 0 && depth == zero) zero = 0
		print line
		next
	}
	if (stripped ~ /^#[ \t]*endif/) {
		if (zero != 0 && depth == zero) zero = 0
		depth--
		print line
		next
	}
	if (zero != 0 && depth >= zero) print ""
	else print line
}
