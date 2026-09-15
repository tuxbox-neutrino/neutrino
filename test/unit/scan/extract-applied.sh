#!/bin/sh
# What the seam between a written setting and the box acts on, read out of the source
# rather than copied from it. The key lists in src/gui/settings_appliers.cpp were
# transcribed from the notifiers by hand, and a list transcribed by hand and read by
# nothing is the shape this whole guard exists to stop.
#
# The chain is followed and no link of it is named here. The registration table gives
# the section and the object; the object gives the applier class; the constructor gives
# the key list; the class's own applyOption gives the object it calls changeNotify on;
# that object's declaration gives the notifier class; and the notifier's changeNotify
# body gives the options it branches on. An applier that decides for itself has its own
# branches read the same way.
#
# Three kinds of record, tab between:
#   registered	section
#   listed	section	key	locale	label
#   acts	section	locale	label	where
#
# The build arms are read rather than followed, so a key behind a condition this build
# does not take is read all the same: one list answers for every box.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: extract-applied.sh <source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
BRANCHES="$HERE/branches.awk"
APPLIERS="$SRC/gui/settings_appliers.cpp"
for f in "$APPLIERS" "$STRIP" "$BRANCHES" "$HERE/extract-bounds.sh"; do
	[ -r "$f" ] || { echo "extract-applied.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# The one map every scan here reads: the enumerator a call site names beside the
# string a declared row carries.
sh "$HERE/extract-bounds.sh" -m "$SRC" > "$tmp/locale"

awk -v keepstrings=1 -f "$STRIP" "$APPLIERS" > "$tmp/appliers"

# Section, applier class and key list, each read off the link before it.
awk '
	/^[ \t]*\{[ \t]*"[a-z_]+",[ \t]*&[A-Za-z_0-9]+[ \t]*\}/ {
		line = $0
		sub(/^[^"]*"/, "", line); sec = line; sub(/".*$/, "", sec)
		sub(/^[^&]*&/, "", line); obj = line; sub(/[^A-Za-z_0-9].*$/, "", obj)
		object[sec] = obj
		order[++n] = sec
		next
	}
	/^static [A-Za-z_0-9]+ [a-z_0-9]+;/ {
		cls = $0; sub(/^static /, "", cls); sub(/ .*$/, "", cls)
		nm = $0; sub(/^static [A-Za-z_0-9]+ /, "", nm); sub(/;.*$/, "", nm)
		klass[nm] = cls
		next
	}
	/^[ \t]*: CSectionSettingsApplier\(/ {
		if (ctor == "") next
		if ($0 ~ /COREAPI_APPLIED\(/) {
			a = $0; sub(/^.*COREAPI_APPLIED\(/, "", a); sub(/\).*$/, "", a)
			rows[ctor] = a
		}
		else
			rows[ctor] = "-"
		ctor = ""
		next
	}
	/^[A-Za-z_0-9]+::[A-Za-z_0-9]+\(\)$/ { ctor = $0; sub(/::.*$/, "", ctor); next }
	END {
		if (n == 0) {
			print "extract-applied.sh: no registration read" > "/dev/stderr"
			exit 1
		}
		for (i = 1; i <= n; i++) {
			sec = order[i]
			cls = klass[object[sec]]
			if (cls == "") {
				print "extract-applied.sh: no class for the applier object " object[sec] > "/dev/stderr"
				exit 1
			}
			if (!(cls in rows)) {
				print "extract-applied.sh: no key list read out of the constructor of " cls > "/dev/stderr"
				exit 1
			}
			print sec "\t" cls "\t" rows[cls]
		}
	}
' "$tmp/appliers" > "$tmp/chain"

# Not a pipeline, so that a link that will not resolve stops the scan here
# rather than being sorted away with a status nobody sees.
while IFS='	' read -r sec cls arr; do
	printf 'registered\t%s\n' "$sec"

	if [ "$arr" != "-" ]; then
		awk -v arr="$arr" -v sec="$sec" '
			$0 ~ ("^const AppliedSetting " arr "\\[\\] =$") { inside = 1; next }
			inside && /^\};/ { inside = 0 }
			inside && /^[ \t]*\{[ \t]*"[A-Za-z_0-9]+",[ \t]*LOCALE_[A-Z0-9_]+[ \t]*\}/ {
				k = $0; sub(/^[^"]*"/, "", k); sub(/".*$/, "", k)
				l = $0; sub(/^.*LOCALE_/, "LOCALE_", l); sub(/[^A-Z0-9_].*$/, "", l)
				print "listed\t" sec "\t" k "\t" l
				++n
			}
			END { if (n == 0) exit 1 }
		' "$tmp/appliers" || {
			echo "extract-applied.sh: no entry read out of the key list $arr of $cls" >&2
			exit 1
		}
	fi

	# What the applier itself branches on, which is the whole of what one that
	# calls no notifier acts on and the deciding half of one whose notifiers
	# take the option and do not read it. Kept rather than only printed,
	# because whether it read anything is what says an applier without a
	# notifier still acts on something.
	awk -v cls="$cls" -v member="applyOption" -v where="$APPLIERS" -f "$BRANCHES" "$tmp/appliers" \
		| awk -v sec="$sec" -F'\t' '{ print "acts\t" sec "\t" $1 "\t" $2 }' > "$tmp/own"
	cat "$tmp/own"

	awk -v cls="$cls" '
		$0 ~ ("^bool " cls "::applyOption\\(") { inside = 1 }
		inside && /^\}/ { inside = 0; print; next }
		inside { print }
	' "$tmp/appliers" > "$tmp/body"
	[ -s "$tmp/body" ] || { echo "extract-applied.sh: no applyOption body for $cls" >&2; exit 1; }

	grep -oE '[A-Za-z_0-9]+(->|\.)changeNotify' "$tmp/body" \
		| sed -e 's/->changeNotify$//' -e 's/\.changeNotify$//' | sort -u > "$tmp/objects"
	# An applier that calls no notifier is one that does the work itself, and
	# what it branches on above is then the whole of what it acts on. One that
	# calls no notifier and branches on nothing acts on nothing this can read,
	# which is the state this scan exists to refuse.
	if [ ! -s "$tmp/objects" ] && [ ! -s "$tmp/own" ]; then
		echo "extract-applied.sh: $cls calls no notifier and branches on no option" >&2
		exit 1
	fi

	while read -r obj; do
		# a local of the body first, then a declaration in the same file, then
		# one anywhere in the tree
		type=`sed -n -E "s/^[ 	]*(static )?([A-Za-z_0-9]+) $obj;.*/\2/p" "$tmp/body" | head -1`
		if [ -z "$type" ]; then
			type=`sed -n -E "s/^extern ([A-Za-z_0-9]+) \\*?$obj;.*/\1/p" "$tmp/appliers" | head -1`
		fi
		if [ -z "$type" ]; then
			# The pointer the program keeps the object in, wherever it declares
			# it. What stands before the type is extern on some and a macro of
			# the program's own on others, so the type is read off the star.
			type=`grep -rhoE "[A-Za-z_0-9]+ \\*$obj;" "$SRC" 2>/dev/null \
				| sed -n -E "s/^([A-Za-z_0-9]+) \\*$obj;/\1/p" | sort -u | head -1`
		fi
		[ -n "$type" ] || { echo "extract-applied.sh: no declaration of $obj for section $sec" >&2; exit 1; }

		file=`grep -rlE "^bool $type::changeNotify\(" "$SRC" 2>/dev/null | head -1`
		[ -n "$file" ] || { echo "extract-applied.sh: no changeNotify of $type" >&2; exit 1; }

		awk -v keepstrings=0 -f "$STRIP" "$file" \
			| awk -v cls="$type" -v member="changeNotify" -v where="$file" -f "$BRANCHES" \
			| awk -v sec="$sec" -F'\t' '{ print "acts\t" sec "\t" $1 "\t" $2 }'
	done < "$tmp/objects"
done < "$tmp/chain" > "$tmp/raw"

# The enumerator a list or a branch names is turned into the string a row
# declares. One the program does not have stops the scan, because a label
# nothing can resolve compares against nothing.
awk -F'\t' -v locfile="$tmp/locale" '
	FILENAME == locfile { loc[$1] = $2; next }
	$1 == "listed" {
		if (!($4 in loc)) { print "extract-applied.sh: " $4 " is no locale the program has" > "/dev/stderr"; bad = 1; next }
		print $1 "\t" $2 "\t" $3 "\t" $4 "\t" loc[$4]
		next
	}
	$1 == "acts" {
		if (!($3 in loc)) { print "extract-applied.sh: " $3 " is no locale the program has" > "/dev/stderr"; bad = 1; next }
		print $1 "\t" $2 "\t" $3 "\t" loc[$3] "\t" $4
		next
	}
	{ print }
	END { if (bad) exit 1 }
' "$tmp/locale" "$tmp/raw" | sort -u > "$tmp/out"

for kind in registered listed acts; do
	awk -F'\t' -v k="$kind" '$1 == k { n++ } END { exit (n > 0) ? 0 : 1 }' "$tmp/out" || {
		echo "extract-applied.sh: not one $kind record was read" >&2
		exit 1
	}
done

cat "$tmp/out"
