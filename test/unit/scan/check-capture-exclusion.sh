#!/bin/sh
# No picture of the screen is taken while it shows a password. The message
# with the box's first password holds a captureguard::Exclusion for as long as
# it stands, and every capture of the screen asks a captureguard::Capture
# before it reads and again before it writes: refused while the message
# stands, and nothing kept of a reading the message came into.
#
# The unit cases drive captureguard; this holds the program to the places
# they assume:
#  1. the password reaches the screen only through showFirstPasswordText,
#     which shows it inside the scope of a named Exclusion;
#  2. the capture that reads the framebuffer (CScreenShot::GetData) asks
#     before its first read and after its last, and nothing is written but
#     after GetData said yes;
#  3. the capture that runs a program (CScreenShot::Start on the boxes that
#     have one) asks before the program runs and before its picture gets a
#     name, says why it refuses, and has the program write into a file no
#     name reaches (captureguard::Staging), which is published only after
#     captureguard agreed and the program left a picture;
#  4. nothing else reads the screen past the guard: no other GetScreenImage,
#     the grab program elsewhere only for the video picture, and under
#     src/driver/glcd only the OSD mirror reads the framebuffer, through
#     captureguard::drawShared under the lock the bitmap is read under;
#  5. the GraphLCD bitmap, which can hold that mirror, is pictured only
#     through the two captures that ask the guard, dump it under that lock
#     into a Staging file and publish that only after captureguard agreed,
#     and they dump it through cGLCD::dumpDisplay, under the lock the
#     display is made and deleted under, holding no pointer to it;
#  6. GET /api/v1/osd/screenshot and both display routes answer 409
#     password-on-screen, and /control/screenshot and /control/glcdscreenshot
#     their error with a reason, and take no name with a control character.
# Neither the screen nor the program can be linked into the test binary, so
# it is checked as text.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-capture-exclusion.sh <top source directory>" >&2
	exit 2
}
HERE=`dirname "$0"`
SETUP=src/gui/nhttpd_setup.cpp
SHOT=src/driver/screenshot.cpp
REAL=src/coreapi/box/screenshotsource_real.cpp
OSD=src/coreapi/osd.cpp
EP=src/httpd/ep/ep_osd.cpp
CONTROL=src/nhttpd/tuxboxapi/controlapi.cpp
for f in $SETUP $SHOT $REAL $OSD $EP $CONTROL; do
	[ -r "$SRC/$f" ] || { echo "check-capture-exclusion.sh: cannot read $SRC/$f" >&2; exit 1; }
done
[ -r "$HERE/strip-comments.awk" ] || { echo "check-capture-exclusion.sh: cannot read $HERE/strip-comments.awk" >&2; exit 1; }

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' EXIT
fail=0
strip() {
	awk -v keepstrings=1 -f "$HERE/strip-comments.awk" "$1"
}
body() {
	awk -v start="$1" 'index($0, start) == 1 { f = 1 } f { print } f && /^}/ { exit }'
}
first() {
	grep -n -E -- "$2" "$1" | head -1 | cut -d: -f1
}
last() {
	grep -n -E -- "$2" "$1" | tail -1 | cut -d: -f1
}
# The statement that begins at line $2 of file $1, through the brace that
# closes it, or the line after it where it has none.
block() {
	sed -n "$2,\$p" "$1" | awk '
		{ print; depth += gsub(/{/, "{") - gsub(/}/, "}") }
		NR > 1 && depth <= 0 { exit }'
}
# Whether the scope open after line $2 of file $1 is still open at the start
# of line $3: the braces never close below the depth line $2 left.
inscope() {
	awk -v from="$2" -v to="$3" '
		NR > from && NR <= to && depth < floor { bad = 1 }
		{ depth += gsub(/{/, "{") - gsub(/}/, "}") }
		NR == from { floor = depth }
		END { exit bad }' "$1"
}
EXCLUSION='captureguard::Exclusion[[:blank:]]+[A-Za-z_][A-Za-z0-9_]*[[:blank:]]*;'
CAPTURE='captureguard::Capture[[:blank:]]+capture[[:blank:]]*;'
SHOWN='(ShowMsg|ShowMsg2UTF|ShowHint|ShowHintUTF|DisplayInfoMessage|DisplayErrorMessage)[[:blank:]]*\('

# 1. The message with the password.
#
# The text that carries it, and whatever hands it out in clear, are named
# nowhere else outside the API layer.
find "$SRC/src" \( -name '*.cpp' -o -name '*.h' \) ! -path "$SRC/src/system/locals*" ! -path "$SRC/src/httpd/*" \
	-exec grep -l -E 'LOCALE_NETWORKMENU_HTTPD_FIRST_PASSWORD_SHOWN|firstPasswordInEffect[[:blank:]]*\(|resetToFirstPassword[[:blank:]]*\(' {} + \
	| sed "s|^$SRC/||" | sort > "$tmp/users"
if [ "`cat "$tmp/users"`" != "$SETUP" ]; then
	echo "the first password reaches the screen elsewhere than $SETUP, where no exclusion holds the capture off:" >&2
	grep -v -x -F "$SETUP" "$tmp/users" | sed 's/^/  /' >&2 || true
	fail=1
fi
strip "$SRC/$SETUP" > "$tmp/setup"
n=`grep -c -F 'LOCALE_NETWORKMENU_HTTPD_FIRST_PASSWORD_SHOWN' "$tmp/setup" || true`
body 'static void showFirstPasswordText(' < "$tmp/setup" > "$tmp/text"
[ -s "$tmp/text" ] || { echo "check-capture-exclusion.sh: no showFirstPasswordText in $SETUP" >&2; exit 1; }
if [ "$n" != 1 ] || ! grep -q -F 'LOCALE_NETWORKMENU_HTTPD_FIRST_PASSWORD_SHOWN' "$tmp/text"; then
	echo "$SETUP: the text with the password is used outside showFirstPasswordText ($n uses)" >&2
	fail=1
fi
held=`first "$tmp/text" "$EXCLUSION"`
if [ -z "$held" ]; then
	echo "$SETUP: showFirstPasswordText holds no named captureguard::Exclusion while it shows the password" >&2
	fail=1
else
	grep -n -E "$SHOWN" "$tmp/text" | cut -d: -f1 > "$tmp/shows"
	[ -s "$tmp/shows" ] || { echo "$SETUP: showFirstPasswordText shows nothing" >&2; fail=1; }
	while read -r at; do
		if [ "$at" -lt "$held" ] || ! inscope "$tmp/text" "$held" "$at"; then
			echo "$SETUP: showFirstPasswordText shows the message at line $at of the function, outside the exclusion begun at line $held" >&2
			fail=1
		fi
	done < "$tmp/shows"
fi

# 2. and 3. The two captures.
# Cut apart by the markers on their preprocessor lines, which the comment strip
# would take away, and stripped afterwards.
awk '/^#if SCREENSHOT_EXTERNAL/ { f = 1 } /^#else \/\/ SCREENSHOT_INTERNAL/ { f = 0 } f' "$SRC/$SHOT" > "$tmp/external.raw"
awk '/^#else \/\/ SCREENSHOT_INTERNAL/ { f = 1 } /^#endif \/\/ SCREENSHOT_INTERNAL/ { f = 0 } f' "$SRC/$SHOT" > "$tmp/internal.raw"
strip "$tmp/external.raw" > "$tmp/external"
strip "$tmp/internal.raw" > "$tmp/internal"

body 'bool CScreenShot::GetData(' < "$tmp/internal" > "$tmp/getdata"
[ -s "$tmp/getdata" ] || { echo "check-capture-exclusion.sh: no CScreenShot::GetData in $SHOT" >&2; exit 1; }
taken=`first "$tmp/getdata" "$CAPTURE"`
asked=`first "$tmp/getdata" 'if[[:blank:]]*\([[:blank:]]*!capture\.admitted\(\)[[:blank:]]*\)'`
read1=`first "$tmp/getdata" '(GetScreenImage|SaveScreen)[[:blank:]]*\('`
readn=`last "$tmp/getdata" '(GetScreenImage|SaveScreen|mergeOsdScreen)[[:blank:]]*\('`
clean=`first "$tmp/getdata" 'if[[:blank:]]*\([[:blank:]]*!capture\.clean\(\)[[:blank:]]*\)'`
done_=`last "$tmp/getdata" 'return[[:blank:]]+true[[:blank:]]*;'`
if [ -z "$taken" ] || [ -z "$asked" ] || [ -z "$read1" ] || [ "$taken" -gt "$asked" ] || [ "$asked" -gt "$read1" ]; then
	echo "$SHOT: CScreenShot::GetData has to ask captureguard before it reads the screen (capture ${taken:-none}, asked ${asked:-none}, first read ${read1:-none})" >&2
	fail=1
fi
if [ -z "$clean" ] || [ -z "$readn" ] || [ -z "$done_" ] || [ "$clean" -lt "$readn" ] || [ "$clean" -gt "$done_" ]; then
	echo "$SHOT: CScreenShot::GetData has to ask captureguard again after its last read and before it answers yes (asked ${clean:-none}, last read ${readn:-none}, yes ${done_:-none})" >&2
	fail=1
fi
for at in $asked $clean; do
	block "$tmp/getdata" "$at" > "$tmp/branch"
	grep -q -E 'excluded[[:blank:]]*=[[:blank:]]*true' "$tmp/branch" && grep -q -E 'return[[:blank:]]+false' "$tmp/branch" || {
		echo "$SHOT: CScreenShot::GetData does not say no, marked excluded, where captureguard refuses (line $at of the function)" >&2
		fail=1
	}
done
# What GetData read is written only once it said yes.
grep -n -E 'SaveFile[[:blank:]]*\(\)|startThread[[:blank:]]*\(\)' "$tmp/internal" | grep -v -E 'bool CScreenShot::' > "$tmp/writes" || true
while IFS=: read -r at text; do
	prev=`sed -n "$((at - 1))p" "$tmp/internal"`
	case "$text" in
	*'bool ret = SaveFile();'*) ;;  # runThread, started only by startThread below
	*)
		printf '%s\n' "$prev" | grep -q -E 'if[[:blank:]]*\([[:blank:]]*GetData\(\)[[:blank:]]*\)' || {
			echo "$SHOT: something is written without GetData having said yes first: $text" >&2
			fail=1
		}
		;;
	esac
done < "$tmp/writes"
[ "`grep -c -E 'startThread[[:blank:]]*\(\)' "$tmp/internal"`" = 2 ] || {
	echo "$SHOT: startThread is reached from elsewhere than Start" >&2
	fail=1
}

body 'bool CScreenShot::Start(' < "$tmp/external" > "$tmp/start"
[ -s "$tmp/start" ] || { echo "check-capture-exclusion.sh: no CScreenShot::Start for the boxes that run a program in $SHOT" >&2; exit 1; }
taken=`first "$tmp/start" "$CAPTURE"`
asked=`first "$tmp/start" 'if[[:blank:]]*\([[:blank:]]*!capture\.admitted\(\)[[:blank:]]*\)'`
runs=`first "$tmp/start" 'system[[:blank:]]*\('`
clean=`first "$tmp/start" 'if[[:blank:]]*\([[:blank:]]*!capture\.clean\(\)[[:blank:]]*\)'`
left=`first "$tmp/start" 'if[[:blank:]]*\([[:blank:]]*!staging\.written\(\)[[:blank:]]*\)'`
named=`first "$tmp/start" 'staging\.publish[[:blank:]]*\([[:blank:]]*filename'`
if [ -z "$taken" ] || [ -z "$asked" ] || [ -z "$runs" ] || [ "$taken" -gt "$asked" ] || [ "$asked" -gt "$runs" ]; then
	echo "$SHOT: CScreenShot::Start has to ask captureguard before it runs the program (capture ${taken:-none}, asked ${asked:-none}, run ${runs:-none})" >&2
	fail=1
fi
if [ -z "$clean" ] || [ -z "$named" ] || [ "$clean" -lt "${runs:-0}" ] || [ "$clean" -gt "$named" ]; then
	echo "$SHOT: CScreenShot::Start has to ask captureguard again after the program and before its picture is published (asked ${clean:-none}, published ${named:-none})" >&2
	fail=1
fi
if [ -z "$left" ] || [ "$left" -lt "${runs:-0}" ] || [ "$left" -gt "${named:-0}" ]; then
	echo "$SHOT: CScreenShot::Start publishes what the program left without asking whether it left a picture" >&2
	fail=1
fi
if ! grep -q -E 'cmd[[:blank:]]*\+=[[:blank:]]*staging\.path\(\)[[:blank:]]*;' "$tmp/start" || grep -q -E 'cmd[[:blank:]]*\+=[[:blank:]]*filename' "$tmp/start"; then
	echo "$SHOT: CScreenShot::Start has the program write anywhere but into its Staging file" >&2
	fail=1
fi
if [ -n "$clean" ]; then
	block "$tmp/start" "$clean" > "$tmp/branch"
	grep -q -E 'excluded[[:blank:]]*=[[:blank:]]*true' "$tmp/branch" && grep -q -E 'return[[:blank:]]+false' "$tmp/branch" || {
		echo "$SHOT: CScreenShot::Start does not say no, marked excluded, where captureguard refuses after the program" >&2
		fail=1
	}
fi
if [ -n "$asked" ]; then
	block "$tmp/start" "$asked" > "$tmp/branch"
	grep -q -E 'excluded[[:blank:]]*=[[:blank:]]*true' "$tmp/branch" && grep -q -E 'return[[:blank:]]+false' "$tmp/branch" || {
		echo "$SHOT: CScreenShot::Start does not say no, marked excluded, where captureguard refuses before the program runs" >&2
		fail=1
	}
fi

# 4. Nothing else reads the screen.
find "$SRC/src" \( -name '*.cpp' -o -name '*.h' -o -name '*.c' \) ! -path "$SRC/$SHOT" \
	-exec grep -l -E 'GetScreenImage[[:blank:]]*\(' {} + | sed "s|^$SRC/||" > "$tmp/readers" || true
if [ -s "$tmp/readers" ]; then
	echo "the screen is read past CScreenShot, where captureguard is not asked:" >&2
	sed 's/^/  /' "$tmp/readers" >&2
	fail=1
fi
find "$SRC/src" \( -name '*.cpp' -o -name '*.h' -o -name '*.c' \) ! -path "$SRC/$SHOT" \
	-exec grep -l -F 'find_executable("grab")' {} + > "$tmp/grabbers" || true
while read -r f; do
	strip "$f" > "$tmp/grabber"
	grep -n -E 'grab\.c_str\(\)' "$tmp/grabber" > "$tmp/runs" || true
	[ -s "$tmp/runs" ] || {
		echo "${f#$SRC/}: finds the grab program and runs it in a way this scan cannot read" >&2
		fail=1
	}
	while IFS= read -r line; do
		printf '%s\n' "$line" | grep -q -E '"-v[a-z]*"' || {
			echo "${f#$SRC/}: runs grab for more than the video picture, past captureguard: ${line#*:}" >&2
			fail=1
		}
	done < "$tmp/runs"
done < "$tmp/grabbers"

# Under src/driver/glcd the OSD mirror is the one reader of the framebuffer:
# it reads through captureguard::drawShared, under the lock the bitmap is
# written out under, so a drawing a password came into is cleared before any
# picture of the bitmap can be taken.
GLCD=src/driver/glcd/glcd.cpp
[ -r "$SRC/$GLCD" ] || { echo "check-capture-exclusion.sh: cannot read $SRC/$GLCD" >&2; exit 1; }
find "$SRC/src/driver/glcd" \( -name '*.cpp' -o -name '*.h' -o -name '*.c' \) \
	-exec grep -l -E 'getFrameBufferPointer[[:blank:]]*\(' {} + | sed "s|^$SRC/||" > "$tmp/fbreaders" || true
if [ "`cat "$tmp/fbreaders"`" != "$GLCD" ]; then
	echo "the framebuffer is read under src/driver/glcd elsewhere than the OSD mirror in $GLCD:" >&2
	grep -v -x -F "$GLCD" "$tmp/fbreaders" | sed 's/^/  /' >&2 || true
	fail=1
fi
strip "$SRC/$GLCD" > "$tmp/glcd"
n=`grep -c -E 'getFrameBufferPointer[[:blank:]]*\(' "$tmp/glcd" || true`
reads=`grep -c -E 'showImage[[:blank:]]*\([[:blank:]]*fbp' "$tmp/glcd" || true`
at=`first "$tmp/glcd" 'captureguard::drawShared[[:blank:]]*\('`
if [ "$n" != 1 ] || [ "$reads" = 0 ] || [ -z "$at" ]; then
	echo "$GLCD: the OSD mirror is not the one place that reads the framebuffer ($n reads), or it does not read through captureguard::drawShared" >&2
	fail=1
else
	# the call, from its opening line to the parenthesis that closes it
	sed -n "$at,\$p" "$tmp/glcd" | awk '
		{ print; d += gsub(/\(/, "(") - gsub(/\)/, ")"); if (d <= 0) exit }' > "$tmp/draw"
	inside=`grep -c -E 'showImage[[:blank:]]*\([[:blank:]]*fbp' "$tmp/draw" || true`
	if [ "$inside" != "$reads" ]; then
		echo "$GLCD: the OSD mirror reads the framebuffer outside captureguard::drawShared ($inside of $reads reads)" >&2
		fail=1
	fi
	grep -q -E 'bitmap->Clear[[:blank:]]*\(' "$tmp/draw" || {
		echo "$GLCD: the OSD mirror hands captureguard::drawShared nothing that clears a spoilt drawing" >&2
		fail=1
	}
	lock=`sed -n "${at}p" "$tmp/glcd" | sed -n 's/.*drawShared[[:blank:]]*([[:blank:]]*\([A-Za-z_][A-Za-z0-9_]*\).*/\1/p'`
	# the lock it is handed is the bitmap's own, cGLCD::Lock and Unlock
	sed -n "1,${at}p" "$tmp/glcd" | tail -20 > "$tmp/before"
	if [ -z "$lock" ] || ! grep -q -E "}[[:blank:]]*$lock[[:blank:]]*;" "$tmp/before" \
		|| ! grep -q -E 'void[[:blank:]]+lock\(\)[[:blank:]]*\{[[:blank:]]*cGLCD::Lock\(\)' "$tmp/before" \
		|| ! grep -q -E 'void[[:blank:]]+unlock\(\)[[:blank:]]*\{[[:blank:]]*cGLCD::Unlock\(\)' "$tmp/before"; then
		echo "$GLCD: the OSD mirror draws under another lock than the one the bitmap is written out under (cGLCD::Lock)" >&2
		fail=1
	fi
fi
# and the bitmap is written out under that lock
body 'bool cGLCD::dumpBuffer(' < "$tmp/glcd" > "$tmp/dump"
locked=`first "$tmp/dump" 'cglcd->Lock\(\)'`
wrote=`first "$tmp/dump" '(fwrite|png_write_|jpeg_write_)[A-Za-z_]*[[:blank:]]*\('`
if [ -z "$locked" ] || [ -z "$wrote" ] || [ "$locked" -gt "$wrote" ]; then
	echo "$GLCD: dumpBuffer reads the bitmap without the lock the mirror draws under" >&2
	fail=1
fi

# 5. The GraphLCD bitmap is written out by these two and nothing else, each
# through cGLCD::dumpDisplay, which keeps the display from being deleted while
# it writes; the web server's threads never hold the display itself. (The
# display's setup menu does, on the thread that also deletes it.)
find "$SRC/src" \( -name '*.cpp' -o -name '*.h' -o -name '*.c' \) ! -path "$SRC/src/driver/glcd/*" \
	-exec grep -l -E 'dump(Buffer|Display)[[:blank:]]*\(' {} + | sed "s|^$SRC/||" | sort > "$tmp/dumpers" || true
if [ "`cat "$tmp/dumpers"`" != "`printf '%s\n%s\n' "$REAL" "$CONTROL" | sort`" ]; then
	echo "the GraphLCD bitmap is written out elsewhere than $REAL and $CONTROL:" >&2
	sed 's/^/  /' "$tmp/dumpers" >&2
	fail=1
fi
find "$SRC/src/coreapi" "$SRC/src/httpd" "$SRC/src/nhttpd" \( -name '*.cpp' -o -name '*.h' -o -name '*.c' \) \
	-exec grep -n -H -E 'cGLCD[[:blank:]]*\*|peekInstance[[:blank:]]*\(|dumpBuffer[[:blank:]]*\(|cGLCD::getInstance[[:blank:]]*\(' {} + \
	| sed "s|^$SRC/||" > "$tmp/holders" || true
if [ -s "$tmp/holders" ]; then
	echo "the web server holds the GraphLCD display, which Exit() may delete under it:" >&2
	sed 's/^/  /' "$tmp/holders" >&2
	fail=1
fi
# In the display, the lock that guards its life is taken where it is made,
# where it is deleted and where a caller off its threads looks at it.
lifelock='pthread_mutex_lock[[:blank:]]*\([[:blank:]]*&display_life[[:blank:]]*\)'
for fn in 'cGLCD *cGLCD::getInstance()' 'bool cGLCD::displayReady()' 'bool cGLCD::dumpDisplay(' 'void cGLCD::Exit()'; do
	body "$fn" < "$tmp/glcd" > "$tmp/lifefn"
	if [ ! -s "$tmp/lifefn" ]; then
		echo "$GLCD: no $fn" >&2
		fail=1
		continue
	fi
	taken=`first "$tmp/lifefn" "$lifelock"`
	case "$fn" in
	*getInstance*) at=`first "$tmp/lifefn" 'new[[:blank:]]+cGLCD'` ;;
	*Exit*) at=`first "$tmp/lifefn" 'delete[[:blank:]]+cglcd'` ;;
	*) at=`first "$tmp/lifefn" 'cglcd->Lock\(\)'` ;;
	esac
	if [ -z "$taken" ] || [ -z "$at" ] || [ "$taken" -gt "$at" ]; then
		echo "$GLCD: $fn does not take display_life before it makes, deletes or reads the display" >&2
		fail=1
	fi
done
# Asked before the dump and after it, dumped into a Staging file, and that
# published only when captureguard still agrees. $1 the function, $2 what a
# refusal answers, $3 what publishes it.
glcdcapture() {
	what="$1"
	taken=`first "$tmp/glcdcap" "$CAPTURE"`
	asked=`first "$tmp/glcdcap" 'if[[:blank:]]*\([[:blank:]]*!capture\.admitted\(\)[[:blank:]]*\)'`
	dump=`first "$tmp/glcdcap" 'cGLCD::dumpDisplay[[:blank:]]*\('`
	clean=`first "$tmp/glcdcap" '!capture\.clean\(\)'`
	named=`first "$tmp/glcdcap" "$3"`
	if [ -z "$taken" ] || [ -z "$asked" ] || [ -z "$dump" ] || [ -z "$clean" ] || [ -z "$named" ] \
		|| [ "$taken" -gt "$asked" ] || [ "$asked" -gt "$dump" ] || [ "$dump" -gt "$clean" ] || [ "$clean" -gt "$named" ]; then
		echo "$what: the GraphLCD picture has to ask captureguard before the dump and again before it is published (capture ${taken:-none}, asked ${asked:-none}, dump ${dump:-none}, asked again ${clean:-none}, published ${named:-none})" >&2
		fail=1
		return
	fi
	sed -n "$dump,$((dump + 1))p" "$tmp/glcdcap" | grep -q -E 'staging\.path\(\)' || {
		echo "$what: the GraphLCD picture is written anywhere but into its Staging file" >&2
		fail=1
	}
	block "$tmp/glcdcap" "$asked" | grep -q -E "$2" || {
		echo "$what: the GraphLCD picture refused before the dump does not say why" >&2
		fail=1
	}
	block "$tmp/glcdcap" "$clean" | grep -q -E "$2" || {
		echo "$what: the GraphLCD picture a password came into is published, or not said why" >&2
		fail=1
	}
}
strip "$SRC/$REAL" > "$tmp/real"
awk '/Status captureDisplay\(/ { f = 1 } f' "$tmp/real" > "$tmp/capdisplay"
at=`first "$tmp/capdisplay" 'if[[:blank:]]*\([[:blank:]]*name[[:blank:]]*==[[:blank:]]*"graphlcd"'`
if [ -z "$at" ]; then
	echo "$REAL: no GraphLCD branch in captureDisplay" >&2
	fail=1
else
	block "$tmp/capdisplay" "$at" > "$tmp/glcdcap"
	glcdcapture "$REAL: captureDisplay" 'return[[:blank:]]+Status::Conflict[[:blank:]]*;' 'staging\.publish[[:blank:]]*\([[:blank:]]*path'
fi
strip "$SRC/$CONTROL" > "$tmp/control"
body 'void CControlAPI::GlcdScreenshotCGI(' < "$tmp/control" > "$tmp/glcdcap"
if [ ! -s "$tmp/glcdcap" ]; then
	echo "$CONTROL: no GlcdScreenshotCGI" >&2
	fail=1
else
	glcdcapture "$CONTROL: GlcdScreenshotCGI" 'SendError\("[^"]+"\)' 'staging\.publish[[:blank:]]*\([[:blank:]]*filename'
fi

# 6. What the routes answer.
grep -A1 -E 'if[[:blank:]]*\([[:blank:]]*shot\.Excluded\(\)[[:blank:]]*\)' "$tmp/real" | grep -q -E 'return[[:blank:]]+Status::Conflict[[:blank:]]*;' || {
	echo "$REAL: the capture refused for a password is not answered as a conflict" >&2
	fail=1
}
strip "$SRC/$OSD" > "$tmp/osd"
body 'Result<std::string> screenshot(' < "$tmp/osd" > "$tmp/screenshot"
conflict=`first "$tmp/screenshot" 'if[[:blank:]]*\([[:blank:]]*s[[:blank:]]*==[[:blank:]]*Status::Conflict[[:blank:]]*\)'`
renamed=`first "$tmp/screenshot" 'std::rename[[:blank:]]*\('`
if [ -z "$conflict" ] || [ -z "$renamed" ] || [ "$conflict" -gt "$renamed" ]; then
	echo "$OSD: screenshot does not refuse a capture the source refused for a password before the picture gets the name" >&2
	fail=1
else
	block "$tmp/screenshot" "$conflict" > "$tmp/branch"
	grep -q -F 'std::remove(taking.c_str())' "$tmp/branch" \
		&& grep -q -E 'fail\([[:blank:]]*Status::Conflict[[:blank:]]*,[[:blank:]]*ErrorCode::PasswordOnScreen' "$tmp/branch" || {
		echo "$OSD: screenshot does not answer a refusal for a password with password-on-screen, its file removed" >&2
		fail=1
	}
fi
body 'Result<std::string> displayScreenshot(const std::string &name)' < "$tmp/osd" > "$tmp/display"
conflict=`first "$tmp/display" 'if[[:blank:]]*\([[:blank:]]*s[[:blank:]]*==[[:blank:]]*Status::Conflict[[:blank:]]*\)'`
if [ -z "$conflict" ] || ! block "$tmp/display" "$conflict" | grep -q -E 'fail\([[:blank:]]*Status::Conflict[[:blank:]]*,[[:blank:]]*ErrorCode::PasswordOnScreen'; then
	echo "$OSD: displayScreenshot does not answer a display refused for a password with password-on-screen" >&2
	fail=1
fi
strip "$SRC/$EP" > "$tmp/ep"
for table in kGetScreenshotRefusals kGetDisplayScreenshotRefusals kGetFirstDisplayScreenshotRefusals; do
	awk -v t="const RouteRefusal $table[]" 'index($0, t) == 1 { f = 1 } f { print } f && /^};/ { exit }' "$tmp/ep" > "$tmp/refusals"
	grep -q -E 'HTTPD_REFUSES\([[:blank:]]*Conflict[[:blank:]]*,[[:blank:]]*PasswordOnScreen[[:blank:]]*,' "$tmp/refusals" || {
		echo "$EP: $table does not declare 409 password-on-screen" >&2
		fail=1
	}
done
body 'void CControlAPI::ScreenshotCGI(' < "$tmp/control" > "$tmp/cgi"
grep -A2 -E 'else[[:blank:]]+if[[:blank:]]*\([[:blank:]]*screenshot->Excluded\(\)[[:blank:]]*\)' "$tmp/cgi" | grep -q -E 'SendError\("[^"]+"\)' || {
	echo "$CONTROL: /control/screenshot does not say why it took no picture while a password is shown" >&2
	fail=1
}
# A picture's name stands in the capture's own log lines, so it carries no
# control character.
body 'static bool screenshotNameOk(' < "$tmp/control" > "$tmp/nameok"
grep -q -E '<[[:blank:]]*0x20' "$tmp/nameok" && grep -q -E '0x7f' "$tmp/nameok" || {
	echo "$CONTROL: screenshotNameOk lets a control character into a picture's name" >&2
	fail=1
}

exit $fail
