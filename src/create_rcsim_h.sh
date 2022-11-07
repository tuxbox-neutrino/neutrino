#!/bin/sh
#
# create a rcsim.h file from the rcinput header
# (C) 2011 Stefan Seyfried
# License: GPL v2
#
# usage: sh ./create_rcsim_h.sh > rcsim.h

cat << EOF
// rcsim.h - automatically created from driver/rcinput.h and driver/rcinput_fake.h

EOF

cat driver/rcinput_fake.h

cat << EOF

enum {
EOF
sed -n '/^[[:space:]]*RC_0/,/^[[:space:]]*RC_analog_off/s/^[[:space:]]*/	/p' driver/rcinput.h
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
sed -n '/^[[:space:]]*RC_0/,/^[[:space:]]*RC_analog_off/ s/^.*=[[:space:]]*\(KEY_.*\),.*/	{ "\1",		\1 },/p' driver/rcinput.h
cat << EOF
	/* to stay backward compatible */
	{ "KEY_SETUP",		KEY_MENU },
	{ "KEY_HOME",		KEY_EXIT },
};
EOF
