/*
 * sectionsd.cpp (network daemon for SI-sections)
 * (dbox-II-project)
 *
 * Copyright (C) 2001 by fnbrd (fnbrd@gmx.de)
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2008-2016 Stefan Seyfried
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

#include <config.h>
#include <malloc.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <connection/basicsocket.h>
#include <connection/basicserver.h>

#include <sectionsdclient/sectionsdMsg.h>
#include <sectionsdclient/sectionsdclient.h>
#include <eventserver.h>
#include <driver/abstime.h>
#include <system/helpers.h>
#include <system/set_threadname.h>
#include <OpenThreads/ScopedLock>

#include "eitd.h"
#include "sectionsd.h"
#include "edvbstring.h"
#include "xmlutil.h"
#include "debug.h"

#include <compatibility.h>

//#define ENABLE_SDT //FIXME

//#define DEBUG_SDT_THREAD
//#define DEBUG_TIME_THREAD

//#define DEBUG_SECTION_THREADS
//#define DEBUG_CN_THREAD

/*static*/ bool reader_ready = true;
static unsigned int max_events;
static bool notify_complete = false;

/* period to remove old events */
#define HOUSEKEEPING_SLEEP (5 * 60) // sleep 5 minutes
//#define HOUSEKEEPING_SLEEP (30) // FIXME 1 min for testing
/* period to clean cached sections and force restart sections read */
#define META_HOUSEKEEPING_COUNT (24 * 60 * 60) / HOUSEKEEPING_SLEEP // meta housekeeping after XX housekeepings - every 24h -
#define STANDBY_HOUSEKEEPING_COUNT (60 * 60) / HOUSEKEEPING_SLEEP
#define EPG_SERVICE_FREQUENTLY_COUNT (60 * 60) / HOUSEKEEPING_SLEEP

// Timeout bei tcp/ip connections in ms
#define READ_TIMEOUT_IN_SECONDS  2
#define WRITE_TIMEOUT_IN_SECONDS 2

// Time in seconds we are waiting for an EIT version number
//#define TIME_EIT_VERSION_WAIT		3 // old
#define TIME_EIT_VERSION_WAIT		10
// number of timeouts after which we stop waiting for an EIT version number
#define TIMEOUTS_EIT_VERSION_WAIT	(2 * CHECK_RESTART_DMX_AFTER_TIMEOUTS)

static unsigned int epg_save_frequently;
static unsigned int epg_read_frequently;
static long secondsToCache;
long int secondsExtendedTextCache = 0;
static long oldEventsAre;
static int scanning = 1;

extern bool epg_filter_is_whitelist;
extern bool epg_filter_except_current_next;
static bool xml_epg_filter;

static bool messaging_zap_detected = false;
/*static*/ bool dvb_time_update = false;

//NTP-Config
#define CONF_FILE CONFIGDIR "/neutrino.conf"

std::string ntp_system_cmd_prefix = find_executable("ntpdate") + " ";

std::string ntp_system_cmd;
std::string ntpserver;
int ntprefresh;
int ntpenable;

std::string		epg_dir("");

/* messaging_current_servicekey does probably not need locking, since it is
   changed from one place */
static t_channel_id    messaging_current_servicekey = 0;
static t_channel_id    current_channel_id = 0;
static bool channel_is_blacklisted = false;

bool timeset = false;

static int	messaging_have_CN = 0x00;	// 0x01 = CURRENT, 0x02 = NEXT
static int	messaging_got_CN = 0x00;	// 0x01 = CURRENT, 0x02 = NEXT
static bool	messaging_neutrino_sets_time = false;
// EVENTS...

static CEventServer *eventServer;

/*static*/ pthread_rwlock_t eventsLock = PTHREAD_RWLOCK_INITIALIZER; // Unsere (fast-)mutex, damit nicht gleichzeitig in die Menge events geschrieben und gelesen wird
static pthread_rwlock_t servicesLock = PTHREAD_RWLOCK_INITIALIZER; // Unsere (fast-)mutex, damit nicht gleichzeitig in die Menge services geschrieben und gelesen wird
static pthread_rwlock_t messagingLock = PTHREAD_RWLOCK_INITIALIZER;
OpenThreads::Mutex filter_mutex;

static CTimeThread threadTIME;
static CEitThread threadEIT;
static CCNThread threadCN;

#ifdef ENABLE_VIASATEPG
// ViaSAT uses pid 0x39 instead of 0x12
static CEitThread threadVSEIT("viasatThread", 0x39);
#endif

#ifdef ENABLE_FREESATEPG
static CFreeSatThread threadFSEIT;
#endif

#ifdef ENABLE_SDT
#define TIME_SDT_NONEWDATA      15
//#define RESTART_DMX_AFTER_TIMEOUTS 5
#define TIME_SDT_SCHEDULED_PAUSE 2* 60* 60
CSdtThread threadSDT;
#endif

#ifdef DEBUG_EVENT_LOCK
static time_t lockstart = 0;
#endif

static int sectionsd_stop = 0;

static bool slow_addevent = true;


inline void readLockServices(void)
{
	pthread_rwlock_rdlock(&servicesLock);
}
#ifdef ENABLE_SDT
inline void writeLockServices(void)
{
	pthread_rwlock_wrlock(&servicesLock);
}
#endif
inline void unlockServices(void)
{
	pthread_rwlock_unlock(&servicesLock);
}

inline void readLockMessaging(void)
{
	pthread_rwlock_rdlock(&messagingLock);
}

inline void writeLockMessaging(void)
{
	pthread_rwlock_wrlock(&messagingLock);
}

inline void unlockMessaging(void)
{
	pthread_rwlock_unlock(&messagingLock);
}

inline void readLockEvents(void)
{
	pthread_rwlock_rdlock(&eventsLock);
}

inline void writeLockEvents(void)
{
	pthread_rwlock_wrlock(&eventsLock);
#ifdef DEBUG_EVENT_LOCK
	lockstart = time_monotonic_ms();
#endif
}

inline void unlockEvents(void)
{
#ifdef DEBUG_EVENT_LOCK
	if (lockstart) {
		time_t tmp = time_monotonic_ms() - lockstart;
		if (tmp > 50)
			xprintf("locked ms %d\n", tmp);
		lockstart = 0;
	}
#endif
	pthread_rwlock_unlock(&eventsLock);
}

static const SIevent nullEvt; // Null-Event

static MySIeventsOrderUniqueKey mySIeventsOrderUniqueKey;
static MySIeventsOrderUniqueKey mySIeventsNVODorderUniqueKey;
/*static*/ MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey;
static MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey;

static SIevent * myCurrentEvent = NULL;
static SIevent * myNextEvent = NULL;

// Hier landen alle Service-Ids von Meta-Events inkl. der zugehoerigen Event-ID (nvod)
// d.h. key ist der Unique Service-Key des Meta-Events und Data ist der unique Event-Key
static MySIeventUniqueKeysMetaOrderServiceUniqueKey mySIeventUniqueKeysMetaOrderServiceUniqueKey;

static MySIservicesOrderUniqueKey mySIservicesOrderUniqueKey;
static MySIservicesNVODorderUniqueKey mySIservicesNVODorderUniqueKey;

/* needs write lock held! */
static bool deleteEvent(const event_id_t uniqueKey)
{
	bool ret = false;
	// writeLockEvents();
	MySIeventsOrderUniqueKey::iterator e = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (e != mySIeventsOrderUniqueKey.end()) {
		if (!e->second->times.empty()) {
			mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.erase(e->second);
			mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.erase(e->second);
		}

#ifndef USE_BOOST_SHARED_PTR
		delete e->second;
#endif
		mySIeventsOrderUniqueKey.erase(uniqueKey);
		mySIeventsNVODorderUniqueKey.erase(uniqueKey);
		ret = true;
	}
	// unlockEvents();
	return ret;
}

/* if cn == true (if called by cnThread), then myCurrentEvent and myNextEvent is updated, too */
/*static*/ void addEvent(const SIevent &evt, const time_t zeit, bool cn = false)
{
	filter_mutex.lock();
	bool EPG_filtered = checkEPGFilter(evt.original_network_id, evt.transport_stream_id, evt.service_id);
	filter_mutex.unlock();

	/* more readable in "plain english":
	   if current/next are not to be filtered and table_id is current/next -> continue
	   else {
		if epg filter is blacklist and filter matched -> stop. (return)
		if epg filter is whitelist and filter did not match -> stop also.
	   }
	 */
	if (!(epg_filter_except_current_next && (evt.table_id == 0x4e || evt.table_id == 0x4f)) &&
			(evt.table_id != 0xFF)) {
		if (!epg_filter_is_whitelist && EPG_filtered) {
			//dprintf("addEvent: blacklist and filter did match\n");
			return;
		}
		if (epg_filter_is_whitelist && !EPG_filtered) {
			//dprintf("addEvent: whitelist and filter did not match\n");
			return;
		}
	}

	if (cn) { // current-next => fill current or next event...
//xprintf("addEvent: current %012" PRIx64 " event %012" PRIx64 " messaging_got_CN %d\n", messaging_current_servicekey, evt.get_channel_id(), messaging_got_CN);
		readLockMessaging();
		// only if it is the current channel... and if we don't have them already.
		if (evt.get_channel_id() == messaging_current_servicekey && 
				(messaging_got_CN != 0x03)) { 
xprintf("addEvent: ch %012" PRIx64 " running %d (%s) got_CN %d\n", evt.get_channel_id(), evt.runningStatus(), evt.runningStatus() > 2 ? "curr" : "next", messaging_got_CN);

			unlockMessaging();
			writeLockEvents();
			if (evt.runningStatus() > 2) { // paused or currently running
				//TODO myCurrentEvent/myNextEvent without pointers.
				if (!myCurrentEvent || (myCurrentEvent && (*myCurrentEvent).uniqueKey() != evt.uniqueKey())) {
					delete myCurrentEvent;
					myCurrentEvent = new SIevent(evt);
					writeLockMessaging();
					messaging_got_CN |= 0x01;
					if (myNextEvent && (*myNextEvent).uniqueKey() == evt.uniqueKey()) {
						dprintf("addevent-cn: removing next-event\n");
						/* next got "promoted" to current => trigger re-read */
						delete myNextEvent;
						myNextEvent = NULL;
						messaging_got_CN &= 0x01;
					}
					unlockMessaging();
					dprintf("addevent-cn: added running (%d) event 0x%04x '%s'\n",
						evt.runningStatus(), evt.eventID, evt.getName().c_str());
				} else {
					writeLockMessaging();
					messaging_got_CN |= 0x01;
					unlockMessaging();
					dprintf("addevent-cn: not add runn. (%d) event 0x%04x '%s'\n",
						evt.runningStatus(), evt.eventID, evt.getName().c_str());
				}
			} else {
				if ((!myNextEvent    || (myNextEvent    && (*myNextEvent).uniqueKey()    != evt.uniqueKey() && (*myNextEvent).times.begin()->startzeit < evt.times.begin()->startzeit)) &&
						(!myCurrentEvent || (myCurrentEvent && (*myCurrentEvent).uniqueKey() != evt.uniqueKey()))) {
					delete myNextEvent;
					myNextEvent = new SIevent(evt);
					writeLockMessaging();
					messaging_got_CN |= 0x02;
					unlockMessaging();
					dprintf("addevent-cn: added next    (%d) event 0x%04x '%s'\n",
						evt.runningStatus(), evt.eventID, evt.getName().c_str());
				} else {
					dprintf("addevent-cn: not added next(%d) event 0x%04x '%s'\n",
						evt.runningStatus(), evt.eventID, evt.getName().c_str());
					writeLockMessaging();
					messaging_got_CN |= 0x02;
					unlockMessaging();
				}
			}
			unlockEvents();
		} else
			unlockMessaging();
	}

	writeLockEvents();
	MySIeventsOrderUniqueKey::iterator si = mySIeventsOrderUniqueKey.find(evt.uniqueKey());
	bool already_exists = (si != mySIeventsOrderUniqueKey.end());
	if (already_exists && (evt.table_id < si->second->table_id))
	{
		/* if the new event has a lower (== more recent) table ID, replace the old one */
		already_exists = false;
		dprintf("replacing event %012" PRIx64 ":%02x with %04x:%02x '%.40s'\n", si->second->uniqueKey(),
			si->second->table_id, evt.eventID, evt.table_id, evt.getName().c_str());
	}
	else if (already_exists && ( (evt.table_id == 0x51 || evt.table_id == 0x50 || evt.table_id == 0x4e) && evt.table_id == si->second->table_id && evt.version != si->second->version ))
	{
		//replace event if new version
		dprintf("replacing event version old 0x%02x new 0x%02x'\n", si->second->version, evt.version );
		already_exists = false;
	}

	/* Check size of some descriptors of the new event before comparing
	   them with the old ones, because the same event can be complete
	   on one German Sky channel and incomplete on another one. So we
	   make sure to keep the complete event, if applicable. */

	if (already_exists && (!evt.components.empty()) && (evt.components != si->second->components))
		already_exists = false;

	if (already_exists && (!evt.linkage_descs.empty()) && (evt.linkage_descs != si->second->linkage_descs))
		already_exists = false;

	if (already_exists && (!evt.ratings.empty()) && (evt.ratings != si->second->ratings))
		already_exists = false;

	if (already_exists && (evt.times != si->second->times))
		already_exists = false;

	if ((already_exists) && (SIlanguage::getMode() == CSectionsdClient::LANGUAGE_MODE_OFF)) {
		si->second->classifications = evt.classifications;
#ifdef USE_ITEM_DESCRIPTION
		si->second->itemDescription = evt.itemDescription;
		si->second->item = evt.item;
#endif
		//si->second->vps = evt.vps;
		if ((!evt.getExtendedText().empty()) && !evt.times.empty() &&
				(evt.times.begin()->startzeit < zeit + secondsExtendedTextCache))
			si->second->setExtendedText(0 /*"OFF"*/,evt.getExtendedText());
		if (!evt.getText().empty())
			si->second->setText(0 /*"OFF"*/,evt.getText());
		if (!evt.getName().empty())
			si->second->setName(0 /*"OFF"*/,evt.getName());
	}
	else {

		SIevent *eptr = new SIevent(evt);

		if (!eptr)
		{
			printf("[sectionsd::addEvent] new SIevent failed.\n");
			unlockEvents();
			return;
		}

		SIeventPtr e(eptr);

		//Strip ExtendedDescription if too far in the future
		if ((e->times.begin()->startzeit > zeit + secondsExtendedTextCache) &&
				(SIlanguage::getMode() == CSectionsdClient::LANGUAGE_MODE_OFF) && (zeit != 0))
			e->setExtendedText(0 /*"OFF"*/,"");

		/*
		 * this is test code, so indentation is deliberately wrong :-)
		 * we'll hopefully remove this if clause after testing is done
		 */
		if (slow_addevent)
		{
			std::vector<event_id_t> to_delete;
			unsigned short eventID = e->eventID;
			event_id_t e_key = e->uniqueKey();
			t_channel_id e_chid = e->get_channel_id();
			time_t start_time = e->times.begin()->startzeit;
			time_t end_time = e->times.begin()->startzeit + (long)e->times.begin()->dauer;
			/* create an event that's surely behind the one to check in the sort order */
			e->eventID = 0xFFFF; /* lowest order sort criteria is eventID */
			/* returns an iterator that's behind 'e' */
			MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator x =
				mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.upper_bound(e);
			e->eventID = eventID;

			/* the first decrement of the iterator gives us an event that's a potential
			 * match *or* from a different channel, then no event for this channel is stored */
			while (x != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin())
			{
				--x;
				if ((*x)->get_channel_id() != e_chid)
					break;
				else
				{
					event_id_t x_key = (*x)->uniqueKey();
					if (x_key == e_key)
					{
						/* the present event has a higher table_id than the new one
						 * => delete and insert the new one */
						if ((*x)->table_id >= e->table_id)
							continue;
						/* else: keep the old event with the lower table_id */
						unlockEvents();
						delete eptr;
						return;
					}
					if ((*x)->times.begin()->startzeit >= end_time)
						continue;
					/* iterating backwards: if the endtime of the stored events
					 * is earlier than the starttime of the new one, we'll never
					 * find an identical one => bail out */
					if ((*x)->times.begin()->startzeit + (long)(*x)->times.begin()->dauer <= start_time)
						break;
					if ((*x)->table_id < e->table_id)
					{
						/* don't add the higher table_id */
						dprintf("%s: don't replace 0x%012" PRIx64 ".%02x with 0x%012" PRIx64 ".%02x\n",
							__func__, x_key, (*x)->table_id, e_key, e->table_id);
						unlockEvents();
						delete eptr;
						return;
					}
					/* SRF special case: advertising is inserted with start time of
					 * an existing event. Duration may differ. To avoid holes in EPG caused
					 * by the (not useful) advertising event, don't delete the (useful)
					 * original event */
					if ((*x)->table_id == e->table_id && (e->table_id & 0xFE) == 0x4e &&
					    (*x)->times.begin()->startzeit == start_time)
						continue;
					/* here we have an overlapping event */
					dprintf("%s: delete 0x%012" PRIx64 ".%02x time = 0x%012" PRIx64 ".%02x\n", __func__,
						x_key, (*x)->table_id, e_key, e->table_id);
					to_delete.push_back(x_key);
				}
			}

			while (! to_delete.empty())
			{
				deleteEvent(to_delete.back());
				to_delete.pop_back();
			}
		}
		// Damit in den nicht nach Event-ID sortierten Mengen
		// Mehrere Events mit gleicher ID sind, diese vorher loeschen
		deleteEvent(e->uniqueKey());
		if ( !mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.empty() && mySIeventsOrderUniqueKey.size() >= max_events && max_events != 0 ) {
			MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator lastEvent =
				mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin();

/* if you don't want the new "delete old events first" method but
 * the old-fashioned "delete future events always", invert this */
#if 0
			bool back = true;
#else
			time_t now = time(NULL);
			bool back = false;
			if ((*lastEvent)->times.size() == 1)
			{
				if ((*lastEvent)->times.begin()->startzeit + (long)(*lastEvent)->times.begin()->dauer >= now - oldEventsAre)
					back = true;
			} else
				printf("[sectionsd] addevent: times.size != 1, please report\n");
#endif
			if (back)
			{
				// fprintf(stderr, "<");
				lastEvent = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end();
				--lastEvent;

				//preserve events of current channel
				readLockMessaging();
				while ((lastEvent != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin()) &&
					((*lastEvent)->get_channel_id() == messaging_current_servicekey)) {
					--lastEvent;
				}
				unlockMessaging();
			}
			event_id_t uniqueKey = (*lastEvent)->uniqueKey();
			// else fprintf(stderr, ">");
			deleteEvent(uniqueKey);
		}
		// Pruefen ob es ein Meta-Event ist
		MySIeventUniqueKeysMetaOrderServiceUniqueKey::iterator i = mySIeventUniqueKeysMetaOrderServiceUniqueKey.find(e->get_channel_id());

		if (i != mySIeventUniqueKeysMetaOrderServiceUniqueKey.end())
		{
			// ist ein MetaEvent, d.h. mit Zeiten fuer NVOD-Event

			if (!e->times.empty())
			{
				// D.h. wir fuegen die Zeiten in das richtige Event ein
				MySIeventsOrderUniqueKey::iterator ie = mySIeventsOrderUniqueKey.find(i->second);

				if (ie != mySIeventsOrderUniqueKey.end())
				{

					// Event vorhanden
					// Falls das Event in den beiden Mengen mit Zeiten nicht vorhanden
					// ist, dieses dort einfuegen
					MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator i2 = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.find(ie->second);
					if (i2 == mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end())
					{
						// nicht vorhanden -> einfuegen
						mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.insert(ie->second);
						mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.insert(ie->second);
					}

					// Und die Zeiten im Event updaten
					ie->second->times.insert(e->times.begin(), e->times.end());
				}
			}
		}
//		printf("Adding: %04x\n", (int) e->uniqueKey());

		// normales Event
		mySIeventsOrderUniqueKey.insert(std::make_pair(e->uniqueKey(), e));

		if (!e->times.empty())
		{
			// diese beiden Mengen enthalten nur Events mit Zeiten
			mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.insert(e);
			mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.insert(e);
		}
	}
	unlockEvents();
}

static void addNVODevent(const SIevent &evt)
{
	SIevent *eptr = new SIevent(evt);

	if (!eptr)
	{
		printf("[sectionsd::addNVODevent] new SIevent failed.\n");
		return;
	}

	SIeventPtr e(eptr);

	writeLockEvents();
	MySIeventsOrderUniqueKey::iterator e2 = mySIeventsOrderUniqueKey.find(e->uniqueKey());

	if (e2 != mySIeventsOrderUniqueKey.end())
	{
		// bisher gespeicherte Zeiten retten
		e->times.insert(e2->second->times.begin(), e2->second->times.end());
	}

	// Damit in den nicht nach Event-ID sortierten Mengen
	// mehrere Events mit gleicher ID sind, diese vorher loeschen
	deleteEvent(e->uniqueKey());
	if ( !mySIeventsOrderUniqueKey.empty() && mySIeventsOrderUniqueKey.size() >= max_events  && max_events != 0 ) {
		//TODO: Set Old Events to 0 if limit is reached...
		MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator lastEvent =
			mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end();
		--lastEvent;

		//preserve events of current channel
		readLockMessaging();
		while ((lastEvent != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin()) &&
				((*lastEvent)->get_channel_id() == messaging_current_servicekey)) {
			--lastEvent;
		}
		unlockMessaging();
		deleteEvent((*lastEvent)->uniqueKey());
	}
	mySIeventsOrderUniqueKey.insert(std::make_pair(e->uniqueKey(), e));

	mySIeventsNVODorderUniqueKey.insert(std::make_pair(e->uniqueKey(), e));
	if (!e->times.empty())
	{
		// diese beiden Mengen enthalten nur Events mit Zeiten
		mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.insert(e);
		mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.insert(e);
	}
	unlockEvents();
}

static void removeOldEvents(const long seconds)
{
	std::vector<event_id_t> to_delete;

	// Alte events loeschen
	time_t zeit = time(NULL);

	writeLockEvents();
	unsigned total_events = mySIeventsOrderUniqueKey.size();

	MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin();

	while ((e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end()) && (!messaging_zap_detected)) {
		bool goodtimefound = false;
		for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t) {
			if (t->startzeit + (long)t->dauer >= zeit - seconds) {
				goodtimefound=true;
				// one time found -> exit times loop
				break;
			}
		}

		if (!goodtimefound)
			to_delete.push_back((*e)->uniqueKey());
		++e;
	}
	for (std::vector<event_id_t>::iterator i = to_delete.begin(); i != to_delete.end(); ++i)
		deleteEvent(*i);
	unlockEvents();

	readLockEvents();
	xprintf("[sectionsd] Removed %d old events (%d left), zap detected %d.\n", (int)(total_events - mySIeventsOrderUniqueKey.size()), (int)mySIeventsOrderUniqueKey.size(), messaging_zap_detected);
	unlockEvents();
	return;
}

//------------------------------------------------------------
// misc. functions
//------------------------------------------------------------
static const SIevent& findSIeventForEventUniqueKey(const event_id_t eventUniqueKey)
{
	// Event (eventid) suchen
	MySIeventsOrderUniqueKey::iterator e = mySIeventsOrderUniqueKey.find(eventUniqueKey);

	if (e != mySIeventsOrderUniqueKey.end())
		return *(e->second);

	return nullEvt;
}

static const SIevent& findActualSIeventForServiceUniqueKey(const t_channel_id serviceUniqueKey, SItime& zeit, long plusminus = 0, unsigned *flag = 0)
{
	time_t azeit = time(NULL);

	if (flag != 0)
		*flag = 0;

	for (MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin(); e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end(); ++e)
		if ((*e)->get_channel_id() == serviceUniqueKey)
		{
			if (flag != 0)
				*flag |= CSectionsdClient::epgflags::has_anything; // berhaupt was da...

//			for (SItimes::reverse_iterator t = (*e)->times.rend(); t != (*e)->times.rbegin(); t--) {
			for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t) {
				if ((long)(azeit + plusminus) < (long)(t->startzeit + t->dauer))
				{
					if (flag != 0)
						*flag |= CSectionsdClient::epgflags::has_later; // later events are present...

					if (t->startzeit <= (long)(azeit + plusminus))
					{
						//printf("azeit %d, startzeit+t->dauer %d \n", azeit, (long)(t->startzeit+t->dauer) );

						if (flag != 0)
							*flag |= CSectionsdClient::epgflags::has_current; // aktuelles event da...

						zeit = *t;

						return *(*e);
					}
				}
			}
		}

	return nullEvt;
}

static const SIevent& findNextSIeventForServiceUniqueKey(const t_channel_id serviceUniqueKey, SItime& zeit)
{
	time_t azeit = time(NULL);

	for (MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin(); e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end(); ++e)
		if ((*e)->get_channel_id() == serviceUniqueKey)
		{
			for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t)
				if ((long)(azeit) < (long)(t->startzeit + t->dauer))
				{
					zeit = *t;
					return *(*e);
				}
		}

	return nullEvt;
}

// Finds the next event based on unique key and start time
static const SIevent &findNextSIevent(const event_id_t uniqueKey, SItime &zeit)
{
	MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end())
	{
		SItimes::iterator nextnvodtimes = eFirst->second->times.end();
		//SItimes::iterator nexttimes = eFirst->second->times.end();

		if (eFirst->second->times.size() > 1)
		{
			//find next nvod
			nextnvodtimes = eFirst->second->times.begin();
			while ( nextnvodtimes != eFirst->second->times.end() ) {
				if ( nextnvodtimes->startzeit == zeit.startzeit )
					break;
				else
					++nextnvodtimes;
			}
		}

		MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator eNext;

		SItimes::iterator nexttimes;
		bool nextfound = false;
		//Startzeit not first - we can't use the ordered list...
		for (MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin(); e !=
				mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end(); ++e ) {
			if ((*e)->get_channel_id() == eFirst->second->get_channel_id()) {
				for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t) {
					if (t->startzeit > zeit.startzeit) {
						//if (nexttimes != eFirst->second->times.end())
						if(nextfound)
						{
							if (t->startzeit < nexttimes->startzeit) {
								eNext = e;
								nexttimes = t;
							}
						}
						else {
							eNext = e;
							nexttimes = t;
							nextfound = true;
						}
					}
				}
			}
		}
		if (nextnvodtimes != eFirst->second->times.end())
			++nextnvodtimes;
		//Compare
		//if (nexttimes != eFirst->second->times.end())
		if(nextfound)
		{
			if (nextnvodtimes != eFirst->second->times.end()) {
				//both times are set - take the first
				if (nexttimes->startzeit < nextnvodtimes->startzeit) {
					zeit = *nexttimes;
					return *(*eNext);

				} else {
					zeit = *nextnvodtimes;
					return *(eFirst->second);
				}
			} else {
				//only nexttimes set
				zeit = *nexttimes;
				return *(*eNext);
			}
		} else if (nextnvodtimes != eFirst->second->times.end()) {
			//only nextnvodtimes set
			zeit = *nextnvodtimes;
			return *(eFirst->second);
		}
	}

	return nullEvt;
}

/*
 * communication with sectionsdclient:
 */

inline bool readNbytes(int fd, char *buf, const size_t numberOfBytes, const time_t timeoutInSeconds)
{
	timeval timeout;
	timeout.tv_sec  = timeoutInSeconds;
	timeout.tv_usec = 0;
	return receive_data(fd, buf, numberOfBytes, timeout);
}

inline bool writeNbytes(int fd, const char *buf,  const size_t numberOfBytes, const time_t timeoutInSeconds)
{
	timeval timeout;
	timeout.tv_sec  = timeoutInSeconds;
	timeout.tv_usec = 0;
	return send_data(fd, buf, numberOfBytes, timeout);
}

/* send back an empty response */
static void sendEmptyResponse(int connfd, char *, const unsigned)
{
	struct sectionsd::msgResponseHeader msgResponse;
	msgResponse.dataLength = 0;
	writeNbytes(connfd, (const char *)&msgResponse, sizeof(msgResponse), WRITE_TIMEOUT_IN_SECONDS);
	return;
}

//---------------------------------------------------------------------
//			connection-thread
// handles incoming requests
//---------------------------------------------------------------------

static void wakeupAll()
{
	threadCN.change(0);
	threadEIT.change(0);
#ifdef ENABLE_VIASATEPG
	threadVSEIT.change(0);
#endif

#ifdef ENABLE_FREESATEPG
	threadFSEIT.change(0);
#endif
#ifdef ENABLE_SDT
	threadSDT.change(0);
#endif
}

static void commandPauseScanning(int connfd, char *data, const unsigned dataLength)
{
	if (dataLength != sizeof(int))
		return;

	int pause = *(int *)data;

	xprintf("Request of %s scanning (now %s).\n", pause ? "stop" : "continue", scanning ? "scanning" : "idle");

	if (scanning && pause)
	{
#if 0
		threadCN.request_pause();
		threadEIT.request_pause();
#ifdef ENABLE_FREESATEPG
		threadFSEIT.request_pause();
#endif
#ifdef ENABLE_SDT
		threadSDT.request_pause();
#endif
#endif
		scanning = 0;
		writeLockMessaging();
		messaging_zap_detected = false;
		unlockMessaging();
	}
	else if (!pause && !scanning)
	{
#if 0
		threadCN.request_unpause();
		threadEIT.request_unpause();
#ifdef ENABLE_FREESATEPG
		threadFSEIT.request_unpause();
#endif
#ifdef ENABLE_SDT
		threadSDT.request_unpause();
#endif
#endif
		writeLockEvents();
		delete myCurrentEvent;
		myCurrentEvent = NULL;
		delete myNextEvent;
		myNextEvent = NULL;
		unlockEvents();

		writeLockMessaging();
		messaging_have_CN = 0x00;
		messaging_got_CN = 0x00;
		messaging_zap_detected = true;
		unlockMessaging();

		scanning = 1;
		/* FIXME should we stop time updates if not scanning ? flag if ntp update was good ? */
		//if (!ntpenable)
		{
			threadTIME.change(0);
		}
		wakeupAll();
	}
	sendEmptyResponse(connfd, NULL, 0);
}

static void commandserviceChanged(int connfd, char *data, const unsigned dataLength)
{
	sendEmptyResponse(connfd, NULL, 0);
	if (dataLength != sizeof(sectionsd::commandSetServiceChanged))
		return;

	sectionsd::commandSetServiceChanged * cmd = (sectionsd::commandSetServiceChanged *)data;
	t_channel_id uniqueServiceKey = cmd->channel_id;

	xprintf("[sectionsd] commandserviceChanged: Service change to " PRINTF_CHANNEL_ID_TYPE " demux #%d\n", uniqueServiceKey, cmd->dnum);
	/* assume live demux always 0, other means background scan */
	if (cmd->dnum) {
		/* dont wakeup EIT, if we have max events allready */
		if (max_events == 0  || (mySIeventsOrderUniqueKey.size() < max_events)) {
			current_channel_id = uniqueServiceKey;
			writeLockMessaging();
			messaging_zap_detected = true;
			unlockMessaging();
			threadEIT.setDemux(cmd->dnum);
			threadEIT.setCurrentService(uniqueServiceKey);
		}
		return;
	}

	static t_channel_id time_trigger_last = 0;

	if (current_channel_id != uniqueServiceKey) {
		current_channel_id = uniqueServiceKey;

		dvb_time_update = !checkNoDVBTimelist(uniqueServiceKey);
		dprintf("[sectionsd] commandserviceChanged: DVB time update is %s\n", dvb_time_update ? "allowed" : "blocked!");

		channel_is_blacklisted = checkBlacklist(uniqueServiceKey);
		dprintf("[sectionsd] commandserviceChanged: service is %s\n", channel_is_blacklisted ? "filtered!" : "not filtered");

		writeLockEvents();
		delete myCurrentEvent;
		myCurrentEvent = NULL;
		delete myNextEvent;
		myNextEvent = NULL;
		unlockEvents();

		writeLockMessaging();
		messaging_current_servicekey = uniqueServiceKey & 0xFFFFFFFFFFFFULL;
		messaging_have_CN = 0x00;
		messaging_got_CN = 0x00;
		messaging_zap_detected = true;
		unlockMessaging();

		threadCN.setCurrentService(messaging_current_servicekey);
		threadEIT.setDemux(cmd->dnum);
		threadEIT.setCurrentService(uniqueServiceKey /*messaging_current_servicekey*/);
#ifdef ENABLE_VIASATEPG
		threadVSEIT.setCurrentService(messaging_current_servicekey);
#endif
#ifdef ENABLE_FREESATEPG
		threadFSEIT.setCurrentService(messaging_current_servicekey);
#endif
#ifdef ENABLE_SDT
		threadSDT.setCurrentService(messaging_current_servicekey);
#endif
		if (time_trigger_last != (messaging_current_servicekey & 0xFFFFFFFF0000ULL)) {
			time_trigger_last = messaging_current_servicekey & 0xFFFFFFFF0000ULL;
			threadTIME.setCurrentService(messaging_current_servicekey);
		}
	}
	else
		dprintf("[sectionsd] commandserviceChanged: no change...\n");

	dprintf("[sectionsd] commandserviceChanged: Service changed to " PRINTF_CHANNEL_ID_TYPE "\n", uniqueServiceKey);
}

static void commandserviceStopped(int connfd, char * /* data */, const unsigned /* dataLength */)
{
	xprintf("[sectionsd] commandserviceStopped\n");
	current_channel_id = 0;
	sendEmptyResponse(connfd, NULL, 0);
	threadEIT.stop();
	threadCN.stop();
	threadCN.stopUpdate();
	xprintf("[sectionsd] commandserviceStopped done\n");
}

static void commandGetIsScanningActive(int connfd, char* /*data*/, const unsigned /*dataLength*/)
{
	struct sectionsd::msgResponseHeader responseHeader;

	responseHeader.dataLength = sizeof(scanning);

	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true)
	{
		writeNbytes(connfd, (const char *)&scanning, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");
}

static void commandDumpStatusInformation(int /*connfd*/, char* /*data*/, const unsigned /*dataLength*/)
{
	dputs("Request of status information");

	readLockEvents();

	unsigned anzEvents = mySIeventsOrderUniqueKey.size();

	unsigned anzNVODevents = mySIeventsNVODorderUniqueKey.size();

	unsigned anzMetaServices = mySIeventUniqueKeysMetaOrderServiceUniqueKey.size();

	unlockEvents();

	readLockServices();

	unsigned anzServices = mySIservicesOrderUniqueKey.size();

	unsigned anzNVODservices = mySIservicesNVODorderUniqueKey.size();

	//  unsigned anzServices=services.size();
	unlockServices();

	//  struct rusage resourceUsage;
	//  getrusage(RUSAGE_CHILDREN, &resourceUsage);
	//  getrusage(RUSAGE_SELF, &resourceUsage);
	time_t zeit = time(NULL);

#define MAX_SIZE_STATI	2024
	char stati[MAX_SIZE_STATI];

	snprintf(stati, MAX_SIZE_STATI,
		 "Current time: %s"
		 "Hours to cache: %ld\n"
		 "Hours to cache extended text: %ld\n"
		 "Events to cache: %u\n"
		 "Events are old %ldmin after their end time\n"
		 "Number of cached services: %u\n"
		 "Number of cached nvod-services: %u\n"
		 "Number of cached events: %u\n"
		 "Number of cached nvod-events: %u\n"
		 "Number of cached meta-services: %u\n"
		 //    "Resource-usage: maxrss: %ld ixrss: %ld idrss: %ld isrss: %ld\n"
#ifdef ENABLE_FREESATEPG
		 "FreeSat enabled\n"
#else
		 ""
#endif
		 ,ctime(&zeit),
		 secondsToCache / (60*60L), secondsExtendedTextCache / (60*60L), max_events, oldEventsAre / 60, anzServices, anzNVODservices, anzEvents, anzNVODevents, anzMetaServices
		 //    resourceUsage.ru_maxrss, resourceUsage.ru_ixrss, resourceUsage.ru_idrss, resourceUsage.ru_isrss,
		);
	printf("%s\n", stati);
	comp_malloc_stats(NULL);
	return ;
}

static void commandGetIsTimeSet(int connfd, char* /*data*/, const unsigned /*dataLength*/)
{
	sectionsd::responseIsTimeSet rmsg;

	rmsg.IsTimeSet = timeset;

	dprintf("Request of Time-Is-Set %d\n", rmsg.IsTimeSet);

	struct sectionsd::msgResponseHeader responseHeader;

	responseHeader.dataLength = sizeof(rmsg);

	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true)
	{
		writeNbytes(connfd, (const char *)&rmsg, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");
}


static void commandRegisterEventClient(int /*connfd*/, char *data, const unsigned dataLength)
{
	if (dataLength == sizeof(CEventServer::commandRegisterEvent))
	{
		eventServer->registerEvent2(((CEventServer::commandRegisterEvent*)data)->eventID, ((CEventServer::commandRegisterEvent*)data)->clientID, ((CEventServer::commandRegisterEvent*)data)->udsName);

		if (((CEventServer::commandRegisterEvent*)data)->eventID == CSectionsdClient::EVT_TIMESET)
			messaging_neutrino_sets_time = true;
	}
}

static void commandUnRegisterEventClient(int /*connfd*/, char *data, const unsigned dataLength)
{
	if (dataLength == sizeof(CEventServer::commandUnRegisterEvent))
		eventServer->unRegisterEvent2(((CEventServer::commandUnRegisterEvent*)data)->eventID, ((CEventServer::commandUnRegisterEvent*)data)->clientID);
}

static void commandSetConfig(int connfd, char *data, const unsigned /*dataLength*/)
{
	sendEmptyResponse(connfd, NULL, 0);

	struct sectionsd::commandSetConfig *pmsg;

	pmsg = (struct sectionsd::commandSetConfig *)data;

	/* writeLockEvents not needed because write lock will block if read lock active */
	readLockEvents();
	secondsToCache = (long)(pmsg->epg_cache)*24*60L*60L;
	oldEventsAre = (long)(pmsg->epg_old_events)*60L*60L;
	secondsExtendedTextCache = (long)(pmsg->epg_extendedcache)*60L*60L;
	max_events = pmsg->epg_max_events;
	epg_save_frequently = pmsg->epg_save_frequently;
	epg_read_frequently = pmsg->epg_read_frequently;

	unlockEvents();

	bool time_wakeup = false;
	if (ntpserver.compare((std::string)&data[sizeof(struct sectionsd::commandSetConfig)])) {
		time_wakeup = true;
	}
	if (ntprefresh != pmsg->network_ntprefresh) {
		dprintf("new network_ntprefresh = %d\n", pmsg->network_ntprefresh);
		time_wakeup = true;
	}
	if (ntpenable ^ (pmsg->network_ntpenable == 1))	{
		dprintf("new network_ntpenable = %d\n", pmsg->network_ntpenable);
		time_wakeup = true;
	}

	if (time_wakeup) {
		ntpserver = (std::string)&data[sizeof(struct sectionsd::commandSetConfig)];
		dprintf("new network_ntpserver = %s\n", ntpserver.c_str());
		ntp_system_cmd = ntp_system_cmd_prefix + ntpserver;
		ntprefresh = pmsg->network_ntprefresh;
		ntpenable = (pmsg->network_ntpenable == 1);
		if(timeset)
			threadTIME.change(1);
	}

	epg_dir= (std::string)&data[sizeof(struct sectionsd::commandSetConfig) + strlen(&data[sizeof(struct sectionsd::commandSetConfig)]) + 1];
}

static void deleteSIexceptEPG()
{
	threadCN.dropCachedSectionIDs();
	threadEIT.dropCachedSectionIDs();
#ifdef ENABLE_SDT
	writeLockServices();
	mySIservicesOrderUniqueKey.clear();
	unlockServices();
	threadSDT.dropCachedSectionIDs();
#endif
}

static void FreeMemory()
{
	xprintf("[sectionsd] free memory...\n");
	deleteSIexceptEPG();

	writeLockEvents();

#ifndef USE_BOOST_SHARED_PTR
	std::set<SIeventPtr> allevents;

	allevents.insert(mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin(), mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end());
	/* this probably not needed, but takes only additional ~2 seconds
	 * with even count > 70000 */
	allevents.insert(mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin(), mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end());
	MySIeventsOrderUniqueKey::iterator it;
	for(it = mySIeventsOrderUniqueKey.begin(); it != mySIeventsOrderUniqueKey.end(); ++it)
		allevents.insert(it->second);
	for(it = mySIeventsNVODorderUniqueKey.begin(); it != mySIeventsNVODorderUniqueKey.end(); ++it)
		allevents.insert(it->second);

	for(std::set<SIeventPtr>::iterator ait = allevents.begin(); ait != allevents.end(); ++ait)
		delete (*ait);
#endif
	mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.clear();
	mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.clear();
	mySIeventsOrderUniqueKey.clear();
	mySIeventsNVODorderUniqueKey.clear();

	unlockEvents();

	comp_malloc_stats(NULL);
	xprintf("[sectionsd] free memory done\n");
	//wakeupAll(); //FIXME should we re-start eit here ?
}

static void commandFreeMemory(int connfd, char * /*data*/, const unsigned /*dataLength*/)
{
	sendEmptyResponse(connfd, NULL, 0);
	FreeMemory();
}

static void commandReadSIfromXML(int connfd, char *data, const unsigned dataLength)
{
	pthread_t thrInsert;

	sendEmptyResponse(connfd, NULL, 0);

	if (dataLength > 100)
		return ;

	writeLockMessaging();
	data[dataLength] = '\0';
	static std::string epg_dir_tmp = (std::string)data + "/";
	unlockMessaging();


	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (pthread_create (&thrInsert, &attr, insertEventsfromFile, (void *)epg_dir_tmp.c_str() ))
	{
		perror("sectionsd: pthread_create()");
	}

	pthread_attr_destroy(&attr);
}

static void commandWriteSI2XML(int connfd, char *data, const unsigned dataLength)
{
	sendEmptyResponse(connfd, NULL, 0);
	if (mySIeventsOrderUniqueKey.empty() || (!reader_ready) || (dataLength > 100)){
		eventServer->sendEvent(CSectionsdClient::EVT_WRITE_SI_FINISHED, CEventServer::INITID_SECTIONSD);
		return;
	}

	data[dataLength] = '\0';

	writeEventsToFile(data);

	eventServer->sendEvent(CSectionsdClient::EVT_WRITE_SI_FINISHED, CEventServer::INITID_SECTIONSD);
}

struct s_cmd_table
{
	void (*cmd)(int connfd, char *, const unsigned);
	std::string sCmd;
};

static s_cmd_table connectionCommands[sectionsd::numberOfCommands] = {
	{	commandDumpStatusInformation,		"commandDumpStatusInformation"		},
	{	commandPauseScanning,                   "commandPauseScanning"			},
	{	commandGetIsScanningActive,             "commandGetIsScanningActive"		},
	{	commandGetIsTimeSet,                    "commandGetIsTimeSet"			},
	{	commandserviceChanged,                  "commandserviceChanged"			},
	{	commandserviceStopped,                  "commandserviceStopped"			},
	{	commandRegisterEventClient,             "commandRegisterEventClient"		},
	{	commandUnRegisterEventClient,           "commandUnRegisterEventClient"		},
	{	commandFreeMemory,			"commandFreeMemory"			},
	{	commandReadSIfromXML,			"commandReadSIfromXML"			},
	{	commandWriteSI2XML,			"commandWriteSI2XML"			},
	{	commandSetConfig,			"commandSetConfig"			},
};

bool sectionsd_parse_command(CBasicMessage::Header &rmsg, int connfd)
{
	dprintf("Connection from UDS\n");

	struct sectionsd::msgRequestHeader header;

	memmove(&header, &rmsg, sizeof(CBasicMessage::Header));
	memset(((char *)&header) + sizeof(CBasicMessage::Header), 0, sizeof(header) - sizeof(CBasicMessage::Header));

	bool readbytes = readNbytes(connfd, ((char *)&header) + sizeof(CBasicMessage::Header), sizeof(header) - sizeof(CBasicMessage::Header), READ_TIMEOUT_IN_SECONDS);

	if (readbytes == true)
	{
		dprintf("version: %hhd, cmd: %hhd, numbytes: %d\n", header.version, header.command, readbytes);

		if (header.command < sectionsd::numberOfCommands)
		{
			dprintf("data length: %hd\n", header.dataLength);
			char *data = new char[header.dataLength + 1];

			if (!data)
				fprintf(stderr, "low on memory!\n");
			else
			{
				bool rc = true;

				if (header.dataLength)
					rc = readNbytes(connfd, data, header.dataLength, READ_TIMEOUT_IN_SECONDS);

				if (rc == true)
				{
					dprintf("%s\n", connectionCommands[header.command].sCmd.c_str());
					if (connectionCommands[header.command].cmd == sendEmptyResponse)
						printf("sectionsd_parse_command: UNUSED cmd used: %d (%s)\n", header.command, connectionCommands[header.command].sCmd.c_str());
					connectionCommands[header.command].cmd(connfd, data, header.dataLength);
				}

				delete[] data;
			}
		}
		else
			dputs("Unknown format or version of request!");
	}

	return true;
}

static void dump_sched_info(std::string label)
{
	int policy;
	struct sched_param parm;
	int rc = pthread_getschedparam(pthread_self(), &policy, &parm);
	printf("%s: getschedparam %d policy %d prio %d\n", label.c_str(), rc, policy, parm.sched_priority);
}

//---------------------------------------------------------------------
//			Time-thread
// updates system time according TOT every 30 minutes
//---------------------------------------------------------------------

CTimeThread::CTimeThread()
	: CSectionThread("timeThread", 0x14)
{
	timeoutInMSeconds = 36000;
	cache = false;
	wait_for_time = false;

	first_time = true;
	time_ntp = false;
};

void CTimeThread::sendTimeEvent(bool ntp, time_t tim)
{
#if 0
	time_t actTime = time(NULL);
	if (!ntp) {
		struct tm *tmTime = localtime(&actTime);
		xprintf("%s: current: %02d.%02d.%04d %02d:%02d:%02d, dvb: %s", name.c_str(),
				tmTime->tm_mday, tmTime->tm_mon+1, tmTime->tm_year+1900, tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec, ctime(&tim));
		actTime = tim;
	}
	eventServer->sendEvent(CSectionsdClient::EVT_TIMESET, CEventServer::INITID_SECTIONSD, &actTime, sizeof(actTime) );
#endif
	if(ntp || tim) {}
	if (timediff)
		eventServer->sendEvent(CSectionsdClient::EVT_TIMESET, CEventServer::INITID_SECTIONSD, &timediff, sizeof(timediff));
	setTimeSet();
}

void CTimeThread::setTimeSet()
{
	time_mutex.lock();
	timeset = true;
	time_cond.broadcast();
	time_mutex.unlock();
}

void CTimeThread::waitForTimeset(void)
{
	time_mutex.lock();
	while(!timeset)
		time_cond.wait(&time_mutex);
	time_mutex.unlock();
}

bool CTimeThread::setSystemTime(time_t tim, bool force)
{
	struct timeval tv;
	struct tm t;

	gettimeofday(&tv, NULL);
	timediff = int64_t(tim * 1000000 - (tv.tv_usec + tv.tv_sec * 1000000));
	localtime_r(&tv.tv_sec, &t);

	xprintf("%s: timediff %" PRId64 ", current: %02d.%02d.%04d %02d:%02d:%02d, dvb: %s",
		name.c_str(), timediff,
		t.tm_mday, t.tm_mon+1, t.tm_year+1900, t.tm_hour, t.tm_min, t.tm_sec, ctime(&tim));
#if 0
	/* if new time less than current for less than 1 second, ignore */
	if(timediff < 0 && timediff > (int64_t) -1000000) {
		timediff = 0;
		return;
	}
#endif
	if (timeset && abs(tim - tv.tv_sec) < 120) { /* abs() is int */
		struct timeval oldd;
		tv.tv_sec = time_t(timediff / 1000000LL);
		tv.tv_usec = suseconds_t(timediff % 1000000LL);
		if (adjtime(&tv, &oldd))
			xprintf("adjtime(%d, %d) failed: %m\n", (int)tv.tv_sec, (int)tv.tv_usec);
		else {
			xprintf("difference is < 120s, using adjtime(%d, %d). oldd(%d, %d)\n",
				(int)tv.tv_sec, (int)tv.tv_usec, (int)oldd.tv_sec, (int)oldd.tv_usec);
			timediff = 0;
			return true;
		}
	} else if (timeset && ! force) {
		xprintf("difference is > 120s, try again and set 'force=true'\n");
		return false;
	}
	/* still fall through if adjtime() failed */

	tv.tv_sec = tim;
	tv.tv_usec = 0;
	errno=0;
	if (settimeofday(&tv, NULL) == 0)
		return true;

	perror("[sectionsd] settimeofday");
	return errno==EPERM;
}

void CTimeThread::addFilters()
{
	addfilter(0x70, 0xff);
	addfilter(0x73, 0xff);
}

void CTimeThread::run()
{
	time_t dvb_time = 0;
	bool retry = false; /* if time seems fishy, set to true and try again */
	xprintf("%s::run:: starting, pid %d (%lu)\n", name.c_str(), getpid(), pthread_self());
	const std::string tn = ("sd:" + name).c_str();
	set_threadname(tn.c_str());
	addFilters();
	DMX::start();

	while(running) {
		if(sendToSleepNow) {
#ifdef DEBUG_TIME_THREAD
			xprintf("%s: going to sleep %d seconds, running %d scanning %d\n",
					name.c_str(), sleep_time, running, scanning);
#endif
			do {
				if(!scanning)
					sleep_time = 0;
				real_pause();
#ifndef DEBUG_TIME_THREAD
				Sleep();
#else
				int rs = Sleep();
				xprintf("%s: wakeup, running %d scanning %d channel %" PRIx64 " reason %d\n",
						name.c_str(), running, scanning, current_service, rs);
#endif
			} while (running && !scanning);

			if (!running)
				break;
		}
		bool success = false;
		time_ntp = false;
		dvb_time = 0;
		timediff = 0;

		if (ntpenable && system( ntp_system_cmd.c_str() ) == 0) {
			time_ntp = true;
			success = true;
		} else if (dvb_time_update) {
			if(!first_time)
				change(1);
			else
				change(0);

			xprintf("%s: get DVB time ch 0x%012" PRIx64 " (isOpen %d)\n",
				name.c_str(), current_service, isOpen());
			int rc;
#if HAVE_COOL_HARDWARE
			/* libcoolstream does not like the repeated read if the dmx is not yet running
			 * (e.g. during neutrino start) and causes strange openthreads errors which in
			 * turn cause complete malfunction of the dmx, so we cannot use the "speed up
			 * shutdown" hack on with libcoolstream... :-( */
			rc = dmx->Read(static_buf, MAX_SECTION_LENGTH, timeoutInMSeconds);
#else
			time_t start = time_monotonic_ms();
			/* speed up shutdown by looping around Read() */
			do {
				rc = dmx->Read(static_buf, MAX_SECTION_LENGTH, timeoutInMSeconds / 12);
			} while (running && rc == 0
				 && (time_monotonic_ms() - start) < (time_t)timeoutInMSeconds);
#endif
			xprintf("%s: get DVB time ch 0x%012" PRIx64 " rc: %d neutrino_sets_time %d\n",
				name.c_str(), current_service, rc, messaging_neutrino_sets_time);
			if (rc > 0) {
				SIsectionTIME st(static_buf);
				if (st.is_parsed()) {
					dvb_time = st.getTime();
					success = true;
				}
			} else
				retry = false; /* reset bogon detector after invalid read() */
		}
		/* default sleep time */
		sleep_time = ntprefresh * 60;
		if(success) {
			if(dvb_time) {
				bool ret = setSystemTime(dvb_time, retry);
				if (! ret) {
					xprintf("%s: time looks wrong, trying again\n", name.c_str());
					sendToSleepNow = false;
					retry = true;
					continue;
				}
				retry = false;
				/* retry a second time immediately after start, to get TOT ? */
				if(first_time)
					sleep_time = 5;
			}
			/* in case of wrong TDT date, dont send time is set, 1325376000 - 01.01.2012 */
			if(time_ntp || (dvb_time > (time_t) 1325376000)) {
				sendTimeEvent(time_ntp, dvb_time);
				xprintf("%s: Time set via %s, going to sleep for %d seconds.\n", name.c_str(),
						time_ntp ? "NTP" : first_time ? "DVB (TDT)" : "DVB (TOT)", sleep_time);
			}
			first_time = false;
		} else {
			xprintf("%s: Time set FAILED (enabled: ntp %d dvb %d)\n", name.c_str(), ntpenable, dvb_time_update);
			if(!timeset && first_time)
				sleep_time = 1;
		}
		sendToSleepNow = true;
	}
	delete[] static_buf;
	printf("[sectionsd] timeThread stopped\n");
}

/********************************************************************************/
/* abstract CSectionThread functions						*/
/********************************************************************************/
/* sleep for sleep_time seconds, forever if sleep_time = 0 */
int CSectionThread::Sleep()
{
	int rs;
	struct timespec abs_wait;
	struct timeval now;

	if (sleep_time) {
		gettimeofday(&now, NULL);
		TIMEVAL_TO_TIMESPEC(&now, &abs_wait);
		abs_wait.tv_sec += sleep_time;
	}
	dprintf("%s: going to sleep for %d seconds...\n", name.c_str(), sleep_time);
	pthread_mutex_lock(&start_stop_mutex);

	beforeWait();
	if (sleep_time)
		rs = pthread_cond_timedwait( &change_cond, &start_stop_mutex, &abs_wait );
	else
		rs = pthread_cond_wait(&change_cond, &start_stop_mutex);

	afterWait();

	pthread_mutex_unlock( &start_stop_mutex );
	return rs;
}

/* common thread main function */
void CSectionThread::run()
{
	xprintf("%s::run:: starting, pid %d (%lu)\n", name.c_str(), getpid(), pthread_self());
	const std::string tn = ("sd:" + name).c_str();
	set_threadname(tn.c_str());
	if (sections_debug)
		dump_sched_info(name);

	addFilters();

	real_pauseCounter = 1;
	if (wait_for_time) {
		threadTIME.waitForTimeset();
		time_t now = time(NULL);
		xprintf("%s::run:: time set: %s", name.c_str(), ctime(&now));
	}
	real_pauseCounter = 0;

	DMX::start();

	while (running) {
		if (shouldSleep()) {
#ifdef DEBUG_SECTION_THREADS
			xprintf("%s: going to sleep %d seconds, running %d scanning %d blacklisted %d events %d\n",
					name.c_str(), sleep_time, running, scanning, channel_is_blacklisted, event_count);
#endif
			event_count = 0;

			beforeSleep();
			int rs = 0;
			do {
				real_pause();
				rs = Sleep();
#ifdef DEBUG_SECTION_THREADS
				xprintf("%s: wakeup, running %d scanning %d blacklisted %d reason %d\n",
						name.c_str(), running, scanning, channel_is_blacklisted, rs);
#endif
			} while (checkSleep());

			if (!running)
				break;

			afterSleep();

			if (rs == ETIMEDOUT)
				change(0); // -> restart, FIXME

			sendToSleepNow = false;
		}

		processSection();

		time_t zeit = time_monotonic();
		bool need_change = false;

		if (timeoutsDMX < 0 || timeoutsDMX >= skipTimeouts) {
#ifdef DEBUG_SECTION_THREADS
			xprintf("%s: skipping to next filter %d from %d (timeouts %d)\n",
				name.c_str(), filter_index+1, (int)filters.size(), timeoutsDMX);
#endif
			if (timeoutsDMX == -3)
				sendToSleepNow = true;
			else
				need_change = true;

			timeoutsDMX = 0;
		}
		if (zeit > lastChanged + skipTime) {
#ifdef DEBUG_SECTION_THREADS
			xprintf("%s: skipping to next filter %d from %d (seconds %d)\n", 
				name.c_str(), filter_index+1, (int)filters.size(), (int)(zeit - lastChanged));
#endif
			need_change = true;
		}
		if (running && need_change && scanning) {
			readLockMessaging();
			if (!next_filter())
				sendToSleepNow = true;
			unlockMessaging();
		}
	} // while running
	delete[] static_buf;
	cleanup();
	printf("[sectionsd] %s stopped\n", name.c_str());
}

/********************************************************************************/
/* abstract CEventsThread functions						*/
/********************************************************************************/
bool CEventsThread::addEvents()
{
	SIsectionEIT eit(static_buf);

	if (!eit.is_parsed())
		return false;

	dprintf("[%s] adding %d events (begin)\n", name.c_str(), (int)eit.events().size());
	time_t zeit = time(NULL);

	for (SIevents::const_iterator e = eit.events().begin(); e != eit.events().end(); ++e) {
		if (!(e->times.empty())) {
#if 0
			if ( ( e->times.begin()->startzeit < zeit + secondsToCache ) &&
					( ( e->times.begin()->startzeit + (long)e->times.begin()->dauer ) > zeit - oldEventsAre ) &&
					( e->times.begin()->dauer < 60 ) ) {
				char x_startTime[10];
				struct tm *x_tmStartTime = localtime(&e->times.begin()->startzeit);
				strftime(x_startTime, sizeof(x_startTime)-1, "%H:%M", x_tmStartTime );
				printf("####[%s - #%d] - startzeit: %s, dauer: %d, channel_id: 0x%llX\n", __FUNCTION__, __LINE__, x_startTime, e->times.begin()->dauer, e->get_channel_id());
			}
#endif
			if ( ( e->times.begin()->startzeit < zeit + secondsToCache ) &&
					( ( e->times.begin()->startzeit + (long)e->times.begin()->dauer ) > zeit - oldEventsAre ) &&
					( e->times.begin()->dauer > 1 ) )
			{
				addEvent(*e, wait_for_time ? zeit: 0, e->table_id == 0x4e);
				event_count++;
			}
		} else {
			// pruefen ob nvod event
			readLockServices();
			MySIservicesNVODorderUniqueKey::iterator si = mySIservicesNVODorderUniqueKey.find(e->get_channel_id());

			if (si != mySIservicesNVODorderUniqueKey.end()) {
				// Ist ein nvod-event
				writeLockEvents();

				for (SInvodReferences::iterator i = si->second->nvods.begin(); i != si->second->nvods.end(); ++i)
					mySIeventUniqueKeysMetaOrderServiceUniqueKey.insert(std::make_pair(i->uniqueKey(), e->uniqueKey()));

				unlockEvents();
				addNVODevent(*e);
			}
			unlockServices();
		}
	} // for
	return true;
}

bool CCNThread::shouldSleep()
{
	if (!scanning || channel_is_blacklisted)
		return true;
	if (!sendToSleepNow)
		return false;
	if (eit_version != 0xff)
		return true;

	/* on first retry, restart the demux. I'm not sure if it is a driver bug
	 * or a bug in our logic, but without this, I'm sometimes missing CN events
	 * and / or the eit_version and thus the update filter will stop working */
	if (++eit_retry < 2) {
		xprintf("%s::%s first retry (%d) -> restart demux\n", name.c_str(), __func__, eit_retry);
		change(0); /* this also resets lastChanged */
	}
	/* ugly, this has been checked before. But timeoutsDMX can be < 0 for multiple reasons,
	 * and only skipTime should send CNThread finally to sleep if eit_version is not found */
	time_t since = time_monotonic() - lastChanged;
	if (since > skipTime) {
		xprintf("%s::%s timed out after %lds -> going to sleep\n", name.c_str(), __func__, since);
		return true;
	}
	/* retry */
	sendToSleepNow = false;
	return false;
}

/* default check if thread should go to sleep */
bool CEventsThread::shouldSleep()
{
	return (sendToSleepNow || !scanning || channel_is_blacklisted);
}

/* default check if thread should continue to sleep */
bool CEventsThread::checkSleep()
{
	return (running && (!scanning || channel_is_blacklisted));
}

/* default section process */
void CEventsThread::processSection()
{
	int rc = getSection(static_buf, timeoutInMSeconds, timeoutsDMX);
	if (rc <= 0)
		return;
	addEvents();
}

/********************************************************************************/
/* EIT thread to read other TS CN + all scheduled events 			*/
/********************************************************************************/
CEitThread::CEitThread()
	: CEventsThread("eitThread")
{
}

CEitThread::CEitThread(std::string tname, unsigned short pid)
	: CEventsThread(tname, pid)
{
}

/* EIT thread hooks */
void CEitThread::addFilters()
{
	/* These filters are a bit tricky (index numbers):
	   - 0   Dummy filter, to make this thread sleep for some seconds
	   - 1   then get other TS's current/next (this TS's cur/next are
	   handled in dmxCN)
	   - 2/3 then get scheduled events on this TS
	   - 4   then get the other TS's scheduled events,
	   - 4ab (in two steps to reduce the POLLERRs on the DMX device)
	   */
	//addfilter(0x00, 0x00); //0 dummy filter
	addfilter(0x50, 0xf0); //1  current TS, scheduled
	addfilter(0x4f, 0xff); //2  other TS, current/next
#if 1
	addfilter(0x60, 0xf1); //3a other TS, scheduled, even
	addfilter(0x61, 0xf1); //3b other TS, scheduled, odd
#else
	addfilter(0x60, 0xf0); //3  other TS, scheduled
#endif
}

void CEitThread::beforeSleep()
{
	writeLockMessaging();
	messaging_zap_detected = false;
	unlockMessaging();
	if (scanning && current_channel_id) {
		eventServer->sendEvent(CSectionsdClient::EVT_EIT_COMPLETE,
				CEventServer::INITID_SECTIONSD,
				&current_service,
				sizeof(messaging_current_servicekey));
	}
	if(notify_complete)
		system(CONFIGDIR "/epgdone.sh");
}

/********************************************************************************/
/* CN thread to read current TS CN events 					*/
/********************************************************************************/
CCNThread::CCNThread()
	: CEventsThread("cnThread")
{
	sleep_time = 0;
	cache = false;
	skipTimeouts = 5000 / timeoutInMSeconds; // 5 seconds
	skipTime = TIME_EIT_VERSION_WAIT;

	updating = false;
	eitDmx = new cDemux(0);
	eit_retry = 0;
}

/* CN thread hooks */
void CCNThread::cleanup()
{
	delete eitDmx;
}

void CCNThread::addFilters()
{
	addfilter(0x4e, 0xff); //0  current TS, current/next
}

void CCNThread::beforeWait()
{
	xprintf("%s: eit update filter, ch 0x%012" PRIx64 ", current ver 0x%02x  got events %d (%s)\n",
			name.c_str(), messaging_current_servicekey, eit_version, messaging_have_CN,
			updating ? "active" : "not active");

	if (updating || eit_version == 0xff)
		return;

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];
	unsigned char mode[DMX_FILTER_SIZE];
	memset(&filter, 0, DMX_FILTER_SIZE);
	memset(&mask, 0, DMX_FILTER_SIZE);
	memset(&mode, 0, DMX_FILTER_SIZE);

	filter[0] = 0x4e;   /* table_id */
	filter[1] = (unsigned char)(current_service >> 8);
	filter[2] = (unsigned char) current_service;

	mask[0] = 0xFF;
	mask[1] = 0xFF;
	mask[2] = 0xFF;

	filter[3] = (eit_version << 1) | 0x01;
	mask[3] = (0x1F << 1) | 0x01;
	mode[3] = 0x1F << 1;

	update_mutex.lock();
	eitDmx->Open(DMX_PSI_CHANNEL);
	eitDmx->sectionFilter(0x12, filter, mask, 4, 0 /*timeout*/, mode);
	updating = true;
	update_mutex.unlock();
}

void CCNThread::stopUpdate()
{
	xprintf("%s: stop eit update filter (%s)\n", name.c_str(), updating ? "active" : "not active");
	update_mutex.lock();
	if (updating) {
		updating = false;
		eitDmx->Close();
	}
	update_mutex.unlock();
}

void CCNThread::afterWait()
{
	stopUpdate();
}

void CCNThread::beforeSleep()
{
	if (sendToSleepNow && messaging_have_CN == 0x00) {
		/* send a "no epg" event anyway before going to sleep */
		sendCNEvent();
	}
	eit_retry = 0;
}

void CCNThread::processSection()
{
	int rc = getSection(static_buf, timeoutInMSeconds, timeoutsDMX);
	if (rc <= 0)
		return;

	addEvents();
	readLockMessaging();
	if (messaging_got_CN != messaging_have_CN) {
		unlockMessaging();
		writeLockMessaging();
		messaging_have_CN = messaging_got_CN;
		unlockMessaging();

#ifdef DEBUG_CN_THREAD
		xprintf("%s: have CN: timeoutsDMX %d messaging_have_CN %x messaging_got_CN %x\n",
				name.c_str(), timeoutsDMX, messaging_have_CN, messaging_got_CN);
#endif
		dprintf("[cnThread] got current_next (0x%x) - sending event!\n", messaging_have_CN);
		sendCNEvent();
		lastChanged = time_monotonic();
	}
	else
		unlockMessaging();
}

/* CN specific functions */
bool CCNThread::checkUpdate()
{
	unsigned char buf[MAX_SECTION_LENGTH];

	update_mutex.lock();
	if (!updating) {
		update_mutex.unlock();
		return false;
	}

	int ret = eitDmx->Read(buf, MAX_SECTION_LENGTH, 10);
	update_mutex.unlock();

	if (ret > 0) {
		LongSection section(buf);
		xprintf("%s: eit update filter: ### new version 0x%02x ###, Activate thread\n",
				name.c_str(), section.getVersionNumber());

		writeLockMessaging();
		messaging_have_CN = 0x00;
		messaging_got_CN = 0x00;
		unlockMessaging();
		return true;
	}
	return false;
}

void CCNThread::sendCNEvent()
{
	eventServer->sendEvent(CSectionsdClient::EVT_GOT_CN_EPG,
			CEventServer::INITID_SECTIONSD,
			&messaging_current_servicekey,
			sizeof(messaging_current_servicekey));
}

#ifdef ENABLE_FREESATEPG
/********************************************************************************/
/* Freesat EIT thread 								*/
/********************************************************************************/
CFreeSatThread::CFreeSatThread()
	: CEventsThread("freeSatThread", 3842)
{
	skipTime = TIME_FSEIT_SKIPPING;
};

/* Freesat hooks */
void CFreeSatThread::addFilters()
{
	//other TS, scheduled, freesat epg is only broadcast using table_ids 0x60 (scheduled) and 0x61 (scheduled later)
	addfilter(0x60, 0xfe); 
}
#endif

#ifdef ENABLE_SDT
static bool addService(const SIservice &s, const int is_actual)
{
	bool already_exists;
	bool is_new = false;

	readLockServices();
	MySIservicesOrderUniqueKey::iterator si = mySIservicesOrderUniqueKey.find(s.uniqueKey());
	already_exists = (si != mySIservicesOrderUniqueKey.end());
	unlockServices();

	if ( (!already_exists) || ((is_actual & 7) && (!si->second->is_actual)) ) {
		if (already_exists)
		{
			writeLockServices();
			mySIservicesOrderUniqueKey.erase(s.uniqueKey());
			unlockServices();
		}

		SIservice *sp = new SIservice(s);

		if (!sp)
		{
			printf("[sectionsd::addService] new SIservice failed.\n");
			return false;
		}

		SIservicePtr sptr(sp);

		sptr->is_actual = is_actual;

		writeLockServices();
		mySIservicesOrderUniqueKey.insert(std::make_pair(sptr->uniqueKey(), sptr));
		unlockServices();

		if (!sptr->nvods.empty())
		{
			writeLockServices();
			mySIservicesNVODorderUniqueKey.insert(std::make_pair(sptr->uniqueKey(), sptr));
			unlockServices();
		}
		is_new = true;
	}
	return is_new;
}

/********************************************************************************/
/* SDT thread to cache actual/other TS service ids				*/
/********************************************************************************/
CSdtThread::CSdtThread()
	: CSectionThread("sdtThread", 0x11)
{
	skipTime = TIME_SDT_NONEWDATA;
	sleep_time = TIME_SDT_SCHEDULED_PAUSE;
	cache = false;
	wait_for_time = false;
};

/* SDT thread hooks */
void CSdtThread::addFilters()
{
	addfilter(0x42, 0xfb ); //SDT actual = 0x42 + SDT other = 0x46
}

bool CSdtThread::shouldSleep()
{
	return (sendToSleepNow || !scanning);
}

bool CSdtThread::checkSleep()
{
	return (running && !scanning);
}

void CSdtThread::processSection()
{
	int rc = getSection(static_buf, timeoutInMSeconds, timeoutsDMX);
	if (rc <= 0)
		return;

	if (addServices())
		lastChanged = time_monotonic();
}

/* SDT specific */
bool CSdtThread::addServices()
{
	bool is_new = false;

	LongSection sec(static_buf);
	uint8_t table_id = sec.getTableId();
	if ((table_id == 0x42) || (table_id == 0x46)) {
		SIsectionSDT sdt(static_buf);

		bool is_actual = (sdt.getTableId() == 0x42) ? 1 : 0;

		if (is_actual && !sdt.getLastSectionNumber())
			is_actual = 2;

		is_actual = (is_actual | 8);

		for (SIservices::iterator s = sdt.services().begin(); s != sdt.services().end(); ++s) {
			if (addService(*s, is_actual))
				is_new = true;
			event_count++;
		}
	}
	return is_new;
}
#endif

/* helper function for the housekeeping-thread */
static void print_meminfo(void)
{
	if (!sections_debug)
		return;

	comp_malloc_stats(NULL);
}

//---------------------------------------------------------------------
// housekeeping-thread
// does cleaning on fetched datas
//---------------------------------------------------------------------
static void *houseKeepingThread(void *)
{
	int count = 0, scount = 0, ecount = 0;
	set_threadname("sd:housekeeping");

	dprintf("housekeeping-thread started.\n");
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	while (!sectionsd_stop)
	{
		if (count == META_HOUSEKEEPING_COUNT) {
			dprintf("meta housekeeping - deleting all transponders, services, bouquets.\n");
			deleteSIexceptEPG();
			count = 0;
		}

		int i = HOUSEKEEPING_SLEEP;

		while (i > 0 && !sectionsd_stop) {
			sleep(1);
			i--;
		}
		if (sectionsd_stop)
			break;

		if (!scanning) {
			scount++;
			if (scount < STANDBY_HOUSEKEEPING_COUNT)
				continue;
		}
		scount = 0;

		dprintf("housekeeping.\n");

		removeOldEvents(oldEventsAre); // alte Events

		ecount++;
		if (ecount == EPG_SERVICE_FREQUENTLY_COUNT)
		{
			if (epg_save_frequently > 0)
			{
				std::string d = epg_dir;
				if (d.length() > 1)
				{
					std::string::iterator it = d.end() - 1;
					if (*it == '/')
						d.erase(it);
				}
				writeEventsToFile(d.c_str());
			}
			if (epg_read_frequently > 0)
			{
				pthread_t thrInsert;
				pthread_attr_t attr;
				pthread_attr_init(&attr);
				pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
				std::string d = epg_dir + "/";

				printf("[%s]: %s\n",__func__,d.c_str());

				if (pthread_create (&thrInsert, &attr, insertEventsfromFile, (void *)d.c_str() ))
					{
					perror("sectionsd: pthread_create()");
					}
				pthread_attr_destroy(&attr);
			}
			ecount = 0;
		}

		readLockEvents();
		dprintf("Number of sptr events (event-ID): %u\n", (unsigned)mySIeventsOrderUniqueKey.size());
		dprintf("Number of sptr events (service-id, start time, event-id): %u\n", (unsigned)mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.size());
		dprintf("Number of sptr events (end time, service-id, event-id): %u\n", (unsigned)mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.size());
		dprintf("Number of sptr nvod events (event-ID): %u\n", (unsigned)mySIeventsNVODorderUniqueKey.size());
		dprintf("Number of cached meta-services: %u\n", (unsigned)mySIeventUniqueKeysMetaOrderServiceUniqueKey.size());
		unlockEvents();

		print_meminfo();

		count++;
	} // for endlos
	dprintf("housekeeping-thread ended.\n");

	pthread_exit(NULL);
}

CEitManager* CEitManager::manager = NULL;
OpenThreads::Mutex CEitManager::m;

CEitManager::CEitManager()
{
	running = false;
}

CEitManager::~CEitManager()
{
}

CEitManager * CEitManager::getInstance()
{
	m.lock();
	if(manager == NULL)
		manager = new CEitManager();
	m.unlock();
	return manager;
}

bool CEitManager::Start()
{
	xprintf("[sectionsd] start\n");
	if(running)
		return false;

	ntpserver = config.network_ntpserver;
	ntprefresh = config.network_ntprefresh;
	ntpenable = config.network_ntpenable;
	ntp_system_cmd = ntp_system_cmd_prefix + ntpserver;

	secondsToCache = config.epg_cache *24*60L*60L; //days
	secondsExtendedTextCache = config.epg_extendedcache*60L*60L; //hours
	oldEventsAre = config.epg_old_events*60L*60L; //hours
	max_events = config.epg_max_events;
	epg_save_frequently = config.epg_save_frequently;
	epg_read_frequently = config.epg_read_frequently;

	if (find_executable("ntpdate").empty()){
		ntp_system_cmd_prefix = find_executable("ntpd");
		if (!ntp_system_cmd_prefix.empty()){
			ntp_system_cmd_prefix += " -n -q -p ";
			ntp_system_cmd = ntp_system_cmd_prefix + ntpserver;
		}
		else{
			printf("[sectionsd] NTP Error: time sync not possible, ntpdate/ntpd not found\n");
			ntpenable = false;
		}
	}

	printf("[sectionsd] Caching: %d days, %d hours Extended Text, max %d events, Events are old %d hours after end time\n",
		config.epg_cache, config.epg_extendedcache, config.epg_max_events, config.epg_old_events);
	printf("[sectionsd] NTP: %s, command %s\n", ntpenable ? "enabled" : "disabled", ntp_system_cmd.c_str());

	xml_epg_filter = readEPGFilter();

	if (!sectionsd_server.prepare(SECTIONSD_UDS_NAME)) {
		fprintf(stderr, "[sectionsd] failed to prepare basic server\n");
		return false;
	}

	eventServer = new CEventServer;

	running = true;
	return (OpenThreads::Thread::start() == 0);
}

bool CEitManager::Stop()
{
	if(!running)
		return false;
	running = false;
	int ret = (OpenThreads::Thread::join() == 0);
	return ret;
}

void CEitManager::run()
{
	pthread_t /*threadTOT,*/ threadHouseKeeping;
	int rc;

	xprintf("[sectionsd] starting\n");
	set_threadname("sd:eitmanager");
printf("SIevent size: %d\n", (int)sizeof(SIevent));

	/* "export NO_SLOW_ADDEVENT=true" to disable this */
	slow_addevent = (getenv("NO_SLOW_ADDEVENT") == NULL);
	if (slow_addevent)
		printf("====> USING SLOW ADDEVENT. export 'NO_SLOW_ADDEVENT=1' to avoid <===\n");

	/* for debugging / benchmarking, "export STARTUP_WAIT=true" to wait with startup for
	 * the EPG loading to finish
	 * this wil fail badly if no EPG saving / loading is configured! */
	reader_ready = (getenv("STARTUP_WAIT") == NULL);
	if (!reader_ready)
		printf("====> sectionsd waiting with startup until saved EPG is read <===\n");

	SIlanguage::loadLanguages();

	tzset(); // TZ auswerten

	readDVBTimeFilter();
	readEncodingFile();

	/* threads start left here for now, if any problems found, will be moved to Start() */
	threadTIME.Start();
	threadEIT.Start();
	threadCN.Start();
#ifdef ENABLE_VIASATEPG
	threadVSEIT.Start();
#endif

#ifdef ENABLE_FREESATEPG
	threadFSEIT.Start();
#endif
#ifdef ENABLE_SDT
	threadSDT.Start();
#endif

	// housekeeping-Thread starten
	rc = pthread_create(&threadHouseKeeping, 0, houseKeepingThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create housekeeping-thread (rc=%d)\n", rc);
		return;
	}

	if (sections_debug)
		dump_sched_info("main");

	while (running && sectionsd_server.run(sectionsd_parse_command, sectionsd::ACTVERSION, true)) {
		sched_yield();
		if (threadCN.checkUpdate()) {
			sched_yield();
			threadCN.change(0);
			sched_yield();
		}

		sched_yield();
		/* 10 ms is the minimal timeslice anyway (HZ = 100), so let's
		   wait 20 ms at least to lower the CPU load */
		usleep(20000);
	}

	printf("[sectionsd] stopping...\n");

	threadEIT.StopRun();
	threadCN.StopRun();
	threadTIME.StopRun();
#ifdef ENABLE_VIASATEPG
	threadVSEIT.StopRun();
#endif
#ifdef ENABLE_SDT
	threadSDT.StopRun();
#endif
#ifdef ENABLE_FREESATEPG
	threadFSEIT.StopRun();
#endif

	xprintf("broadcasting...\n");

	threadTIME.setTimeSet();

	xprintf("pausing...\n");

	/* sometimes, pthread_cancel seems to not work, maybe it is a bad idea to mix
	 * plain pthread and OpenThreads? */
	sectionsd_stop = 1;
	pthread_cancel(threadHouseKeeping);
	xprintf("join Housekeeping\n");
	pthread_join(threadHouseKeeping, NULL);

	xprintf("join TOT\n");
	threadTIME.Stop();

	xprintf("join EIT\n");
	threadEIT.Stop();

	xprintf("join CN\n");
	threadCN.Stop();
#ifdef ENABLE_VIASATEPG
	xprintf("join VSEIT\n");
	threadVSEIT.Stop();
#endif

#ifdef ENABLE_SDT
	xprintf("join SDT\n");
	threadSDT.Stop();
#endif
#ifdef ENABLE_FREESATEPG
	xprintf("join FSEIT\n");
	threadFSEIT.Stop();
#endif
#ifdef EXIT_CLEANUP
	xprintf("[sectionsd] cleanup...\n");
	delete myNextEvent;
	delete myCurrentEvent;
	FreeMemory();
#endif
	xprintf("[sectionsd] stopped\n");
}

/* was: commandAllEventsChannelID sendAllEvents */
void CEitManager::getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search, std::string search_text,bool all_chann, int genre,int fsk)
{
	dprintf("sendAllEvents for " PRINTF_CHANNEL_ID_TYPE "\n", serviceUniqueKey);
	if(!eList.empty() && search == 0)//skip on search mode
		eList.clear();

	t_channel_id serviceUniqueKey64 = serviceUniqueKey& 0xFFFFFFFFFFFFULL; //0xFFFFFFFFFFFFULL for CREATE_CHANNEL_ID64
	if(serviceUniqueKey64 == 0 && !all_chann)
		return;

	// service Found
	readLockEvents();
	int serviceIDfound = 0;
	if (!search_text.empty())
		std::transform(search_text.begin(), search_text.end(), search_text.begin(), tolower);

	for (MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin(); e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end(); ++e)
	{
		if ((*e)->get_channel_id() == serviceUniqueKey64 || (all_chann)) {
			serviceIDfound = 1;

			bool copy = true;
			if(search){
				if ((search == 1 /*EventList::SEARCH_EPG_TITLE*/) || (search == 5 /*EventList::SEARCH_EPG_ALL*/))
				{
					std::string eName = (*e)->getName();
					std::transform(eName.begin(), eName.end(), eName.begin(), tolower);
					copy = (eName.find(search_text) != std::string::npos);
				}
				if ((search == 2 /*EventList::SEARCH_EPG_INFO1*/) || (!copy && (search == 5 /*EventList::SEARCH_EPG_ALL*/)))
				{
					std::string eText = (*e)->getText();
					std::transform(eText.begin(), eText.end(), eText.begin(), tolower);
					copy = (eText.find(search_text) != std::string::npos);
				}
				if ((search == 3 /*EventList::SEARCH_EPG_INFO2*/) || (!copy && (search == 5 /*EventList::SEARCH_EPG_ALL*/)))
				{
					std::string eExtendedText = (*e)->getExtendedText();
					std::transform(eExtendedText.begin(), eExtendedText.end(), eExtendedText.begin(), tolower);
					copy = (eExtendedText.find(search_text) != std::string::npos);
				}
				if(copy && genre != 0xFF)
				{
					if((*e)->classifications.content==0)
						copy=false;
					if(copy && ((*e)->classifications.content < (genre & 0xf0 ) || (*e)->classifications.content > genre))
						copy=false;
				}
				if(copy && fsk != 0)
				{
					if(fsk<0)
					{
						if( (*e)->getFSK() > abs(fsk))
							copy=false;
					}else if( (*e)->getFSK() < fsk)
						copy=false;
				}
			}
			if(copy) {
				for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t)
				{
					CChannelEvent aEvent;
					aEvent.eventID = (*e)->uniqueKey();
					aEvent.startTime = t->startzeit;
					aEvent.duration = t->dauer;
					aEvent.description = (*e)->getName();
					if (((*e)->getText()).empty())
						aEvent.text = (*e)->getExtendedText().substr(0, 120);
					else
						aEvent.text = (*e)->getText();
					if(all_chann)//hack for all channel search
						aEvent.channelID = (*e)->get_channel_id();
					else
						aEvent.channelID = serviceUniqueKey;
					eList.push_back(aEvent);
				}
			} // if = serviceID
		}
		else if ( serviceIDfound )
			break; // sind nach serviceID und startzeit sortiert -> nicht weiter suchen
	}
	unlockEvents();
}

/* invalidate current/next events, if current event times expired */
void CEitManager::checkCurrentNextEvent(void)
{
	time_t curtime = time(NULL);
	writeLockEvents();
	if (scanning || !myCurrentEvent || myCurrentEvent->times.empty()) {
		unlockEvents();
		return;
	}
	if ((long)(myCurrentEvent->times.begin()->startzeit + myCurrentEvent->times.begin()->dauer) < (long)curtime) {
		delete myCurrentEvent;
		myCurrentEvent = NULL;
		delete myNextEvent;
		myNextEvent = NULL;
	}
	unlockEvents();
}

/* send back the current and next event for the channel id passed to it
 * Works like that:
 * - if the currently running program is requested, return myCurrentEvent and myNextEvent,
 *   if they are present (filled in by cnThread)
 * - if one or both of those are not present, or if a different program than the currently
 *   running is requested, search the missing events in the list of events gathered by the
 *   EIT and PPT threads, based on the current time.
 *
 * TODO: the handling of "flag" should be vastly simplified.
 */
/* was: commandCurrentNextInfoChannelID */
void CEitManager::getCurrentNextServiceKey(t_channel_id uniqueServiceKey, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next )
{
	dprintf("[sectionsd] Request of current/next information for " PRINTF_CHANNEL_ID_TYPE "\n", uniqueServiceKey);

	SIevent currentEvt;
	SIevent nextEvt;
	unsigned flag = 0, flag2=0;
	/* ugly hack: retry fetching current/next by restarting dmxCN if this is true */
	//bool change = false;//TODO remove ?

	uniqueServiceKey &= 0xFFFFFFFFFFFFULL;

	checkCurrentNextEvent();

	readLockEvents();
	/* if the currently running program is requested... */
	if (uniqueServiceKey == messaging_current_servicekey) {
		/* ...check for myCurrentEvent and myNextEvent */
		if (!myCurrentEvent) {
			dprintf("!myCurrentEvent ");
			//change = true;
			flag |= CSectionsdClient::epgflags::not_broadcast;
		} else {
			currentEvt = *myCurrentEvent;
			flag |= CSectionsdClient::epgflags::has_current; // aktuelles event da...
			flag |= CSectionsdClient::epgflags::has_anything;
		}
		if (!myNextEvent) {
			dprintf("!myNextEvent ");
			//change = true;
		} else {
			nextEvt = *myNextEvent;
			if (flag & CSectionsdClient::epgflags::not_broadcast) {
				dprintf("CSectionsdClient::epgflags::has_no_current\n");
				flag = CSectionsdClient::epgflags::has_no_current;
			}
			flag |= CSectionsdClient::epgflags::has_next; // aktuelles event da...
			flag |= CSectionsdClient::epgflags::has_anything;
		}
	}

	//dprintf("flag: 0x%x, has_current: 0x%x has_next: 0x%x\n", flag, CSectionsdClient::epgflags::has_current, CSectionsdClient::epgflags::has_next);
	/* if another than the currently running program is requested, then flag will still be 0
	   if either the current or the next event is not found, this condition will be true, too.
	   */
	if ((flag & (CSectionsdClient::epgflags::has_current|CSectionsdClient::epgflags::has_next)) !=
			(CSectionsdClient::epgflags::has_current|CSectionsdClient::epgflags::has_next)) {
		//dprintf("commandCurrentNextInfoChannelID: current or next missing!\n");
		SItime zeitEvt1(0, 0);
		if (!(flag & CSectionsdClient::epgflags::has_current)) {
			currentEvt = findActualSIeventForServiceUniqueKey(uniqueServiceKey, zeitEvt1, 0, &flag2);
		} else if(!currentEvt.times.empty()) {
			zeitEvt1.startzeit = currentEvt.times.begin()->startzeit;
			zeitEvt1.dauer = currentEvt.times.begin()->dauer;
		}
		SItime zeitEvt2(zeitEvt1);

		if (currentEvt.getName().empty() && flag2 != 0)
		{
			dprintf("commandCurrentNextInfoChannelID change1\n");
			//change = true;
		}

		if (currentEvt.service_id != 0)
		{	//Found
			flag &= (CSectionsdClient::epgflags::has_no_current|CSectionsdClient::epgflags::not_broadcast)^(unsigned)-1;
			flag |= CSectionsdClient::epgflags::has_current;
			flag |= CSectionsdClient::epgflags::has_anything;
			dprintf("[sectionsd] current EPG found. service_id: %x, flag: 0x%x\n",currentEvt.service_id, flag);

			if (!(flag & CSectionsdClient::epgflags::has_next)) {
				dprintf("*nextEvt not from cur/next V1!\n");
				nextEvt = findNextSIevent(currentEvt.uniqueKey(), zeitEvt2);
			}
		}
		else
		{	// no current event...
			if ( flag2 & CSectionsdClient::epgflags::has_anything )
			{
				flag |= CSectionsdClient::epgflags::has_anything;
				if (!(flag & CSectionsdClient::epgflags::has_next)) {
					dprintf("*nextEvt not from cur/next V2!\n");
					nextEvt = findNextSIeventForServiceUniqueKey(uniqueServiceKey, zeitEvt2);
				}

				/* FIXME what this code should do ? why search channel id as event key ?? */
#if 0
				if (nextEvt.service_id != 0)
				{
					MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(uniqueServiceKey);

					if (eFirst != mySIeventsOrderUniqueKey.end())
					{
						// this is a race condition if first entry found is == mySIeventsOrderUniqueKey.begin()
						// so perform a check
						if (eFirst != mySIeventsOrderUniqueKey.begin())
							--eFirst;

						if (eFirst != mySIeventsOrderUniqueKey.begin())
						{
							time_t azeit = time(NULL);

							if (!eFirst->second->times.empty() && eFirst->second->times.begin()->startzeit < azeit &&
									eFirst->second->uniqueKey() == nextEvt.uniqueKey() - 1)
								flag |= CSectionsdClient::epgflags::has_no_current;
						}
					}
				}
#endif
			}
		}
		if (nextEvt.service_id != 0)
		{
			flag &= CSectionsdClient::epgflags::not_broadcast^(unsigned)-1;
			dprintf("[sectionsd] next EPG found. service_id: %x, flag: 0x%x\n",nextEvt.service_id, flag);
			flag |= CSectionsdClient::epgflags::has_next;
		}
		else if (flag != 0)
		{
			dprintf("commandCurrentNextInfoChannelID change2 flag: 0x%02x\n", flag);
			//change = true;
		}
	}

	if (currentEvt.service_id != 0)
	{
		/* check for nvod linkage */
		for (unsigned int i = 0; i < currentEvt.linkage_descs.size(); i++)
			if (currentEvt.linkage_descs[i].linkageType == 0xB0)
			{
				fprintf(stderr,"[sectionsd] linkage in current EPG found.\n");
				flag |= CSectionsdClient::epgflags::current_has_linkagedescriptors;
				break;
			}
	} else
		flag |= CSectionsdClient::epgflags::has_no_current;

	time_t now;

	dprintf("currentEvt: '%s' (%04x) nextEvt: '%s' (%04x) flag: 0x%02x\n",
			currentEvt.getName().c_str(), currentEvt.eventID,
			nextEvt.getName().c_str(), nextEvt.eventID, flag);

	CSectionsdClient::sectionsdTime time_cur;
	CSectionsdClient::sectionsdTime time_nxt;
	now = time(NULL);
	time_cur.startzeit = time_cur.dauer = 0;
	if(!currentEvt.times.empty()) {
		time_cur.startzeit = currentEvt.times.begin()->startzeit;
		time_cur.dauer = currentEvt.times.begin()->dauer;
	}
	time_nxt.startzeit = time_nxt.dauer = 0;
	if(!nextEvt.times.empty()) {
		time_nxt.startzeit = nextEvt.times.begin()->startzeit;
		time_nxt.dauer = nextEvt.times.begin()->dauer;
	}
	/* for nvod events that have multiple times, find the one that matches the current time... */
	if (currentEvt.times.size() > 1) {
		for (SItimes::iterator t = currentEvt.times.begin(); t != currentEvt.times.end(); ++t) {
			if ((long)now < (long)(t->startzeit + t->dauer) && (long)now > (long)t->startzeit) {
				time_cur.startzeit = t->startzeit;
				time_cur.dauer =t->dauer;
				break;
			}
		}
	}
	/* ...and the one after that. */
	if (nextEvt.times.size() > 1) {
		for (SItimes::iterator t = nextEvt.times.begin(); t != nextEvt.times.end(); ++t) {
			if ((long)(time_cur.startzeit + time_cur.dauer) <= (long)(t->startzeit)) { // TODO: it's not "long", it's "time_t"
				time_nxt.startzeit = t->startzeit;
				time_nxt.dauer =t->dauer;
				break;
			}
		}
	}

	current_next.current_uniqueKey = currentEvt.uniqueKey();
	current_next.current_zeit.startzeit = time_cur.startzeit;
	current_next.current_zeit.dauer = time_cur.dauer;
	current_next.current_name = currentEvt.getName();

	current_next.next_uniqueKey = nextEvt.uniqueKey();
	current_next.next_zeit.startzeit = time_nxt.startzeit;
	current_next.next_zeit.dauer = time_nxt.dauer;
	current_next.next_name = nextEvt.getName();

	current_next.flags = flag;
	current_next.current_fsk = currentEvt.getFSK();

	unlockEvents();
}

/* commandEPGepgIDshort */
bool CEitManager::getEPGidShort(event_id_t epgID, CShortEPGData * epgdata)
{
	bool ret = false;
	dprintf("Request of current EPG for 0x%" PRIx64 "\n", epgID);

	readLockEvents();

	const SIevent& e = findSIeventForEventUniqueKey(epgID);

	if (e.service_id != 0)
	{	// Event found
		dputs("EPG found.");
		epgdata->title = e.getName();
		epgdata->info1 = e.getText();
		epgdata->info2 = e.getExtendedText();
		ret = true;
	} else
		dputs("EPG not found!");

	unlockEvents();
	return ret;
}

/*was getEPGid commandEPGepgID(int connfd, char *data, const unsigned dataLength) */
/* TODO item / itemDescription */
bool CEitManager::getEPGid(const event_id_t epgID, const time_t startzeit, CEPGData * epgdata)
{
	bool ret = false;
	dprintf("Request of actual EPG for 0x%" PRIx64 " 0x%lx\n", epgID, startzeit);

	const SIevent& evt = findSIeventForEventUniqueKey(epgID);

	epgdata->itemDescriptions.clear();
	epgdata->items.clear();

	readLockEvents();
	if (evt.service_id != 0) { // Event found
		SItimes::iterator t = evt.times.begin();

		for (; t != evt.times.end(); ++t)
			if (t->startzeit == startzeit)
				break;

		if (t == evt.times.end()) {
			dputs("EPG not found!");
		} else {
			dputs("EPG found.");
			epgdata->eventID = evt.uniqueKey();
			epgdata->title = evt.getName();
			epgdata->info1 = evt.getText();
			epgdata->info2 = evt.getExtendedText();
			/* FIXME printf("itemDescription: %s\n", evt.itemDescription.c_str()); */
#ifdef FULL_CONTENT_CLASSIFICATION
			evt.classifications.get(epgdata->contentClassification, epgdata->userClassification);
#else
			epgdata->contentClassification = evt.classifications.content;
			epgdata->userClassification = evt.classifications.user;
#endif
			epgdata->fsk = evt.getFSK();
			epgdata->table_id = evt.table_id;

			epgdata->epg_times.startzeit = t->startzeit;
			epgdata->epg_times.dauer = t->dauer;

			ret = true;
		}
	} else {
		dputs("EPG not found!");
	}
	unlockEvents();
	return ret;
}
/* was  commandActualEPGchannelID(int connfd, char *data, const unsigned dataLength) */
bool CEitManager::getActualEPGServiceKey(const t_channel_id channel_id, CEPGData * epgdata)
{
	bool ret = false;
	SIevent evt;
	SItime zeit(0, 0);

	dprintf("[commandActualEPGchannelID] Request of current EPG for " PRINTF_CHANNEL_ID_TYPE "\n", channel_id);

	t_channel_id uniqueServiceKey = channel_id & 0xFFFFFFFFFFFFULL;
	checkCurrentNextEvent();
	readLockEvents();
	if (uniqueServiceKey == messaging_current_servicekey) {
		if (myCurrentEvent) {
			evt = *myCurrentEvent;
			if(!evt.times.empty()) {
				zeit.startzeit = evt.times.begin()->startzeit;
				zeit.dauer = evt.times.begin()->dauer;
			}
			if (evt.times.size() > 1) {
				time_t now = time(NULL);
				for (SItimes::iterator t = evt.times.begin(); t != evt.times.end(); ++t) {
					if ((long)now < (long)(t->startzeit + t->dauer) && (long)now > (long)t->startzeit) {
						zeit.startzeit = t->startzeit;
						zeit.dauer = t->dauer;
						break;
					}
				}
			}
		}
	}

	if (evt.service_id == 0)
	{
		dprintf("[commandActualEPGchannelID] evt.service_id == 0 ==> no myCurrentEvent!\n");
		evt = findActualSIeventForServiceUniqueKey(uniqueServiceKey, zeit);
	}

	if (evt.service_id != 0)
	{
		dprintf("EPG found.\n");
		epgdata->eventID = evt.uniqueKey();
		epgdata->title = evt.getName();
		epgdata->info1 = evt.getText();
		epgdata->info2 = evt.getExtendedText();
		/* FIXME printf("itemDescription: %s\n", evt.itemDescription.c_str());*/
#ifdef FULL_CONTENT_CLASSIFICATION
		evt.classifications.get(epgdata->contentClassification, epgdata->userClassification);
#else
		epgdata->contentClassification = evt.classifications.content;
		epgdata->userClassification = evt.classifications.user;
#endif

		epgdata->fsk = evt.getFSK();
		epgdata->table_id = evt.table_id;

		epgdata->epg_times.startzeit = zeit.startzeit;
		epgdata->epg_times.dauer = zeit.dauer;

		ret = true;
	} else
		dprintf("EPG not found!\n");

	unlockEvents();
	return ret;
}

bool channel_in_requested_list(t_channel_id * clist, t_channel_id chid, int len)
{
	if(len == 0) return true;
	for(int i = 0; i < len; i++) {
		if((clist[i] & 0xFFFFFFFFFFFFULL) == chid)
			return true;
	}
	return false;
}

/* was static void sendEventList(int connfd, const unsigned char serviceTyp1, const unsigned char serviceTyp2 = 0, int sendServiceName = 1, t_channel_id * chidlist = NULL, int clen = 0) */
void CEitManager::getChannelEvents(CChannelEventList &eList, t_channel_id *chidlist, int clen)
{
	t_channel_id uniqueNow = 0;
	t_channel_id uniqueOld = 0;
	bool found_already = true;
	time_t azeit = time(NULL);

	// showProfiling("sectionsd_getChannelEvents start");
	readLockEvents();

	/* !!! FIX ME: if the box starts on a channel where there is no EPG sent, it hangs!!!	*/
	for (MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin(); e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end(); ++e)
	{
		uniqueNow = (*e)->get_channel_id();

		if (uniqueNow != uniqueOld)
		{
			uniqueOld = uniqueNow;
			if (!channel_in_requested_list(chidlist, uniqueNow, clen))
				continue;

			found_already = false;
		}

		if (!found_already)
		{
			for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t)
			{
				if (t->startzeit <= azeit && azeit <= (long)(t->startzeit + t->dauer))
				{
					//TODO CChannelEvent constructor from SIevent ?
					CChannelEvent aEvent;
					aEvent.eventID = (*e)->uniqueKey();
					aEvent.startTime = t->startzeit;
					aEvent.duration = t->dauer;
					aEvent.description = (*e)->getName();
					if (((*e)->getText()).empty())
						aEvent.text = (*e)->getExtendedText().substr(0, 120);
					else
						aEvent.text = (*e)->getText();
					eList.push_back(aEvent);

					found_already = true;
					break;
				}
			}
			if(found_already && clen && (clen == (int) eList.size()))
				break;
		}
	}

	// showProfiling("sectionsd_getChannelEvents end");
	unlockEvents();
}

/*was static void commandComponentTagsUniqueKey(int connfd, char *data, const unsigned dataLength) */
bool CEitManager::getComponentTagsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::ComponentTagList& tags)
{
	bool ret = false;
	dprintf("Request of ComponentTags for 0x%" PRIx64 "\n", uniqueKey);

	tags.clear();

	readLockEvents();

	MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end()) {
		CSectionsdClient::responseGetComponentTags response;
		ret = true;

		for (SIcomponents::iterator cmp = eFirst->second->components.begin(); cmp != eFirst->second->components.end(); ++cmp) {
			response.component = cmp->getComponentName();
			response.componentType = cmp->componentType;
			response.componentTag = cmp->componentTag;
			response.streamContent = cmp->streamContent;

			tags.insert(tags.end(), response);
		}
	}

	unlockEvents();
	return ret;
}

/* was static void commandLinkageDescriptorsUniqueKey(int connfd, char *data, const unsigned dataLength) */
bool CEitManager::getLinkageDescriptorsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::LinkageDescriptorList& descriptors)
{
	bool ret = false;
	dprintf("Request of LinkageDescriptors for 0x%" PRIx64 "\n", uniqueKey);

	descriptors.clear();
	readLockEvents();

	MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end()) {
		for (SIlinkage_descs::iterator linkage_desc = eFirst->second->linkage_descs.begin(); linkage_desc != eFirst->second->linkage_descs.end(); ++linkage_desc)
		{
			if (linkage_desc->linkageType == 0xB0) {

				CSectionsdClient::responseGetLinkageDescriptors response;
				response.name = linkage_desc->name.c_str();
				response.transportStreamId = linkage_desc->transportStreamId;
				response.originalNetworkId = linkage_desc->originalNetworkId;
				response.serviceId = linkage_desc->serviceId;
				descriptors.insert( descriptors.end(), response);
				ret = true;
			}
		}
	}

	unlockEvents();
	return ret;
}

/* was static void commandTimesNVODservice(int connfd, char *data, const unsigned dataLength) */
bool CEitManager::getNVODTimesServiceKey(const t_channel_id channel_id, CSectionsdClient::NVODTimesList& nvod_list)
{
	bool ret = false;
	dprintf("Request of NVOD times for " PRINTF_CHANNEL_ID_TYPE "\n", channel_id);

	t_channel_id uniqueServiceKey = channel_id & 0xFFFFFFFFFFFFULL;
	nvod_list.clear();

	readLockServices();
	readLockEvents();

	MySIservicesNVODorderUniqueKey::iterator si = mySIservicesNVODorderUniqueKey.find(uniqueServiceKey);
	if (si != mySIservicesNVODorderUniqueKey.end())
	{
		dprintf("NVODServices: %u\n", (unsigned)si->second->nvods.size());

		if (!si->second->nvods.empty()) {
			for (SInvodReferences::iterator ni = si->second->nvods.begin(); ni != si->second->nvods.end(); ++ni) {
				SItime zeitEvt1(0, 0);
				findActualSIeventForServiceUniqueKey(ni->uniqueKey(), zeitEvt1, 15*60);

				CSectionsdClient::responseGetNVODTimes response;

				response.service_id =  ni->service_id;
				response.original_network_id = ni->original_network_id;
				response.transport_stream_id = ni->transport_stream_id;
				response.zeit.startzeit = zeitEvt1.startzeit;
				response.zeit.dauer = zeitEvt1.dauer;

				nvod_list.insert( nvod_list.end(), response);
				ret = true;
			}
		}
	}

	unlockEvents();
	unlockServices();
	return ret;
}

void CEitManager::setLanguages(const std::vector<std::string>& newLanguages)
{
	SIlanguage::setLanguages(newLanguages);
	SIlanguage::saveLanguages();
}

unsigned CEitManager::getEventsCount()
{
	readLockEvents();
	unsigned anzEvents = mySIeventsOrderUniqueKey.size();
	unlockEvents();
	return anzEvents;
}

void CEitManager::addChannelFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> slock(filter_mutex);
	if (xml_epg_filter)
		return;
	epg_filter_except_current_next = true;
	epg_filter_is_whitelist = true;
	addEPGFilter(onid, tsid, sid);
}

void CEitManager::clearChannelFilters()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> slock(filter_mutex);
	if (xml_epg_filter)
		return;
	clearEPGFilter();
	epg_filter_is_whitelist = false;
}
