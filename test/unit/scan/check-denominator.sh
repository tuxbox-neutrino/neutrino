#!/bin/sh
# What the coverage of the settings is measured against, recomputed here from the
# settings struct rather than read from a number written down. A field added to the
# struct otherwise moves the denominator and nothing says so, which leaves a criterion
# reporting complete because nothing was compared.
#
# Three sets, and the whole of the check is that the first is exactly the other two
# together:
#   every member of SNeutrinoSettings a row could carry,
#   every field the declaration tables name,
#   every field listed beside them as left out on purpose.
#
# Names and not counts, so a field that changed sides shows as the field it is rather
# than as an arithmetic that still adds up. The parse of the struct is held to the same
# three sets: one that stopped finding members would drop names the other two still
# carry, and that fails here.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-denominator.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
SETTINGS="$SRC/src/system/settings.h"
UNDECLARED="$SRC/src/coreapi/settings/settingsundeclared.cpp"
for f in "$SETTINGS" "$UNDECLARED" "$HERE/fields.awk" "$HERE/strip-comments.awk"; do
	[ -r "$f" ] || { echo "check-denominator.sh: cannot read $f" >&2; exit 1; }
done

# Below these the scan is not reading the struct or the tables any more. The
# tree held 474 members, 444 of them carriable, and 416 declared rows when this
# was written.
MEMBER_FLOOR=400
DECLARED_FLOOR=300

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$HERE/strip-comments.awk" "$SETTINGS" \
	| awk -f "$HERE/fields.awk" > "$tmp/members"

members=`awk 'END { print NR }' "$tmp/members"`
if [ "$members" -lt "$MEMBER_FLOOR" ]; then
	echo "check-denominator.sh: $members members read out of $SETTINGS," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

awk -F'\t' '$2 == "scalar" { print $1 }' "$tmp/members" | sort > "$tmp/carriable"
awk -F'\t' '$2 == "scalar" && $3 == "plain" { print $1 }' "$tmp/members" | sort > "$tmp/unconditional"

# Every macro a row writes its field with, named one by one rather than matched
# by a pattern wide enough to take whatever is added next: a row written with a
# macro nothing here names would leave the member it carries unaccounted for,
# and this is the one place that would say so. The member a row names is always
# the first argument, whatever else the macro takes after it.
FIELD_MACROS='COREAPI_(NUMBER_FIELD_ASKED|NUMBER_FIELD|TEXT_FIELD|MASK_BIT_FIELD|CHANNEL_ID_FIELD|SERVICE_FIELD)'

# The declared set is every field a row points at. The listed file is not among
# these, so the two sets are read apart.
grep -hoE "$FIELD_MACROS"'\([A-Za-z_][A-Za-z_0-9]*[,)]' "$SRC"/src/coreapi/settings/settingstable*.cpp \
	| sed -E 's/.*\(([A-Za-z_0-9]*)[,)]$/\1/' | sort -u > "$tmp/declared"

grep -hoE "$FIELD_MACROS"'\([A-Za-z_][A-Za-z_0-9]*[,)]' "$UNDECLARED" \
	| sed -E 's/.*\(([A-Za-z_0-9]*)[,)]$/\1/' | sort > "$tmp/listed.raw"
sort -u "$tmp/listed.raw" > "$tmp/listed"

carriable=`awk 'END { print NR }' "$tmp/carriable"`
declared=`awk 'END { print NR }' "$tmp/declared"`
listed=`awk 'END { print NR }' "$tmp/listed"`

if [ "$declared" -lt "$DECLARED_FLOOR" ]; then
	echo "check-denominator.sh: $declared declared fields found, the scan has stopped matching" >&2
	exit 1
fi
if [ "$listed" -lt 1 ]; then
	echo "check-denominator.sh: no listed field found in $UNDECLARED," >&2
	echo "  the scan has stopped matching" >&2
	exit 1
fi

fail=0

if [ "$listed" -ne "`awk 'END { print NR }' "$tmp/listed.raw"`" ]; then
	echo "a field is listed twice:" >&2
	sort "$tmp/listed.raw" | uniq -d | sed 's/^/  /' >&2
	fail=1
fi

both=`comm -12 "$tmp/declared" "$tmp/listed"`
if [ -n "$both" ]; then
	echo "declared by a row and listed as undeclared:" >&2
	printf '%s\n' "$both" | sed 's/^/  /' >&2
	fail=1
fi

# The listed file is compiled by this build alone, so a field it names that only
# some builds have would reach the others uncompiled.
conditional=`comm -23 "$tmp/listed" "$tmp/unconditional"`
if [ -n "$conditional" ]; then
	echo "listed as undeclared but not a member every build has:" >&2
	printf '%s\n' "$conditional" | sed 's/^/  /' >&2
	fail=1
fi

sort -u "$tmp/declared" "$tmp/listed" > "$tmp/accounted"

missing=`comm -23 "$tmp/carriable" "$tmp/accounted"`
if [ -n "$missing" ]; then
	echo "a member of SNeutrinoSettings a row could carry that no row declares and nothing lists:" >&2
	printf '%s\n' "$missing" | sed 's/^/  /' >&2
	fail=1
fi

stray=`comm -13 "$tmp/carriable" "$tmp/accounted"`
if [ -n "$stray" ]; then
	echo "named by a row or by the list and not a member of SNeutrinoSettings a row could carry:" >&2
	printf '%s\n' "$stray" | sed 's/^/  /' >&2
	fail=1
fi

if [ "$fail" -ne 0 ]; then
	echo "  the settings struct holds $carriable such members; $declared are declared and $listed are listed" >&2
	exit 1
fi

echo "settings the struct holds that a row could carry   $carriable"
echo "declared by a row                                  $declared"
echo "listed with a reason                               $listed"
exit 0
