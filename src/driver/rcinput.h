/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __MOD_rcinput__
#define __MOD_rcinput__

#include <config.h>
#include <linux/input.h>
#include <stdint.h>
#include <sys/types.h>
#include <string>
#include <vector>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#ifdef BOXMODEL_CS_HD2
#ifdef HAVE_COOLSTREAM_CS_IR_GENERIC_H
#include <cs_ir_generic.h>
#endif
#else
#ifdef HAVE_COOLSTREAM_NEVIS_IR_H
#include <nevis_ir.h>
#endif
#endif

#include <driver/neutrino_msg_t.h>

#ifndef KEY_OK
#define KEY_OK           0x160
#endif

#ifndef KEY_RED
#define KEY_RED          0x18e
#endif

#ifndef KEY_GREEN
#define KEY_GREEN        0x18f
#endif

#ifndef KEY_YELLOW
#define KEY_YELLOW       0x190
#endif

#ifndef KEY_BLUE
#define KEY_BLUE         0x191
#endif

#ifndef KEY_GAMES
#define KEY_GAMES        0x1a1   /* Media Select Games */
#endif
/* SAGEM remote controls have the following additional keys */

#ifndef KEY_TOPLEFT
#define KEY_TOPLEFT      0x1a2
#endif

#ifndef KEY_TOPRIGHT
#define KEY_TOPRIGHT     0x1a3
#endif

#ifndef KEY_BOTTOMLEFT
#define KEY_BOTTOMLEFT   0x1a4
#endif

#ifndef KEY_BOTTOMRIGHT
#define KEY_BOTTOMRIGHT  0x1a5
#endif

#define KEY_POWERON	KEY_FN_F1
#define KEY_POWEROFF	KEY_FN_F2
#define KEY_STANDBYON	KEY_FN_F3
#define KEY_STANDBYOFF	KEY_FN_F4
#define KEY_MUTEON	KEY_FN_F5
#define KEY_MUTEOFF	KEY_FN_F6
#define KEY_ANALOGON	KEY_FN_F7
#define KEY_ANALOGOFF	KEY_FN_F8

#define KEY_TTTV	KEY_FN_1
#define KEY_TTZOOM	KEY_FN_2
#define KEY_REVEAL	KEY_FN_D
/* only defined in newer kernels / headers... */
#ifndef KEY_ZOOMIN
#define KEY_ZOOMIN	KEY_FN_E
#endif
#ifndef KEY_ZOOMOUT
#define KEY_ZOOMOUT	KEY_FN_F
#endif
/* still available, even in 2.6.12:
	#define KEY_FN_S
	#define KEY_FN_B
*/

class CRCInput
{
	private:
		struct event
		{
			neutrino_msg_t      msg;
			neutrino_msg_data_t data;
		};

		struct timer
		{
			uint			id;
			uint64_t		interval;
			uint64_t		times_out;
			bool			correct_time;
		};

		struct in_dev
		{
			int fd;
			std::string path;
		};

		uint32_t               timerid;
		std::vector<timer> timers;

		uint32_t	*repeatkeys;
		uint64_t	longPressEnd;
		bool		longPressAny;
		int 		fd_pipe_high_priority[2];
		int 		fd_pipe_low_priority[2];
		int         	fd_gamerc;
		std::vector<in_dev> indev;
		int		fd_keyb;
		int		fd_event;

		int		fd_max;
		__u16 rc_last_key;
		OpenThreads::Mutex mutex;

		void open(bool recheck = false);
		bool checkpath(in_dev id);
		bool checkdev();
		void close();
		int translate(int code);
		void calculateMaxFd(void);
		int checkTimers();
		bool mayRepeat(uint32_t key, bool bAllowRepeatLR = false);
		bool mayLongPress(uint32_t key, bool bAllowRepeatLR = false);
#ifdef IOC_IR_SET_PRI_PROTOCOL
		void set_rc_hw(ir_protocol_t ir_protocol, unsigned int ir_address);
#endif
	public:
		//rc-code definitions
		static const neutrino_msg_t RC_Repeat   = 0x0400;
		static const neutrino_msg_t RC_Release  = 0x0800;
		static const neutrino_msg_t RC_MaxRC    = KEY_MAX | RC_Repeat | RC_Release; /* /include/linux/input.h: #define KEY_MAX 0x1ff */
		static const neutrino_msg_t RC_KeyBoard = 0x4000;
		static const neutrino_msg_t RC_Events   = 0x80000000;
		static const neutrino_msg_t RC_Messages = 0x90000000;
		static const neutrino_msg_t RC_WithData = 0xA0000000;
		enum
		{
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
#if defined BOXMODEL_VUSOLO4K
			RC_info		= 0xFFFE,
			RC_epg		= KEY_INFO,
#else
			RC_info		= KEY_INFO,
			RC_epg		= KEY_EPG,
#endif
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
			RC_sub		= KEY_SUBTITLE,
			RC_pos		= KEY_MOVE,
			RC_sleep	= KEY_SLEEP,
			RC_nextsong	= KEY_NEXTSONG,
			RC_previoussong	= KEY_PREVIOUSSONG,
			RC_bookmarks	= KEY_BOOKMARKS,
			RC_program	= KEY_PROGRAM,
			RC_playpause	= KEY_PLAYPAUSE,

			RC_power_on	= KEY_POWERON,
			RC_power_off	= KEY_POWEROFF,
			RC_standby_on	= KEY_STANDBYON,
			RC_standby_off	= KEY_STANDBYOFF,
			RC_mute_on	= KEY_MUTEON,
			RC_mute_off	= KEY_MUTEOFF,
			RC_analog_on	= KEY_ANALOGON,
			RC_analog_off	= KEY_ANALOGOFF,

			/* tripledragon keys */
			RC_eject	= KEY_EJECTCD,
			RC_aux		= KEY_AUX,          /* 0x186 */
			RC_timer	= KEY_TIME,
			RC_tttv		= KEY_TTTV,
			RC_ttzoom	= KEY_TTZOOM,
			RC_ttreveal	= KEY_REVEAL,
			RC_zoomin	= KEY_ZOOMIN,
			RC_zoomout	= KEY_ZOOMOUT,

			RC_timeout	= 0xFFFFFFFF,
			RC_nokey	= RC_NOKEY
		};

		//rc-hardware definitions
		enum
		{
			RC_HW_COOLSTREAM	= 0,
			RC_HW_DBOX		= 1,
			RC_HW_PHILIPS		= 2,
			RC_HW_TRIPLEDRAGON	= 3
		};
		void set_rc_hw(void);

		void stopInput(const bool ext = false);
		void restartInput(const bool ext = false);
		bool isLocked(void);

		uint64_t repeat_block;
		uint64_t repeat_block_generic;
		bool		firstKey;
		CRCInput();      //constructor - opens rc-device and starts needed threads
		~CRCInput();     //destructor - closes rc-device


		static bool isNumeric(const neutrino_msg_t key);
		static int getNumericValue(const neutrino_msg_t key);
		static unsigned int convertDigitToKey(const unsigned int digit);
		static const char *getUnicodeValue(const neutrino_msg_t key);
		uint32_t *setAllowRepeat(uint32_t *);

		static const char * getSpecialKeyName(const unsigned int key);
		static const char *getKeyNameC(const unsigned int key);
		static std::string getKeyName(const unsigned int key);

		int addTimer(uint64_t Interval, bool oneshot= true, bool correct_time= true );
		int addTimer(struct timeval Timeout);
		int addTimer(const time_t *Timeout);

		void killTimer(uint32_t &id);

		static int64_t calcTimeoutEnd_MS(const int timeout_in_milliseconds);
		static int64_t calcTimeoutEnd(const int timeout_in_seconds);

		void getMsgAbsoluteTimeout(neutrino_msg_t * msg, neutrino_msg_data_t * data, uint64_t *TimeoutEnd, bool bAllowRepeatLR= false);
		void getMsg(neutrino_msg_t * msg, neutrino_msg_data_t * data, int Timeout, bool bAllowRepeatLR= false);        //get message, timeout in 1/10 secs :)
		void getMsg_ms(neutrino_msg_t * msg, neutrino_msg_data_t * data, int Timeout, bool bAllowRepeatLR= false);     //get message, timeout in msecs :)
		void getMsg_us(neutrino_msg_t * msg, neutrino_msg_data_t * data, uint64_t Timeout, bool bAllowRepeatLR= false);//get message, timeout in µsecs :)
		void postMsg(const neutrino_msg_t msg, const neutrino_msg_data_t data, const bool Priority = true);            // push message back into buffer
		void clearRCMsg();

		int messageLoop( bool anyKeyCancels = false, int timeout= -1 );

		void setLongPressAny(bool b) { longPressAny = b; };
		void setKeyRepeatDelay(unsigned int start_ms, unsigned int repeat_ms);
};


#endif
