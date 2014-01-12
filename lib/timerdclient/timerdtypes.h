/*
	Timer-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	$Id: timerdtypes.h,v 1.20 2006/02/28 21:51:01 zwen Exp $

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


#ifndef __timerdtypes__
#define __timerdtypes__

#include <zapit/client/zapittypes.h>
#include <sectionsdclient/sectionsdtypes.h>

#include <vector>

#define REMINDER_MESSAGE_MAXLEN 31
#define EXEC_PLUGIN_NAME_MAXLEN 31
#define RECORD_DIR_MAXLEN 100
#define EPG_TITLE_MAXLEN 50

#define TIMERD_APIDS_CONF 0x00
#define TIMERD_APIDS_STD  0x01
#define TIMERD_APIDS_ALT  0x02
#define TIMERD_APIDS_AC3  0x04
#define TIMERD_APIDS_ALL  0xFF

class CTimerd
{
	public:
		enum CTimerEventRepeat 
		{ 
			TIMERREPEAT_ONCE = 0,
			TIMERREPEAT_DAILY, 
			TIMERREPEAT_WEEKLY, 
			TIMERREPEAT_BIWEEKLY, 
			TIMERREPEAT_FOURWEEKLY, 
			TIMERREPEAT_MONTHLY, 
			TIMERREPEAT_BYEVENTDESCRIPTION,
			TIMERREPEAT_WEEKDAYS = 0x100 // Bits 9-15 specify weekdays (9=mo,10=di,...)
		};

		enum CTimerEventTypes
		{
			TIMER_SHUTDOWN = 1,
			__TIMER_NEXTPROGRAM, /* unused, here to keep compatibility with old timerd.conf */
			TIMER_ZAPTO,
			TIMER_STANDBY,
			TIMER_RECORD,
			TIMER_REMIND,
			TIMER_SLEEPTIMER,
			TIMER_EXEC_PLUGIN,
 			TIMER_IMMEDIATE_RECORD
		};
		
		enum CTimerEventStates 
		{ 
			TIMERSTATE_SCHEDULED, 
			TIMERSTATE_PREANNOUNCE, 
			TIMERSTATE_ISRUNNING, 
			TIMERSTATE_HASFINISHED, 
			TIMERSTATE_TERMINATED 
		};

		struct EventInfo
		{
			event_id_t    epgID;
			time_t        epg_starttime;
			t_channel_id  channel_id;
			unsigned char apids;
			bool          recordingSafety;
		};

		struct TransferEventInfo
		{
			event_id_t    epgID;
			time_t        epg_starttime;
			t_channel_id  channel_id;
			unsigned char apids;
			bool          recordingSafety;
		};

		struct TransferRecordingInfo : TransferEventInfo
		{
			char         recordingDir[RECORD_DIR_MAXLEN];
			char         epgTitle[EPG_TITLE_MAXLEN];

		};

		class RecordingInfo : public EventInfo
			{
			public:
				RecordingInfo(){};
				RecordingInfo(EventInfo& e)
					{
						apids = e.apids;
						channel_id = e.channel_id;
						epgID = e.epgID;
						epg_starttime = e.epg_starttime;
						recordingSafety = e.recordingSafety;
					};
				RecordingInfo& operator = (EventInfo& e)
					{
						apids = e.apids;
						channel_id = e.channel_id;
						epgID = e.epgID;
						epg_starttime = e.epg_starttime;
						recordingSafety = e.recordingSafety;
						return *this;
					}
				unsigned char apids;
				int eventID;
				char recordingDir[RECORD_DIR_MAXLEN];
				char epgTitle[EPG_TITLE_MAXLEN];
			};

		struct RecordingStopInfo
		{
			int eventID;
			t_channel_id  channel_id;
		};

		struct responseGetTimer
		{		
			int               eventID;
			CTimerEventTypes  eventType;
			CTimerEventStates eventState;
			CTimerEventRepeat eventRepeat;
			uint32_t	  repeatCount;
			time_t            alarmTime;
			time_t            announceTime;
			time_t            stopTime;
			t_channel_id      channel_id;                       //only filled if applicable
			event_id_t        epgID;                            //only filled if applicable
			time_t            epg_starttime;                    //only filled if applicable
			unsigned char     apids;                            //only filled if applicable
			bool              standby_on;                       //only filled if applicable
			char              message[REMINDER_MESSAGE_MAXLEN];         //only filled if applicable
			char              pluginName[EXEC_PLUGIN_NAME_MAXLEN];      //only filled if applicable
			char              recordingDir[RECORD_DIR_MAXLEN];       //only filled if applicable
			char              epgTitle[EPG_TITLE_MAXLEN];       //only filled if applicable
			
			bool operator< (const responseGetTimer& a) const
			{
				return this->alarmTime < a.alarmTime ;
			}
		};
		
		typedef std::vector<responseGetTimer> TimerList;
};
#endif
