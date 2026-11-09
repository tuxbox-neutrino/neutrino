#!/bin/sh
# The files the storage layer refuses to read, write or remove are the ones a web
# server on this box keeps its access store in, the name that store had before
# and the working files the two it writes are written through included. Which
# files those are is stated by
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
# has none. The web server, which never hands one of them out as a file nor writes a
# log into one, is told the same files by the control API, held to the same macros
# below.
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
grep -qE 'realpath\(internal::access_store_dir,' "$STORAGE" \
	&& grep -qE '^const char \*access_store_dir = CONFIGDIR;' "$STORAGE" || {
	echo "check-access-store.sh: $STORAGE does not work the names out under CONFIGDIR" >&2
	exit 1
}

# What the two programs say their own store is called.
access=`grep -E '^#define[[:blank:]]+HTTPD_ACCESS_FILE' "$GLOBAL" | grep -oE '"/[^"]+"' | tr -d '"/' || true`
[ -n "$access" ] || {
	echo "check-access-store.sh: no HTTPD_ACCESS_FILE definition in $GLOBAL" >&2
	exit 1
}
grep -qE '^#define[[:blank:]]+HTTPD_ACCESS_FILE[[:blank:]]+CONFIGDIR' "$GLOBAL" || {
	echo "check-access-store.sh: HTTPD_ACCESS_FILE is no longer under CONFIGDIR," >&2
	echo "  so the storage layer is working its name out under the wrong directory" >&2
	exit 1
}

old=`grep -E '^#define[[:blank:]]+HTTPD_SERVER_FILE' "$GLOBAL" | grep -oE '"/[^"]+"' | tr -d '"/' || true`
[ -n "$old" ] || {
	echo "check-access-store.sh: no HTTPD_SERVER_FILE definition in $GLOBAL" >&2
	exit 1
}
grep -qE '^#define[[:blank:]]+HTTPD_SERVER_FILE[[:blank:]]+CONFIGDIR' "$GLOBAL" || {
	echo "check-access-store.sh: HTTPD_SERVER_FILE is no longer under CONFIGDIR," >&2
	echo "  so the storage layer is working the server file's name out under the wrong directory" >&2
	exit 1
}

# And the name the access store had before, which a box may still carry where the
# first start could not remove it.
former=`grep -E '^#define[[:blank:]]+HTTPD_ACCESS_FORMER_FILE' "$GLOBAL" | grep -oE '"/[^"]+"' | tr -d '"/' || true`
[ -n "$former" ] || {
	echo "check-access-store.sh: no HTTPD_ACCESS_FORMER_FILE definition in $GLOBAL" >&2
	exit 1
}
grep -qE '^#define[[:blank:]]+HTTPD_ACCESS_FORMER_FILE[[:blank:]]+CONFIGDIR' "$GLOBAL" || {
	echo "check-access-store.sh: HTTPD_ACCESS_FORMER_FILE is no longer under CONFIGDIR," >&2
	echo "  so the storage layer is working the former name out under the wrong directory" >&2
	exit 1
}

# And the working files the two it writes are written through, which is the name
# with what httpd::workingFile appends.
WEBCONFIG="$SRC/src/httpd/webconfig.cpp"
[ -r "$WEBCONFIG" ] || { echo "check-access-store.sh: cannot read $WEBCONFIG" >&2; exit 1; }
suffix=`sed -n '/^std::string workingFile(/,/^}/p' "$WEBCONFIG" | grep -oE 'return path \+ "[^"]+"' | grep -oE '"[^"]+"' | tr -d '"' || true`
[ -n "$suffix" ] || {
	echo "check-access-store.sh: $WEBCONFIG no longer says what httpd::workingFile appends" >&2
	exit 1
}

want=`printf '%s\n%s\n%s\n%s\n%s\n' "$access" "$old" "$former" "$access$suffix" "$old$suffix" | LC_ALL=C sort`
if [ "$refused" != "$want" ]; then
	echo "check-access-store.sh: the storage layer refuses" >&2
	printf '%s\n' "$refused" | sed 's/^/  /' >&2
	echo "  and the two servers keep their access stores and their working files in" >&2
	printf '%s\n' "$want" | sed 's/^/  /' >&2
	exit 1
fi

# The restore over the network keeps those files as the box has them and puts the
# server's own file back with the box's account in it (restoreOf). It tells that
# file from the stores by one more name, which has to be the server file's; and
# the lines it keeps are the ones the first start takes the account out of.
server=`grep -E '^const char kServerFileName\[\] = "[^"]+";' "$STORAGE" | grep -oE '"[^"]+"' | tr -d '"' || true`
[ "$server" = "$old" ] || {
	echo "check-access-store.sh: kServerFileName in $STORAGE is '$server'," >&2
	echo "  not the server file $old, so a restore would keep the wrong file's account" >&2
	exit 1
}
BACKUP="$SRC/src/coreapi/backup.cpp"
[ -r "$BACKUP" ] || { echo "check-access-store.sh: cannot read $BACKUP" >&2; exit 1; }
keys=`grep -E '^const char \*const kAccountKeys\[\]' "$BACKUP" || true`
[ -n "$keys" ] || {
	echo "check-access-store.sh: no kAccountKeys array in $BACKUP" >&2
	exit 1
}
for name in kOldUser kOldPassword; do
	value=`grep -E "^const char $name\[\][[:blank:]]*=" "$WEBCONFIG" | grep -oE '"[^"]+"' || true`
	[ -n "$value" ] && printf '%s\n' "$keys" | grep -qF "$value" || {
		echo "check-access-store.sh: the restore does not keep the line $value ($name in $WEBCONFIG)," >&2
		echo "  so an archive could bring that part of the account along" >&2
		exit 1
	}
done
grep -qF 'storage::restoreOf(dir, leaf)' "$BACKUP" || {
	echo "check-access-store.sh: $BACKUP no longer asks restoreOf what to do with a file" >&2
	exit 1
}

# The web server is told the same three files, each with the working file it is
# written through, so that no static answer hands one out and no log is written
# into one (yprotect). Named by the macros and not by their text, which leaves the
# list nothing to drift from but the macros it names.
CONTROL="$SRC/src/nhttpd/tuxboxapi/controlapi.cpp"
[ -r "$CONTROL" ] || { echo "check-access-store.sh: cannot read $CONTROL" >&2; exit 1; }
told=`grep -E 'stores\[\][[:blank:]]*=' "$CONTROL" || true`
[ -n "$told" ] || {
	echo "check-access-store.sh: $CONTROL no longer lists the stores it names to the web server" >&2
	exit 1
}
for macro in HTTPD_ACCESS_FILE HTTPD_SERVER_FILE HTTPD_ACCESS_FORMER_FILE; do
	printf '%s\n' "$told" | grep -qE "(^|[^A-Z_])$macro([^A-Z_]|$)" || {
		echo "check-access-store.sh: the web server is not told about $macro," >&2
		echo "  so a static answer could hand that file out and a log could be written into it" >&2
		exit 1
	}
done
grep -qF 'names.push_back(httpd::workingFile(stores[i]));' "$CONTROL" \
	&& grep -qF 'yprotect::setFiles(accessFiles())' "$CONTROL" || {
	echo "check-access-store.sh: $CONTROL no longer hands the stores and their working files" >&2
	echo "  to yprotect::setFiles" >&2
	exit 1
}

# What a restore left in the store's directory when the program ended before it
# was done, the copy of nhttpd.conf with the account lines among it, is kept by
# nothing else across a start: the start keeps it and removes it before the first
# start's migration can remove a working file, and so before the web server
# answers anybody, and the web server is told what stays of it.
MAIN="$SRC/src/neutrino.cpp"
[ -r "$MAIN" ] || { echo "check-access-store.sh: cannot read $MAIN" >&2; exit 1; }
kept=`grep -n 'coreapi::backup::keepLeftovers(' "$MAIN" | head -1 | cut -d: -f1`
migrated=`grep -n 'httpd::ensureConfigFile(' "$MAIN" | head -1 | cut -d: -f1`
[ -n "$kept" ] && [ -n "$migrated" ] && [ "$kept" -lt "$migrated" ] || {
	echo "check-access-store.sh: $MAIN does not keep what a restore left before the migration" >&2
	exit 1
}
grep -qF 'coreapi::backup::leftovers()' "$CONTROL" || {
	echo "check-access-store.sh: $CONTROL no longer tells the web server what a restore left" >&2
	exit 1
}

exit 0
