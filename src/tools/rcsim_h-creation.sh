#!/bin/sh
#
# create a rcsim.h file from the rcinput header
# (C) 2011 Stefan Seyfried
# License: GPL v2
#
# usage: sh ./rcsim_h-creation.sh top_srcdir > rcsim.h

top_srcdir=$1

rcinput_fake_h="$top_srcdir/src/driver/rcinput_fake.h"
rcinput_h="$top_srcdir/src/driver/rcinput.h"

if [ -z "$top_srcdir" ] || [ ! -r "$rcinput_fake_h" ] || [ ! -r "$rcinput_h" ]; then
	echo "$0: cannot read rcinput headers below '$top_srcdir/src/driver'" >&2
	echo "usage: sh ./rcsim_h-creation.sh top_srcdir > rcsim.h" >&2
	exit 1
fi

cat << EOF
// rcsim.h - automatically created from rcinput.h and rcinput_fake.h

EOF

cat "$rcinput_fake_h"

cat << EOF

enum {
EOF
sed -n '/^[[:space:]]*RC_0/,/^[[:space:]]*RC_analog_off/s/^[[:space:]]*/	/p' "$rcinput_h"
cat << EOF
};

enum {	// not defined in input.h but used like that, at least in 2.4.22
	KEY_RELEASED = 0,
	KEY_PRESSED,
	KEY_AUTOREPEAT
};

struct key{
	const char *name;
	const unsigned long code;
};

static const struct key keyname[] = {
EOF
sed -n '/^[[:space:]]*RC_0/,/^[[:space:]]*RC_analog_off/ s/^.*=[[:space:]]*\(KEY_.*\),.*/	{ "\1",		\1 },/p' "$rcinput_h"
cat << EOF
	/* to stay backward compatible */
	{ "KEY_SETUP",		KEY_MENU },
	{ "KEY_HOME",		KEY_EXIT },
};
EOF
