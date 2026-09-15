#!/bin/sh
# levels.cpp keeps its own table of the 74 legacy endpoint names and their assigned
# level, because a host test cannot call into controlapi.cpp's own yCgiCallList:
# the symbol lives in controlapi.o, and asking the linker for any symbol in that
# object pulls it whole, with every real Neutrino singleton every one of the 74
# handlers reaches (see levels.h). Nothing at run time can notice the two tables
# drifting apart, in either direction: an endpoint added to yCgiCallList and never
# given a row here answers whatever levelFor() does for a name nobody wrote down,
# and a row added here for a name yCgiCallList does not carry is dead weight that
# looks like coverage.
#
# Comparing names alone only catches a row appearing or disappearing. It says
# nothing about a row that stays but is given the wrong level: relabelling
# "shutdown" KindRead leaves both name lists exactly as they were. This is what
# stood between a LAN caller and /control/shutdown, so below is a second table,
# name paired with the level it is meant to carry, compared against what kTable
# currently says. Deliberate duplication: a table that gates /control/shutdown is
# worth writing down twice.
#
# A third, narrower drift cannot be caught by comparing names or levels at all.
# Six endpoints are Read only because their handler is an unimplemented stub today
# (lcd, videooutput, vcroutput, avinputmode, audio, crypt). If one of them gains a
# real body later, both comparisons stay clean while the level quietly stops being
# true. So this also holds a checksum over each of those six bodies, taken when the
# level was assigned, and fails if the body has moved since.
set -e
LC_ALL=C
export LC_ALL

CONTROLAPI="$1"
LEVELS="$2"
[ -n "$CONTROLAPI" ] && [ -n "$LEVELS" ] && [ -r "$CONTROLAPI" ] && [ -r "$LEVELS" ] || {
	echo "usage: check-legacy-levels.sh <controlapi.cpp> <levels.cpp>" >&2
	exit 2
}

HERE=`dirname "$0"`
STRIP="$HERE/strip-comments.awk"
[ -r "$STRIP" ] || { echo "check-legacy-levels.sh: cannot read $STRIP" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT

# {"name", &CControlAPI::Handler, "mime"} - one row per legacy endpoint.
# Comments stripped first (keepstrings=1, the names themselves are what a
# scan needs to keep) so a row mentioned only in a comment cannot satisfy
# this.
awk -v keepstrings=1 -f "$STRIP" "$CONTROLAPI" \
	| grep -oE '\{"[A-Za-z0-9_.]+"[[:space:]]*,[[:space:]]*&CControlAPI::' \
	| sed -E 's/^\{"([^"]+)".*/\1/' \
	| sort -u > "$tmp/table_names"

# { "name", KindWhatever }, one row per assigned level.
awk -v keepstrings=1 -f "$STRIP" "$LEVELS" \
	| grep -oE '\{[[:space:]]*"[A-Za-z0-9_.]+"[[:space:]]*,[[:space:]]*Kind[A-Za-z]+' \
	| sed -E 's/.*"([^"]+)".*/\1/' \
	| sort -u > "$tmp/levels_names"

# The same rows, this time keeping the Kind: "name:KindWhatever", one per row.
awk -v keepstrings=1 -f "$STRIP" "$LEVELS" \
	| grep -oE '\{[[:space:]]*"[A-Za-z0-9_.]+"[[:space:]]*,[[:space:]]*Kind[A-Za-z]+' \
	| sed -E 's/.*"([^"]+)"[[:space:]]*,[[:space:]]*(Kind[A-Za-z]+)/\1:\2/' \
	| sort -u > "$tmp/levels_pairs"

[ -s "$tmp/table_names" ]  || { echo "no legacy endpoint names found in $CONTROLAPI" >&2; exit 1; }
[ -s "$tmp/levels_names" ] || { echo "no level rows found in $LEVELS" >&2; exit 1; }

STATUS=0

MISSING_LEVEL=`comm -23 "$tmp/table_names" "$tmp/levels_names"`
if [ -n "$MISSING_LEVEL" ]; then
	echo "endpoint(s) named in $CONTROLAPI with no level in $LEVELS:" >&2
	echo "$MISSING_LEVEL" | sed 's/^/  /' >&2
	STATUS=1
fi

MISSING_ENDPOINT=`comm -13 "$tmp/table_names" "$tmp/levels_names"`
if [ -n "$MISSING_ENDPOINT" ]; then
	echo "level(s) in $LEVELS naming no endpoint in $CONTROLAPI:" >&2
	echo "$MISSING_ENDPOINT" | sed 's/^/  /' >&2
	STATUS=1
fi

# The level every one of the 74 names is meant to carry, read off kTable by
# hand when this guard was written and kept here on purpose rather than
# derived: deriving it from kTable would compare kTable with itself and
# notice nothing.
cat > "$tmp/expected_pairs" <<'EOF'
addbouquet:KindWrite
aspectratio:KindRead
audio:KindRead
avinputmode:KindRead
boxinfo:KindRead
build_live_url:KindRead
build_playlist:KindRead
changebouquet:KindWrite
channellist:KindRead
config:KindMixed
crypt:KindRead
currenttpchannels:KindRead
deletebouquet:KindWrite
epg:KindRead
epgsearch:KindRead
epgsearchxml:KindRead
exec:KindSystem
file:KindMixed
get_logo:KindRead
getbouquet:KindRead
getbouquets:KindRead
getbouquetsxml:KindRead
getchannel:KindRead
getchannelid:KindRead
getchannelinfo:KindRead
getdate:KindRead
getdir:KindRead
getepgid:KindRead
getmode:KindRead
getmovies:KindRead
getonidsid:KindRead
getservicesxml:KindRead
gettime:KindRead
getubouquetsxml:KindRead
glcdscreenshot:KindWrite
info:KindRead
infoicons:KindMixed
lcd:KindRead
logolist:KindRead
message:KindWrite
movebouquet:KindWrite
rc:KindMixed
rcem:KindWrite
reboot:KindSystem
reloadchannels:KindWrite
reloadplugins:KindWrite
reloadsetup:KindWrite
renamebouquet:KindWrite
restart:KindSystem
savebouquet:KindWrite
screenshot:KindWrite
sendalltimers:KindWrite
setbouquet:KindWrite
setmode:KindMixed
shutdown:KindSystem
signal:KindRead
standby:KindMixed
startplugin:KindSystem
statfs:KindRead
system:KindSystem
timer:KindMixed
updatebouquet:KindWrite
updateinfo:KindRead
vcroutput:KindRead
version:KindRead
videoformat:KindMixed
videooutput:KindRead
volume:KindMixed
xmltv.data:KindRead
xmltv.m3u:KindRead
xmltv.xml:KindRead
xmltvlist:KindWrite
yweb:KindSystem
zapto:KindMixed
EOF
sort -u "$tmp/expected_pairs" -o "$tmp/expected_pairs"

[ `wc -l < "$tmp/expected_pairs"` -eq 74 ] || {
	echo "check-legacy-levels.sh's own recorded name:kind list does not have 74 rows" >&2
	STATUS=1
}

WRONG_HERE=`comm -23 "$tmp/expected_pairs" "$tmp/levels_pairs"`
if [ -n "$WRONG_HERE" ]; then
	echo "recorded here as the level $LEVELS should assign, not found there (name missing, renamed, or level changed):" >&2
	echo "$WRONG_HERE" | sed 's/^/  /' >&2
	STATUS=1
fi

WRONG_THERE=`comm -13 "$tmp/expected_pairs" "$tmp/levels_pairs"`
if [ -n "$WRONG_THERE" ]; then
	echo "$LEVELS assigns this, not recorded here (name added, renamed, or level changed):" >&2
	echo "$WRONG_THERE" | sed 's/^/  /' >&2
	STATUS=1
fi

# The six stubs, and the checksum of each body when its Read level was
# assigned. body() prints a named CControlAPI member function's body, braces
# balanced on the comment- and string-stripped source (keepstrings=0, so a
# brace inside a string literal was already replaced and cannot end the body
# early or late).
body() {
	awk -v keepstrings=0 -f "$STRIP" "$CONTROLAPI" | awk -v fn="$1" '
		found == 0 && $0 ~ ("^void CControlAPI::" fn "\\(") { found = 1 }
		found == 1 {
			print
			n = length($0)
			for (i = 1; i <= n; i++) {
				c = substr($0, i, 1)
				if (c == "{") depth++
				else if (c == "}") { depth--; if (depth == 0) exit }
			}
		}
	'
}

checksum() {
	body "$1" | md5sum | cut -d' ' -f1
}

check_stub() {
	name="$1"; fn="$2"; want="$3"
	got=`checksum "$fn"`
	[ -n "$got" ] || {
		echo "$fn: no such function found in $CONTROLAPI, cannot guard the '$name' level" >&2
		STATUS=1
		return
	}
	if [ "$got" != "$want" ]; then
		echo "$fn's body in $CONTROLAPI has changed since '$name' was recorded Read in $LEVELS:" >&2
		echo "  recorded checksum: $want" >&2
		echo "  current checksum:  $got" >&2
		echo "  re-check whether '$name' still qualifies as Read" >&2
		STATUS=1
	fi
}

check_stub lcd          LCDAction       e0ade386514f2b5ae6b3fa4bdab53a48
check_stub videooutput  VideoOutputCGI  7f4293f80215bc6e15a3aa6eb99a7bea
check_stub vcroutput    VCROutputCGI    aa8c7953946c48ad052ae9e7ccb7f033
check_stub avinputmode  AVInputModeCGI  0bff24b30dfd040b1b6da92351a90642
check_stub audio        AudioCGI        ae29e6b501a085a5087306c403443bed
check_stub crypt        CryptCGI        9bca869749a5265cc811908eee830b57

exit $STATUS
