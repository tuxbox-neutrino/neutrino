/*
	Timer-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011-2012 Stefan Seyfried

   $Id: timermanager.cpp,v 1.86 2006/03/04 09:51:47 zwen Exp $

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
#include <config.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <sstream>

#include <timerdclient/timerdclient.h>
#include <timerdclient/timerdmsg.h>
#include <sectionsdclient/sectionsdclient.h>
#include <eitd/sectionsd.h>

#include <vector>
#include <cstdlib>

#include "debug.h"
#include "timermanager.h"


extern bool timeset;
time_t timer_minutes;
bool timer_is_rec;
static pthread_mutex_t tm_eventsMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

//------------------------------------------------------------
CTimerManager::CTimerManager()
{
	Init();
}

void CTimerManager::Init(void)
{
	eventID = 0;
	eventServer = new CEventServer;
	m_saveEvents = false;
	m_isTimeSet = false;
	timer_is_rec = false;
	wakeup = 0;
	loadRecordingSafety();

	//thread starten
	if(pthread_create (&thrTimer, NULL, timerThread, (void *) this) != 0 )
	{
		dprintf("CTimerManager::CTimerManager create timerThread failed\n");
	}
	dprintf("timermanager created\n");
}

//------------------------------------------------------------
CTimerManager* CTimerManager::getInstance()
{
	static CTimerManager *instance=NULL;
	if(!instance)
		instance = new CTimerManager;
	return instance;
}

//------------------------------------------------------------
void* CTimerManager::timerThread(void *arg)
{
	pthread_mutex_t dummy_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t dummy_cond = PTHREAD_COND_INITIALIZER;
	struct timespec wait;

	CTimerManager *timerManager = (CTimerManager*) arg;

	int sleeptime=(timerd_debug)?10:20;
	while(1)
	{
		if(!timerManager->m_isTimeSet)
		{ // time not set yet
			if (timeset)
			{
				dprintf("sectionsd says \"time ok\"\n");
				timerManager->m_isTimeSet=true;
				timerManager->loadEventsFromConfig();
			}
			else
			{
				dprintf("waiting for time to be set\n");
				wait.tv_sec = time(NULL) + 5 ;
				wait.tv_nsec = 0;
				pthread_cond_timedwait(&dummy_cond, &dummy_mutex, &wait);
			}
		}
		else
		{
			time_t now = time(NULL);
			dprintf("Timer Thread time: %u: %s", (uint) now, ctime(&now));

			// fire events who's time has come
			CTimerEvent *event;

			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL);
			pthread_mutex_lock(&tm_eventsMutex);

			CTimerEventMap::iterator pos = timerManager->events.begin();
			for(;pos != timerManager->events.end();++pos)
			{
				event = pos->second;
				dprintf("checking event: %03d\n",event->eventID);
				if (timerd_debug)
					event->printEvent();

				if(event->announceTime > 0 && event->eventState == CTimerd::TIMERSTATE_SCHEDULED ) // if event wants to be announced
					if( event->announceTime <= now )	// check if event announcetime has come
					{
						event->setState(CTimerd::TIMERSTATE_PREANNOUNCE);
						dprintf("announcing event\n");
						event->announceEvent();							// event specific announce handler
						timerManager->m_saveEvents = true;
					}

				if(event->alarmTime > 0 && (event->eventState == CTimerd::TIMERSTATE_SCHEDULED || event->eventState == CTimerd::TIMERSTATE_PREANNOUNCE) )	// if event wants to be fired
					if( event->alarmTime <= now )	// check if event alarmtime has come
					{
						event->setState(CTimerd::TIMERSTATE_ISRUNNING);
						dprintf("firing event\n");
						event->fireEvent();										// fire event specific handler
						if(event->stopTime == 0)					// if event needs no stop event
							event->setState(CTimerd::TIMERSTATE_HASFINISHED);
						timerManager->m_saveEvents = true;
					}

				if(event->stopTime > 0 && event->eventState == CTimerd::TIMERSTATE_ISRUNNING  )		// check if stopevent is wanted
					if( event->stopTime <= now ) // check if event stoptime has come
					{
						dprintf("stopping event\n");
						event->stopEvent();							//  event specific stop handler
						event->setState(CTimerd::TIMERSTATE_HASFINISHED);
						timerManager->m_saveEvents = true;
					}

				if(event->eventState == CTimerd::TIMERSTATE_HASFINISHED)
				{
					if((event->eventRepeat != CTimerd::TIMERREPEAT_ONCE) && (event->repeatCount != 1))
					{
						dprintf("rescheduling event\n");
						event->Reschedule();
					} else {
						dprintf("event terminated\n");
						event->setState(CTimerd::TIMERSTATE_TERMINATED);
					}
					timerManager->m_saveEvents = true;
				}

				if(event->eventState == CTimerd::TIMERSTATE_TERMINATED)				// event is terminated, so delete it
				{
					dprintf("deleting event\n");
					if (timerd_debug)
						pos->second->printEvent();
					dprintf("\n");
					delete pos->second;										// delete event
					timerManager->events.erase(pos++);				// remove from list
					timerManager->m_saveEvents = true;
					if(pos == timerManager->events.end())
						break;
				}
			}
			pthread_mutex_unlock(&tm_eventsMutex);

			// save events if requested
			if(timerManager->m_saveEvents)
			{
				timerManager->saveEventsToConfig();
			}
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);

			wait.tv_sec = (((time(NULL) / sleeptime) * sleeptime) + sleeptime);
			wait.tv_nsec = 0;
			pthread_cond_timedwait(&dummy_cond, &dummy_mutex, &wait);
		}
	}
	return 0;
}

//------------------------------------------------------------
CTimerEvent* CTimerManager::getNextEvent()
{
	pthread_mutex_lock(&tm_eventsMutex);
	CTimerEvent *erg = events[0];
	CTimerEventMap::iterator pos = events.begin();
	for(;pos!=events.end();++pos)
	{
		if(pos->second <= erg)
		{
			erg = pos->second;
		}
	}
	pthread_mutex_unlock(&tm_eventsMutex);
	return erg;
}

//------------------------------------------------------------
int CTimerManager::addEvent(CTimerEvent* evt, bool save)
{
	pthread_mutex_lock(&tm_eventsMutex);
	eventID++;						// increase unique event id
	evt->eventID = eventID;
	if(evt->eventRepeat==CTimerd::TIMERREPEAT_WEEKDAYS)
		// Weekdays without weekday specified reduce to once
		evt->eventRepeat=CTimerd::TIMERREPEAT_ONCE;
	events[eventID] = evt;			// insert into events
	m_saveEvents = m_saveEvents || save;
	if (timerd_debug)
	{
		dprintf("adding event:\n");
		evt->printEvent();
		dprintf("\n");
	}
	pthread_mutex_unlock(&tm_eventsMutex);
	return eventID;					// return unique id
}

//------------------------------------------------------------
bool CTimerManager::removeEvent(int peventID)
{
	bool res = false;
	pthread_mutex_lock(&tm_eventsMutex);
	if(events.find(peventID)!=events.end())							 // if i have a event with this id
	{
		if( (events[peventID]->eventState == CTimerd::TIMERSTATE_ISRUNNING) && (events[peventID]->stopTime > 0) )
			events[peventID]->stopEvent();	// if event is running an has stopTime

		events[peventID]->eventState = CTimerd::TIMERSTATE_TERMINATED;		// set the state to terminated
		res = true;															// so timerthread will do the rest for us
	}
	else
		res = false;
	pthread_mutex_unlock(&tm_eventsMutex);
	return res;
}
//------------------------------------------------------------
bool CTimerManager::stopEvent(int peventID)
{
	bool res = false;
	pthread_mutex_lock(&tm_eventsMutex);
	if(events.find(peventID)!=events.end())							 // if i have a event with this id
	{
		if( (events[peventID]->eventState == CTimerd::TIMERSTATE_ISRUNNING) && (events[peventID]->stopTime > 0) )
			events[peventID]->stopEvent();	// if event is running an has stopTime
		events[peventID]->eventState = CTimerd::TIMERSTATE_HASFINISHED;		// set the state to finished
		res = true;															// so timerthread will do the rest for us
	}
	else
		res = false;
	pthread_mutex_unlock(&tm_eventsMutex);
	return res;
}

//------------------------------------------------------------
bool CTimerManager::listEvents(CTimerEventMap &Events)
{
	if(!&Events)
		return false;

	pthread_mutex_lock(&tm_eventsMutex);

	Events.clear();
	for (CTimerEventMap::iterator pos = events.begin(); pos != events.end(); ++pos)
	{
		pos->second->Refresh();
		Events[pos->second->eventID] = pos->second;
	}
	pthread_mutex_unlock(&tm_eventsMutex);
	return true;
}
//------------------------------------------------------------

CTimerd::CTimerEventTypes* CTimerManager::getEventType(int peventID)
{
	CTimerd::CTimerEventTypes *res = NULL;
	pthread_mutex_lock(&tm_eventsMutex);
	if(events.find(peventID)!=events.end())
	{
		res = &(events[peventID]->eventType);
	}
	else
		res = NULL;
	pthread_mutex_unlock(&tm_eventsMutex);
	return res;
}
//------------------------------------------------------------

int CTimerManager::modifyEvent(int peventID, time_t announceTime, time_t alarmTime, time_t stopTime, uint32_t repeatCount, CTimerd::CTimerEventRepeat evrepeat, CTimerd::responseGetTimer& data)
{
	int res = 0;
	pthread_mutex_lock(&tm_eventsMutex);
	if(events.find(peventID)!=events.end())
	{
		CTimerEvent *event = events[peventID];
		event->announceTime = announceTime;
		event->alarmTime = alarmTime;
		event->stopTime = stopTime;
		if(event->eventState==CTimerd::TIMERSTATE_PREANNOUNCE)
			event->eventState = CTimerd::TIMERSTATE_SCHEDULED;
		event->eventRepeat = evrepeat;
		if(event->eventRepeat==CTimerd::TIMERREPEAT_WEEKDAYS)
			// Weekdays without weekday specified reduce to once
			event->eventRepeat=CTimerd::TIMERREPEAT_ONCE;
		event->repeatCount = repeatCount;
		switch (event->eventType)
		{
			case CTimerd::TIMER_SHUTDOWN:
			case CTimerd::TIMER_NEXTPROGRAM:
			case CTimerd::TIMER_STANDBY:
			case CTimerd::TIMER_REMIND:
			case CTimerd::TIMER_SLEEPTIMER:
			case CTimerd::TIMER_EXEC_PLUGIN:
			case CTimerd::TIMER_IMMEDIATE_RECORD:
				break;
			case CTimerd::TIMER_RECORD:
			{
				(static_cast<CTimerEvent_Record*>(event))->recordingDir = data.recordingDir;
				(static_cast<CTimerEvent_Record*>(event))->getEpgId();
				break;
			}
			case CTimerd::TIMER_ZAPTO:
			{
				(static_cast<CTimerEvent_Zapto*>(event))->getEpgId(); 
				break;
			}
			default:
				break;
		}

		m_saveEvents=true;
		res = peventID;
	}
	else
		res = 0;
	pthread_mutex_unlock(&tm_eventsMutex);
	return res;
}

int CTimerManager::modifyEvent(int peventID, unsigned char apids)
{
	dprintf("Modify Event %d apid 0x%X\n",peventID,apids);
	int res = 0;
	pthread_mutex_lock(&tm_eventsMutex);
	if(events.find(peventID)!=events.end())
	{
		CTimerEvent *event = events[peventID];
		if(event->eventType == CTimerd::TIMER_RECORD)
		{
			((CTimerEvent_Record*) (event))->eventInfo.apids = apids;
			m_saveEvents=true;
			res = eventID;
		}
		else if(event->eventType == CTimerd::TIMER_ZAPTO)
		{
			((CTimerEvent_Zapto*) (event))->eventInfo.apids = apids;
			m_saveEvents=true;
			res = peventID;
		}
	}
	pthread_mutex_unlock(&tm_eventsMutex);
	return res;
}

int CTimerManager::rescheduleEvent(int peventID, time_t announceTime, time_t alarmTime, time_t stopTime)
{
	int res = 0;
	pthread_mutex_lock(&tm_eventsMutex);

	if(events.find(peventID)!=events.end())
	{
		CTimerEvent *event = events[peventID];
		if(event->announceTime > 0)
			event->announceTime += announceTime;
		if(event->alarmTime > 0)
			event->alarmTime += alarmTime;
		if(event->stopTime > 0)
			event->stopTime += stopTime;
		event->eventState = CTimerd::TIMERSTATE_SCHEDULED;
		m_saveEvents=true;
		res = peventID;
	}
	else
		res = 0;
	pthread_mutex_unlock(&tm_eventsMutex);
	return res;
}
// ---------------------------------------------------------------------------------
void CTimerManager::loadEventsFromConfig()
{
	CConfigFile config(',');

	if(!config.loadConfig(CONFIGFILE))
	{
		/* set defaults if no configuration file exists */
		dprintf("%s not found\n", CONFIGFILE);
	}
	else
	{
		std::vector<int> savedIDs;
		savedIDs = config.getInt32Vector ("IDS");
		dprintf("%d timer(s) in config\n", (int)savedIDs.size());
		for(unsigned int i=0; i < savedIDs.size(); i++)
		{
			std::stringstream ostr;
			ostr << savedIDs[i];
			std::string id=ostr.str();
			CTimerd::CTimerEventTypes type=(CTimerd::CTimerEventTypes)config.getInt32 ("EVENT_TYPE_"+id,0);
			dprintf("loading timer %d, id %s, EVENT_TYPE %d\n",i,id.c_str(),type);
			time_t now = time(NULL);
			switch(type)
			{
				case CTimerd::TIMER_SHUTDOWN :
					{
						CTimerEvent_Shutdown *event=
						new CTimerEvent_Shutdown(&config, savedIDs[i]);
						if((event->alarmTime >= now) || (event->stopTime > now))
						{
							addEvent(event,false);
						}
						else if(event->eventRepeat != CTimerd::TIMERREPEAT_ONCE)
						{
							// old periodic timers need to be rescheduled
							event->eventState = CTimerd::TIMERSTATE_HASFINISHED;
							addEvent(event,false);
						}
						else
						{
							dprintf("shutdown timer (%d) too old %d/%d\n",
								i,(int)now,(int) event->alarmTime);
							delete event;
						}
						break;
					}
				case CTimerd::TIMER_NEXTPROGRAM :
					{
						CTimerEvent_NextProgram *event=
						new CTimerEvent_NextProgram(&config, savedIDs[i]);
						if((event->alarmTime >= now) || (event->stopTime > now))
						{
							addEvent(event,false);
						}
						else if(event->eventRepeat != CTimerd::TIMERREPEAT_ONCE)
						{
							// old periodic timers need to be rescheduled
							event->eventState = CTimerd::TIMERSTATE_HASFINISHED;
							addEvent(event,false);
						}
						else
						{
							dprintf("next program timer (%d) too old %d/%d\n",i,
								(int)now,(int) event->alarmTime);
							delete event;
						}
						break;
					}
				case CTimerd::TIMER_ZAPTO :
					{
						CTimerEvent_Zapto *event=
						new CTimerEvent_Zapto(&config, savedIDs[i]);
						if((event->alarmTime >= now) || (event->stopTime > now))
						{
							addEvent(event,false);
						}
						else if(event->eventRepeat != CTimerd::TIMERREPEAT_ONCE)
						{
							// old periodic timers need to be rescheduled
							event->eventState = CTimerd::TIMERSTATE_HASFINISHED;
							addEvent(event,false);
						}
						else
						{
							dprintf("zapto timer (%d) too old %d/%d\n",
								i,(int)now,(int) event->alarmTime);
							delete event;
						}
						break;
					}
				case CTimerd::TIMER_STANDBY :
					{
						CTimerEvent_Standby *event=
						new CTimerEvent_Standby(&config, savedIDs[i]);
						if((event->alarmTime >= now) || (event->stopTime > now))
						{
							addEvent(event,false);
						}
						else if(event->eventRepeat != CTimerd::TIMERREPEAT_ONCE)
						{
							// old periodic timers need to be rescheduled
							event->eventState = CTimerd::TIMERSTATE_HASFINISHED;
							addEvent(event,false);
						}
						else
						{
							dprintf("standby timer (%d) too old %d/%d\n",
								i,(int)now,(int) event->alarmTime);
							delete event;
						}
						break;
					}
				case CTimerd::TIMER_RECORD :
					{
						CTimerEvent_Record *event=
						new CTimerEvent_Record(&config, savedIDs[i]);
						if((event->alarmTime >= now) || (event->stopTime > now))
						{
							addEvent(event,false);
						}
						else if(event->eventRepeat != CTimerd::TIMERREPEAT_ONCE)
						{
							// old periodic timers need to be rescheduled
							event->eventState = CTimerd::TIMERSTATE_HASFINISHED;
							addEvent(event,false);
						}
						else
						{
							dprintf("record timer (%d) too old %d/%d\n",
								i,(int)now,(int) event->alarmTime);
							delete event;
						}
						break;
					}
				case CTimerd::TIMER_SLEEPTIMER :
					{
						CTimerEvent_Sleeptimer *event=
						new CTimerEvent_Sleeptimer(&config, savedIDs[i]);
						if((event->alarmTime >= now) || (event->stopTime > now))
						{
							addEvent(event,false);
						}
						else if(event->eventRepeat != CTimerd::TIMERREPEAT_ONCE)
						{
							// old periodic timers need to be rescheduled
							event->eventState = CTimerd::TIMERSTATE_HASFINISHED;
							addEvent(event,false);
						}
						else
						{
							dprintf("sleep timer (%d) too old %d/%d\n",
								i,(int)now,(int) event->alarmTime);
							delete event;
						}
						break;
					}
				case CTimerd::TIMER_REMIND :
					{
						CTimerEvent_Remind *event=
						new CTimerEvent_Remind(&config, savedIDs[i]);
						if((event->alarmTime >= now) || (event->stopTime > now))
						{
							addEvent(event,false);
						}
						else if(event->eventRepeat != CTimerd::TIMERREPEAT_ONCE)
						{
							// old periodic timers need to be rescheduled
							event->eventState = CTimerd::TIMERSTATE_HASFINISHED;
							addEvent(event,false);
						}
						else
						{
							dprintf("remind timer (%d) too old %d/%d\n",
								i,(int)now,(int) event->alarmTime);
							delete event;
						}
						break;
					}
				case CTimerd::TIMER_EXEC_PLUGIN :
					{
						CTimerEvent_ExecPlugin *event=
						new CTimerEvent_ExecPlugin(&config, savedIDs[i]);
						if((event->alarmTime >= now) || (event->stopTime > now))
						{
							addEvent(event,false);
						}
						else if(event->eventRepeat != CTimerd::TIMERREPEAT_ONCE)
						{
							// old periodic timers need to be rescheduled
							event->eventState = CTimerd::TIMERSTATE_HASFINISHED;
							addEvent(event,false);
						}
						else
						{
							dprintf("exec plugin timer (%d) too old %d/%d\n",
								i,(int)now,(int) event->alarmTime);
							delete event;
						}
						break;
					}
				default:
					printf("Unknown timer on load %d\n",type);
			}
		}
	}
	saveEventsToConfig();
}
// -------------------------------------------------------------------------------------
void CTimerManager::loadRecordingSafety()
{
	CConfigFile config(',');

	if(!config.loadConfig(CONFIGFILE))
	{
		/* set defaults if no configuration file exists */
		dprintf("%s not found\n", CONFIGFILE);
		m_extraTimeStart = 300;
		m_extraTimeEnd = 300;
		config.saveConfig(CONFIGFILE);
	}
	else
	{
		m_extraTimeStart = config.getInt32 ("EXTRA_TIME_START",0);
		m_extraTimeEnd  = config.getInt32 ("EXTRA_TIME_END",0);
	}
}
// -------------------------------------------------------------------------------------
void CTimerManager::saveEventsToConfig()
{
	pthread_mutex_lock(&tm_eventsMutex);

	// Sperren !!!
	CConfigFile config(',');
	config.clear();
	dprintf("save %d events to config ...\n", (int)events.size());
	CTimerEventMap::iterator pos = events.begin();
	for(;pos != events.end();++pos)
	{
		CTimerEvent *event = pos->second;
		dprintf("event #%d\n",event->eventID);
		event->saveToConfig(&config);
	}
	dprintf("\n");
	config.setInt32 ("EXTRA_TIME_START", m_extraTimeStart);
	dprintf("setting EXTRA_TIME_START to %d\n",m_extraTimeStart);
	config.setInt32 ("EXTRA_TIME_END", m_extraTimeEnd);
	dprintf("setting EXTRA_TIME_END to %d\n",m_extraTimeEnd);
	dprintf("now saving config to %s...\n",CONFIGFILE);
	config.saveConfig(CONFIGFILE);
	dprintf("config saved!\n");
	m_saveEvents=false;

	// Freigeben !!!
	pthread_mutex_unlock(&tm_eventsMutex);
}
//------------------------------------------------------------
bool CTimerManager::shutdown()
{
	timerd_debug = 1; //FIXME
	time_t nextAnnounceTime=0;
	bool status=false;
	timer_is_rec = false;

	dprintf("stopping timermanager thread ...\n");

	dprintf("Waiting for timermanager thread to terminate ...\n");
	pthread_cancel(thrTimer);
	pthread_join(thrTimer,NULL);
	dprintf("Timermanager thread terminated\n");

	if(m_saveEvents)
	{
		dprintf("shutdown: saving config\n");
		saveEventsToConfig();
		dprintf("shutdown: saved config\n");
	}
	if (pthread_mutex_trylock(&tm_eventsMutex) == EBUSY)
	{
		dprintf("error: mutex is still LOCKED\n");
		return false;
	}

	CTimerEventMap::iterator pos = events.begin();
	for(;pos != events.end();++pos)
	{
		CTimerEvent *event = pos->second;
		dprintf("shutdown: timer type %d state %d announceTime: %ld\n", event->eventType, event->eventState, event->announceTime);
		if((event->eventType == CTimerd::TIMER_RECORD ||
			 event->eventType == CTimerd::TIMER_ZAPTO ) &&
			event->eventState < CTimerd::TIMERSTATE_ISRUNNING)
		{
			// Wir wachen nur für Records und Zaptos wieder auf
			if(event->announceTime < nextAnnounceTime || nextAnnounceTime==0)
			{
				nextAnnounceTime=event->announceTime;
				dprintf("shutdown: nextAnnounceTime %ld\n", nextAnnounceTime);
				if ( event->eventType == CTimerd::TIMER_RECORD )
					timer_is_rec = true;
				else
					timer_is_rec = false;
			}
		}
	}

	timer_minutes = 0;
	if(nextAnnounceTime != 0)
	{
		timer_minutes = (nextAnnounceTime - 3*60)/60;
	}
	dprintf("shutdown: timeset: %d timer_minutes %ld\n", timeset, timer_minutes);

	pthread_mutex_unlock(&tm_eventsMutex);
	return status;
}
//------------------------------------------------------------
void CTimerManager::shutdownOnWakeup(int currEventID)
{
	time_t nextAnnounceTime=0;

	pthread_mutex_lock(&tm_eventsMutex);
	if(wakeup == 0) {
		pthread_mutex_unlock(&tm_eventsMutex);
		return;
	}

	CTimerEventMap::iterator pos = events.begin();
	for(;pos != events.end();++pos)
	{
		CTimerEvent *event = pos->second;
		if((event->eventType == CTimerd::TIMER_RECORD ||
		    event->eventType == CTimerd::TIMER_ZAPTO ) &&
		   (event->eventState == CTimerd::TIMERSTATE_SCHEDULED ||
		    event->eventState == CTimerd::TIMERSTATE_PREANNOUNCE ||
		    event->eventState == CTimerd::TIMERSTATE_ISRUNNING) &&
			event->eventID != currEventID)
		{
			// Bei anstehendem/laufendem RECORD oder ZAPTO Timer nicht runterfahren
			if(event->announceTime < nextAnnounceTime || nextAnnounceTime==0)
			{
				nextAnnounceTime=event->announceTime;
			}
		}
	}
	time_t now = time(NULL);
	if((nextAnnounceTime-now) > 600 || nextAnnounceTime==0)
	{ // in den naechsten 10 min steht nix an
		dprintf("Programming shutdown event\n");
		CTimerEvent_Shutdown* event = new CTimerEvent_Shutdown(now+120, now+180);
		shutdown_eventID = addEvent(event);
		wakeup = 0;
	}
	pthread_mutex_unlock(&tm_eventsMutex);
}

void CTimerManager::cancelShutdownOnWakeup()
{
	pthread_mutex_lock(&tm_eventsMutex);
	if (shutdown_eventID > -1) {
		removeEvent(shutdown_eventID);
		shutdown_eventID = -1;
	}
	wakeup = 0;
	pthread_mutex_unlock(&tm_eventsMutex);
}

void CTimerManager::setRecordingSafety(int pre, int post)
{
	m_extraTimeStart=pre;
	m_extraTimeEnd=post;
   m_saveEvents=true; // also saves extra times
}
//------------------------------------------------------------
//=============================================================
// event functions
//=============================================================
//------------------------------------------------------------
CTimerEvent::CTimerEvent( CTimerd::CTimerEventTypes evtype, time_t announcetime, time_t alarmtime, time_t stoptime, CTimerd::CTimerEventRepeat evrepeat, uint32_t repeatcount)
{
	eventRepeat = evrepeat;
	eventState = CTimerd::TIMERSTATE_SCHEDULED;
	eventType = evtype;
	announceTime = announcetime;
	alarmTime = alarmtime;
	stopTime = stoptime;
	repeatCount = repeatcount;
	previousState = CTimerd::TIMERSTATE_SCHEDULED;

	time_t diff = 0;
	time_t mtime = time(NULL);
	struct tm *tmtime = localtime(&mtime);
	int isdst1 = tmtime->tm_isdst;

	mtime = alarmtime;
	tmtime = localtime(&mtime);
	int isdst2 = tmtime->tm_isdst;

	if(isdst2 > isdst1) //change from winter to summer
	{
		diff-=3600;
	}
	else if(isdst1 > isdst2) //change from summer to winter
	{
		diff+=3600;
	}
#if 0 //FIXME EPG vs manual timer ?
	printf("############## CTimerEvent dst %d -> %d diff %d\n", isdst1, isdst2, diff);
	alarmTime += diff;
	announceTime += diff;
	stopTime += diff;
#endif
}

//------------------------------------------------------------
CTimerEvent::CTimerEvent( CTimerd::CTimerEventTypes evtype, int mon, int day, int hour, int min, CTimerd::CTimerEventRepeat evrepeat, uint32_t repeatcount)
{

	time_t mtime = time(NULL);
	struct tm *tmtime = localtime(&mtime);

	if(mon > 0)
		tmtime->tm_mon = mon -1;
	if(day > 0)
		tmtime->tm_mday = day;
	tmtime->tm_hour = hour;
	tmtime->tm_min = min;
	CTimerEvent(evtype, (time_t) 0, mktime(tmtime), (time_t)0, evrepeat, repeatcount);
}
//------------------------------------------------------------
CTimerEvent::CTimerEvent(CTimerd::CTimerEventTypes evtype,CConfigFile *config, int iId)
{
	dprintf("CTimerEvent: constructor with config\n");
	std::stringstream ostr;
	ostr << iId;
	std::string id=ostr.str();
	dprintf("timer id: int %d string %s\n",iId,id.c_str());
	time_t announcetime=config->getInt32("ANNOUNCE_TIME_"+id);
	dprintf("read ANNOUNCE_TIME_%s %ld\n",id.c_str(),(long)announcetime);
	time_t alarmtime=config->getInt32("ALARM_TIME_"+id);
	dprintf("read ALARM_TIME_%s %ld\n",id.c_str(),(long)alarmtime);
	time_t stoptime=config->getInt32("STOP_TIME_"+id);
	dprintf("read STOP_TIME_%s %ld\n",id.c_str(),(long)stoptime);
	CTimerd::CTimerEventRepeat evrepeat=(CTimerd::CTimerEventRepeat)config->getInt32("EVENT_REPEAT_"+id);
	dprintf("read EVENT_REPEAT_%s %d\n",id.c_str(),evrepeat);
	uint32_t repeatcount = config->getInt32("REPEAT_COUNT_"+id);
	dprintf("read REPEAT_COUNT_%s %d\n",id.c_str(),repeatcount);
	eventRepeat = evrepeat;
	eventState = CTimerd::TIMERSTATE_SCHEDULED;
	eventType = evtype;
	announceTime = announcetime;
	alarmTime = alarmtime;
	stopTime = stoptime;
	repeatCount = repeatcount;
	eventState = (CTimerd::CTimerEventStates ) config->getInt32 ("EVENT_STATE_"+id);
	dprintf("read EVENT_STATE_%s %d\n",id.c_str(),eventState);

	previousState = (CTimerd::CTimerEventStates) config->getInt32("PREVIOUS_STATE_"+id);
	dprintf("read PREVIOUS_STATE_%s %d\n",id.c_str(),previousState);
}
//------------------------------------------------------------
void CTimerEvent::Reschedule()
{
	if(eventRepeat == CTimerd::TIMERREPEAT_ONCE)
	{
		eventState = CTimerd::TIMERSTATE_TERMINATED;
		dprintf("event %d not rescheduled, event will be terminated\n",eventID);
	}
	else
	{
		time_t now = time(NULL);
		while(alarmTime <= now)
		{
			time_t diff = 0;
			struct tm *t= localtime(&alarmTime);
			int isdst1=t->tm_isdst;
			switch(eventRepeat)
			{
				case CTimerd::TIMERREPEAT_ONCE :
					break;
				case CTimerd::TIMERREPEAT_DAILY:
					t->tm_mday++;
				break;
				case CTimerd::TIMERREPEAT_WEEKLY:
					t->tm_mday+=7;
				break;
				case CTimerd::TIMERREPEAT_BIWEEKLY:
					t->tm_mday+=14;
				break;
				case CTimerd::TIMERREPEAT_FOURWEEKLY:
					t->tm_mday+=28;
				break;
				case CTimerd::TIMERREPEAT_MONTHLY:
					t->tm_mon++;
				break;
				case CTimerd::TIMERREPEAT_BYEVENTDESCRIPTION :
					// todo !!
					break;
				default:
					if(eventRepeat >= CTimerd::TIMERREPEAT_WEEKDAYS)
					{
						int weekdays = ((int)eventRepeat) >> 9;
						if(weekdays > 0)
						{
							bool weekday_arr[7];
							weekday_arr[0]=((weekdays & 0x40) > 0); //So
							weekday_arr[1]=((weekdays & 0x1) > 0); //Mo
							weekday_arr[2]=((weekdays & 0x2) > 0); //Di
							weekday_arr[3]=((weekdays & 0x4) > 0); //Mi
							weekday_arr[4]=((weekdays & 0x8) > 0); //Do
							weekday_arr[5]=((weekdays & 0x10) > 0); //Fr
							weekday_arr[6]=((weekdays & 0x20) > 0); //Sa
							struct tm *lt= localtime(&alarmTime);
							int day;
							for(day=1 ; !weekday_arr[(lt->tm_wday+day)%7] ; day++){}
							lt->tm_mday+=day;
						}
					}
					else
						dprintf("unknown repeat type %d\n",eventRepeat);
			}
			diff = mktime(t)-alarmTime;
			alarmTime += diff;
			t = localtime(&alarmTime);
			int isdst2 = t->tm_isdst;
			if(isdst2 > isdst1) //change from winter to summer
			{
				diff-=3600;
				alarmTime-=3600;
			}
			else if(isdst1 > isdst2) //change from summer to winter
			{
				diff+=3600;
				alarmTime+=3600;
			}
			if(announceTime > 0)
				announceTime += diff;
			if(stopTime > 0)
				stopTime += diff;
		}
		eventState = CTimerd::TIMERSTATE_SCHEDULED;
		if (repeatCount > 0)
			repeatCount -= 1;
		dprintf("event %d rescheduled\n",eventID);
	}
}


//------------------------------------------------------------
void CTimerEvent::printEvent(void)
{
	struct tm *alarmtime, *announcetime;
	dprintf("eventID: %03d type: %d state: %d repeat: %d ,repeatCount %d\n",eventID,eventType,eventState,((int)eventRepeat)&0x1FF,repeatCount);
	announcetime = localtime(&announceTime);
	dprintf(" announce: %u %02d.%02d. %02d:%02d:%02d\n",(uint) announceTime,announcetime->tm_mday,announcetime->tm_mon+1,announcetime->tm_hour,announcetime->tm_min,announcetime->tm_sec);
	alarmtime = localtime(&alarmTime);
	dprintf(" alarm: %u %02d.%02d. %02d:%02d:%02d\n",(uint) alarmTime,alarmtime->tm_mday,alarmtime->tm_mon+1,alarmtime->tm_hour,alarmtime->tm_min,alarmtime->tm_sec);
	switch(eventType)
	{
		case CTimerd::TIMER_ZAPTO :
			dprintf(" Zapto: "
				PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				" epg: %s (%" PRIx64 ")\n",
				static_cast<CTimerEvent_Zapto*>(this)->eventInfo.channel_id,
				static_cast<CTimerEvent_Zapto*>(this)->epgTitle.c_str(),
				static_cast<CTimerEvent_Zapto*>(this)->eventInfo.epgID);
			break;

		case CTimerd::TIMER_RECORD :
			dprintf(" Record: "
				PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				" epg: %s(%" PRIx64 ") apids: 0x%X\n dir: %s\n",
					  static_cast<CTimerEvent_Record*>(this)->eventInfo.channel_id,
					  static_cast<CTimerEvent_Record*>(this)->epgTitle.c_str(),
					  static_cast<CTimerEvent_Record*>(this)->eventInfo.epgID,
					  static_cast<CTimerEvent_Record*>(this)->eventInfo.apids,
					  static_cast<CTimerEvent_Record*>(this)->recordingDir.c_str());
			break;

		case CTimerd::TIMER_STANDBY :
			dprintf(" standby: %s\n",(static_cast<CTimerEvent_Standby*>(this)->standby_on == 1)?"on":"off");
			break;

		default:
			dprintf(" (no extra data)\n");
	}
	dprintf("\n");
}
//------------------------------------------------------------
void CTimerEvent::saveToConfig(CConfigFile *config)
{
	dprintf("CTimerEvent::saveToConfig\n");
	std::vector<int> allIDs;
	allIDs.clear();
	if (!(config->getString("IDS").empty()))
	{
		// sonst bekommen wir den bloeden 0er
		allIDs=config->getInt32Vector("IDS");
	}

	allIDs.push_back(eventID);
	dprintf("adding %d to IDS\n",eventID);
	//SetInt-Vector haengt komischerweise nur an, deswegen erst loeschen
	config->setString("IDS","");
	config->setInt32Vector ("IDS",allIDs);

	std::stringstream ostr;
	ostr << eventID;
	std::string id=ostr.str();
	config->setInt32("EVENT_TYPE_"+id, eventType);
	dprintf("set EVENT_TYPE_%s to %d\n",id.c_str(),eventType);

	config->setInt32("EVENT_STATE_"+id, eventState);
	dprintf("set EVENT_STATE_%s to %d\n",id.c_str(),eventState);

	config->setInt32("PREVIOUS_STATE_"+id, previousState);
	dprintf("set PREVIOUS_STATE_%s to %d\n",id.c_str(),previousState);

	config->setInt32("EVENT_REPEAT_"+id, eventRepeat);
	dprintf("set EVENT_REPEAT_%s to %d\n",id.c_str(),eventRepeat);

	config->setInt32("ANNOUNCE_TIME_"+id, announceTime);
	dprintf("set ANNOUNCE_TIME_%s to %ld\n",id.c_str(),(long)announceTime);

	config->setInt32("ALARM_TIME_"+id, alarmTime);
	dprintf("set ALARM_TIME_%s to %ld\n",id.c_str(),(long)alarmTime);

	config->setInt32("STOP_TIME_"+id, stopTime);
	dprintf("set STOP_TIME_%s to %ld\n",id.c_str(),(long)stopTime);

	config->setInt32("REPEAT_COUNT_"+id,repeatCount);
	dprintf("set REPEAT_COUNT_%s to %d\n",id.c_str(),repeatCount);
}

//=============================================================
// Shutdown Event
//=============================================================
void CTimerEvent_Shutdown::announceEvent()
{
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_ANNOUNCE_SHUTDOWN,
								  CEventServer::INITID_TIMERD);
}
//------------------------------------------------------------
void CTimerEvent_Shutdown::fireEvent()
{
	dprintf("Shutdown Timer fired\n");
	//event in neutrinos remoteq. schreiben
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_SHUTDOWN,
								  CEventServer::INITID_TIMERD);
}
//=============================================================
// Sleeptimer Event
//=============================================================
void CTimerEvent_Sleeptimer::announceEvent()
{
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_ANNOUNCE_SLEEPTIMER,
								  CEventServer::INITID_TIMERD);
}
//------------------------------------------------------------
void CTimerEvent_Sleeptimer::fireEvent()
{
	dprintf("Sleeptimer Timer fired\n");
	//event in neutrinos remoteq. schreiben
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_SLEEPTIMER,
								  CEventServer::INITID_TIMERD);
}
//=============================================================
// Standby Event
//=============================================================
CTimerEvent_Standby::CTimerEvent_Standby( time_t announce_Time, time_t alarm_Time,
					  bool sb_on,
					  CTimerd::CTimerEventRepeat evrepeat,
					  uint32_t repeatcount):
	CTimerEvent(CTimerd::TIMER_STANDBY, announce_Time, alarm_Time, (time_t) 0, evrepeat,repeatcount)
{
	standby_on = sb_on;
}
CTimerEvent_Standby::CTimerEvent_Standby(CConfigFile *config, int iId):
CTimerEvent(CTimerd::TIMER_STANDBY, config, iId)
{
	std::stringstream ostr;
	ostr << iId;
	std::string id=ostr.str();
	standby_on = config->getBool("STANDBY_ON_"+id);
	dprintf("read STANDBY_ON_%s %d\n",id.c_str(),standby_on);
}
//------------------------------------------------------------
void CTimerEvent_Standby::fireEvent()
{
	dprintf("Standby Timer fired: %s\n",standby_on?"on":"off");
	CTimerManager::getInstance()->getEventServer()->sendEvent(
		(standby_on)?CTimerdClient::EVT_STANDBY_ON:CTimerdClient::EVT_STANDBY_OFF,
		CEventServer::INITID_TIMERD);
}

//------------------------------------------------------------
void CTimerEvent_Standby::saveToConfig(CConfigFile *config)
{
	CTimerEvent::saveToConfig(config);
	std::stringstream ostr;
	ostr << eventID;
	std::string id=ostr.str();
	config->setBool("STANDBY_ON_"+id,standby_on);
	dprintf("set STANDBY_ON_%s to %d\n",id.c_str(),standby_on);
}
//=============================================================
// Record Event
//=============================================================
CTimerEvent_Record::CTimerEvent_Record(time_t announce_Time, time_t alarm_Time, time_t stop_Time,
				       t_channel_id channel_id,
				       event_id_t epgID,
				       time_t epg_starttime, unsigned char apids,
				       CTimerd::CTimerEventRepeat evrepeat,
				       uint32_t repeatcount, const std::string &recDir) :
	CTimerEvent(getEventType(), announce_Time, alarm_Time, stop_Time, evrepeat, repeatcount)
{
	eventInfo.epgID = epgID;
	eventInfo.epg_starttime = epg_starttime;
	eventInfo.channel_id = channel_id;
	eventInfo.apids = apids;
	recordingDir = recDir;
	epgTitle="";
	CShortEPGData epgdata;
	if (CEitManager::getInstance()->getEPGidShort(epgID, &epgdata))
		epgTitle=epgdata.title;

}
//------------------------------------------------------------
CTimerEvent_Record::CTimerEvent_Record(CConfigFile *config, int iId):
	CTimerEvent(getEventType(), config, iId)
{
	std::stringstream ostr;
	ostr << iId;
	std::string id=ostr.str();
	eventInfo.epgID = config->getInt64("EVENT_INFO_EPG_ID_"+id);
	dprintf("read EVENT_INFO_EPG_ID_%s %ld\n",id.c_str(),(long)eventInfo.epgID);

	eventInfo.epg_starttime = config->getInt64("EVENT_INFO_EPG_STARTTIME_"+id);
	dprintf("read EVENT_INFO_EPG_STARTTIME_%s %ld\n",id.c_str(),(long)eventInfo.epg_starttime);

	eventInfo.channel_id = config->getInt64("EVENT_INFO_CHANNEL_ID_"+id);
	dprintf("read EVENT_INFO_CHANNEL_ID_%s %ld\n",id.c_str(),(long)eventInfo.channel_id);

	eventInfo.apids = config->getInt32("EVENT_INFO_APIDS_"+id);
	dprintf("read EVENT_INFO_APIDS_%s 0x%X (%p)\n",id.c_str(),eventInfo.apids,&eventInfo.apids);

	recordingDir = config->getString("REC_DIR_"+id);
	dprintf("read REC_DIR_%s %s (%p)\n",id.c_str(),recordingDir.c_str(),&recordingDir);

	epgTitle = config->getString("EPG_TITLE_"+id);
	dprintf("read EPG_TITLE_%s %s (%p)\n",id.c_str(),epgTitle.c_str(),&epgTitle);
}
//------------------------------------------------------------
void CTimerEvent_Record::fireEvent()
{
	Refresh();
	CTimerd::RecordingInfo ri=eventInfo;
	ri.eventID=eventID;
	strcpy(ri.recordingDir, recordingDir.substr(0,sizeof(ri.recordingDir)-1).c_str());
	strcpy(ri.epgTitle, epgTitle.substr(0,sizeof(ri.epgTitle)-1).c_str());
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_RECORD_START,
								  CEventServer::INITID_TIMERD,
								  &ri,
								  sizeof(CTimerd::RecordingInfo));
	dprintf("Record Timer fired\n");
}
//------------------------------------------------------------
void CTimerEvent_Record::announceEvent()
{
	Refresh();
	CTimerd::RecordingInfo ri=eventInfo;
	ri.eventID=eventID;
	strcpy(ri.recordingDir, recordingDir.substr(0,sizeof(ri.recordingDir)-1).c_str());
	strcpy(ri.epgTitle, epgTitle.substr(0,sizeof(ri.epgTitle)-1).c_str());
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_ANNOUNCE_RECORD, CEventServer::INITID_TIMERD,
								  &ri,sizeof(CTimerd::RecordingInfo));
	dprintf("Record announcement\n");
}
//------------------------------------------------------------
void CTimerEvent_Record::stopEvent()
{
	CTimerd::RecordingStopInfo stopinfo;
	// Set EPG-ID if not set
	stopinfo.eventID = eventID;
	stopinfo.channel_id = eventInfo.channel_id;
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_RECORD_STOP,
								  CEventServer::INITID_TIMERD,
								  &stopinfo,
								  sizeof(CTimerd::RecordingStopInfo));
	// Programmiere shutdown timer, wenn in wakeup state und kein record/zapto timer in 10 min
	CTimerManager::getInstance()->shutdownOnWakeup(eventID);
	dprintf("Recording stopped\n");
}
//------------------------------------------------------------
void CTimerEvent_Record::saveToConfig(CConfigFile *config)
{
	CTimerEvent::saveToConfig(config);
	std::stringstream ostr;
	ostr << eventID;
	std::string id=ostr.str();
	config->setInt64("EVENT_INFO_EPG_ID_"+id, eventInfo.epgID);
	dprintf("set EVENT_INFO_EPG_ID_%s to %ld\n",id.c_str(),(long)eventInfo.epgID);

	config->setInt64("EVENT_INFO_EPG_STARTTIME_"+id, eventInfo.epg_starttime);
	dprintf("set EVENT_INFO_EPG_STARTTIME_%s to %ld\n",id.c_str(),(long)eventInfo.epg_starttime);

	config->setInt64("EVENT_INFO_CHANNEL_ID_"+id, eventInfo.channel_id);
	dprintf("set EVENT_INFO_CHANNEL_ID_%s to %ld\n",id.c_str(),(long)eventInfo.channel_id);

	config->setInt64("EVENT_INFO_APIDS_"+id, eventInfo.apids);
	dprintf("set EVENT_INFO_APIDS_%s to 0x%X (%p)\n",id.c_str(),eventInfo.apids,&eventInfo.apids);

	config->setString("REC_DIR_"+id,recordingDir);
	dprintf("set REC_DIR_%s to %s (%p)\n",id.c_str(),recordingDir.c_str(), &recordingDir);

	config->setString("EPG_TITLE_"+id,epgTitle);
	dprintf("set EPG_TITLE_%s to %s (%p)\n",id.c_str(),epgTitle.c_str(), &epgTitle);
}
//------------------------------------------------------------
void CTimerEvent_Record::Reschedule()
{
	// clear epgId on reschedule
	eventInfo.epgID = 0;
	eventInfo.epg_starttime = 0;
	epgTitle="";
	CTimerEvent::Reschedule();
	getEpgId();
}
//------------------------------------------------------------
void CTimerEvent_Record::getEpgId()
{
	//TODO: Record/Zapto getEpgId code almost identical !
	CChannelEventList evtlist;
	CEitManager::getInstance()->getEventsServiceKey(eventInfo.channel_id, evtlist);
	// we check for a time in the middle of the recording
	time_t check_time=alarmTime/2 + stopTime/2;
	for ( CChannelEventList::iterator e= evtlist.begin(); e != evtlist.end(); ++e )
	{
		if ( e->startTime <= check_time && (e->startTime + (int)e->duration) >= check_time)
		{
			eventInfo.epgID = e->eventID;
			eventInfo.epg_starttime = e->startTime;
			break;
		}
	}
	if(eventInfo.epgID != 0)
	{
		CShortEPGData epgdata;
		if (CEitManager::getInstance()->getEPGidShort(eventInfo.epgID, &epgdata))
			epgTitle=epgdata.title;
	}
}
//------------------------------------------------------------
void CTimerEvent_Record::Refresh()
{
	if (eventInfo.epgID == 0)
		getEpgId();
}
//=============================================================
// Zapto Event
//=============================================================
void CTimerEvent_Zapto::announceEvent()
{
	Refresh();
	CTimerd::RecordingInfo ri=eventInfo;
	ri.eventID=eventID;
	ri.recordingDir[0] = 0;
	strcpy(ri.epgTitle, epgTitle.substr(0,sizeof(ri.epgTitle)-1).c_str());

	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_ANNOUNCE_ZAPTO,
								  CEventServer::INITID_TIMERD,
								  &ri,sizeof(CTimerd::RecordingInfo));
}
//------------------------------------------------------------
void CTimerEvent_Zapto::fireEvent()
{
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_ZAPTO,
								  CEventServer::INITID_TIMERD,
								  &eventInfo,
								  sizeof(CTimerd::EventInfo));
}


//------------------------------------------------------------
void CTimerEvent_Zapto::getEpgId()
{
	//TODO: Record/Zapto getEpgId code almost identical !
	CChannelEventList evtlist;
	CEitManager::getInstance()->getEventsServiceKey(eventInfo.channel_id, evtlist);
	// we check for a time 5 min after zap
	time_t check_time=alarmTime + 300;
	for ( CChannelEventList::iterator e= evtlist.begin(); e != evtlist.end(); ++e )
	{
    	if ( e->startTime < check_time && (e->startTime + (int)e->duration) > check_time)
		{
			eventInfo.epgID = e->eventID;
			eventInfo.epg_starttime = e->startTime;
			break;
		}
	}
	if(eventInfo.epgID != 0)
	{
		CShortEPGData epgdata;
		if (CEitManager::getInstance()->getEPGidShort(eventInfo.epgID, &epgdata))
			epgTitle=epgdata.title;
	}
}
//=============================================================
// NextProgram Event
//=============================================================
CTimerEvent_NextProgram::CTimerEvent_NextProgram(time_t announce_Time, time_t alarm_Time, time_t stop_Time,
						 t_channel_id channel_id,
						 event_id_t epgID,
						 time_t epg_starttime, CTimerd::CTimerEventRepeat evrepeat,
						 uint32_t repeatcount) :
	CTimerEvent(CTimerd::TIMER_NEXTPROGRAM, announce_Time, alarm_Time, stop_Time, evrepeat,repeatcount)
{
	eventInfo.epgID = epgID;
	eventInfo.epg_starttime = epg_starttime;
	eventInfo.channel_id = channel_id;
}
//------------------------------------------------------------
CTimerEvent_NextProgram::CTimerEvent_NextProgram(CConfigFile *config, int iId):
CTimerEvent(CTimerd::TIMER_NEXTPROGRAM, config, iId)
{
	std::stringstream ostr;
	ostr << iId;
	std::string id=ostr.str();
	eventInfo.epgID = config->getInt64("EVENT_INFO_EPG_ID_"+id);
	dprintf("read EVENT_INFO_EPG_ID_%s %ld\n",id.c_str(),(long)eventInfo.epgID);

	eventInfo.epg_starttime = config->getInt64("EVENT_INFO_EPG_STARTTIME_"+id);
	dprintf("read EVENT_INFO_EPG_STARTTIME_%s %ld\n",id.c_str(),(long)eventInfo.epg_starttime);

	eventInfo.channel_id = config->getInt64("EVENT_INFO_CHANNEL_ID_"+id);
	dprintf("read EVENT_INFO_CHANNEL_ID_%s %ld\n",id.c_str(),(long)eventInfo.channel_id);

	eventInfo.apids = config->getInt32("EVENT_INFO_APIDS_"+id);

	dprintf("read EVENT_INFO_APIDS_%s 0x%X (%p)\n",id.c_str(),eventInfo.apids,&eventInfo.apids);
}
//------------------------------------------------------------

void CTimerEvent_NextProgram::announceEvent()
{
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_ANNOUNCE_NEXTPROGRAM,
								  CEventServer::INITID_TIMERD,
								  &eventInfo,
								  sizeof(eventInfo));
}
//------------------------------------------------------------
void CTimerEvent_NextProgram::fireEvent()
{
	CTimerManager::getInstance()->getEventServer()->sendEvent(CTimerdClient::EVT_NEXTPROGRAM,
								  CEventServer::INITID_TIMERD,
								  &eventInfo,
								  sizeof(eventInfo));
}

//------------------------------------------------------------
void CTimerEvent_NextProgram::saveToConfig(CConfigFile *config)
{
	CTimerEvent::saveToConfig(config);
	std::stringstream ostr;
	ostr << eventID;
	std::string id=ostr.str();
	config->setInt64("EVENT_INFO_EPG_ID_"+id,eventInfo.epgID);
	dprintf("set EVENT_INFO_EPG_ID_%s to %ld\n",id.c_str(),(long)eventInfo.epgID);

	config->setInt64("EVENT_INFO_EPG_STARTTIME_"+id,eventInfo.epg_starttime);
	dprintf("set EVENT_INFO_EPG_STARTTIME_%s to %ld\n",id.c_str(),(long)eventInfo.epg_starttime);

	config->setInt64("EVENT_INFO_CHANNEL_ID_"+id,eventInfo.channel_id);
	dprintf("set EVENT_INFO_CHANNEL_ID_%s to %ld\n",id.c_str(),(long)eventInfo.channel_id);

	config->setInt32("EVENT_INFO_APIDS_"+id,eventInfo.apids);
	dprintf("set EVENT_INFO_APIDS_%s to 0x%X (%p)\n",id.c_str(),eventInfo.apids,&eventInfo.apids);
}
//------------------------------------------------------------
void CTimerEvent_NextProgram::Reschedule()
{
	// clear eogId on reschedule
	eventInfo.epgID = 0;
	eventInfo.epg_starttime = 0;
	CTimerEvent::Reschedule();
}
//=============================================================
// Remind Event
//=============================================================
CTimerEvent_Remind::CTimerEvent_Remind(time_t announce_Time,
				       time_t alarm_Time,
				       const char * const msg,
				       CTimerd::CTimerEventRepeat evrepeat,
				       uint32_t repeatcount) :
	CTimerEvent(CTimerd::TIMER_REMIND, announce_Time, alarm_Time, (time_t) 0, evrepeat,repeatcount)
{
	memset(message, 0, sizeof(message));
	strncpy(message, msg, sizeof(message)-1);
}
//------------------------------------------------------------
CTimerEvent_Remind::CTimerEvent_Remind(CConfigFile *config, int iId):
CTimerEvent(CTimerd::TIMER_REMIND, config, iId)
{
	std::stringstream ostr;
	ostr << iId;
	std::string id=ostr.str();
	strcpy(message, config->getString("MESSAGE_"+id).c_str());
	dprintf("read MESSAGE_%s %s (%p)\n",id.c_str(),message,message);
}
//------------------------------------------------------------
void CTimerEvent_Remind::fireEvent()
{
	CTimerManager::getInstance()->getEventServer()->sendEvent(
		CTimerdClient::EVT_REMIND,
		CEventServer::INITID_TIMERD,
		message,REMINDER_MESSAGE_MAXLEN);
}

//------------------------------------------------------------
void CTimerEvent_Remind::saveToConfig(CConfigFile *config)
{
	CTimerEvent::saveToConfig(config);
	std::stringstream ostr;
	ostr << eventID;
	std::string id=ostr.str();
	config->setString("MESSAGE_"+id,message);
	dprintf("set MESSAGE_%s to %s (%p)\n",id.c_str(),message,message);

}
//=============================================================
// ExecPlugin Event
//=============================================================
CTimerEvent_ExecPlugin::CTimerEvent_ExecPlugin(time_t announce_Time,
					       time_t alarm_Time,
					       const char * const plugin,
					       CTimerd::CTimerEventRepeat evrepeat,
					       uint32_t repeatcount) :
	CTimerEvent(CTimerd::TIMER_EXEC_PLUGIN, announce_Time, alarm_Time, (time_t) 0, evrepeat,repeatcount)
{
	memset(name, 0, sizeof(name));
	strncpy(name, plugin, sizeof(name)-1);
}
//------------------------------------------------------------
CTimerEvent_ExecPlugin::CTimerEvent_ExecPlugin(CConfigFile *config, int iId):
CTimerEvent(CTimerd::TIMER_EXEC_PLUGIN, config, iId)
{
	std::stringstream ostr;
	ostr << iId;
	std::string id=ostr.str();
	strcpy(name, config->getString("NAME_"+id).c_str());
	dprintf("read NAME_%s %s (%p)\n",id.c_str(),name,name);
}
//------------------------------------------------------------
void CTimerEvent_ExecPlugin::fireEvent()
{
	CTimerManager::getInstance()->getEventServer()->sendEvent(
		CTimerdClient::EVT_EXEC_PLUGIN,
		CEventServer::INITID_TIMERD,
		name,EXEC_PLUGIN_NAME_MAXLEN);
}

//------------------------------------------------------------
void CTimerEvent_ExecPlugin::saveToConfig(CConfigFile *config)
{
	CTimerEvent::saveToConfig(config);
	std::stringstream ostr;
	ostr << eventID;
	std::string id=ostr.str();
	config->setString("NAME_"+id,name);
	dprintf("set NAME_%s to %s (%p)\n",id.c_str(),name,name);

}
//=============================================================
