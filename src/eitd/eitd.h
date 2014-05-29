/*
 * sectionsd.cpp (network daemon for SI-sections)
 * (dbox-II-project)
 *
 * (C) 2001 by fnbrd (fnbrd@gmx.de)
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __eitd_h__
#define __eitd_h__

#include <sys/time.h>

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>
#include "dmx.h"

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#include "SIsections.hpp"
#include "SIlanguage.hpp"
#include "debug.h"
//#define USE_BOOST_SHARED_PTR
#ifdef USE_BOOST_SHARED_PTR
#include <boost/shared_ptr.hpp>
typedef boost::shared_ptr<class SIevent> SIeventPtr;
typedef boost::shared_ptr<class SIservice> SIservicePtr;
#else
typedef SIevent * SIeventPtr;
typedef SIservice * SIservicePtr;
#endif

/* period to restart EIT reading */
#define TIME_EIT_SCHEDULED_PAUSE 60 * 60

/* force EIT thread to change filter after, seconds */
#define TIME_EIT_SKIPPING 240 // 90 <- Canal diditaal 19.2e -> ~100 seconds for 0x5x
/* a little more time for freesat epg */
#define TIME_FSEIT_SKIPPING 240
/* Timeout in ms for reading from dmx in EIT threads. Dont make this too long
   since we are holding the start_stop lock during this read! */
#define EIT_READ_TIMEOUT 100

/* Number of DMX read timeouts, after which we check if there is an EIT at all
   for EIT and PPT threads... */
#define CHECK_RESTART_DMX_AFTER_TIMEOUTS (2000 / EIT_READ_TIMEOUT) // 2 seconds

struct OrderServiceUniqueKeyFirstStartTimeEventUniqueKey
{
	bool operator()(const SIeventPtr &p1, const SIeventPtr &p2)
	{
		return
			(p1->get_channel_id() == p2->get_channel_id()) ?
			(p1->times.begin()->startzeit == p2->times.begin()->startzeit ? p1->eventID < p2->eventID : p1->times.begin()->startzeit < p2->times.begin()->startzeit )
			:
			(p1->get_channel_id() < p2->get_channel_id());
	}
};

struct OrderFirstEndTimeServiceIDEventUniqueKey
{
	bool operator()(const SIeventPtr &p1, const SIeventPtr &p2)
	{
		return
			p1->times.begin()->startzeit + (long)p1->times.begin()->dauer == p2->times.begin()->startzeit + (long)p2->times.begin()->dauer ?
			(p1->service_id == p2->service_id ? p1->uniqueKey() > p2->uniqueKey() : p1->service_id < p2->service_id)
			:
			( p1->times.begin()->startzeit + (long)p1->times.begin()->dauer < p2->times.begin()->startzeit + (long)p2->times.begin()->dauer ) ;
	}
};

typedef std::set<SIeventPtr, OrderServiceUniqueKeyFirstStartTimeEventUniqueKey > MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey;
typedef std::set<SIeventPtr, OrderFirstEndTimeServiceIDEventUniqueKey > MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey;

typedef std::map<event_id_t, SIeventPtr, std::less<event_id_t> > MySIeventsOrderUniqueKey;

typedef std::map<t_channel_id, event_id_t, std::less<t_channel_id> > MySIeventUniqueKeysMetaOrderServiceUniqueKey;

typedef std::map<t_channel_id, SIservicePtr, std::less<t_channel_id> > MySIservicesOrderUniqueKey;
typedef std::map<t_channel_id, SIservicePtr, std::less<t_channel_id> > MySIservicesNVODorderUniqueKey;


/* abstract section reading class */
class CSectionThread : public OpenThreads::Thread, public DMX
{
	protected:
		uint8_t		*static_buf;
		int		timeoutsDMX;
		bool		running;
		int		event_count; // debug
		/* section read timeout, msec */
		unsigned	timeoutInMSeconds;
		/* read timeouts count to switch to next filter */
		int		skipTimeouts;
		/* time to switch to next filter */
		int		skipTime;
		bool		sendToSleepNow;

		/* should thread wait for time set before process loop */
		bool	wait_for_time;
		/* time in seconds to sleep when requested, wait forever if 0 */
		int	sleep_time;

		/* thread hooks */
		/* add filters when thread started */
		virtual void addFilters() {};

		/* check if thread should go sleep */
		virtual bool shouldSleep() { return false; };
		/* check if thread should continue to sleep after wakeup or timeout */
		virtual bool checkSleep() { return false; };

		/* called before sleep loop */
		virtual void beforeSleep() {};
		/* called after sleep loop */
		virtual void afterSleep() {};

		/* called inside sleep loop, after lock, before wait */
		virtual void beforeWait() {};
		/* called inside sleep loop, before lock, after wait */
		virtual void afterWait() {};

		/* process section after getSection */
		virtual void processSection() {}; 
		/* cleanup before exit */
		virtual void cleanup() {};

		/* sleep for sleep_time, forever if sleep_time = 0 */
		int Sleep();

		/* main thread function */
		void run();
	public:
		CSectionThread(std::string tname, unsigned short pid)
		{
			name = tname;
			pID = pid;

			static_buf = new uint8_t[MAX_SECTION_LENGTH];
			timeoutsDMX = 0;
			running = false;
			event_count = 0;
			sendToSleepNow = 0;

			/* defaults for threads, redefined if needed in derived */
			timeoutInMSeconds = EIT_READ_TIMEOUT;
			skipTimeouts = CHECK_RESTART_DMX_AFTER_TIMEOUTS;
			skipTime = TIME_EIT_SKIPPING;
			sleep_time = TIME_EIT_SCHEDULED_PAUSE;
			wait_for_time = true;
		}

		~CSectionThread()
		{
		}

		bool Start()
		{
			if(running)
				return false;
			running = true;
			return (OpenThreads::Thread::start() == 0);
		}
		void StopRun()
		{
			xprintf("%s::StopRun: to lock\n", name.c_str());
			lock();
			running = false;
			real_pauseCounter = 1;
			xprintf("%s::StopRun: to closefd\n", name.c_str());
			DMX::closefd();
			xprintf("%s::StopRun: to unlock\n", name.c_str());
			unlock();
		}
		bool Stop()
		{
			xprintf("%s::Stop: to broadcast\n", name.c_str());
			pthread_cond_broadcast(&change_cond);
			xprintf("%s::Stop: to join\n", name.c_str());
			int ret = (OpenThreads::Thread::join() == 0);
			xprintf("%s::Stop: to close\n", name.c_str());
			DMX::close();
			return ret;
		}
};

/* abstract eit events reading class */
class CEventsThread : public CSectionThread
{
	protected:
		/* default hooks */
		bool shouldSleep();
		bool checkSleep();
		void processSection();

		/* EIT-specific */
		bool addEvents();
	public:
		CEventsThread(std::string tname, unsigned short pid = 0x12)
			: CSectionThread(tname, pid)
		{
		};
};

class CEitThread : public CEventsThread
{
	private:
		/* overloaded hooks */
		void addFilters();
		void beforeSleep();
	public:
		CEitThread();
};

class CFreeSatThread : public CEventsThread
{
	private:
		/* overloaded hooks */
		void addFilters();
	public:
		CFreeSatThread(); 
};

class CCNThread : public CEventsThread
{
	private:
		/* overloaded hooks */
		void addFilters();
		bool shouldSleep();
		void beforeSleep();
		void beforeWait();
		void afterWait();
		void processSection(); 
		void cleanup();

		/* CN-specific */
		OpenThreads::Mutex update_mutex;
		bool	updating;
		cDemux * eitDmx;
		int eit_retry;

		void sendCNEvent();
	public:
		CCNThread();
		bool checkUpdate();
};

class CSdtThread : public CSectionThread
{
	private:
		/* overloaded hooks */
		void addFilters();
		bool shouldSleep();
		bool checkSleep();
		void processSection();

		/* SDT-specific */
		bool addServices();
	public:
		CSdtThread();
};

class CTimeThread : public CSectionThread
{
	private:
		/* overloaded hooks */
		void addFilters();

		/* specific */
		bool time_ntp;
		bool first_time;

		int64_t timediff;

		OpenThreads::Mutex time_mutex;
		OpenThreads::Condition time_cond;

		void sendTimeEvent(bool dvb, time_t tim = 0);
		void setSystemTime(time_t tim);
		void run();
	public:
		CTimeThread();
		void waitForTimeset();
		void setTimeSet();
};

#endif
