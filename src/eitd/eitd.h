//
//    sectionsd.cpp (network daemon for SI-sections)
//    (dbox-II-project)
//
//    Copyright (C) 2001 by fnbrd
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2008, 2009 Stefan Seyfried
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#ifndef __eitd_h__
#define __eitd_h__

#include <sys/time.h>

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#include "SIsections.hpp"
#include "SIlanguage.hpp"

//#define USE_BOOST_SHARED_PTR
#ifdef USE_BOOST_SHARED_PTR
#include <boost/shared_ptr.hpp>
typedef boost::shared_ptr<class SIevent> SIeventPtr;
typedef boost::shared_ptr<class SIservice> SIservicePtr;
#else
typedef SIevent * SIeventPtr;
typedef SIservice * SIservicePtr;
#endif

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

#include <OpenThreads/Thread>
#include "dmx.h"

#define MAX_SECTION_LENGTH (0x0fff + 3)

class CSectionThread : public OpenThreads::Thread, public DMX
{
	protected:
		uint8_t		*static_buf;
		int		timeoutsDMX;
		unsigned	timeoutInMSeconds;
		bool		running;
		//std::string	name;
		int		event_count; // debug

		virtual void run() {};
		int Sleep(unsigned int timeout)
		{
			struct timespec abs_wait;
			struct timeval now;
			gettimeofday(&now, NULL);
			TIMEVAL_TO_TIMESPEC(&now, &abs_wait);
			abs_wait.tv_sec += timeout;
			dprintf("%s: going to sleep for %d seconds...\n", name.c_str(), timeout);
			pthread_mutex_lock(&start_stop_mutex);
			int rs = pthread_cond_timedwait( &change_cond, &start_stop_mutex, &abs_wait );
			pthread_mutex_unlock( &start_stop_mutex );
			return rs;
		}
		bool addEvents();

	public:

		CSectionThread()
		{
			static_buf = new uint8_t[MAX_SECTION_LENGTH];
			timeoutsDMX = 0;
			running = false;
			event_count = 0;
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
		bool Stop()
		{
			if(!running)
				return false;
printf("%s::Stop: to lock\n", name.c_str());
			lock();
			running = false;
printf("%s::Stop: to broadcast\n", name.c_str());
			pthread_cond_broadcast(&change_cond);
printf("%s::Stop: to unlock\n", name.c_str());
			unlock();
printf("%s::Stop: to close\n", name.c_str());
			DMX::closefd();
			int ret = (OpenThreads::Thread::join() == 0);
			DMX::close();
			return ret;
		}
};

class CEitThread : public CSectionThread
{
	private:
		void run();
};

class CCNThread : public CSectionThread
{
	private:
		void run();
};

class CSdtThread : public CSectionThread
{
	private:
		bool addServices();
		void run();
};

class CTimeThread : public CSectionThread
{
	private:
		void run();
};

class CEitManager
{
	private:
	public:
};

#endif
