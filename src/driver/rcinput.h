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

#ifndef __rcinput_h__
#define __rcinput_h__

#include <config.h>
#include <linux/input.h>
#include <stdint.h>
#include <sys/types.h>
#include <string>
#include <vector>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#ifdef BOXMODEL_CST_HD2
#ifdef HAVE_COOLSTREAM_CS_IR_GENERIC_H
#include <cs_ir_generic.h>
#endif
#else
#ifdef HAVE_COOLSTREAM_NEVIS_IR_H
#include <nevis_ir.h>
#endif
#endif

#include <driver/neutrino_msg_t.h>
#include <driver/rcinput_fake.h>

class CRCInput
{
	private:
		struct event
		{
			neutrino_msg_t msg;
			neutrino_msg_data_t data;
		};

		struct timer
		{
			uint id;
			uint64_t interval;
			uint64_t times_out;
			bool correct_time;
		};

		struct in_dev
		{
			int fd;
			std::string path;
		};

		uint32_t timerid;
		std::vector<timer> timers;

		uint32_t *repeatkeys;
		uint64_t longPressEnd;
		bool longPressAny;
		int fd_pipe_high_priority[2];
		int fd_pipe_low_priority[2];
		int fd_gamerc;
		std::vector<in_dev> indev;
		int fd_keyb;
		int fd_event;
		int fd_max;
		__u16 rc_last_key;
		OpenThreads::Mutex mutex;
		OpenThreads::Mutex timer_mutex;

		void open(bool recheck = false);
		bool checkpath(in_dev id);
		bool checkdev();
		void close();
		int translate(int code);
		int translate_revert(int code);
		void calculateMaxFd(void);
		int checkTimers();
		bool mayRepeat(uint32_t key, bool bAllowRepeatLR = false);
		bool mayLongPress(uint32_t key, bool bAllowRepeatLR = false);
#ifdef IOC_IR_SET_PRI_PROTOCOL
		void set_rc_hw(ir_protocol_t ir_protocol, unsigned int ir_address);
#endif
	public:
		// rc-code definitions; see /include/linux/input.h
		static const neutrino_msg_t RC_Repeat   = 0x0400;
		static const neutrino_msg_t RC_Release  = 0x0800;
		static const neutrino_msg_t RC_MaxRC    = KEY_MAX | RC_Repeat | RC_Release;
		static const neutrino_msg_t RC_KeyBoard = 0x4000;
		static const neutrino_msg_t RC_Events   = 0x80000000;
		static const neutrino_msg_t RC_Messages = 0x90000000;
		static const neutrino_msg_t RC_WithData = 0xA0000000;
		enum
		{
			RC_0		= KEY_0,
			RC_1		= KEY_1,
			RC_2		= KEY_2,
			RC_3		= KEY_3,
			RC_4		= KEY_4,
			RC_5		= KEY_5,
			RC_6		= KEY_6,
			RC_7		= KEY_7,
			RC_8		= KEY_8,
			RC_9		= KEY_9,
			RC_backspace	= KEY_BACKSPACE,
			RC_up		= KEY_UP,
			RC_left		= KEY_LEFT,
			RC_right	= KEY_RIGHT,
			RC_down		= KEY_DOWN,
			RC_spkr		= KEY_MUTE,
			RC_minus	= KEY_VOLUMEDOWN,
			RC_plus		= KEY_VOLUMEUP,
			RC_standby	= KEY_POWER,
			RC_help		= KEY_HELP,
			RC_back		= KEY_BACK,
			RC_home		= KEY_HOME,
			RC_setup	= KEY_MENU,
			RC_topleft	= KEY_TOPLEFT,
			RC_topright	= KEY_TOPRIGHT,
			RC_page_up	= KEY_PAGEUP,
			RC_page_down	= KEY_PAGEDOWN,
			RC_ok		= KEY_OK,
			RC_red		= KEY_RED,
			RC_green	= KEY_GREEN,
			RC_yellow	= KEY_YELLOW,
			RC_blue		= KEY_BLUE,
			RC_top_left	= KEY_TOPLEFT,
			RC_top_right	= KEY_TOPRIGHT,
			RC_bottom_left	= KEY_BOTTOMLEFT,
			RC_bottom_right	= KEY_BOTTOMRIGHT,

			RC_aux		= KEY_AUX,
			RC_audio	= KEY_AUDIO,
			RC_video	= KEY_VIDEO,
			RC_tv		= KEY_TV,
			RC_tv2		= KEY_TV2,
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
			RC_pvr		= KEY_PVR,
			RC_forward	= KEY_FORWARD,
			RC_rewind	= KEY_REWIND,
			RC_stop		= KEY_STOP,
			RC_timer	= KEY_TIME,
			RC_timeshift	= KEY_TIME,
			RC_mode		= KEY_MODE,
			RC_switchvideomode = KEY_SWITCHVIDEOMODE,
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
			RC_touchpad_toggle = KEY_TOUCHPAD_TOGGLE,
			RC_vod		= KEY_VOD,

			RC_f1		= KEY_F1,
			RC_f2		= KEY_F2,
			RC_f3		= KEY_F3,
			RC_f4		= KEY_F4,
			RC_f5		= KEY_F5,
			RC_f6		= KEY_F6,
			RC_f7		= KEY_F7,
			RC_f8		= KEY_F8,
			RC_f9		= KEY_F9,
			RC_f10		= KEY_F10,

			RC_power_on	= KEY_POWERON,
			RC_power_off	= KEY_POWEROFF,
			RC_standby_on	= KEY_STANDBYON,
			RC_standby_off	= KEY_STANDBYOFF,
			RC_mute_on	= KEY_MUTEON,
			RC_mute_off	= KEY_MUTEOFF,
			RC_analog_on	= KEY_ANALOGON,
			RC_analog_off	= KEY_ANALOGOFF,

			/* RC_analog_off should be the last entry. See src/create_rcsim_h.sh */

			RC_timeout	= 0xFFFFFFFF,
			RC_nokey	= RC_NOKEY
		};

		// rc-hardware definitions
		enum
		{
			RC_HW_COOLSTREAM = 0,
			RC_HW_DBOX = 1,
			RC_HW_PHILIPS = 2
		};
		void set_rc_hw(void);

		void stopInput(const bool ext = false);
		void restartInput(const bool ext = false);
		bool isLocked(void);

		uint64_t repeat_block;
		uint64_t repeat_block_generic;
		bool firstKey;
		CRCInput(); //constructor - opens rc-device and starts needed threads
		~CRCInput(); //destructor - closes rc-device

		static bool isNumeric(const neutrino_msg_t key);
		static int getNumericValue(const neutrino_msg_t key);
		static unsigned int convertDigitToKey(const unsigned int digit);
		static const char *getUnicodeValue(const neutrino_msg_t key);
		uint32_t *setAllowRepeat(uint32_t *);

		static const char *getSpecialKeyName(const unsigned int key);
		static const char *getKeyNameC(const unsigned int key);
		static std::string getKeyName(const unsigned int key);

		int addTimer(uint64_t Interval, bool oneshot = true, bool correct_time = true);
		int addTimer(struct timeval Timeout);
		int addTimer(const time_t *Timeout);

		void killTimer(uint32_t &id);

		static int64_t calcTimeoutEnd_MS(const int timeout_in_milliseconds);
		static int64_t calcTimeoutEnd(const int timeout_in_seconds);

		void getMsgAbsoluteTimeout(neutrino_msg_t *msg, neutrino_msg_data_t *data, uint64_t *TimeoutEnd, bool bAllowRepeatLR = false);
		void getMsg(neutrino_msg_t *msg, neutrino_msg_data_t *data, int Timeout, bool bAllowRepeatLR = false);         // get message, timeout in 1/10 secs
		void getMsg_ms(neutrino_msg_t *msg, neutrino_msg_data_t *data, int Timeout, bool bAllowRepeatLR = false);      // get message, timeout in msecs
		void getMsg_us(neutrino_msg_t *msg, neutrino_msg_data_t *data, uint64_t Timeout, bool bAllowRepeatLR = false); // get message, timeout in µsecs
		void postMsg(const neutrino_msg_t msg, const neutrino_msg_data_t data, const bool Priority = true);            // push message back into buffer
		void clearRCMsg();

		int messageLoop(bool anyKeyCancels = false, int timeout = -1);

		int translateRevert(int c) { return translate_revert(c); };
		void setLongPressAny(bool b) { longPressAny = b; };
		void setKeyRepeatDelay(unsigned int start_ms, unsigned int repeat_ms);
};

#endif
