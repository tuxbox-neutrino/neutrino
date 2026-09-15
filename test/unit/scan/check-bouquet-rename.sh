#!/bin/sh
# A CZapitBouquet carries two names. Name is its identity: it is what is written to
# file, and what existsBouquet compares a new name against. bName is what every list
# on screen prints, and it differs from Name only for the built in bouquets whose
# caption comes from the locale. A rename has to move both, or the box shows one name
# and answers to another, and a second bouquet can then be created under the caption
# the renamed one still carries.
#
# Two places rename a bouquet, and neither can be linked into the test binary: the
# bouquet editor and zapit's own CMD_BQ_RENAME_BOUQUET. What setName does with the two
# fields is a case in the suite; that both of these go through it is checked here as
# text.
#
# The adapter that binds the web layer to the channel stack reads both fields to look
# a bouquet up by name and sends its rename on to the command above. It is checked
# whole rather than one function at a time: what would undo the rule there is any line
# assigning one of the two names.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-bouquet-rename.sh <top source directory>" >&2
	exit 2
}

HERE=`dirname "$0"`
EDITOR="$SRC/src/gui/bedit/bouqueteditor_bouquets.cpp"
ZAPIT="$SRC/src/zapit/zapit.cpp"
ADAPTER="$SRC/src/coreapi/box/channelsource_real.cpp"
STRIP="$HERE/strip-comments.awk"
BLANK="$HERE/blank-if0.awk"
for f in "$EDITOR" "$ZAPIT" "$ADAPTER" "$STRIP" "$BLANK"; do
	[ -r "$f" ] || { echo "check-bouquet-rename.sh: cannot read $f" >&2; exit 1; }
done

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

prepare() {
	awk -v keepstrings=0 -f "$STRIP" "$1" | awk -f "$BLANK"
}

# The block the pattern opens, brace balanced from the line it matches. The
# same isolation check-hook.sh uses for a function body, which serves for a
# case label written with a block of its own just as well.
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

prepare "$EDITOR" > "$tmp/editor"
prepare "$ZAPIT" > "$tmp/zapit"

block_of "$tmp/editor" '^void CBEBouquetWidget::renameBouquet[(]' > "$tmp/gui"
[ -s "$tmp/gui" ] || {
	echo "check-bouquet-rename.sh: no CBEBouquetWidget::renameBouquet definition in $EDITOR" >&2
	exit 1
}

block_of "$tmp/zapit" 'case CZapitMessages::CMD_BQ_RENAME_BOUQUET:' > "$tmp/cmd"
[ -s "$tmp/cmd" ] || {
	echo "check-bouquet-rename.sh: no CMD_BQ_RENAME_BOUQUET case in $ZAPIT" >&2
	exit 1
}

prepare "$ADAPTER" > "$tmp/adapter"
[ -s "$tmp/adapter" ] || {
	echo "check-bouquet-rename.sh: nothing left of $ADAPTER after stripping" >&2
	exit 1
}

for site in gui cmd adapter; do
	case "$site" in
		gui) where="CBEBouquetWidget::renameBouquet" ;;
		cmd) where="the CMD_BQ_RENAME_BOUQUET case" ;;
		adapter) where="the channel stack adapter" ;;
	esac

	# The adapter renames by sending the command above rather than by touching a
	# bouquet, so setName is not its to call; what it must not do is assign
	# either name, and that is the check below.
	if [ "$site" != adapter ]; then
		grep -q 'setName[ \t]*(' "$tmp/$site" || {
			echo "check-bouquet-rename.sh: $where renames without calling setName" >&2
			exit 1
		}
	fi

	# An assignment and not a comparison: "!=" and "==" both end in "=" and
	# both read as one to a scan that only looks for the character.
	direct=`grep -nE '(->|\.)b?Name[ \t]*=[^=]' "$tmp/$site" || true`
	if [ -n "$direct" ]; then
		echo "check-bouquet-rename.sh: $where writes a bouquet name field directly" >&2
		echo "  rather than through setName, which is how the two came apart:" >&2
		printf '%s\n' "$direct" | sed 's/^/  /' >&2
		exit 1
	fi
done

exit 0
