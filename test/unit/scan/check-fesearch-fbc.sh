#!/bin/sh
# A stream gets its frontend from fesearch::pickIdle, and a recording that
# found none learns from fesearch::streamsToEnd which streams to end. Both
# walk the frontends as CFEManager::getFrontend does, on views that
# CFEManager::streamViews fills in. Where the box has FBC tuners, getFrontend
# also asks the tuner manager whether an automatic link has a root that
# carries the channel's input (isFBCAuto, findFBCRoot), and whether a root
# may tune without moving the input from under a locked link tied to it
# (fbcLinksCanTune). The views carry those answers as data: root_carries,
# and fbc_links, which fesearch reads the way fbcLinksCanTune reads the
# links. pickIdleForStream ties an automatic link to its root before it
# locks it, as allocateFE does before zapit tunes.
#
# The unit cases drive fesearch on such views; this holds the frontend
# manager to filling them and to tying the link. The frontend manager cannot
# be linked into the test binary, so it is checked as text. A fingerprint of
# getFrontend and fbcLinksCanTune fails this on any change to either, until
# fesearch has been checked against the change and the new value recorded.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-fesearch-fbc.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
f=src/zapit/femanager.cpp
[ -r "$SRC/$f" ] || { echo "check-fesearch-fbc.sh: cannot read $SRC/$f" >&2; exit 1; }
[ -r "$HERE/strip-comments.awk" ] || { echo "check-fesearch-fbc.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }
command -v md5sum > /dev/null 2>&1 || { echo "check-fesearch-fbc.sh: needs md5sum" >&2; exit 1; }

# getFrontend and fbcLinksCanTune as recorded: md5 of both bodies, comments
# and literals left out, each run of blanks one space, blank lines dropped.
FINGERPRINT=d4a28f69eeb4e5dcb9f01f02ac9dcf32

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0

awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SRC/$f" > "$tmp/src"

# 1. Each function from its head line to the line that closes it.
body() {
	awk -v head="$1" '
		index($0, head) == 1 { inside = 1 }
		inside { print }
		inside && /^}/ { exit }
	' "$tmp/src" > "$tmp/$2"
	if [ ! -s "$tmp/$2" ]; then
		echo "$f: no function starts with \"$1\"" >&2
		fail=1
	fi
}
body 'CFrontend * CFEManager::getFrontend(CZapitChannel * channel)' getfrontend
body 'bool CFEManager::fbcLinksCanTune(CFrontend * root, CZapitChannel * channel)' linkscantune
body 'void CFEManager::streamViews(' streamviews
body 'CFrontend * CFEManager::pickIdleForStream(' pickidle
body 'fesearch::StreamsToEnd CFEManager::streamsToEndFor(' streamstoend
[ $fail -eq 0 ] || exit 1

# 2. Every FBC call getFrontend makes is mirrored in the views.
grep -o -E '(^|[^A-Za-z0-9_])(isFBC[A-Za-z]*|fbc[A-Z][A-Za-z]*|find[A-Za-z]*FBC[A-Za-z]*|IsLinkedTo|IsRoot|IsFBC[A-Za-z]*|IsSameSet)\(' \
	"$tmp/getfrontend" | sed -e 's/^[^A-Za-z]//' -e 's/($//' | sort -u > "$tmp/calls" || true
n=`wc -l < "$tmp/calls"`
if [ $(($n)) -lt 3 ]; then
	echo "$f: getFrontend makes $(($n)) of the FBC calls the stream walk mirrors, not the three of isFBCAuto, findFBCRoot and fbcLinksCanTune" >&2
	fail=1
fi
while read call; do
	case "$call" in
	isFBCAuto) want='isFBCAuto(' ;;
	findFBCRoot) want='findFBCRoot(' ;;
	fbcLinksCanTune) want='IsLinkedTo(' ;;
	*)
		echo "$f: getFrontend uses $call, which the stream walk does not mirror" >&2
		fail=1
		continue
		;;
	esac
	grep -q -F "$want" "$tmp/streamviews" || {
		echo "$f: getFrontend uses $call, and streamViews does not call $want to mirror it" >&2
		fail=1
	}
done < "$tmp/calls"
grep -q -E '\.root_carries[ \t]*=' "$tmp/streamviews" || {
	echo "$f: streamViews does not set root_carries" >&2
	fail=1
}
grep -q -F '.fbc_links.push_back(' "$tmp/streamviews" || {
	echo "$f: streamViews does not fill fbc_links" >&2
	fail=1
}

# 3. fbcLinksCanTune decides by nothing but what fbc_links and loop_can_tune
#    carry: every name in it is one of these, and its two conditions are the
#    ones fesearch reads the links by.
allowed=' bool CFEManager fbcLinksCanTune CFrontend root CZapitChannel channel CFBCTunerManager fbc getInstance if IsRoot fenumber return true false for fe_map_iterator_t it femap begin end fe second continue IsLinkedTo Locked loopCanTune FEDEBUG '
grep -o -E '[A-Za-z_][A-Za-z0-9_]*' "$tmp/linkscantune" | sort -u > "$tmp/names"
: > "$tmp/foreign"
while read name; do
	case "$allowed" in
	*" $name "*) ;;
	*) echo "$name" >> "$tmp/foreign" ;;
	esac
done < "$tmp/names"
if [ -s "$tmp/foreign" ]; then
	echo "$f: fbcLinksCanTune reads what fesearch does not see:" `cat "$tmp/foreign"` >&2
	fail=1
fi
tr -d ' \t\n' < "$tmp/linkscantune" > "$tmp/linkscantune.flat"
for cond in 'if(fe->Locked()&&!loopCanTune(fe,channel))' 'if(fe==root||!fbc->IsLinkedTo(fe->fenumber,root->fenumber))'; do
	n=`grep -o -F "$cond" "$tmp/linkscantune.flat" | wc -l`
	if [ $(($n)) -ne 1 ]; then
		echo "$f: fbcLinksCanTune holds $(($n)) times, not once, the condition fesearch mirrors: $cond" >&2
		fail=1
	fi
done

# 4. Both adapters fill the views the same way.
for b in pickidle:pickIdleForStream streamstoend:streamsToEndFor; do
	grep -q -F 'streamViews(' "$tmp/${b%%:*}" || {
		echo "$f: ${b#*:} does not fill its views through streamViews" >&2
		fail=1
	}
done

# 5. pickIdleForStream ties the link only on the way to locking it, and
#    before: a locked frontend is never tied, and binding is undone only by
#    the unlock.
set -- `awk '
	{ line = $0; gsub(/[ \t]+/, " ", line); sub(/^ /, "", line) }
	!nb && index(line, "if (!bind)") { nb = NR }
	!nl && index(line, "if (lock) {") { nl = NR }
	index(line, "fbcTie(") { nt = NR; tc++ }
	nl && !nf && NR > nl && index(line, "lockFrontend(") { nf = NR }
	nl && !nc && NR > nl && index(line, "}") { nc = NR }
	END { printf "%d %d %d %d %d %d\n", nb, nl, nt, tc, nf, nc }
' "$tmp/pickidle"`
if [ "$4" -ne 1 ] || [ "$1" -eq 0 ] || [ "$1" -ge "$2" ] || [ "$2" -ge "$3" ] || [ "$3" -ge "$5" ] || [ "$5" -ge "$6" ]; then
	echo "$f: pickIdleForStream has to call fbcTie once, after if (!bind), inside if (lock) { and before lockFrontend (bind $1, lock $2, tie $3 x$4, lockFrontend $5, close $6)" >&2
	fail=1
fi

# 6. The fingerprint.
fp=`cat "$tmp/getfrontend" "$tmp/linkscantune" | tr '\t' ' ' | tr -s ' ' |
	sed -e 's/^ //' -e 's/ $//' -e '/^$/d' | md5sum | cut -d' ' -f1`
if [ "$fp" != "$FINGERPRINT" ]; then
	echo "$f: getFrontend or fbcLinksCanTune changed (fingerprint $fp, recorded $FINGERPRINT)." >&2
	echo "  fesearch (src/zapit/fesearch.cpp) walks the frontends as getFrontend does, and streamViews" >&2
	echo "  feeds it; check both against the change, as the comment above getFrontend and the head" >&2
	echo "  of src/zapit/fesearch.h ask, then record the new value as FINGERPRINT in $0." >&2
	fail=1
fi

exit $fail
