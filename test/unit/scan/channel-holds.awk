# Marks every line of a prepared source file (comments, strings and #if 0
# blocks blanked) with whether it stands in a hold of the channel lock, for the
# scans that hold the channel stack's writers and holders to that lock.
#
# A line is held when a guard of the channel lock was declared before it in a
# block that is still open - CServiceManager::ChannelGuard, or a ScopedLock or
# a lock() on channels_mutex -, when it lies between a LockChannels() and the
# UnlockChannels() after it, or when it lies in a function whose name ends in
# Locked, whose caller holds the lock by the rule those functions are named
# after.
#
# A function is told by its definition, which starts in the first column, does
# not end in a semicolon and names the function before its first parenthesis:
# a member as Class::function, a free function after its return type.
#
# Prints, per line: the line number, 1 or 0 for held, 1 or 0 for held already
# before the line (a guard declared on it not counted), the function the line
# is in (empty outside one), and the line itself, separated by tabs.
function guard_at(s,    k) {
	k = index(s, "CServiceManager::ChannelGuard")
	if (k == 0 && index(s, "channels_mutex") > 0 && (index(s, "ScopedLock") > 0 || index(s, ".lock(") > 0))
		k = index(s, "channels_mutex")
	return k
}
# The function a line defines, or "" for a line that defines none.
function defined_name(s,    head, n, part) {
	if (s !~ /^[A-Za-z_~]/ || s ~ /;[ \t]*$/ || index(s, "(") == 0)
		return ""
	if (s ~ /^(if|for|while|switch|return|else|do|case|default|typedef|using|namespace|template|extern|struct|class|union|enum)([^A-Za-z_0-9]|$)/)
		return ""
	head = substr(s, 1, index(s, "(") - 1)
	sub(/[ \t]+$/, "", head)
	if (head ~ /::~?[A-Za-z_][A-Za-z_0-9]*$/) {
		n = split(head, part, "::")
		return part[n]
	}
	if (head ~ /[A-Za-z_0-9*&>][ \t*&]+[A-Za-z_][A-Za-z_0-9]*$/) {
		sub(/^.*[ \t*&]/, "", head)
		return head
	}
	return ""
}
BEGIN { depth = 0; sp = 0; manual = 0; fn = ""; fn_depth = -1 }
{
	line = $0
	# The body is the block the definition opens.
	if (fn_depth < 0) {
		name = defined_name(line)
		if (name != "") {
			fn = name
			pending_fn = 1
		}
	}

	locks = (line ~ /(^|[^A-Za-z_0-9])LockChannels[ \t]*\(/)
	unlocks = (line ~ /(^|[^A-Za-z_0-9])UnlockChannels[ \t]*\(/)
	before = (sp > 0 || manual > 0 || (fn != "" && fn ~ /Locked$/)) ? 1 : 0
	held = before
	g = guard_at(line)
	if (g > 0 || locks)
		held = 1

	print NR "\t" held "\t" before "\t" fn "\t" line

	if (locks)
		manual++
	if (unlocks && manual > 0)
		manual--
	n = length(line)
	for (i = 1; i <= n; i++) {
		c = substr(line, i, 1)
		if (g > 0 && i == g) {
			stack[++sp] = depth
		}
		if (c == "{") {
			depth++
			if (pending_fn) { fn_depth = depth; pending_fn = 0 }
		} else if (c == "}") {
			depth--
			while (sp > 0 && depth < stack[sp])
				sp--
			if (fn_depth >= 0 && depth < fn_depth) {
				fn = ""
				fn_depth = -1
			}
		}
	}
}
