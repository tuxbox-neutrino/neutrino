#!/bin/sh
#
# create a rcsim.h file from the rcinput header
# (C) 2011 Stefan Seyfried
# License: GPL v2
#
# usage: sh ./rcsim_h-creation.sh > rcsim.h

rcinput_fake_h="../driver/rcinput_fake.h"
rcinput_h="../driver/rcinput.h"

cat << EOF
// rcsim.h - automatically created from rcinput.h and rcinput_fake.h

EOF

cat $rcinput_fake_h

cat << EOF

enum {
EOF
sed -n '/^[[:space:]]*RC_0/,/^[[:space:]]*RC_analog_off/s/^[[:space:]]*/	/p' $rcinput_h
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
sed -n '/^[[:space:]]*RC_0/,/^[[:space:]]*RC_analog_off/ s/^.*=[[:space:]]*\(KEY_.*\),.*/	{ "\1",		\1 },/p' $rcinput_h
cat << EOF
	/* to stay backward compatible */
	{ "KEY_SETUP",		KEY_MENU },
	{ "KEY_HOME",		KEY_EXIT },
};
EOF
