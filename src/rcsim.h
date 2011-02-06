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
	RC_0		= KEY_0,	    /* /include/linux/input.h: #define KEY_0			 11   */
	RC_1		= KEY_1,	    /* /include/linux/input.h: #define KEY_1			  2   */
	RC_2		= KEY_2,	    /* /include/linux/input.h: #define KEY_2			  3   */
	RC_3		= KEY_3,	    /* /include/linux/input.h: #define KEY_3			  4   */
	RC_4		= KEY_4,	    /* /include/linux/input.h: #define KEY_4			  5   */
	RC_5		= KEY_5,	    /* /include/linux/input.h: #define KEY_5			  6   */
	RC_6		= KEY_6,	    /* /include/linux/input.h: #define KEY_6			  7   */
	RC_7		= KEY_7,	    /* /include/linux/input.h: #define KEY_7			  8   */
	RC_8		= KEY_8,	    /* /include/linux/input.h: #define KEY_8			  9   */
	RC_9		= KEY_9,	    /* /include/linux/input.h: #define KEY_9			 10   */
	RC_backspace	= KEY_BACKSPACE,    /* /include/linux/input.h: #define KEY_BACKSPACE		 14   */
	RC_up		= KEY_UP,	    /* /include/linux/input.h: #define KEY_UP			103   */
	RC_left		= KEY_LEFT,	    /* /include/linux/input.h: #define KEY_LEFT			105   */
	RC_right	= KEY_RIGHT,	    /* /include/linux/input.h: #define KEY_RIGHT		106   */
	RC_down		= KEY_DOWN,	    /* /include/linux/input.h: #define KEY_DOWN			108   */
	RC_spkr		= KEY_MUTE,	    /* /include/linux/input.h: #define KEY_MUTE			113   */
	RC_minus	= KEY_VOLUMEDOWN,   /* /include/linux/input.h: #define KEY_VOLUMEDOWN		114   */
	RC_plus		= KEY_VOLUMEUP,     /* /include/linux/input.h: #define KEY_VOLUMEUP		115   */
	RC_standby	= KEY_POWER,	    /* /include/linux/input.h: #define KEY_POWER		116   */
	RC_help		= KEY_HELP,	    /* /include/linux/input.h: #define KEY_HELP			138   */
	RC_home		= KEY_EXIT,	    /* /include/linux/input.h: #define KEY_HOME			102   */
	RC_setup	= KEY_MENU,	    /* /include/linux/input.h: #define KEY_SETUP		141   */
	RC_topleft	= KEY_TOPLEFT,	
	RC_topright	= KEY_TOPRIGHT,	
	RC_page_up	= KEY_PAGEUP,	    /* /include/linux/input.h: #define KEY_PAGEUP		104   */
	RC_page_down	= KEY_PAGEDOWN,	    /* /include/linux/input.h: #define KEY_PAGEDOWN		109   */
	RC_ok		= KEY_OK,	    /* /include/linux/input.h: #define KEY_OK			0x160 */ /* in patched input.h */
	RC_red		= KEY_RED,	    /* /include/linux/input.h: #define KEY_RED			0x18e */ /* in patched input.h */
	RC_green	= KEY_GREEN,	    /* /include/linux/input.h: #define KEY_GREEN		0x18f */ /* in patched input.h */
	RC_yellow	= KEY_YELLOW,	    /* /include/linux/input.h: #define KEY_YELLOW		0x190 */ /* in patched input.h */
	RC_blue		= KEY_BLUE,	    /* /include/linux/input.h: #define KEY_BLUE			0x191 */ /* in patched input.h */
	RC_top_left	= KEY_TOPLEFT,	    /* /include/linux/input.h: #define KEY_TOPLEFT		0x1a2 */ /* in patched input.h */
	RC_top_right	= KEY_TOPRIGHT,	    /* /include/linux/input.h: #define KEY_TOPRIGHT		0x1a3 */ /* in patched input.h */
	RC_bottom_left	= KEY_BOTTOMLEFT,   /* /include/linux/input.h: #define KEY_BOTTOMLEFT		0x1a4 */ /* in patched input.h */
	RC_bottom_right	= KEY_BOTTOMRIGHT,  /* /include/linux/input.h: #define KEY_BOTTOMRIGHT		0x1a5 */ /* in patched input.h */
	
	RC_audio	= KEY_AUDIO,
	RC_video	= KEY_VIDEO,
	RC_tv		= KEY_TV,
	RC_radio	= KEY_RADIO,
	RC_text		= KEY_TEXT,
	RC_info		= KEY_INFO,
	RC_epg		= KEY_EPG,
	RC_recall	= KEY_LAST,
	RC_favorites	= KEY_FAVORITES,
	RC_sat		= KEY_SAT,
	RC_sat2		= KEY_SAT2,
	RC_record	= KEY_RECORD,
	RC_play		= KEY_PLAY,
	RC_pause	= KEY_PAUSE,
	RC_forward	= KEY_FORWARD,
	RC_rewind	= KEY_REWIND,
	RC_stop		= KEY_STOP,
	RC_timeshift	= KEY_TIME,
	RC_mode		= KEY_MODE,
	RC_games	= KEY_GAMES,
	RC_next		= KEY_NEXT,
	RC_prev		= KEY_PREVIOUS,
	RC_www		= KEY_WWW,
	
	RC_power_on	= KEY_POWERON,
	RC_power_off	= KEY_POWEROFF,
	RC_standby_on	= KEY_STANDBYON,
	RC_standby_off	= KEY_STANDBYOFF,
	RC_mute_on	= KEY_MUTEON,
	RC_mute_off	= KEY_MUTEOFF,
	RC_analog_on	= KEY_ANALOGON,
	RC_analog_off	= KEY_ANALOGOFF,
};

struct key{
	char *name;
	unsigned long code;
};

static const struct key keyname[] = {
	{ "KEY_0",		KEY_0 },
	{ "KEY_1",		KEY_1 },
	{ "KEY_2",		KEY_2 },
	{ "KEY_3",		KEY_3 },
	{ "KEY_4",		KEY_4 },
	{ "KEY_5",		KEY_5 },
	{ "KEY_6",		KEY_6 },
	{ "KEY_7",		KEY_7 },
	{ "KEY_8",		KEY_8 },
	{ "KEY_9",		KEY_9 },
	{ "KEY_BACKSPACE",		KEY_BACKSPACE },
	{ "KEY_UP",		KEY_UP },
	{ "KEY_LEFT",		KEY_LEFT },
	{ "KEY_RIGHT",		KEY_RIGHT },
	{ "KEY_DOWN",		KEY_DOWN },
	{ "KEY_MUTE",		KEY_MUTE },
	{ "KEY_VOLUMEDOWN",		KEY_VOLUMEDOWN },
	{ "KEY_VOLUMEUP",		KEY_VOLUMEUP },
	{ "KEY_POWER",		KEY_POWER },
	{ "KEY_HELP",		KEY_HELP },
	{ "KEY_EXIT",		KEY_EXIT },
	{ "KEY_MENU",		KEY_MENU },
	{ "KEY_TOPLEFT",		KEY_TOPLEFT },
	{ "KEY_TOPRIGHT",		KEY_TOPRIGHT },
	{ "KEY_PAGEUP",		KEY_PAGEUP },
	{ "KEY_PAGEDOWN",		KEY_PAGEDOWN },
	{ "KEY_OK",		KEY_OK },
	{ "KEY_RED",		KEY_RED },
	{ "KEY_GREEN",		KEY_GREEN },
	{ "KEY_YELLOW",		KEY_YELLOW },
	{ "KEY_BLUE",		KEY_BLUE },
	{ "KEY_TOPLEFT",		KEY_TOPLEFT },
	{ "KEY_TOPRIGHT",		KEY_TOPRIGHT },
	{ "KEY_BOTTOMLEFT",		KEY_BOTTOMLEFT },
	{ "KEY_BOTTOMRIGHT",		KEY_BOTTOMRIGHT },
	{ "KEY_AUDIO",		KEY_AUDIO },
	{ "KEY_VIDEO",		KEY_VIDEO },
	{ "KEY_TV",		KEY_TV },
	{ "KEY_RADIO",		KEY_RADIO },
	{ "KEY_TEXT",		KEY_TEXT },
	{ "KEY_INFO",		KEY_INFO },
	{ "KEY_EPG",		KEY_EPG },
	{ "KEY_LAST",		KEY_LAST },
	{ "KEY_FAVORITES",		KEY_FAVORITES },
	{ "KEY_SAT",		KEY_SAT },
	{ "KEY_SAT2",		KEY_SAT2 },
	{ "KEY_RECORD",		KEY_RECORD },
	{ "KEY_PLAY",		KEY_PLAY },
	{ "KEY_PAUSE",		KEY_PAUSE },
	{ "KEY_FORWARD",		KEY_FORWARD },
	{ "KEY_REWIND",		KEY_REWIND },
	{ "KEY_STOP",		KEY_STOP },
	{ "KEY_TIME",		KEY_TIME },
	{ "KEY_MODE",		KEY_MODE },
	{ "KEY_GAMES",		KEY_GAMES },
	{ "KEY_NEXT",		KEY_NEXT },
	{ "KEY_PREVIOUS",		KEY_PREVIOUS },
	{ "KEY_WWW",		KEY_WWW },
	{ "KEY_POWERON",		KEY_POWERON },
	{ "KEY_POWEROFF",		KEY_POWEROFF },
	{ "KEY_STANDBYON",		KEY_STANDBYON },
	{ "KEY_STANDBYOFF",		KEY_STANDBYOFF },
	{ "KEY_MUTEON",		KEY_MUTEON },
	{ "KEY_MUTEOFF",		KEY_MUTEOFF },
	{ "KEY_ANALOGON",		KEY_ANALOGON },
	{ "KEY_ANALOGOFF",		KEY_ANALOGOFF },
};

