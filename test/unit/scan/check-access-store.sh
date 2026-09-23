#!/bin/sh
# The two files the storage layer refuses to read, write or remove are the two a web
# server on this box keeps its access store in. Which files those are is stated by
# the programs that own them, and the storage layer works the names out a second time
# from the same directory macro.
#
# Two statements of one fact can drift, and this one drifts silently: rename the file
# in the program that writes it and the storage layer goes on protecting a name
# nothing uses, while the file that now holds the password is handed to whoever asks.
# Nothing at run time can see that.
#
# So the names are held to their owner here. The storage layer cannot simply include
# the header that states them: it drags the whole program's globals into a file that
# has none.
set -e
LC_ALL=C
export LC_ALL

SRC="$1"
[ -n "$SRC" ] && [ -d "$SRC" ] || {
	echo "usage: check-access-store.sh <top source directory>" >&2
	exit 2
}

STORAGE="$SRC/src/coreapi/storage.cpp"
GLOBAL="$SRC/src/include/global.h"
for f in "$STORAGE" "$GLOBAL"; do
	[ -r "$f" ] || { echo "check-access-store.sh: cannot read $f" >&2; exit 1; }
done

# What the layer refuses, read off the one array that states it.
line=`grep -E '^const char \*const kAccessStoreNames\[\]' "$STORAGE" || true`
[ -n "$line" ] || {
	echo "check-access-store.sh: no kAccessStoreNames array in $STORAGE," >&2
	echo "  so nothing here can say which files the layer refuses" >&2
	exit 1
}
refused=`printf '%s\n' "$line" | grep -oE '"[^"]+"' | tr -d '"' | LC_ALL=C sort`

# And the directory it works them out under, which has to be the one the
# programs below name theirs under.
printf '%s\n' "$line" | grep -q 'CONFIGDIR' && {
	echo "check-access-store.sh: kAccessStoreNames holds a directory as well as names;" >&2
	echo "  the directory belongs in the lookup, so that the two cannot disagree" >&2
	exit 1
}
grep -qE 'realpath\(CONFIGDIR,' "$STORAGE" || {
	echo "check-access-store.sh: $STORAGE does not work the names out under CONFIGDIR" >&2
	exit 1
}

# What the two programs say their own store is called.
niweb=`grep -E '^#define[[:blank:]]+NI_WEB_SETTINGS_FILE' "$GLOBAL" | grep -oE '"/[^"]+"' | tr -d '"/' || true`
[ -n "$niweb" ] || {
	echo "check-access-store.sh: no NI_WEB_SETTINGS_FILE definition in $GLOBAL" >&2
	exit 1
}
grep -qE '^#define[[:blank:]]+NI_WEB_SETTINGS_FILE[[:blank:]]+CONFIGDIR' "$GLOBAL" || {
	echo "check-access-store.sh: NI_WEB_SETTINGS_FILE is no longer under CONFIGDIR," >&2
	echo "  so the storage layer is working its name out under the wrong directory" >&2
	exit 1
}

old=`grep -E '^#define[[:blank:]]+NI_WEB_OLD_SETTINGS_FILE' "$GLOBAL" | grep -oE '"/[^"]+"' | tr -d '"/' || true`
[ -n "$old" ] || {
	echo "check-access-store.sh: no NI_WEB_OLD_SETTINGS_FILE definition in $GLOBAL" >&2
	exit 1
}
grep -qE '^#define[[:blank:]]+NI_WEB_OLD_SETTINGS_FILE[[:blank:]]+CONFIGDIR' "$GLOBAL" || {
	echo "check-access-store.sh: NI_WEB_OLD_SETTINGS_FILE is no longer under CONFIGDIR," >&2
	echo "  so the storage layer is working the removed server's name out under the wrong directory" >&2
	exit 1
}

want=`printf '%s\n%s\n' "$niweb" "$old" | LC_ALL=C sort`
if [ "$refused" != "$want" ]; then
	echo "check-access-store.sh: the storage layer refuses" >&2
	printf '%s\n' "$refused" | sed 's/^/  /' >&2
	echo "  and the two servers keep their access stores in" >&2
	printf '%s\n' "$want" | sed 's/^/  /' >&2
	exit 1
fi

exit 0
