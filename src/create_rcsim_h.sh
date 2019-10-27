#!/bin/sh
#
# create a rcsim.h file from the rcinput header
# (C) 2011 Stefan Seyfried
# License: GPL v2
#
# usage: sh ./create_rcsim_h.sh > rcsim.h

cat << EOF
// rcsim.h - automatically created from driver/rcinput.h

#ifndef KEY_NONEXISTING
#define KEY_NONEXISTING 0x0
#endif

#ifndef KEY_GAMES
#define KEY_GAMES	0x1a1   /* Media Select Games */
#endif

#ifndef KEY_TOPLEFT
#define KEY_TOPLEFT	0x1a2
#endif

#ifndef KEY_TOPRIGHT
#define KEY_TOPRIGHT	0x1a3
#endif

#ifndef KEY_BOTTOMLEFT
#define KEY_BOTTOMLEFT	0x1a4
#endif

#ifndef KEY_BOTTOMRIGHT
#define KEY_BOTTOMRIGHT	0x1a5
#endif

#define KEY_POWERON	KEY_FN_F1
#define KEY_POWEROFF	KEY_FN_F2
#define KEY_STANDBYON	KEY_FN_F3
#define KEY_STANDBYOFF	KEY_FN_F4
#define KEY_MUTEON	KEY_FN_F5
#define KEY_MUTEOFF	KEY_FN_F6
#define KEY_ANALOGON	KEY_FN_F7
#define KEY_ANALOGOFF	KEY_FN_F8

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

