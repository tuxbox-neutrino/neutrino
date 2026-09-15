#!/bin/sh
# Both bouquet files are replaced rather than written over. A write that stops partway
# used to leave a truncated file under the name the box reads at its next start, and a
# truncated bouquet list reads as a complete short one: the channels that did not make
# it into the file are simply gone, with nothing anywhere saying so.
#
# What does the replacing is CAtomicFileWriter, and the cases in test_atomicfile.cpp
# drive that object itself. The bouquet manager that calls it cannot be linked into the
# test binary; that both of its save functions go through the object, and answer what
# the object answered, is checked here as text.
#
# The rule is three lines: neither save function opens a file of its own, the one
# function that writes opens it through the writer and refuses when the writer has
# nothing to write into, and what it returns is the writer's answer about the
# replacement rather than a constant.
#
# The command that carries that answer to a caller is checked here too, and for the
# same reason. It used to answer before it started writing, when all it could say was
# that the command had arrived. So what is checked is the order: the answer goes out
# after both files have been written.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-bouquet-save-atomic.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
BOUQUETS="$SRC/src/zapit/bouquets.cpp"
ZAPIT="$SRC/src/zapit/zapit.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$BOUQUETS" "$ZAPIT" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-bouquet-save-atomic.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

awk -v keepstrings=0 -f "$STRIP" "$BOUQUETS" | awk -f "$BLANK" > "$tmp/bouquets"
[ -s "$tmp/bouquets" ] || {
	echo "check-bouquet-save-atomic.sh: nothing left of $BOUQUETS after stripping" >&2
	exit 1
}

# The block the pattern opens, brace balanced from the line it matches, the
# same isolation the other checks here use for a function body.
block_of() {
	awk -v pat="$2" '
		found == 0 && $0 ~ pat { found = 1 }
		found == 1 {
			print
			n = length($0)
			for (i = 1; i <= n; i++) {
				c = substr($0, i, 1)
				if (c == "{") depth++
				else if (c == "}") { depth--; if (depth == 0) exit }
			}
		}
	' "$1"
}

# A bouquet file opened anywhere in this file by name is one written in place,
# whatever else is around it.
direct=`grep -nE 'fopen[ \t]*\([ \t]*U?BOUQUETS_XML' "$tmp/bouquets" || true`
if [ -n "$direct" ]; then
	echo "check-bouquet-save-atomic.sh: a bouquet file is opened for writing under its own name" >&2
	echo "  rather than beside it, which is what leaves a truncated one behind:" >&2
	printf '%s\n' "$direct" | sed 's/^/  /' >&2
	exit 1
fi

writer=`block_of "$tmp/bouquets" '^bool CBouquetManager::writeBouquetFile[(]'`
[ -n "$writer" ] || {
	echo "check-bouquet-save-atomic.sh: no CBouquetManager::writeBouquetFile definition in $BOUQUETS" >&2
	exit 1
}

printf '%s\n' "$writer" > "$tmp/writer"

grep -qE 'CAtomicFileWriter[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\(' "$tmp/writer" || {
	echo "check-bouquet-save-atomic.sh: writeBouquetFile does not write through CAtomicFileWriter" >&2
	exit 1
}

# Nothing is written into a stream that was never opened, and the refusal says
# so rather than carrying on and reporting success at the end.
grep -qE 'return[ \t]+false[ \t]*;' "$tmp/writer" || {
	echo "check-bouquet-save-atomic.sh: writeBouquetFile has no refusal for a file it could not open" >&2
	exit 1
}

grep -qE 'return[ \t]+[A-Za-z_][A-Za-z0-9_]*\.commit[ \t]*\([ \t]*\)[ \t]*;' "$tmp/writer" || {
	echo "check-bouquet-save-atomic.sh: writeBouquetFile does not answer what the replacement answered" >&2
	exit 1
}

# Both of the two the rest of the program calls, so that one of them left
# answering yes is not one file that goes on claiming to have been written.
for fn in saveBouquets saveUBouquets; do
	body=`block_of "$tmp/bouquets" "^bool CBouquetManager::$fn[(]void[)]"`
	[ -n "$body" ] || {
		echo "check-bouquet-save-atomic.sh: no bool CBouquetManager::$fn(void) definition in $BOUQUETS" >&2
		exit 1
	}
	printf '%s\n' "$body" | grep -qE 'return[ \t]+writeBouquetFile[ \t]*\(' || {
		echo "check-bouquet-save-atomic.sh: $fn does not answer what writing the file answered" >&2
		exit 1
	}
done

awk -v keepstrings=0 -f "$STRIP" "$ZAPIT" | awk -f "$BLANK" > "$tmp/zapit"
block_of "$tmp/zapit" 'case CZapitMessages::CMD_BQ_SAVE_BOUQUETS:' > "$tmp/cmd"
[ -s "$tmp/cmd" ] || {
	echo "check-bouquet-save-atomic.sh: no CMD_BQ_SAVE_BOUQUETS case in $ZAPIT" >&2
	exit 1
}

# An acknowledgement carries no answer, so a branch that sends one is a branch
# that tells the caller nothing whatever it does afterwards.
if grep -qE 'SendCmdReady[ \t]*\(' "$tmp/cmd"; then
	echo "check-bouquet-save-atomic.sh: the save command answers with a bare acknowledgement" >&2
	echo "  which says the command arrived and nothing about the files" >&2
	exit 1
fi

last_save=`grep -nE 'g_bouquetManager->saveU?Bouquets[ \t]*\([ \t]*\)' "$tmp/cmd" | tail -1 | cut -d: -f1`
saves=`grep -cE 'g_bouquetManager->saveU?Bouquets[ \t]*\([ \t]*\)' "$tmp/cmd" || true`
answer=`grep -nE 'send_data[ \t]*\(' "$tmp/cmd" | head -1 | cut -d: -f1`

[ "$saves" = 2 ] || {
	echo "check-bouquet-save-atomic.sh: the save command writes $saves of the two bouquet files" >&2
	exit 1
}
[ -n "$answer" ] || {
	echo "check-bouquet-save-atomic.sh: the save command sends no answer at all" >&2
	exit 1
}
[ "$answer" -gt "$last_save" ] || {
	echo "check-bouquet-save-atomic.sh: the save command answers before it has written the files," >&2
	echo "  so what it answers cannot be whether they were written" >&2
	exit 1
}

exit 0
