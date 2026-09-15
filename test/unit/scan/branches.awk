# Every option one member function branches on, read out of its own body. Which
# member is asked for is handed in, because the seam has two shapes: a notifier
# that takes the option and decides on it, and an applier that decides for itself.
#
# cls, member and where are handed in. One line out per naming, tab between: the
# enumerator and where it was read. A body naming one option twice answers twice
# and the caller sorts.
#
# An arm the source itself has switched off with #if 0 is skipped. Every other arm
# is read, because a branch behind a build condition is one some box takes.
BEGIN { inside = 0; dead = 0 }

/^[ \t]*#[ \t]*if/ {
	if (dead > 0) { dead++; next }
	if ($0 ~ /^[ \t]*#[ \t]*if[ \t]+0([^0-9]|$)/) dead = 1
	next
}
/^[ \t]*#[ \t]*(else|elif)/ { if (dead == 1) dead = 0; next }
/^[ \t]*#[ \t]*endif/ { if (dead > 0) dead -= 1; next }
dead > 0 { next }

$0 ~ ("^bool " cls "::" member "\\(") { inside = 1; next }
inside && /^\}/ { inside = 0 }
inside {
	line = $0
	while (match(line, /LOCALE_[A-Z0-9_]+/)) {
		print substr(line, RSTART, RLENGTH) "\t" where ":" FNR
		line = substr(line, RSTART + RLENGTH)
	}
}
