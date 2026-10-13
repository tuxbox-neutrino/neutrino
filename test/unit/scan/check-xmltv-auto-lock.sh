#!/bin/sh
# xmltv_xml_auto is the list of the guides the web channel lists name. Every
# load of the bouquets empties and refills it, and a load runs on the box's
# loop as well as on the channel daemon's thread and the scan's. The loop walks
# the list after a reload of its own, and a walk of the list while another
# thread's load empties it stands on freed nodes.
#
# The rule this holds the tree to: the list is changed only by the two helpers
# in neutrino.h, which take the settings' text lock, and read only through
# settingsCopy, which takes the same lock and answers with a copy. The
# accessor's own lock is checked as well, or the rest of this checks nothing.
#
# None of this links into the test binary, so it is read as text, with comments
# and #if 0 blocks blanked the way the compiler would.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC/src" ] || {
	echo "usage: check-xmltv-auto-lock.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
SETTINGS="$SRC/src/system/settings.h"
NEUTRINO="$SRC/src/neutrino.h"
for f in "$STRIP" "$BLANK" "$SETTINGS" "$NEUTRINO"; do
	[ -r "$f" ] || { echo "check-xmltv-auto-lock.sh: cannot read $f" >&2; exit 1; }
done

GUARD='CSettingsTextGuard lock;'
HELPERS='settingsCopy g_settings_xmltv_xml_auto_clear g_settings_xmltv_xml_auto_pushback'

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# Every mention of the list as a whole name, file by file, with the helper whose
# body it stands in ("-" outside them), and per helper whether its body takes
# the lock. A helper is read from the line that names it with an opening
# parenthesis to the brace that closes its body. It takes the lock where the
# guard is declared in the body's own block, not in one nested inside it, whose
# end would let the lock go before what follows, and ahead of every mention of
# the list in it; what settingsCopy returns follows the guard in the same
# block.
SCAN='
	BEGIN { n = split(helpers, h, " ") }
	inside == "" {
		for (i = 1; i <= n; i++)
			if (index($0, h[i] "(") && $0 !~ /[;]/) {
				inside = h[i]; depth = 0; opened = 0; locked = 0; early = 0
			}
	}
	/(^|[^A-Za-z0-9_])xmltv_xml_auto([^A-Za-z0-9_]|$)/ {
		print file "\t" FNR "\t" (inside == "" ? "-" : inside) "\t" $0
		if (inside != "" && !locked) early = 1
	}
	inside != "" {
		at = index($0, guard)
		m = split($0, ch, "")
		for (i = 1; i <= m; i++) {
			if (i == at && opened && depth == 1) locked = 1
			if (ch[i] == "{") { depth++; opened = 1 }
			if (ch[i] == "}") depth--
		}
		if (opened && depth == 0) {
			print file "\t" inside "\t" (locked && !early) >> out
			inside = ""
		}
	}'
scan() {
	awk -v keepstrings=0 -f "$STRIP" "$1" | awk -f "$BLANK" | awk -v file="$2" \
		-v helpers="$HELPERS" -v guard="$GUARD" -v out="$3" "$SCAN"
}

# The judgement first, on two helpers written for it: one that holds the lock,
# and one whose guard ends in a block of its own before the copy is made.
cat > "$tmp/held.h" <<'SAMPLE'
template <class C>
inline C settingsCopy(const C &list)
{
	CSettingsTextGuard lock;
	return list;
}
SAMPLE
cat > "$tmp/expired.h" <<'SAMPLE'
template <class C>
inline C settingsCopy(const C &list)
{
	{ CSettingsTextGuard lock; }
	return list;
}
SAMPLE
scan "$tmp/held.h" held "$tmp/control" > /dev/null
scan "$tmp/expired.h" expired "$tmp/control" > /dev/null
[ "`cat "$tmp/control"`" = "`printf 'held\tsettingsCopy\t1\nexpired\tsettingsCopy\t0'`" ] || {
	echo "check-xmltv-auto-lock.sh: the scan judges a held and an expired guard wrongly:" >&2
	cat "$tmp/control" >&2
	exit 1
}

: > "$tmp/helpers"
find "$SRC/src" "$SRC/lib" \( -name '*.cpp' -o -name '*.h' \) | sort | while read -r f; do
	scan "$f" "${f#"$SRC"/}" "$tmp/helpers"
done > "$tmp/mentions"

fail=0

# Each helper defined once where it belongs, and taking the lock.
for spec in src/system/settings.h:settingsCopy \
	    src/neutrino.h:g_settings_xmltv_xml_auto_clear \
	    src/neutrino.h:g_settings_xmltv_xml_auto_pushback; do
	file=${spec%%:*}; fn=${spec#*:}
	got=`awk -F '\t' -v file="$file" -v fn="$fn" '$1 == file && $2 == fn { print $3 }' "$tmp/helpers"`
	case "$got" in
		1) ;;
		"") echo "check-xmltv-auto-lock.sh: no $fn in $file" >&2; fail=1 ;;
		*) echo "check-xmltv-auto-lock.sh: $fn in $file takes no $GUARD" >&2; fail=1 ;;
	esac
done

# Every other mention: the declaration, the two helpers' own bodies, and a copy
# taken through the accessor.
awk -F '\t' '
	# the line itself may hold tabs of its own
	{ line = $0; sub(/^[^\t]*\t[^\t]*\t[^\t]*\t/, "", line) }
	$3 == "g_settings_xmltv_xml_auto_clear" || $3 == "g_settings_xmltv_xml_auto_pushback" { next }
	$1 == "src/system/settings.h" && line ~ /std::list<std::string>[ \t]+xmltv_xml_auto;/ { next }
	line ~ /settingsCopy\(g_settings\.xmltv_xml_auto\)/ { next }
	{ print $1 ":" $2 ": xmltv_xml_auto used outside its helpers and settingsCopy" }
' "$tmp/mentions" > "$tmp/bad"
if [ -s "$tmp/bad" ]; then
	sed 's/^/check-xmltv-auto-lock.sh: /' "$tmp/bad" >&2
	fail=1
fi

# Below this the scan is not reading the tree any more. There were six such
# lines when this was written: the declaration, three in the two helpers and
# the two copies.
FLOOR=4
count=`wc -l < "$tmp/mentions"`
if [ "$count" -lt "$FLOOR" ]; then
	echo "check-xmltv-auto-lock.sh: only $count mentions of xmltv_xml_auto found, fewer than $FLOOR" >&2
	fail=1
fi

[ "$fail" -eq 0 ] || exit 1
echo "check-xmltv-auto-lock.sh: $count mentions of xmltv_xml_auto, all through the lock"
