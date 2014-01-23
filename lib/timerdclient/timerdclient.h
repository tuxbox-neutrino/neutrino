/*
	Timer-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	$Id: timerdclient.h,v 1.47 2006/02/14 22:38:28 zwen Exp $

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


#ifndef __timerdclient__
#define __timerdclient__

#include <string>
#include <cstring>

#include <connection/basicclient.h>

#include <timerdclient/timerdtypes.h>

class CTimerdClient:private CBasicClient
{
 private:
	virtual unsigned char   getVersion   () const;
	virtual const          char * getSocketName() const;

 public:
		virtual ~CTimerdClient() {};
		enum events
		{
			EVT_SHUTDOWN = 1,
			EVT_ANNOUNCE_SHUTDOWN,
			EVT_ZAPTO,
			EVT_ANNOUNCE_ZAPTO,
			EVT_STANDBY_ON,
			EVT_STANDBY_OFF,
			EVT_RECORD_START,
			EVT_RECORD_STOP,
			EVT_ANNOUNCE_RECORD,
			EVT_ANNOUNCE_SLEEPTIMER,
			EVT_SLEEPTIMER,
			EVT_REMIND,
			EVT_EXEC_PLUGIN
		};

		void registerEvent(unsigned int eventID, unsigned int clientID, const char * const udsName);
		void unRegisterEvent(unsigned int eventID, unsigned int clientID);

		bool isTimerdAvailable();			// check if timerd is running

		CTimerd::TimerList getOverlappingTimers(time_t& announcetime, time_t& stoptime);

		int addTimerEvent( CTimerd::CTimerEventTypes evType, void* data, time_t alarmtime,time_t announcetime = 0, time_t stoptime = 0,
				   CTimerd::CTimerEventRepeat evrepeat = CTimerd::TIMERREPEAT_ONCE, uint32_t repeatcount = 0, bool forceadd=true);

		void removeTimerEvent( int evId);	// remove timer event
		void stopTimerEvent( int evId);	// set timer state to stoped (rescedule on demand)

		void getTimerList( CTimerd::TimerList &timerlist);		// returns the list of all timers
		void getTimer( CTimerd::responseGetTimer &timer, unsigned timerID);		// returns specified timer

		// modify existing timer event
		bool modifyTimerEvent(int eventid, time_t announcetime, time_t alarmtime, time_t stoptime, CTimerd::CTimerEventRepeat evrepeat = CTimerd::TIMERREPEAT_ONCE, uint32_t repeatcount=0);
		bool modifyTimerEvent(int eventid, time_t announcetime, time_t alarmtime, time_t stoptime, CTimerd::CTimerEventRepeat evrepeat, uint32_t repeatcount,
				      void *data,int datalen=0);

		bool modifyRecordTimerEvent(int eventid, time_t announcetime, time_t alarmtime, time_t stoptime, CTimerd::CTimerEventRepeat evrepeat, uint32_t repeatcount, const char * const recordingdir);

		void modifyTimerAPid(int eventid, unsigned char apids);

		// set existing sleeptimer to new times or create new sleeptimer with these times
		int setSleeptimer(time_t announcetime, time_t alarmtime, int timerid = 0);

		// returns the id of sleeptimer, 0 of no sleeptimer exists
		int getSleeptimerID();
		// returns remaining mins, -1 if no sleeptimer exists
		int getSleepTimerRemaining();


		// add diff to existing timer event
		bool rescheduleTimerEvent(int eventid, time_t diff);

		// add diff to existing timer event
		bool rescheduleTimerEvent(int eventid, time_t announcediff, time_t alarmdiff, time_t stoptime);

		// adds new sleeptimer event
		int addSleepTimerEvent(time_t announcetime,time_t alarmtime)	// sleeptimer setzen
			{return addTimerEvent(CTimerd::TIMER_SLEEPTIMER, NULL, announcetime, alarmtime, 0);};

		// adds new shutdown timer event
		int addShutdownTimerEvent(time_t alarmtime, time_t announcetime = 0, time_t stoptime = 0)
			{return addTimerEvent(CTimerd::TIMER_SHUTDOWN, NULL, announcetime, alarmtime, stoptime);};

		// adds new record timer event
		int addRecordTimerEvent(const t_channel_id channel_id, time_t alarmtime, time_t stoptime,
					uint64_t epgID=0, time_t epg_starttime=0, time_t announcetime = 0,
					unsigned char apids=TIMERD_APIDS_STD, bool safety=false,std::string recDir="", bool forceAdd=true)
		{
			CTimerd::RecordingInfo eventInfo;
			eventInfo.channel_id = channel_id;
			eventInfo.epgID = epgID;
			eventInfo.epg_starttime = epg_starttime;
			eventInfo.apids = apids;
			eventInfo.recordingSafety = safety;
			strncpy(eventInfo.recordingDir, recDir.c_str(), RECORD_DIR_MAXLEN);
			return addTimerEvent(CTimerd::TIMER_RECORD, &eventInfo, announcetime, alarmtime, stoptime,CTimerd::TIMERREPEAT_ONCE, 0,forceAdd);
		};

		int addImmediateRecordTimerEvent(const t_channel_id channel_id, time_t alarmtime, time_t stoptime,
						 uint64_t epgID=0, time_t epg_starttime=0,unsigned char apids=TIMERD_APIDS_STD)
		{
			CTimerd::EventInfo eventInfo;
			eventInfo.channel_id = channel_id;
			eventInfo.epgID = epgID;
			eventInfo.epg_starttime = epg_starttime;
			eventInfo.apids = apids;
			eventInfo.recordingSafety = false;
			return addTimerEvent(CTimerd::TIMER_IMMEDIATE_RECORD, &eventInfo, 0, alarmtime, stoptime);
		};

		// adds new standby timer event
		int addStandbyTimerEvent(bool standby_on,time_t alarmtime, time_t announcetime = 0, time_t stoptime = 0)
			{return addTimerEvent(CTimerd::TIMER_STANDBY, &standby_on,  announcetime, alarmtime, stoptime);};

		// adds new zapto timer event
		int addZaptoTimerEvent(const t_channel_id channel_id, time_t alarmtime, time_t announcetime = 0,
				       time_t stoptime = 0, uint64_t epgID=0, time_t epg_starttime=0,
				       unsigned char apids=TIMERD_APIDS_STD)
		{
			CTimerd::EventInfo eventInfo;
			eventInfo.channel_id = channel_id;
			eventInfo.epgID = epgID;
			eventInfo.epg_starttime = epg_starttime;
			eventInfo.apids = apids;
			eventInfo.recordingSafety = false;
			return addTimerEvent(CTimerd::TIMER_ZAPTO, &eventInfo, announcetime, alarmtime, stoptime);
		};

#if 0
		int addNextProgramTimerEvent(CTimerd::EventInfo eventInfo,time_t alarmtime, time_t announcetime = 0, time_t stoptime = 0)
		{
			// mal auf verdacht eingebaut
			// keine ahnung ob / was hier noch fehlt
			return addTimerEvent(CTimerd::TIMER_NEXTPROGRAM, &eventInfo, alarmtime, announcetime, stoptime);
		};
#endif

		// Exit timerd and programm wakeup
		bool shutdown();

		// set and get recording safety time (in sec)
		void setRecordingSafety(int pre, int post);
		void getRecordingSafety(int &pre, int &post);

		// Convert String of O and X to repeat type and vice versa
		//void getWeekdaysFromStr(int *rep, const char* str);
		void getWeekdaysFromStr(CTimerd::CTimerEventRepeat *rep, std::string &str);
		void setWeekdaysToStr(CTimerd::CTimerEventRepeat rep, std::string &str);
};

#endif
