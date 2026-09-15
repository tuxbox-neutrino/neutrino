#!/bin/sh
# A declaration table follows the settings struct into its build conditions, so a row
# can name a field that exists on one box and not on another, and a whole arm of rows
# can be written for a build nobody here makes. The build that runs this suite
# compiles one arm of every such condition and the rows in the other arm are compiled
# by nothing, checked by nothing, and reach a box for the first time when someone
# builds for it.
#
# So every table is compiled once per condition the tree names, and the hardware is
# only one of the three families that decide what a row may say: the optional features
# and the box models do it as well. This does not put the rows in the unbuilt arm under
# any of the cases beside it. What it does is refuse a row whose field does not exist
# on the build it is written for.
#
# One define at a time never reaches an arm that sits inside another arm, so the
# conditions a table names itself are compiled in every combination as well.
set -e
LC_ALL=C
export LC_ALL

CXX="$1"
CONFIG="$2"
DIR="$3"
shift 3

[ -n "$CXX" ] && [ -r "$CONFIG" ] && [ -d "$DIR" ] || {
	echo "usage: check-hardware.sh <compiler> <config.h> <table directory> [flags...]" >&2
	exit 2
}

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# Read from the configuration rather than listed here, so a condition added to
# the tree is compiled without this being edited to remember it. A digit is part
# of a name: ENABLE_LCD4LINUX and ENABLE_LCD are two macros, and a pattern that
# stops at the digit reads the first as the second and compiles neither arm.
grep -oE 'HAVE_[A-Z_0-9]+_HARDWARE|ENABLE_[A-Z_0-9]+|BOXMODEL_[A-Z_0-9]+' "$CONFIG" \
	| sort -u > "$tmp/macros"
MACROS=`cat "$tmp/macros"`

# A pattern that stopped matching would leave every arm compiled by nothing
# again and would say so nowhere, so each family has to still be found. The
# tree names four of the first and two dozen of each of the others.
hw=`grep -c '_HARDWARE$' "$tmp/macros"` || hw=0
en=`grep -c '^ENABLE_' "$tmp/macros"` || en=0
bx=`grep -c '^BOXMODEL_' "$tmp/macros"` || bx=0
if [ "$hw" -lt 2 ] || [ "$en" -lt 10 ] || [ "$bx" -lt 10 ]; then
	echo "check-hardware.sh: $hw hardware, $en feature and $bx box model conditions found" >&2
	echo "  in $CONFIG, the scan has stopped matching" >&2
	exit 1
fi

TABLES=`ls "$DIR"/settingstable*.cpp 2>/dev/null || true`
tcount=`printf '%s\n' "$TABLES" | grep -c .` || tcount=0
if [ "$tcount" -lt 1 ]; then
	echo "check-hardware.sh: no declaration table in $DIR" >&2
	exit 1
fi

# One compilation per condition, and one per combination of the conditions a
# table names, written out first and run below. No object file is wanted and
# two of these run at once, so the compiler is asked for the front end alone.
: > "$tmp/work"
for m in $MACROS; do
	for t in $TABLES; do
		printf '%s\t-D%s=1\n' "$t" "$m" >> "$tmp/work"
	done
done

: > "$tmp/subsets"
idx=0
for t in $TABLES; do
	idx=$((idx + 1))
	grep -hE '^[ 	]*#[ 	]*(if|ifdef|ifndef|elif)([ 	(!]|$)' "$t" \
		| grep -oE '[A-Za-z_][A-Za-z_0-9]*' | sort -u > "$tmp/named" || true
	comm -12 "$tmp/macros" "$tmp/named" > "$tmp/own.$idx"
	n=`grep -c . "$tmp/own.$idx"` || n=0
	if [ "$n" -gt 8 ]; then
		echo "check-hardware.sh: $t names $n build conditions, more than this enumerates" >&2
		exit 1
	fi
	awk -v tbl="$t" -v idx="$idx" '
		{ a[NR] = $0 }
		END {
			total = 1
			for (i = 0; i < NR; i++) total = total * 2
			for (m = 0; m < total; m++) {
				s = ""
				for (b = 1; b <= NR; b++) {
					h = m
					for (j = 1; j < b; j++) h = int(h / 2)
					if (h % 2) s = s " -D" a[b] "=1"
				}
				# the combination has no define in it at all, so the field that
				# can be empty is written last: a tab is IFS whitespace and two
				# adjacent ones read as one, which shifts every field after it
				print idx "\t" m "\t" tbl "\t" s
			}
		}' "$tmp/own.$idx" >> "$tmp/subsets"
done
cut -f3,4 "$tmp/subsets" >> "$tmp/work"

jobs=`getconf _NPROCESSORS_ONLN 2>/dev/null` || jobs=4
[ -n "$jobs" ] || jobs=4

k=0
running=0
while IFS='	' read -r t defs; do
	k=$((k + 1))
	(
		if $CXX "$@" $defs -fsyntax-only "$t" > "$tmp/log.$k" 2>&1; then
			rm -f "$tmp/log.$k"
		else
			printf 'a declaration table does not compile under%s: %s\n' " $defs" "$t" \
				> "$tmp/fail.$k"
		fi
	) &
	running=$((running + 1))
	if [ "$running" -ge "$jobs" ]; then
		wait
		running=0
	fi
done < "$tmp/work"
wait

bad=`ls "$tmp" | grep -c '^fail\.'` || bad=0
if [ "$bad" -gt 0 ]; then
	for f in "$tmp"/fail.*; do
		cat "$f" >&2
		log="$tmp/log.`basename "$f" | sed 's/^fail\.//'`"
		if [ -r "$log" ]; then
			sed -n '1,20p' "$log" >&2
		fi
		break
	done
	if [ "$bad" -gt 1 ]; then
		echo "  and $((bad - 1)) more combination it does not compile under" >&2
	fi
	exit 1
fi

# Compiling the same thing once per condition would pass and say nothing, which
# is what a check has to be unable to do. So every condition a table names has
# to make that table read differently under some combination of the others.
#
# The comparison is of the table's own lines and not of the whole translation
# unit: the settings struct is itself under these conditions, so every file that
# includes it comes out of the preprocessor differently and comparing that would
# be satisfied by a table with no condition in it at all.
own() {
	d="$1"; f="$2"; shift 2
	$CXX "$@" $d -E "$f" 2>/dev/null | awk -v want="$f" '
		/^# [0-9]+ "/ {
			g = $0
			sub(/^# [0-9]+ "/, "", g)
			sub(/".*$/, "", g)
			mine = (g == want)
			next
		}
		mine { print }
	'
}

running=0
while IFS='	' read -r idx m t defs; do
	( own "$defs" "$t" "$@" > "$tmp/pp.$idx.$m" ) &
	running=$((running + 1))
	if [ "$running" -ge "$jobs" ]; then
		wait
		running=0
	fi
done < "$tmp/subsets"
wait

# A condition the configuration already defines cannot be shown this way: adding
# it again changes nothing, and the arm behind it is the one this build compiles
# and every case beside it reads.
grep -oE '^#define[ 	]+(HAVE_[A-Z_0-9]+_HARDWARE|ENABLE_[A-Z_0-9]+|BOXMODEL_[A-Z_0-9]+)' "$CONFIG" \
	| grep -oE '(HAVE|ENABLE|BOXMODEL)_[A-Z_0-9]+' | sort -u > "$tmp/defined"

idx=0
for t in $TABLES; do
	idx=$((idx + 1))
	n=`grep -c . "$tmp/own.$idx"` || n=0
	if [ "$n" -eq 0 ]; then
		continue
	fi
	total=1
	j=0
	while [ "$j" -lt "$n" ]; do
		total=$((total * 2))
		j=$((j + 1))
	done
	b=0
	bit=1
	while read -r macro; do
		b=$((b + 1))
		if [ "$b" -gt 1 ]; then
			bit=$((bit * 2))
		fi
		if grep -qx "$macro" "$tmp/defined"; then
			continue
		fi
		live=0
		m=0
		while [ "$m" -lt "$total" ]; do
			if [ $((m / bit % 2)) -eq 1 ]; then
				o=$((m - bit))
				if ! cmp -s "$tmp/pp.$idx.$m" "$tmp/pp.$idx.$o"; then
					live=1
					break
				fi
			fi
			m=$((m + 1))
		done
		if [ "$live" -eq 0 ]; then
			echo "$t names $macro in a condition and reads the same with it and without it," >&2
			echo "  so compiling it proved nothing" >&2
			exit 1
		fi
	done < "$tmp/own.$idx"
done
exit 0
