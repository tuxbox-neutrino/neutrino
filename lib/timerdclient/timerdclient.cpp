/*
	Timer Daemon  -   DBoxII-Project

	Copyright (C) 2002 Dirk Szymanski 'Dirch'
	
	$Id: timerdclient.cpp,v 1.55 2007/10/09 20:46:05 guenther Exp $

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

#include <string.h>

#include <eventserver.h>
#include <timerdclient/timerdmsg.h>
#include <timerdclient/timerdclient.h>

#ifdef PEDANTIC_VALGRIND_SETUP
#define VALGRIND_PARANOIA(x) memset(&x, 0, sizeof(x))
#else
#define VALGRIND_PARANOIA(x) {}
#endif

unsigned char   CTimerdClient::getVersion   () const
{
	return CTimerdMsg::ACTVERSION;
}

const          char * CTimerdClient::getSocketName() const
{
	return TIMERD_UDS_NAME;
}

//-------------------------------------------------------------------------

void CTimerdClient::registerEvent(unsigned int eventID, unsigned int clientID, const char * const udsName)
{
	CEventServer::commandRegisterEvent msg2;
	VALGRIND_PARANOIA(msg2);

	msg2.eventID = eventID;
	msg2.clientID = clientID;

	strcpy(msg2.udsName, udsName);

	send(CTimerdMsg::CMD_REGISTEREVENT, (char*)&msg2, sizeof(msg2));

	close_connection();
}
//-------------------------------------------------------------------------

void CTimerdClient::unRegisterEvent(unsigned int eventID, unsigned int clientID)
{
	CEventServer::commandUnRegisterEvent msg2;
	VALGRIND_PARANOIA(msg2);

	msg2.eventID = eventID;
	msg2.clientID = clientID;

	send(CTimerdMsg::CMD_UNREGISTEREVENT, (char*)&msg2, sizeof(msg2));

	close_connection();
}
//-------------------------------------------------------------------------

int CTimerdClient::setSleeptimer(time_t announcetime, time_t alarmtime, int timerid)
{
	int timerID;

	if(timerid == 0)
		timerID = getSleeptimerID();
	else
		timerID = timerid;

	if(timerID != 0)
	{
		modifyTimerEvent(timerID, announcetime, alarmtime, 0);
	}
	else
	{
		timerID = addTimerEvent(CTimerd::TIMER_SLEEPTIMER,NULL,announcetime,alarmtime,0);
	}

	return timerID;   
}
//-------------------------------------------------------------------------

int CTimerdClient::getSleeptimerID()
{
	send(CTimerdMsg::CMD_GETSLEEPTIMER);
	CTimerdMsg::responseGetSleeptimer response;
	if(!receive_data((char*)&response, sizeof(CTimerdMsg::responseGetSleeptimer)))
		response.eventID =0;
	close_connection();  
	return response.eventID;
}
//-------------------------------------------------------------------------

int CTimerdClient::getSleepTimerRemaining()
{
	int timerID;
	if((timerID = getSleeptimerID()) != 0)
	{
		CTimerd::responseGetTimer timer;
		getTimer( timer, timerID);
		int min=(((timer.alarmTime + 1 - time(NULL)) / 60)+1); //aufrunden auf n�chst gr��erere Min.
		if(min <1)
			min=1;
		return min;
	}
	else
		return 0;
}
//-------------------------------------------------------------------------

void CTimerdClient::getTimerList(CTimerd::TimerList &timerlist)
{
        CTimerdMsg::generalInteger responseInteger;
	CTimerd::responseGetTimer  response;

	send(CTimerdMsg::CMD_GETTIMERLIST);

	timerlist.clear();

        if (CBasicClient::receive_data((char* )&responseInteger, sizeof(responseInteger)))
        {
                while (responseInteger.number-- > 0)
                {
                        if (CBasicClient::receive_data((char*)&response, sizeof(response)))
				if (response.eventState != CTimerd::TIMERSTATE_TERMINATED)
					timerlist.push_back(response);
                };
        }

	close_connection();
}
//-------------------------------------------------------------------------

void CTimerdClient::getTimer( CTimerd::responseGetTimer &timer, unsigned timerID)
{
	send(CTimerdMsg::CMD_GETTIMER, (char*)&timerID, sizeof(timerID));

	CTimerd::responseGetTimer response;
	receive_data((char*)&response, sizeof(CTimerd::responseGetTimer));
	timer = response;
	close_connection();
}
//-------------------------------------------------------------------------


bool CTimerdClient::modifyTimerEvent(int eventid, time_t announcetime, time_t alarmtime, time_t stoptime, CTimerd::CTimerEventRepeat evrepeat, uint32_t repeatcount)
{
	return modifyTimerEvent(eventid,announcetime,alarmtime,stoptime,evrepeat,repeatcount,NULL);
}
//-------------------------------------------------------------------------

bool CTimerdClient::modifyTimerEvent(int eventid, time_t announcetime, time_t alarmtime, time_t stoptime, CTimerd::CTimerEventRepeat evrepeat, uint32_t repeatcount, void *data,
				     int datalen)
{
	// set new time values for event eventid

	CTimerdMsg::commandModifyTimer msgModifyTimer;
	VALGRIND_PARANOIA(msgModifyTimer);
	msgModifyTimer.eventID = eventid;
	msgModifyTimer.announceTime = announcetime;
	msgModifyTimer.alarmTime = alarmtime;
	msgModifyTimer.stopTime = stoptime;
	msgModifyTimer.eventRepeat = evrepeat;
	msgModifyTimer.repeatCount = repeatcount;
	send(CTimerdMsg::CMD_MODIFYTIMER, (char*) &msgModifyTimer, sizeof(msgModifyTimer));

	if (data && datalen)
		send_data((char*)data,datalen);

	CTimerdMsg::responseStatus response;
	receive_data((char*)&response, sizeof(response));

	close_connection();
	return true;
}
//-------------------------------------------------------------------------

bool CTimerdClient::modifyRecordTimerEvent(int eventid, time_t announcetime, time_t alarmtime, time_t stoptime, CTimerd::CTimerEventRepeat evrepeat, uint32_t repeatcount,
					   const char * const recordingdir)
{
	CTimerdMsg::commandRecordDir rdir;
	strncpy(rdir.recDir,recordingdir,RECORD_DIR_MAXLEN-1);
	return modifyTimerEvent(eventid,announcetime,alarmtime,stoptime,evrepeat,repeatcount,&rdir,sizeof(rdir));
}
//-------------------------------------------------------------------------

bool CTimerdClient::rescheduleTimerEvent(int eventid, time_t diff)
{
	rescheduleTimerEvent(eventid,diff,diff,diff);
	return true;
}
//-------------------------------------------------------------------------

bool CTimerdClient::rescheduleTimerEvent(int eventid, time_t announcediff, time_t alarmdiff, time_t stopdiff)
{
	CTimerdMsg::commandModifyTimer msgModifyTimer;
	VALGRIND_PARANOIA(msgModifyTimer);
	msgModifyTimer.eventID = eventid;
	msgModifyTimer.announceTime = announcediff;
	msgModifyTimer.alarmTime = alarmdiff;
	msgModifyTimer.stopTime = stopdiff;

	send(CTimerdMsg::CMD_RESCHEDULETIMER, (char*) &msgModifyTimer, sizeof(msgModifyTimer));

	CTimerdMsg::responseStatus response;
	receive_data((char*)&response, sizeof(response));

	close_connection();
	return response.status;
}
//-------------------------------------------------------------------------

/*
int CTimerdClient::addTimerEvent( CTimerEventTypes evType, void* data , int min, int hour, int day, int month, CTimerd::CTimerEventRepeat evrepeat)
{
	time_t actTime_t;
	time(&actTime_t);
	struct tm* actTime = localtime(&actTime_t);

	actTime->tm_min = min;
	actTime->tm_hour = hour;

	if (day > 0)
		actTime->tm_mday = day;
	if (month > 0)
		actTime->tm_mon = month -1; 
	
	addTimerEvent(evType,true,data,0,mktime(actTime),0);
}
*/
//-------------------------------------------------------------------------
int CTimerdClient::addTimerEvent( CTimerd::CTimerEventTypes evType, void* data, time_t announcetime, time_t alarmtime,time_t stoptime,
				  CTimerd::CTimerEventRepeat evrepeat, uint32_t repeatcount,bool forceadd)
{

	if (!forceadd)
	{
		//printf("[CTimerdClient] checking for overlapping timers\n");
		CTimerd::TimerList overlappingTimer;
		overlappingTimer = getOverlappingTimers(alarmtime, stoptime);
		if (!overlappingTimer.empty())
		{
			// timerd starts eventID at 0 so we can return -1
			return -1;
		}
	}

	CTimerd::TransferEventInfo tei; 
	CTimerd::TransferRecordingInfo tri;
	CTimerdMsg::commandAddTimer msgAddTimer;
	VALGRIND_PARANOIA(tei);
	VALGRIND_PARANOIA(tri);
	VALGRIND_PARANOIA(msgAddTimer);
	msgAddTimer.alarmTime  = alarmtime;
	msgAddTimer.announceTime = announcetime;
	msgAddTimer.stopTime   = stoptime;
	msgAddTimer.eventType = evType;
	msgAddTimer.eventRepeat = evrepeat;
	msgAddTimer.repeatCount = repeatcount;
	int length;
	if( evType == CTimerd::TIMER_SHUTDOWN || evType == CTimerd::TIMER_SLEEPTIMER )
	{
		length = 0;
	}
	/* else if(evType == CTimerd::TIMER_NEXTPROGRAM || evType == CTimerd::TIMER_ZAPTO || */
	else if (evType == CTimerd::TIMER_ZAPTO ||
		evType == CTimerd::TIMER_IMMEDIATE_RECORD )
	{
		CTimerd::EventInfo *ei=static_cast<CTimerd::EventInfo*>(data); 
		tei.apids = ei->apids;
		tei.channel_id = ei->channel_id;
		tei.epg_starttime	= ei->epg_starttime;
		tei.epgID = ei->epgID;
		tei.recordingSafety = ei->recordingSafety;
		length = sizeof( CTimerd::TransferEventInfo);
		data = &tei;
	}
	else if(evType == CTimerd::TIMER_RECORD)
	{
		CTimerd::RecordingInfo *ri=static_cast<CTimerd::RecordingInfo*>(data); 
		tri.apids = ri->apids;
		tri.channel_id = ri->channel_id;
		tri.epg_starttime	= ri->epg_starttime;
		tri.epgID = ri->epgID;
		tri.recordingSafety = ri->recordingSafety;
		strncpy(tri.recordingDir, ri->recordingDir, RECORD_DIR_MAXLEN-1);
		length = sizeof( CTimerd::TransferRecordingInfo);
		data = &tri;
	}
	else if(evType == CTimerd::TIMER_STANDBY)
	{
		length = sizeof(CTimerdMsg::commandSetStandby);
	}
	else if(evType == CTimerd::TIMER_REMIND)
	{
		length = sizeof(CTimerdMsg::commandRemind);
	}
	else if(evType == CTimerd::TIMER_EXEC_PLUGIN)
	{
		length = sizeof(CTimerdMsg::commandExecPlugin);
	}
	else
	{
		length = 0;
	}

	send(CTimerdMsg::CMD_ADDTIMER, (char*)&msgAddTimer, sizeof(msgAddTimer));

	if((data != NULL) && (length > 0))
		send_data((char*)data, length);

	CTimerdMsg::responseAddTimer response;
	receive_data((char*)&response, sizeof(response));
	close_connection();

	return( response.eventID);
}
//-------------------------------------------------------------------------

void CTimerdClient::removeTimerEvent( int evId)
{
	CTimerdMsg::commandRemoveTimer msgRemoveTimer;
	VALGRIND_PARANOIA(msgRemoveTimer);

	msgRemoveTimer.eventID  = evId;

	send(CTimerdMsg::CMD_REMOVETIMER, (char*) &msgRemoveTimer, sizeof(msgRemoveTimer));

	close_connection();  
}
//-------------------------------------------------------------------------

bool CTimerdClient::isTimerdAvailable()
{
	if(!send(CTimerdMsg::CMD_TIMERDAVAILABLE))
		return false;

	CTimerdMsg::responseAvailable response;
	bool ret=receive_data((char*)&response, sizeof(response));
	close_connection();
	return ret;
}
//-------------------------------------------------------------------------

CTimerd::TimerList CTimerdClient::getOverlappingTimers(time_t& startTime, time_t& stopTime)
{
	CTimerd::TimerList timerlist; 
	CTimerd::TimerList overlapping;
	int timerPre;
	int timerPost;

	getTimerList(timerlist);
	getRecordingSafety(timerPre,timerPost);

	for (CTimerd::TimerList::iterator it = timerlist.begin();
	     it != timerlist.end();++it)
	{
		if(it->stopTime != 0 && stopTime != 0)
		{
			// Check if both timers have start and end. In this case do not show conflict, if endtime is the same than the starttime of the following timer
			if ((stopTime+timerPost > it->alarmTime) && (startTime-timerPre < it->stopTime))
			{
				overlapping.push_back(*it);
			}
		}
		else
		{
			if (!((stopTime < it->announceTime) || (startTime > it->stopTime)))
			{
				overlapping.push_back(*it);
			}
		}
	}
	return overlapping;
}

//-------------------------------------------------------------------------

bool CTimerdClient::shutdown()
{
	send(CTimerdMsg::CMD_SHUTDOWN);

	CTimerdMsg::responseStatus response;
	receive_data((char*)&response, sizeof(response));

	close_connection();
	return response.status;
}
//-------------------------------------------------------------------------
void CTimerdClient::modifyTimerAPid(int eventid, unsigned char apids)
{
	CTimerdMsg::commandSetAPid data;
	VALGRIND_PARANOIA(data);
	data.eventID=eventid;
	data.apids = apids;
	send(CTimerdMsg::CMD_SETAPID, (char*) &data, sizeof(data)); 
	close_connection();
}

//-------------------------------------------------------------------------
void CTimerdClient::setRecordingSafety(int pre, int post)
{
	CTimerdMsg::commandRecordingSafety data;
	VALGRIND_PARANOIA(data);
	data.pre = pre;
	data.post = post;
	send(CTimerdMsg::CMD_SETRECSAFETY, (char*) &data, sizeof(data)); 
	close_connection();
}

//-------------------------------------------------------------------------
void CTimerdClient::getRecordingSafety(int &pre, int &post)
{
	send(CTimerdMsg::CMD_GETRECSAFETY);
	CTimerdMsg::commandRecordingSafety data;

	bool success = receive_data((char*)&data, sizeof(data));
	close_connection();
	if (success)
	{
		pre = data.pre;
		post = data.post;
	}
	else
	{
		/* fill with default values (cf. timermanager.cpp) */
		pre  = 0;
		post = 0;
	}
}

//-------------------------------------------------------------------------
//void CTimerdClient::getWeekdaysFromStr(int *rep, const char* str)
void CTimerdClient::getWeekdaysFromStr(CTimerd::CTimerEventRepeat *eventRepeat, const char* str)
{
	int rep = (int) *eventRepeat;
	if(rep >= (int)CTimerd::TIMERREPEAT_WEEKDAYS)
	{
		for(int n=0;n<7;n++)
		{
			if(str[n]=='X' || str[n]=='x')
			{
				rep |= (1 << (n+9));
			}
			else
			{
				rep &= (~(1 << (n+9)));
			}
		}
	}
	*eventRepeat = (CTimerd::CTimerEventRepeat) rep;
}
//-------------------------------------------------------------------------
void CTimerdClient::setWeekdaysToStr(CTimerd::CTimerEventRepeat rep, char* str)
{
	if(rep >= CTimerd::TIMERREPEAT_WEEKDAYS)
	{
		for(int n=0;n<7;n++)
		{
			if(rep & (1 << (n+9)))
				str[n]='X';
			else
				str[n]='-';
		}
		str[7]=0;
	}
	else
		strcpy(str,"-------");
}
//-------------------------------------------------------------------------
void CTimerdClient::stopTimerEvent( int evId)
{
	CTimerdMsg::commandRemoveTimer msgRemoveTimer;
	VALGRIND_PARANOIA(msgRemoveTimer);

	msgRemoveTimer.eventID  = evId;

	send(CTimerdMsg::CMD_STOPTIMER, (char*) &msgRemoveTimer, sizeof(msgRemoveTimer));

	close_connection();  
}

