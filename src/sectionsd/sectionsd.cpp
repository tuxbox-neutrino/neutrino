//
//  $Id: sectionsd.cpp,v 1.305 2009/07/30 12:41:39 seife Exp $
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
//
//

#include <config.h>
#include <malloc.h>
#include <dmxapi.h>
#include <dmx.h>
#include <debug.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
//#include <sys/resource.h> // getrusage
#include <set>
#include <map>
#include <algorithm>
#include <string>
#include <limits>

#include <sys/wait.h>
#include <sys/time.h>

#include <connection/basicsocket.h>
#include <connection/basicserver.h>

#include <xmltree/xmlinterface.h>
#include <zapit/settings.h>
//#include <zapit/frontend.h>
#include <configfile.h>

// Daher nehmen wir SmartPointers aus der Boost-Lib (www.boost.org)
#include <boost/shared_ptr.hpp>

#include <sectionsdclient/sectionsdMsg.h>
#include <sectionsdclient/sectionsdclient.h>
#include <eventserver.h>
#include <driver/abstime.h>

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#ifdef UPDATE_NETWORKS
#include "SIbouquets.hpp"
#include "SInetworks.hpp"
#endif
#include "SIsections.hpp"
#include "SIlanguage.hpp"

#include "edvbstring.h"
//#include "timerdclient.h"
//#include "../timermanager.h"

// 60 Minuten Zyklus...
#define TIME_EIT_SCHEDULED_PAUSE 60 * 60
// -- 5 Minutes max. pause should improve behavior  (rasc, 2005-05-02)
// #define TIME_EIT_SCHEDULED_PAUSE 5* 60
// Zeit die fuer die gewartet wird, bevor der Filter weitergeschaltet wird, falls es automatisch nicht klappt
#define TIME_EIT_SKIPPING 90

#ifdef ENABLE_FREESATEPG
// a little more time for freesat epg
#define TIME_FSEIT_SKIPPING 240
#endif

//#define MAX_EVENTS 6000
static unsigned int max_events;
// sleep 5 minutes
//#define HOUSEKEEPING_SLEEP (30 * 60)
#define HOUSEKEEPING_SLEEP (5 * 60)
// meta housekeeping after XX housekeepings - every 24h -
#define META_HOUSEKEEPING (24 * 60 * 60) / HOUSEKEEPING_SLEEP

// 12h Pause fr SDT
//#define TIME_SDT_SCHEDULED_PAUSE 12* 60* 60
// -- shorter time for pause should  result in better behavior  (rasc, 2005-05-02)
#define TIME_SDT_SCHEDULED_PAUSE 2* 60* 60
//#define TIME_SDT_SKIPPING 30
//We are very nice here. Start scanning for channels, if the user stays for XX secs on that channel
//#define TIME_SDT_BACKOFF	120
//Sleeping when TIME_SDT_NODATA seconds no NEW section was received
#define TIME_SDT_NONEWDATA	5
//How many BATs shall we read per transponder
#define MAX_BAT 10
//How many other SDTs shall we puzzle per transponder at the same time
//#define MAX_CONCURRENT_OTHER_SDT 5
//How many other SDTs shall we assume per tranponder
//#define MAX_OTHER_SDT 70
#define MAX_SDTs 70
//How many sections can a table consist off?
#define MAX_SECTIONS 0x1f
//Okay, since zapit has got nothing do to with scanning - we read it on our own
#define NEUTRINO_SCAN_SETTINGS_FILE     CONFIGDIR "/scan.conf"

//Set pause for NIT
#define TIME_NIT_SCHEDULED_PAUSE 2* 60* 60
//We are very nice here. Start scanning for channels, if the user stays for XX secs on that channel
//#define TIME_NIT_BACKOFF	20
//Sleeping when TIME_NIT_NODATA seconds no NEW section was received
#define TIME_NIT_NONEWDATA	5
//How many other NITs shall we puzzle per transponder at the same time
//#define MAX_CONCURRENT_OTHER_NIT 5
//How many other SDTs shall we assume per tranponder
//#define MAX_OTHER_NIT 10
#define MAX_NIDs 10

// Timeout bei tcp/ip connections in ms
#define READ_TIMEOUT_IN_SECONDS  2
#define WRITE_TIMEOUT_IN_SECONDS 2

// Gibt die Anzahl Timeouts an, nach der die Verbindung zum DMX neu gestartet wird (wegen evtl. buffer overflow)
// for NIT and SDT threads...
#define RESTART_DMX_AFTER_TIMEOUTS 5

// Timeout in ms for reading from dmx in EIT threads. Dont make this too long
// since we are holding the start_stop lock during this read!
#define EIT_READ_TIMEOUT 100
// Number of DMX read timeouts, after which we check if there is an EIT at all
// for EIT and PPT threads...
#define CHECK_RESTART_DMX_AFTER_TIMEOUTS (2000 / EIT_READ_TIMEOUT) // 2 seconds

// Time in seconds we are waiting for an EIT version number
#define TIME_EIT_VERSION_WAIT		3
// number of timeouts after which we stop waiting for an EIT version number
#define TIMEOUTS_EIT_VERSION_WAIT	(2 * CHECK_RESTART_DMX_AFTER_TIMEOUTS)

// the maximum length of a section (0x0fff) + header (3)
#define MAX_SECTION_LENGTH (0x0fff + 3)

// Wieviele Sekunden EPG gecached werden sollen
//static long secondsToCache=4*24*60L*60L; // 4 Tage - weniger Prozessorlast?!
//static long secondsToCache = 14*24*60L*60L; // 14 Tage - Prozessorlast <3% (rasc)
static long secondsToCache;
static long secondsExtendedTextCache;
// Ab wann ein Event als alt gilt (in Sekunden)
//static long oldEventsAre = 60*60L; // 2h  (sometimes want to know something about current/last movie)
static long oldEventsAre;
static int scanning = 1;

std::string epg_filter_dir = "/var/tuxbox/config/zapit/epgfilter.xml";
static bool epg_filter_is_whitelist = false;
static bool epg_filter_except_current_next = false;
// static bool bouquet_filter_is_whitelist = false;
static bool messaging_zap_detected = false;

std::string dvbtime_filter_dir = "/var/tuxbox/config/zapit/dvbtimefilter.xml";
static bool dvb_time_update = false;

//NTP- Config
#define CONF_FILE "/var/tuxbox/config/neutrino.conf"
//const std::string ntp_system_cmd_prefix = "/sbin/rdate -s ";
const std::string ntp_system_cmd_prefix = "/sbin/ntpdate ";
std::string ntp_system_cmd;
CConfigFile ntp_config(',');
std::string ntpserver;
int ntprefresh;
int ntpenable;

static int eit_update_fd = -1;
static bool update_eit = true;

/* messaging_current_servicekey does probably not need locking, since it is
   changed from one place */
static t_channel_id    messaging_current_servicekey = 0;
static bool channel_is_blacklisted = false;
// EVENTS...

static CEventServer *eventServer;
//CTimerdClient   *timerdClient;
//bool            timerd = false;

static pthread_rwlock_t eventsLock = PTHREAD_RWLOCK_INITIALIZER; // Unsere (fast-)mutex, damit nicht gleichzeitig in die Menge events geschrieben und gelesen wird
static pthread_rwlock_t servicesLock = PTHREAD_RWLOCK_INITIALIZER; // Unsere (fast-)mutex, damit nicht gleichzeitig in die Menge services geschrieben und gelesen wird
static pthread_rwlock_t transpondersLock = PTHREAD_RWLOCK_INITIALIZER; // Unsere (fast-)mutex, damit nicht gleichzeitig in die Menge transponders geschrieben und gelesen wird
static pthread_rwlock_t bouquetsLock = PTHREAD_RWLOCK_INITIALIZER; // Unsere (fast-)mutex, damit nicht gleichzeitig in die Menge bouquets geschrieben und gelesen wird
static pthread_rwlock_t messagingLock = PTHREAD_RWLOCK_INITIALIZER;

static pthread_cond_t timeThreadSleepCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t timeThreadSleepMutex = PTHREAD_MUTEX_INITIALIZER;

// k.A. ob volatile im Kampf gegen Bugs trotz mutex's was bringt,
// falsch ist es zumindest nicht
/*
static DMX dmxEIT(0x12, 0x4f, (0xff- 0x01), 0x50, (0xff- 0x0f), 256);
static DMX dmxSDT(0x11, 0x42, 0xff, 0x42, 0xff, 256);
*/
// Houdini: changed sizes, EIT thread no more receives POLLER, saves some mem in sdt
//static DMX dmxEIT(0x12, 256);
//static DMX dmxSDT(0x11, 256);
/* no matter how big the buffer, we will receive spurious POLLERR's in table 0x60,
   but those are not a big deal, so let's save some memory */
static DMX dmxEIT(0x12, 3000 /*320*/);
#ifdef ENABLE_FREESATEPG
static DMX dmxFSEIT(3842, 320);
#endif
static DMX dmxCN(0x12, 512, false, 1);
#ifdef UPDATE_NETWORKS
static DMX dmxSDT(0x11, 512, true, 1);
static DMX dmxNIT(0x10, 128);
#endif
#ifdef ENABLE_PPT
// Houdini: added for Premiere Private EPG section for Sport/Direkt Portal
static DMX dmxPPT(0x00, 256);
unsigned int privatePid=0;
#endif
int sectionsd_stop = 0;

inline void readLockServices(void)
{
	pthread_rwlock_rdlock(&servicesLock);
}

inline void writeLockServices(void)
{
	pthread_rwlock_wrlock(&servicesLock);
}

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
}

inline void unlockEvents(void)
{
	pthread_rwlock_unlock(&eventsLock);
}

inline void readLockTransponders(void)
{
	pthread_rwlock_rdlock(&transpondersLock);
}

inline void writeLockTransponders(void)
{
	pthread_rwlock_wrlock(&transpondersLock);
}

inline void unlockTransponders(void)
{
	pthread_rwlock_unlock(&transpondersLock);
}

inline void readLockBouquets(void)
{
	pthread_rwlock_rdlock(&bouquetsLock);
}

inline void writeLockBouquets(void)
{
	pthread_rwlock_wrlock(&bouquetsLock);
}

inline void unlockBouquets(void)
{
	pthread_rwlock_unlock(&bouquetsLock);
}

#if 0
/* access to the data structures is protected by locks anyway, so there is no
   reason to pause the EITThreads anymore for the sake of locking. It was not
   used otherwise at all anyway and it did not work correctly since quite some
   time.
   To be removed, just kept for reference. 10.2008-seife */
inline int EITThreadsPause(void)
{
	return(	dmxEIT.pause() ||
		dmxCN.pause() ||
#ifdef ENABLE_FREESATEPG
		dmxFSEIT.pause() ||
#endif
		dmxPPT.pause());
}

inline int EITThreadsUnPause(void)
{
	return(	dmxCN.unpause() ||
		dmxEIT.unpause() ||
#ifdef ENABLE_FREESATEPG
		dmxFSEIT.unpause() ||
#endif
		dmxPPT.unpause());
}
#endif

bool timeset = false;
bool bTimeCorrect = false;
pthread_cond_t timeIsSetCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t timeIsSetMutex = PTHREAD_MUTEX_INITIALIZER;

//static bool	messaging_wants_current_next_Event = false;
//static bool	messaging_got_current = false;
//static bool	messaging_got_next = false;
static int	messaging_have_CN = 0x00;	// 0x01 = CURRENT, 0x02 = NEXT
static int	messaging_got_CN = 0x00;	// 0x01 = CURRENT, 0x02 = NEXT
static time_t	messaging_last_requested = time_monotonic();
static bool	messaging_neutrino_sets_time = false;
//static bool 	messaging_WaitForServiceDesc = false;

inline bool waitForTimeset(void)
{
	pthread_mutex_lock(&timeIsSetMutex);
	while(!timeset)
		pthread_cond_wait(&timeIsSetCond, &timeIsSetMutex);
	pthread_mutex_unlock(&timeIsSetMutex);
	/* we have time synchronization issues, at least on kernel 2.4, so
	   sometimes the time in the threads is still 1.1.1970, even after
	   waitForTimeset() returns. Let's hope that we work around this issue
	   with this sleep */
	sleep(1);
	writeLockMessaging();
	messaging_last_requested = time_monotonic();
	unlockMessaging();
	return true;
}

static int64_t last_profile_call;

void showProfiling( std::string text )
{
	struct timeval tv;

	gettimeofday( &tv, NULL );
	int64_t now = (int64_t) tv.tv_usec + (int64_t)((int64_t) tv.tv_sec * (int64_t) 1000000);

	int64_t tmp = now - last_profile_call;
	dprintf("--> '%s' %lld.%03lld\n", text.c_str(), tmp / 1000LL, tmp % 1000LL);
	last_profile_call = now;
}

static const SIevent nullEvt; // Null-Event

//------------------------------------------------------------
// Wir verwalten die events in SmartPointers
// und nutzen verschieden sortierte Menge zum Zugriff
//------------------------------------------------------------

// SmartPointer auf SIevent
//typedef Loki::SmartPtr<class SIevent, Loki::RefCounted, Loki::DisallowConversion, Loki::NoCheck>
//  SIeventPtr;
typedef boost::shared_ptr<class SIevent>
SIeventPtr;

typedef std::map<event_id_t, SIeventPtr, std::less<event_id_t> > MySIeventsOrderUniqueKey;
static MySIeventsOrderUniqueKey mySIeventsOrderUniqueKey;
static SIevent * myCurrentEvent = NULL;
static SIevent * myNextEvent = NULL;

// Mengen mit SIeventPtr sortiert nach Event-ID fuer NVOD-Events (mehrere Zeiten)
static MySIeventsOrderUniqueKey mySIeventsNVODorderUniqueKey;

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

typedef std::set<SIeventPtr, OrderServiceUniqueKeyFirstStartTimeEventUniqueKey > MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey;
static MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey;

struct OrderFirstEndTimeServiceIDEventUniqueKey
{
	bool operator()(const SIeventPtr &p1, const SIeventPtr &p2)
	{
		return
		    p1->times.begin()->startzeit + (long)p1->times.begin()->dauer == p2->times.begin()->startzeit + (long)p2->times.begin()->dauer ?
		    //      ( p1->serviceID == p2->serviceID ? p1->uniqueKey() < p2->uniqueKey() : p1->serviceID < p2->serviceID )
		    (p1->service_id == p2->service_id ? p1->uniqueKey() > p2->uniqueKey() : p1->service_id < p2->service_id)
				    :
				    ( p1->times.begin()->startzeit + (long)p1->times.begin()->dauer < p2->times.begin()->startzeit + (long)p2->times.begin()->dauer ) ;
	}
};

typedef std::set<SIeventPtr, OrderFirstEndTimeServiceIDEventUniqueKey > MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey;
static MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey;

// Hier landen alle Service-Ids von Meta-Events inkl. der zugehoerigen Event-ID (nvod)
// d.h. key ist der Unique Service-Key des Meta-Events und Data ist der unique Event-Key
typedef std::map<t_channel_id, event_id_t, std::less<t_channel_id> > MySIeventUniqueKeysMetaOrderServiceUniqueKey;
static MySIeventUniqueKeysMetaOrderServiceUniqueKey mySIeventUniqueKeysMetaOrderServiceUniqueKey;

/*
class NvodSubEvent {
  public:
    NvodSubEvent() {
      uniqueServiceID=0;
      uniqueEventID=0;
    }
    NvodSubEvent(const NvodSubEvent &n) {
      uniqueServiceID=n.uniqueServiceID;
      uniqueEventID=n.uniqueEventID;
    }
    t_channel_id uniqueServiceID;
    event_id_t   uniqueMetaEventID; // ID des Meta-Events
    event_id_t   uniqueMetaEventID; // ID des eigentlichen Events
};

// Menge sortiert nach Meta-ServiceIDs (NVODs)
typedef std::multimap<t_channel_id, class NvodSubEvent *, std::less<t_channel_id> > nvodSubEvents;
*/

struct EPGFilter
{
	t_original_network_id onid;
	t_transport_stream_id tsid;
	t_service_id sid;
	EPGFilter *next;
};

struct ChannelBlacklist
{
	t_channel_id chan;
	t_channel_id mask;
	ChannelBlacklist *next;
};

struct ChannelNoDVBTimelist
{
	t_channel_id chan;
	t_channel_id mask;
	ChannelNoDVBTimelist *next;
};

EPGFilter *CurrentEPGFilter = NULL;
ChannelBlacklist *CurrentBlacklist = NULL;
ChannelNoDVBTimelist *CurrentNoDVBTime = NULL;

static bool checkEPGFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	EPGFilter *filterptr = CurrentEPGFilter;
	while (filterptr)
	{
		if (((filterptr->onid == onid) || (filterptr->onid == 0)) &&
			((filterptr->tsid == tsid) || (filterptr->tsid == 0)) &&
			((filterptr->sid == sid) || (filterptr->sid == 0)))
			return true;
		filterptr = filterptr->next;
	}
	return false;
}

static bool checkBlacklist(t_channel_id channel_id)
{
	ChannelBlacklist *blptr = CurrentBlacklist;
	while (blptr)
	{
		if (blptr->chan == (channel_id & blptr->mask))
			return true;
		blptr = blptr->next;
	}
	return false;
}

static bool checkNoDVBTimelist(t_channel_id channel_id)
{
	ChannelNoDVBTimelist *blptr = CurrentNoDVBTime;
	while (blptr)
	{
		if (blptr->chan == (channel_id & blptr->mask))
			return true;
		blptr = blptr->next;
	}
	return false;
}

static void addEPGFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	if (!checkEPGFilter(onid, tsid, sid))
	{
		dprintf("Add EPGFilter for onid=\"%04x\" tsid=\"%04x\" service_id=\"%04x\"\n", onid, tsid, sid);
        	EPGFilter *node = new EPGFilter;
	        node->onid = onid;
		node->tsid = tsid;
		node->sid = sid;
        	node->next = CurrentEPGFilter;
        	CurrentEPGFilter = node;
	}
}

static void addBlacklist(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	t_channel_id channel_id =
		CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(sid, onid, tsid);
	t_channel_id mask =
		CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
			(sid ? 0xFFFF : 0), (onid ? 0xFFFF : 0), (tsid ? 0xFFFF : 0)
		);
	if (!checkBlacklist(channel_id))
	{
		xprintf("Add Channel Blacklist for channel 0x%012llx, mask 0x%012llx\n", channel_id, mask);
		ChannelBlacklist *node = new ChannelBlacklist;
		node->chan = channel_id;
		node->mask = mask;
		node->next = CurrentBlacklist;
		CurrentBlacklist = node;
	}
}

static void addNoDVBTimelist(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	t_channel_id channel_id =
		CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(sid, onid, tsid);
	t_channel_id mask =
		CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
			(sid ? 0xFFFF : 0), (onid ? 0xFFFF : 0), (tsid ? 0xFFFF : 0)
		);
	if (!checkNoDVBTimelist(channel_id))
	{
		xprintf("Add channel 0x%012llx, mask 0x%012llx to NoDVBTimelist\n", channel_id, mask);
		ChannelNoDVBTimelist *node = new ChannelNoDVBTimelist;
		node->chan = channel_id;
		node->mask = mask;
		node->next = CurrentNoDVBTime;
		CurrentNoDVBTime = node;
	}
}

#if 0
static void removeEPGFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{

}

struct ExceptService
{
	t_service_id		sid;
	ExceptService		*next;
};

struct BouquetAdderEntry
{
#define MAX_SIZE_MYBOUQUETS_STR 50
	char 			ProviderName[MAX_SIZE_MYBOUQUETS_STR];
	t_original_network_id	onid;
	t_transport_stream_id	tsid;
	ExceptService		*es;
	BouquetAdderEntry	*next;
};

struct BouquetAdder
{
	char 			BouquetName[MAX_SIZE_MYBOUQUETS_STR];
	t_bouquet_id 		bid;
	BouquetAdderEntry	*bae;
	BouquetAdder		*next;
};

struct BouquetFilter
{
	t_bouquet_id bid;
	BouquetFilter *next;
};

BouquetFilter	*CurrentBouquetFilter = NULL;
BouquetAdder	*CurrentBouquetAdder = NULL;

static bool checkBouquetFilter(t_bouquet_id bid)
{
	BouquetFilter *filterptr = CurrentBouquetFilter;
	while (filterptr)
	{
		if ((filterptr->bid == bid) || (filterptr->bid == 0))
			return true;
		filterptr = filterptr->next;
	}
	return false;
}

static void addBouquetFilter(t_bouquet_id bid)
{
	if (!checkBouquetFilter(bid))
	{
		dprintf("Add Bouquet Filter for bouquet_id=\"%04x\"\n", bid);
        	BouquetFilter *node = new BouquetFilter;
		node->bid = bid;
        	node->next = CurrentBouquetFilter;
        	CurrentBouquetFilter = node;
	}
}
#endif

// Loescht ein Event aus allen Mengen
static bool deleteEvent(const event_id_t uniqueKey)
{
	readLockEvents();
	MySIeventsOrderUniqueKey::iterator e = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (e != mySIeventsOrderUniqueKey.end())
	{
		if (e->second->times.size())
		{
			unlockEvents();
			writeLockEvents();
			mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.erase(e->second);
			mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.erase(e->second);
		}

		unlockEvents();
		writeLockEvents();
		mySIeventsOrderUniqueKey.erase(uniqueKey);
		mySIeventsNVODorderUniqueKey.erase(uniqueKey);

//		printf("Deleting: %04x\n", (int) uniqueKey);
		unlockEvents();
		return true;
	}
	else
	{
		unlockEvents();
		return false;
	}

	/*
	  for(MySIeventIDsMetaOrderServiceID::iterator i=mySIeventIDsMetaOrderServiceID.begin(); i!=mySIeventIDsMetaOrderServiceID.end(); i++)
	    if(i->second==eventID)
	      mySIeventIDsMetaOrderServiceID.erase(i);
	*/
}

// Fuegt ein Event in alle Mengen ein
/* if cn == true (if called by cnThread), then myCurrentEvent and myNextEvent is updated, too */
static void addEvent(const SIevent &evt, const unsigned table_id, const time_t zeit, bool cn = false)
{
	bool EPG_filtered = checkEPGFilter(evt.original_network_id, evt.transport_stream_id, evt.service_id);

	/* more readable in "plain english":
	   if current/next are not to be filtered and table_id is current/next -> continue
	   else {
		if epg filter is blacklist and filter matched -> stop. (return)
		if epg filter is whitelist and filter did not match -> stop also.
	   }
	 */
	if (!(epg_filter_except_current_next && (table_id == 0x4e || table_id == 0x4f)) &&
	    (table_id != 0)) {
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
		readLockMessaging();
		if (evt.get_channel_id() == messaging_current_servicekey && // but only if it is the current channel...
		    (messaging_got_CN != 0x03)) { // ...and if we don't have them already.
			unlockMessaging();
			SIevent *eptr = new SIevent(evt);
			if (!eptr)
			{
				printf("[sectionsd::addEvent] new SIevent1 failed.\n");
				return;
				//throw std::bad_alloc();
			}

			SIeventPtr e(eptr);

			writeLockEvents();
			if (e->runningStatus() > 2) { // paused or currently running
				if (!myCurrentEvent || (myCurrentEvent && (*myCurrentEvent).uniqueKey() != e->uniqueKey())) {
					if (myCurrentEvent)
						delete myCurrentEvent;
					myCurrentEvent = new SIevent(evt);
					writeLockMessaging();
					messaging_got_CN |= 0x01;
					if (myNextEvent && (*myNextEvent).uniqueKey() == e->uniqueKey()) {
						dprintf("addevent-cn: removing next-event\n");
						/* next got "promoted" to current => trigger re-read */
						delete myNextEvent;
						myNextEvent = NULL;
						messaging_got_CN &= 0x01;
					}
					unlockMessaging();
					dprintf("addevent-cn: added running (%d) event 0x%04x '%s'\n",
						e->runningStatus(), e->eventID, e->getName().c_str());
				} else {
					writeLockMessaging();
					messaging_got_CN |= 0x01;
					unlockMessaging();
					dprintf("addevent-cn: not add runn. (%d) event 0x%04x '%s'\n",
						e->runningStatus(), e->eventID, e->getName().c_str());
				}
			} else {
				if ((!myNextEvent    || (myNextEvent    && (*myNextEvent).uniqueKey()    != e->uniqueKey() && (*myNextEvent).times.begin()->startzeit < e->times.begin()->startzeit)) &&
				    (!myCurrentEvent || (myCurrentEvent && (*myCurrentEvent).uniqueKey() != e->uniqueKey()))) {
					if (myNextEvent)
						delete myNextEvent;
					myNextEvent = new SIevent(evt);
					writeLockMessaging();
					messaging_got_CN |= 0x02;
					unlockMessaging();
					dprintf("addevent-cn: added next    (%d) event 0x%04x '%s'\n",
						e->runningStatus(), e->eventID, e->getName().c_str());
				} else {
					dprintf("addevent-cn: not added next(%d) event 0x%04x '%s'\n",
						e->runningStatus(), e->eventID, e->getName().c_str());
					writeLockMessaging();
					messaging_got_CN |= 0x02;
					unlockMessaging();
				}
			}
			unlockEvents();
		} else
			unlockMessaging();
	}

	readLockEvents();
	MySIeventsOrderUniqueKey::iterator si = mySIeventsOrderUniqueKey.find(evt.uniqueKey());
	bool already_exists = (si != mySIeventsOrderUniqueKey.end());

	if (already_exists && (evt.table_id < si->second->table_id))
	{
		/* if the new event has a lower (== more recent) table ID, replace the old one */
		already_exists = false;
		dprintf("replacing event %016llx:%02x with %04x:%02x '%.40s'\n", si->second->uniqueKey(),
			si->second->table_id, evt.eventID, evt.table_id, evt.getName().c_str());
	}

	/* Check size of some descriptors of the new event before comparing
	   them with the old ones, because the same event can be complete
	   on one German Sky channel and incomplete on another one. So we
	   make sure to keep the complete event, if applicable. */

	if ((already_exists) && (evt.components.size() > 0)) {
		if (si->second->components.size() != evt.components.size())
			already_exists = false;
		else {
			SIcomponents::iterator c1 = si->second->components.begin();
			SIcomponents::iterator c2 = evt.components.begin();
			while ((c1 != si->second->components.end()) && (c2 != evt.components.end())) {
				if ((c1->componentType != c2->componentType) ||
					(c1->componentTag != c2->componentTag) ||
					(c1->streamContent != c2->streamContent) ||
					(strcmp(c1->component.c_str(),c2->component.c_str()) != 0)){
					already_exists = false;
					break;
				}
				c1++;
				c2++;
			}
		}
	}
 
	if ((already_exists) && (evt.linkage_descs.size() > 0)) {
		if (si->second->linkage_descs.size() != evt.linkage_descs.size())
			already_exists = false;
		else {
			for (unsigned int i = 0; i < si->second->linkage_descs.size(); i++) {
				if ((si->second->linkage_descs[i].linkageType !=
						evt.linkage_descs[i].linkageType) ||
					(si->second->linkage_descs[i].originalNetworkId !=
						evt.linkage_descs[i].originalNetworkId) ||
					(si->second->linkage_descs[i].transportStreamId !=
						evt.linkage_descs[i].transportStreamId) ||
					(strcmp(si->second->linkage_descs[i].name.c_str(),
						evt.linkage_descs[i].name.c_str()) != 0)) {
					already_exists = false;
					break;
				}
			}
		}
	}
 
	if ((already_exists) && (evt.ratings.size() > 0)) {
		if (si->second->ratings.size() != evt.ratings.size())
			already_exists = false;
		else {
			SIparentalRatings::iterator p1 = si->second->ratings.begin();
			SIparentalRatings::iterator p2 = evt.ratings.begin();
			while ((p1 != si->second->ratings.end()) && (p2 != evt.ratings.end())) {
				if ((p1->rating != p2->rating) ||
					(strcmp(p1->countryCode.c_str(),p2->countryCode.c_str()) != 0)) {
					already_exists = false;
					break;
				}
				p1++;
				p2++;
			}
		}
	}
 
	if (already_exists) {
		if (si->second->times.size() != evt.times.size())
			already_exists = false;
		else {
			SItimes::iterator t1 = si->second->times.begin();
			SItimes::iterator t2 = evt.times.begin();
			while ((t1 != si->second->times.end()) && (t2 != evt.times.end())) {
				if ((t1->startzeit != t2->startzeit) ||
					(t1->dauer != t2->dauer)) {
					already_exists = false;
					break;
				}
				t1++;
				t2++;
			}
		}
	}

	if ((already_exists) && (SIlanguage::getMode() == CSectionsdClient::LANGUAGE_MODE_OFF)) {
		si->second->setName("OFF",evt.getName().c_str());
		si->second->contentClassification = evt.contentClassification;
		si->second->userClassification = evt.userClassification;
		if ((evt.getExtendedText().length() > 0) &&
				(evt.times.begin()->startzeit < zeit + secondsExtendedTextCache))
			si->second->setExtendedText("OFF",evt.getExtendedText().c_str());
		if (evt.getText().length() > 0)
			si->second->setText("OFF",evt.getText().c_str());
		if (evt.getName().length() > 0)
			si->second->setName("OFF",evt.getName().c_str());
	}
	else {

		SIevent *eptr = new SIevent(evt);

		if (!eptr)
		{
			printf("[sectionsd::addEvent] new SIevent failed.\n");
			unlockEvents();
			return;
			// throw std::bad_alloc();
		}

		SIeventPtr e(eptr);

		//Strip ExtendedDescription if too far in the future
		if ((e->times.begin()->startzeit > zeit + secondsExtendedTextCache) &&
		   (SIlanguage::getMode() == CSectionsdClient::LANGUAGE_MODE_OFF) && (zeit != 0))
			e->setExtendedText("OFF","");

		// Damit in den nicht nach Event-ID sortierten Mengen
		// Mehrere Events mit gleicher ID sind, diese vorher loeschen
		unlockEvents();
		deleteEvent(e->uniqueKey());
		readLockEvents();
		if (mySIeventsOrderUniqueKey.size() >= max_events) {
			//FIXME: Set Old Events to 0 if limit is reached...
			MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator lastEvent =
				mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end();
			lastEvent--;

			//preserve events of current channel
			readLockMessaging();
			while ((lastEvent != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin()) &&
				((*lastEvent)->get_channel_id() == messaging_current_servicekey)) {
				lastEvent--;
			}
			unlockMessaging();
			unlockEvents();
			deleteEvent((*lastEvent)->uniqueKey());
		}
		else
			unlockEvents();
		readLockEvents();
		// Pruefen ob es ein Meta-Event ist
		MySIeventUniqueKeysMetaOrderServiceUniqueKey::iterator i = mySIeventUniqueKeysMetaOrderServiceUniqueKey.find(e->get_channel_id());

		if (i != mySIeventUniqueKeysMetaOrderServiceUniqueKey.end())
		{
			// ist ein MetaEvent, d.h. mit Zeiten fuer NVOD-Event

			if (e->times.size())
			{
				// D.h. wir fuegen die Zeiten in das richtige Event ein
				MySIeventsOrderUniqueKey::iterator ie = mySIeventsOrderUniqueKey.find(i->second);

				if (ie != mySIeventsOrderUniqueKey.end())
				{

					// Event vorhanden
					// Falls das Event in den beiden Mengen mit Zeiten nicht vorhanden
					// ist, dieses dort einfuegen
					MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator i2 = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.find(ie->second);
					unlockEvents();
					writeLockEvents();

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
		unlockEvents();
		writeLockEvents();
//		printf("Adding: %04x\n", (int) e->uniqueKey());

		// normales Event
		mySIeventsOrderUniqueKey.insert(std::make_pair(e->uniqueKey(), e));

		if (e->times.size())
		{
			// diese beiden Mengen enthalten nur Events mit Zeiten
			mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.insert(e);
			mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.insert(e);
		}
	}
	unlockEvents();
}
#if 0
// Fuegt zusaetzliche Zeiten in ein Event ein
static void addEventTimes(const SIevent &evt, const unsigned table_id)
{
	if (evt.times.size())
	{
		readLockEvents();
		// D.h. wir fuegen die Zeiten in das richtige Event ein
		MySIeventsOrderUniqueKey::iterator e = mySIeventsOrderUniqueKey.find(evt.uniqueKey());

		if (e != mySIeventsOrderUniqueKey.end())
		{
			// Event vorhanden
			// Falls das Event in den beiden Mengen mit Zeiten vorhanden ist, dieses dort loeschen
			unlockEvents();
			writeLockEvents();
			if (e->second->times.size())
			{
				mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.erase(e->second);
				mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.erase(e->second);
				//unlockEvents();
			}

			// Und die Zeiten im Event updaten
			e->second->times.insert(evt.times.begin(), evt.times.end());

			// Und das Event in die beiden Mengen mit Zeiten (wieder) einfuegen
			mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.insert(e->second);
			mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.insert(e->second);
			unlockEvents();
//			printf("Updating: %04x times.size() = %d\n", (int) evt.uniqueKey(), e->second->times.size());
		}
		else
		{
			unlockEvents();
			// Event nicht vorhanden -> einfuegen
			addEvent(evt, table_id, 0);
		}
	}
}
#endif
static void addNVODevent(const SIevent &evt)
{
	SIevent *eptr = new SIevent(evt);

	if (!eptr)
	{
		printf("[sectionsd::addNVODevent] new SIevent failed.\n");
		return;
		//throw std::bad_alloc();
	}

	SIeventPtr e(eptr);

	readLockEvents();
	MySIeventsOrderUniqueKey::iterator e2 = mySIeventsOrderUniqueKey.find(e->uniqueKey());

	if (e2 != mySIeventsOrderUniqueKey.end())
	{
		// bisher gespeicherte Zeiten retten
		unlockEvents();
		writeLockEvents();
		e->times.insert(e2->second->times.begin(), e2->second->times.end());
	}
	unlockEvents();

	// Damit in den nicht nach Event-ID sortierten Mengen
	// mehrere Events mit gleicher ID sind, diese vorher loeschen
	deleteEvent(e->uniqueKey());
	readLockEvents();
	if (mySIeventsOrderUniqueKey.size() >= max_events) {
		//FIXME: Set Old Events to 0 if limit is reached...
		MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator lastEvent =
										mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end();
		lastEvent--;

		//preserve events of current channel
		readLockMessaging();
		while ((lastEvent != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin()) &&
			((*lastEvent)->get_channel_id() == messaging_current_servicekey)) {
		  lastEvent--;
		}
		unlockMessaging();
		unlockEvents();
		deleteEvent((*lastEvent)->uniqueKey());
	}
	else
		unlockEvents();
	writeLockEvents();
	mySIeventsOrderUniqueKey.insert(std::make_pair(e->uniqueKey(), e));

	mySIeventsNVODorderUniqueKey.insert(std::make_pair(e->uniqueKey(), e));
	unlockEvents();
	if (e->times.size())
	{
		// diese beiden Mengen enthalten nur Events mit Zeiten
		writeLockEvents();
		mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.insert(e);
		mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.insert(e);
		unlockEvents();
	}
}

#if 0
static void removeNewEvents(void)
{
	// Alte events loeschen
	time_t zeit = time(NULL);

	// Mal umgekehrt wandern

	for (MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin(); e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end(); e++)
		if ((*e)->times.begin()->startzeit > zeit + secondsToCache)
			deleteEvent((*e)->uniqueKey());

	return ;
}
#endif
/*
static void removeOldEvents(const long seconds)
{
	bool goodtimefound;
	MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator etmp;

	// Alte events loeschen
	time_t zeit = time(NULL);

	MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin();
	while (e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end()) {
		goodtimefound = false;
		for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); t++) {
			if (t->startzeit + (long)t->dauer >= zeit - seconds) {
				goodtimefound=true;
				// one time found -> exit times loop
				break;
			}
		}
		if (false == goodtimefound) {
			// keep track of our iterator
			etmp = e;
			if (etmp == mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin()) {
				etmp++; // get next element
				deleteEvent((*e)->uniqueKey());
			} else {
				etmp--; // get last element and iterate later
				deleteEvent((*e)->uniqueKey());
				etmp++;
			}
			e = etmp;
		}
		else
			e++;   // solange das nicht richtig funktioniert einfach bis zum ende suchen
			// break; // sortiert nach Endzeit, daher weiteres Suchen unnoetig
	}
	return ;
}
*/
#ifdef UPDATE_NETWORKS
xmlNodePtr getProvbyPosition(xmlNodePtr node, const int position) {
	while (node) {
		if (xmlGetSignedNumericAttribute(node, "position", 16) == position)
			return node;
		node = node->xmlNextNode;
	}
	return NULL;
}

//Parses services.xml and delivers the node with the concerning transponder
xmlNodePtr FindTransponder(xmlNodePtr provider, const t_original_network_id onid, const t_transport_stream_id tsid)
{
	//EUTELSAT & SIRIUS: This is for you: Obey DVB rules please!!! Neither of you is allowed to use onid 0001! FIX IT!
	if ( (tsid == 1) && (onid == 1) ) {
		if ((getProvbyPosition(provider, 0x50) != NULL) && (getProvbyPosition(provider, 0x130) != NULL)) {
			//If 5E and 13E are used together we can't determine whose SDT this is.
			printf("Sirius and Eutelsat suck big time!\n");
			return NULL;
		}
	}

	while (provider)
	{
		dprintf("going to search dvb-%c provider %s\n", xmlGetName(provider)[0], xmlGetAttribute(provider, "name"));
		xmlNodePtr transponder = provider->xmlChildrenNode;

		while (transponder)
		{
			if ((xmlGetNumericAttribute(transponder, "id", 16) == tsid) && (xmlGetNumericAttribute(transponder, "onid", 16) == onid))
				return transponder;
			else
			 	transponder = transponder->xmlNextNode;
		}
		provider = provider->xmlNextNode;
	}
	return NULL;
}
#endif
static void removeOldEvents(const long seconds)
{
	bool goodtimefound;

	// Alte events loeschen
	time_t zeit = time(NULL);

	readLockEvents();

	MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin();

	while ((e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end()) && (!messaging_zap_detected)) {
		unlockEvents();
		goodtimefound = false;
		for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); t++) {
			if (t->startzeit + (long)t->dauer >= zeit - seconds) {
				goodtimefound=true;
				// one time found -> exit times loop
				break;
			}
		}

		if (false == goodtimefound)
			deleteEvent((*(e++))->uniqueKey());
		else
			++e;
		readLockEvents();
	}
	unlockEvents();

	return;
}

/* Remove duplicate events (same Service, same start and endtime)
 * with different eventID. Use the one from the lower table_id.
 * This routine could be extended to remove overlapping events also,
 * but let's keep that for later
 */
static void removeDupEvents(void)
{
	MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e1, e2, del;

	readLockEvents();
	e1 = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin();
	while ((e1 != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end()) && !messaging_zap_detected)
	{
		e2 = e1;
		e1++;
		if (e1 == mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end())
			break;

		/* check for the same service */
		if ((*e1)->get_channel_id() != (*e2)->get_channel_id())
			continue;
		/* check for same time */
		if ((*e1)->times.begin()->startzeit != (*e2)->times.begin()->startzeit)
			continue;


		del = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end();
		if ((*e1)->table_id > (*e2)->table_id){
			del = e1;
		}
		else if ((*e1)->table_id < (*e2)->table_id){
			del = e2;
		}
		else // ((*e1)->table_id == (*e2)->table_id)
		{
			del = e1;
		}

		/* can not happen. This check is pure paranoia :) */
		if (del == mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end())
			continue;

		xprintf("%s: --------  %llx.%02x '%s'\n", __func__,
			(*del)->uniqueKey(), (*del)->table_id, (*del)->getName().empty() ? "NULL":(*del)->getName().c_str());
		unlockEvents();
		deleteEvent((*del)->uniqueKey());
		readLockEvents();
	}
	unlockEvents();
	return;
}

#ifdef UPDATE_NETWORKS
static void removeWasteEvents()
{
	bool haslinkage;
	bool validevent;
	xmlDocPtr service_parser = parseXmlFile(SERVICES_XML);

	xmlNodePtr services_tp;
	xmlNodePtr node;

	t_service_id          last_service_id = 0;
	t_original_network_id last_original_network_id = 0;
	t_transport_stream_id last_transport_stream_id = 0;
	bool lastidfound = true;

	readLockEvents();

	MySIeventsOrderUniqueKey::iterator e = mySIeventsOrderUniqueKey.begin();

	while ((e != mySIeventsOrderUniqueKey.end()) && (!messaging_zap_detected)) {
		unlockEvents();
		validevent = true;
		haslinkage = false;
		if ((last_original_network_id == e->second->original_network_id) &&
			(last_transport_stream_id == e->second->transport_stream_id) &&
			(last_service_id == e->second->service_id)) {
			if (!lastidfound) {
				validevent = false;
				dprintf("Same ONID:%04x TSID:%04x SID:%04x\n",last_original_network_id,
										last_transport_stream_id,
										last_service_id);
			}
		}
		else {
			for (unsigned int i = 0; i < e->second->linkage_descs.size(); i++)
				if ((e->second->linkage_descs[i].linkageType == 0xB0) ||
					(e->second->linkage_descs[i].linkageType == 0x00))
				{
					haslinkage = true;
					break;
				}
//			printf("here1\n");
			if (validevent && !haslinkage) {
//				printf("here2\n");
				if (service_parser != NULL) {
//					printf("here3\n");
					services_tp = FindTransponder(xmlDocGetRootElement(service_parser)
								->xmlChildrenNode,e->second->original_network_id,
										e->second->transport_stream_id);
					if ( services_tp ) {
//						printf("here4\n");
						node = services_tp->xmlChildrenNode;
						while (xmlGetNextOccurence(node, "channel") != NULL) {
//						printf("here5\n");
						if (e->second->service_id != xmlGetNumericAttribute(node,
											(char*)"service_id", 16))
								node = node->xmlNextNode;
							else break;
						}
//						printf("here6\n");
						if (xmlGetNextOccurence(node, "channel") == NULL)
							validevent = false;
					} else validevent = false;
				} else validevent = false;
				lastidfound = validevent;
				if (!lastidfound)
					dprintf("Wasted ONID:%04x TSID:%04x SID:%04x\n",
									e->second->original_network_id,
									e->second->transport_stream_id,
									e->second->service_id);
			} else lastidfound = true;
			last_service_id = e->second->service_id;
			last_original_network_id = e->second->original_network_id;
			last_transport_stream_id = e->second->transport_stream_id;
		}

		if (!validevent)
			deleteEvent((e++)->first);
		else
			++e;
		readLockEvents();
	}
	unlockEvents();

	xmlFreeDoc(service_parser);
	return;
}
#endif
//  SIservicePtr;
typedef boost::shared_ptr<class SIservice>
SIservicePtr;

typedef std::map<t_channel_id, SIservicePtr, std::less<t_channel_id> > MySIservicesOrderUniqueKey;
static MySIservicesOrderUniqueKey mySIservicesOrderUniqueKey;

typedef std::map<t_channel_id, SIservicePtr, std::less<t_channel_id> > MySIservicesNVODorderUniqueKey;
static MySIservicesNVODorderUniqueKey mySIservicesNVODorderUniqueKey;

// Hier sollte man die hash-funktion fuer strings der stl benutzen
// Muss mal schauen ob es die auch fuer 'ignore_case' gibt
/*
struct OrderServiceName
{
	bool operator()(const SIservicePtr &p1, const SIservicePtr &p2)
	{
		return strcasecmp(p1->serviceName.c_str(), p2->serviceName.c_str()) < 0;
	}
};

typedef std::set<SIservicePtr, OrderServiceName > MySIservicesOrderServiceName;
static MySIservicesOrderServiceName mySIservicesOrderServiceName;
*/
#ifdef UPDATE_NETWORKS
xmlNodePtr findBouquetByName(xmlDocPtr parser,char *name)
{
	xmlNodePtr bouquet = NULL;
	if (parser)
		bouquet = xmlDocGetRootElement(parser)->xmlChildrenNode;
	else {
		dprintf("Bouquet parsing failed!\n");
		return NULL;
	}
	while (xmlGetNextOccurence(bouquet, "Bouquet") != NULL) {
		if ((xmlGetNumericAttribute(bouquet, "type", 8) == 2) &&
			(strcmp(xmlGetAttribute(bouquet, "name"), name) == 0))
			return bouquet;
		bouquet = bouquet->xmlNextNode;
	}
	return NULL;
}

static BouquetAdder* getNextAutoBouquet(BouquetAdder *currentAdder, const char *provname,
					const t_original_network_id onid, const t_transport_stream_id tsid,
					const t_service_id sid)
{
	bool found = false;
	BouquetAdderEntry *currentEntry = NULL;
	ExceptService *excepts = NULL;

	while ((currentAdder) && (!found)) {
		currentEntry = currentAdder->bae;
		while ((currentEntry) && (!found)) {
			if (	((strcmp(currentEntry->ProviderName, provname) == 0) ||
					((strcmp(currentEntry->ProviderName, "") == 0))) &&
				((currentEntry->onid == onid) || (currentEntry->onid == 0)) &&
				((currentEntry->tsid == tsid) || (currentEntry->tsid == 0))	)
				found = true;
			excepts = currentEntry->es;
			while ((excepts) && (found)) {
				if (excepts->sid == sid)
					found = false;
				excepts = excepts->next;
			}
			currentEntry = currentEntry->next;
		}
		if (!found)
			currentAdder = currentAdder->next;
	}
	return currentAdder;
}

static bool bouquetContainsService(const xmlNodePtr bouquet, const t_original_network_id onid,
				const t_transport_stream_id tsid, const t_service_id sid)
{
	if (!bouquet)
		return false;

	xmlNodePtr channel = NULL;

	channel = bouquet->xmlChildrenNode;
	while (channel) {
		if ((xmlGetNumericAttribute(channel, "onid", 16) == onid) &&
			(xmlGetNumericAttribute(channel, "tsid", 16) == tsid) &&
			(xmlGetNumericAttribute(channel, "serviceID", 16) == sid))
			return true;
		channel = channel->xmlNextNode;
	}
	return false;
}

static void write_bouquet_xml_header(FILE * fd)
{
	fprintf(fd,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!--\n"
		"  This file was automatically generated by the sectionsd.\n"
		"  It contains all new or changed bouquets from BAT\n"
		"  which are different from bouquets.xml.\n"
		"  It shall be merged with services.xml when the box shuts down.\n"
		"-->\n"
	       "<zapit>\n");
}

static void write_bouquet_xml_footer(FILE *fd)
{
	fprintf(fd, "</zapit>\n");
}

//stolen from zapitools.cpp
std::string UTF8_to_UTF8XML(const char * s)
{
	std::string r;

	while ((*s) != 0)
	{
		/* cf.
		 * http://www.w3.org/TR/2004/REC-xml-20040204/#syntax
		 * and
		 * http://www.w3.org/TR/2004/REC-xml-20040204/#sec-predefined-ent
		 */
		switch (*s)
		{
		case '<':
			r += "&lt;";
			break;
		case '>':
			r += "&gt;";
			break;
		case '&':
			r += "&amp;";
			break;
		case '\"':
			r += "&quot;";
			break;
		case '\'':
			r += "&apos;";
			break;
		default:
			r += *s;
		}
		s++;
	}
	return r;
}

static void writebouquetwithoutend(FILE *fd, xmlNodePtr bouquet)
{
	std::string name;

	name = xmlGetAttribute(bouquet, "name");

	fprintf(fd, "\t<Bouquet type=\"%01x\" bouquet_id=\"%04x\" name=\"%s\" hidden=\"%01x\" locked=\"%01x\">\n",
		xmlGetNumericAttribute(bouquet, "type", 16),
		xmlGetNumericAttribute(bouquet, "bouquet_id", 16),
		UTF8_to_UTF8XML(name.c_str()).c_str(),
		xmlGetNumericAttribute(bouquet, "hidden", 16),
		xmlGetNumericAttribute(bouquet, "locked", 16));

	xmlNodePtr channel = bouquet->xmlChildrenNode;
	while (xmlGetNextOccurence(channel, "channel") != NULL) {
		if (xmlGetAttribute(channel, "name") != NULL)
			name = xmlGetAttribute(channel, "name");
		else
			name = "";
		if (xmlGetAttribute(channel, "sat") != NULL) {
			fprintf(fd, "\t\t<channel serviceID=\"%04x\" name=\"%s\" tsid=\"%04x\" onid=\"%04x\" sat=\"%03x\"/>\n",
				xmlGetNumericAttribute(channel, "serviceID", 16),
				UTF8_to_UTF8XML(name.c_str()).c_str(),
				xmlGetNumericAttribute(channel, "tsid", 16),
				xmlGetNumericAttribute(channel, "onid", 16),
				xmlGetSignedNumericAttribute(channel, "sat", 16));
		}
		else
			fprintf(fd, "\t\t<channel serviceID=\"%04x\" name=\"%s\" tsid=\"%04x\" onid=\"%04x\"/>\n",
				xmlGetNumericAttribute(channel, "serviceID", 16),
				UTF8_to_UTF8XML(name.c_str()).c_str(),
				xmlGetNumericAttribute(channel, "tsid", 16),
				xmlGetNumericAttribute(channel, "onid", 16));

		channel = channel->xmlNextNode;
	}
}

static bool AddServiceToAutoBouquets(const char *provname, const t_original_network_id onid,
							const t_transport_stream_id tsid, const t_service_id sid,
							const xmlDocPtr bouquet_parser,
							const xmlDocPtr current_parser)
{
	BouquetAdder *currentBouquet = CurrentBouquetAdder;
	FILE * dst;

	xmlNodePtr bouquet = NULL;
	xmlNodePtr bouquet2 = NULL;

	bool returnvalue = false;
	bool current_exists = false;
	bool bouquet_exists = false;
	bool bouquet_found = false;

	if (current_parser)
		current_exists=true;
	if (bouquet_parser)
		bouquet_exists = true;

	currentBouquet = getNextAutoBouquet(currentBouquet, provname, onid, tsid, sid);
	while ((currentBouquet) && (!messaging_zap_detected)) {
		dprintf("Service %s, ONID: 0x%04x, TSID: 0x%04x, SID: 0x%04x will be added to Bouquet: %s\n",
				provname,
				onid,
				tsid,
				sid,
				currentBouquet->BouquetName);

		if (bouquet_exists)
			bouquet = findBouquetByName(bouquet_parser, currentBouquet->BouquetName);
		if ((!bouquetContainsService(bouquet, onid, tsid, sid)) && (!messaging_zap_detected)) {
			if (current_exists)
				bouquet2 = findBouquetByName(current_parser, currentBouquet->BouquetName);
			if ((!bouquetContainsService(bouquet2, onid, tsid, sid)) && (!messaging_zap_detected)) {
				if (!(dst = fopen(CURRENTBOUQUETS_TMP, "w"))) {
					dprintf("unable to open %s for writing\n", CURRENTBOUQUETS_TMP);
				}
				else {
					write_bouquet_xml_header(dst);
					if (current_exists) {
						bouquet_found = false;
						bouquet2 = xmlDocGetRootElement(current_parser)->xmlChildrenNode;
						while (bouquet2) {
							writebouquetwithoutend(dst, bouquet2);
							if ((strcmp(currentBouquet->BouquetName,
								xmlGetAttribute(bouquet2, "name")) == 0) &&
							(xmlGetNumericAttribute(bouquet2, "type", 8) == 2)) {
								fprintf(dst, "\t\t<channel serviceID=\"%04x\" name=\"\" tsid=\"%04x\" onid=\"%04x\"/>\n",
								sid,
								tsid,
								onid);
								bouquet_found = true;
							}
							fprintf(dst, "\t</Bouquet>\n");
							bouquet2 = bouquet2->xmlNextNode;
						}
						if (!bouquet_found) {
							fprintf(dst, "\t<Bouquet type=\"2\" bouquet_id=\"%04x\" name=\"%s\" hidden=\"0\" locked=\"0\">\n",
								currentBouquet->bid,
								currentBouquet->BouquetName);
							fprintf(dst, "\t\t<channel serviceID=\"%04x\" name=\"\" tsid=\"%04x\" onid=\"%04x\"/>\n",
							sid,
							tsid,
							onid);
							fprintf(dst, "\t</Bouquet>\n");
						}
					}
					else {
						if (bouquet)
							writebouquetwithoutend(dst, bouquet);
						else
							fprintf(dst, "\t<Bouquet type=\"2\" bouquet_id=\"%04x\" name=\"%s\" hidden=\"0\" locked=\"0\">\n",
								currentBouquet->bid,
								currentBouquet->BouquetName);
						fprintf(dst, "\t\t<channel serviceID=\"%04x\" name=\"\" tsid=\"%04x\" onid=\"%04x\"/>\n",
						sid,
						tsid,
						onid);
						fprintf(dst, "\t</Bouquet>\n");
					}
					write_bouquet_xml_footer(dst);
					returnvalue = true;
					fclose(dst);
					rename(CURRENTBOUQUETS_TMP, CURRENTBOUQUETS_XML);
				}
			}
		}
		currentBouquet = currentBouquet->next;
		currentBouquet = getNextAutoBouquet(currentBouquet, provname, onid, tsid, sid);
	}

	return returnvalue;
}

// Fuegt ein Service in alle Mengen ein
static bool addService(const SIservice &s, const int is_actual)
{
	bool already_exists;
	bool is_new = false;

	//if (mySIservicesNVODorderUniqueKey.find(s.uniqueKey()))
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
			//throw std::bad_alloc();
		}

		SIservicePtr sptr(sp);

	// Leere Servicenamen in ServiceID in hex umbenennen
#define MAX_SIZE_SERVICENAME	50
	char servicename[MAX_SIZE_SERVICENAME];

		if (sptr->serviceName.empty()) {
			sprintf(servicename, "%04x",  sptr->service_id);
			servicename[sizeof(servicename) - 1] = 0;
			sptr->serviceName = servicename;
		}

		sptr->is_actual = is_actual;

		writeLockServices();
		mySIservicesOrderUniqueKey.insert(std::make_pair(sptr->uniqueKey(), sptr));
		unlockServices();

		if (sptr->nvods.size())
		{
			writeLockServices();
			mySIservicesNVODorderUniqueKey.insert(std::make_pair(sptr->uniqueKey(), sptr));
			unlockServices();
		}
		//  if(sptr->serviceID==0x01 || sptr->serviceID==0x02 || sptr->serviceID==0x04)
//		mySIservicesOrderServiceName.insert(sptr);
		is_new = true;
	}

	return is_new;
}

 //  SIsPtr;
typedef boost::shared_ptr<class SIbouquet>
SIbouquetPtr;

typedef std::map<t_bouquetentry_id, SIbouquetPtr, std::less<t_bouquetentry_id> > MySIbouquetsOrderUniqueKey;
static MySIbouquetsOrderUniqueKey mySIbouquetsOrderUniqueKey;

// Fuegt einen BouquetEntry in alle Mengen ein
static int addBouquetEntry(const SIbouquet &s/*, int section_nr, int count*/)
{
	bool already_exists;
	uint16_t bouquet_id = 0;

	//s.position = (uint16_t) (((section_nr & 0x1f) << 11) + (count & 0x7ff));

	//if (mySIservicesNVODorderUniqueKey.find(s.uniqueKey()))
	readLockBouquets();
	MySIbouquetsOrderUniqueKey::iterator si = mySIbouquetsOrderUniqueKey.find(s.uniqueKey());
	already_exists = (si != mySIbouquetsOrderUniqueKey.end());

	if (!already_exists) {

		SIbouquet *bp = new SIbouquet(s);

		if (!bp)
		{
			printf("[sectionsd::addBouquetEntry] new SIbouquet failed.\n");
			unlockBouquets();
			throw std::bad_alloc();
		}

		SIbouquetPtr bpptr(bp);
		/*
		bpptr->position = (uint16_t) (((section_nr & 0x1f) << 11) + (number & 0x7ff));

		printf("Section Number: %d Count: %d position: %04x\n", section_nr, number, bpptr->position);
		*/
		unlockBouquets();
		writeLockBouquets();
		mySIbouquetsOrderUniqueKey.insert(std::make_pair(bpptr->uniqueKey(), bpptr));

		//Because of Bouquet_Id misuse. see SIsections.cpp. IDs are introduced there
		if ((bpptr->bouquet_id == 0x3ffe) || (bpptr->bouquet_id == 0x3fff))
			bouquet_id = bpptr->bouquet_id;
		else
			bouquet_id = 0;

	}
	unlockBouquets();
	return bouquet_id << 1 | (int) !already_exists;
}

/*
 *
 * communication with sectionsdclient:
 *
 */

 //  SIsPtr;
typedef boost::shared_ptr<class SInetwork>
SInetworkPtr;

typedef std::map<t_transponder_id, SInetworkPtr, std::less<t_transponder_id> > MySItranspondersOrderUniqueKey;
static MySItranspondersOrderUniqueKey mySItranspondersOrderUniqueKey;

// Fuegt einen Tranponder in alle Mengen ein
static bool addTransponder(const SInetwork &s, const bool is_actual)
{
	readLockTransponders();
	MySItranspondersOrderUniqueKey::iterator si = mySItranspondersOrderUniqueKey.find(s.uniqueKey());
	bool already_exists = (si != mySItranspondersOrderUniqueKey.end());

	if (!already_exists) {

		SInetwork *nw = new SInetwork(s);

		if (!nw)
		{
			printf("[sectionsd::updateNetwork] new SInetwork failed.\n");
			unlockTransponders();
			throw std::bad_alloc();
		}

		SInetworkPtr tpptr(nw);

		tpptr->is_actual = is_actual;

		unlockTransponders();
		writeLockTransponders();
		mySItranspondersOrderUniqueKey.insert(std::make_pair(tpptr->uniqueKey(), tpptr));
	}
	unlockTransponders();
	return !already_exists;
}
#endif
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


//------------------------------------------------------------
// misc. functions
//------------------------------------------------------------
/*
static t_channel_id findServiceUniqueKeyforServiceName(const char * const serviceName)
{
	SIservice *sp = new SIservice(0, 0, 0);

	if (!sp)
	{
		printf("[sectionsd::findServiceUniqueKeyforServiceName] new SIservice failed.\n");
		throw std::bad_alloc();
	}

	SIservicePtr s(sp);

	s->serviceName = serviceName;

	dprintf("Search for Service '%s'\n", serviceName);

	MySIservicesOrderServiceName::iterator si = mySIservicesOrderServiceName.find(s);

	if (si != mySIservicesOrderServiceName.end())
		return (*si)->uniqueKey();

	dputs("Service not found");

	return 0;
}
*/
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

	for (MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin(); e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end(); e++)
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
#if 0
static bool ServiceUniqueKeyHasCurrentNext(const t_channel_id serviceUniqueKey)
{
	time_t azeit = time(NULL);
	time_t nextstart = time(NULL);
	bool found = false;
	SItimes::iterator t;

	MySIeventsOrderFirstEndTimeServiceIDEventUniqueKey::iterator e =
					mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin();

	while ((e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end()) && (!found)) {
		if ((*e)->get_channel_id() == serviceUniqueKey)
		{
			t = (*e)->times.begin();
			while ((t != (*e)->times.end()) && (!found)) {
				if (((long)(azeit) < (long)(t->startzeit + t->dauer)) && (t->startzeit <= (long)(azeit))) {
					nextstart = t->startzeit + t->dauer;
					found = true;
				}
				else
					t++;
			}
		}
		if (!found)
			e++;
	}
	if (found) {
		dprintf("Current Event: %s\n",(*e)->getName().c_str());
		//current is there; check if next is too
		if (++t != (*e)->times.end())
			return true;
		while (e != mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end()) {
			if ((*e)->get_channel_id() == serviceUniqueKey) {
				t = (*e)->times.begin();
				while (t != (*e)->times.end()) {
					if (t->startzeit >= nextstart) {
						dprintf("Next Event: %s\n",(*e)->getName().c_str());
						return true;
					}
					t++;
				}
			}
			e++;
		}
	}

	return false;
}
#endif
/*
static const SIevent &findActualSIeventForServiceName(const char * const serviceName, SItime& zeit)
{
	t_channel_id serviceUniqueKey = findServiceUniqueKeyforServiceName(serviceName);

	if (serviceUniqueKey)
		return findActualSIeventForServiceUniqueKey(serviceUniqueKey, zeit);

	return nullEvt;
}
*/
// Sucht das naechste Event anhand unique key und Startzeit
static const SIevent &findNextSIevent(const event_id_t uniqueKey, SItime &zeit)
{
	MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end())
	{
		SItimes::iterator nextnvodtimes = eFirst->second->times.end();
		SItimes::iterator nexttimes = eFirst->second->times.end();

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

		MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator eNext;

		//if ((nextnvodtimes != eFirst->second->times.begin()) && (nextnvodtimes != eFirst->second->times.end())) {
			//Startzeit not first - we can't use the ordered list...
			for ( MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e = mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.begin(); e !=
			 	mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.end(); ++e ) {
				if ((*e)->get_channel_id() == eFirst->second->get_channel_id()) {
					for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t) {
						if (t->startzeit > zeit.startzeit) {
							if (nexttimes != eFirst->second->times.end()) {
								if (t->startzeit < nexttimes->startzeit) {
									eNext = e;
									nexttimes = t;
								}
							}
							else {
								eNext = e;
								nexttimes = t;
							}
						}
					}
				}
			}
/*		} else {
			//find next normal
			eNext = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.find(eFirst->second);
			eNext++;

			if (eNext != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end())
			{
				if ((*eNext)->get_channel_id() == eFirst->second->get_channel_id())
					nexttimes = (*eNext)->times.begin();
			}
		}
*/
		if (nextnvodtimes != eFirst->second->times.end())
			++nextnvodtimes;
		//Compare
		if (nexttimes != eFirst->second->times.end()) {
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

// Sucht das naechste Event anhand unique key und Startzeit
static const SIevent &findNextSIevent(const event_id_t uniqueKey, SItime &zeit)
{
	MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end())
	{

		SItimes::iterator t = eFirst->second->times.end();

		if (eFirst->second->times.size() > 1)
		{
			// Wir haben ein NVOD-Event
			// d.h. wir suchen die aktuelle Zeit und nehmen die naechste davon, falls existent

			for ( t = eFirst->second->times.begin(); t != eFirst->second->times.end(); t++)
				if (t->startzeit == zeit.startzeit)
				{
					t++;

					if (t != eFirst->second->times.end())
					{
					//	zeit = *t;
					//	return *(eFirst->second);
						break;
					}

					t = eFirst->second->times.end();
					break; // ganz normal naechstes Event suchen
				}
		}

		MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator eNext = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.find(eFirst->second);
		eNext++;

		if (eNext != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end())
		{
			if ((*eNext)->get_channel_id() == eFirst->second->get_channel_id())
			{
				if (t != eFirst->second->times.end()) {
					if (t->startzeit < (*eNext)->times.begin()->startzeit) {
						zeit = *t;
						return *(eFirst->second);
					}
				}
				MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator ePrev =
					mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin();
				while ( ((*ePrev)->times.begin()->startzeit < zeit.startzeit) &&
					(ePrev != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end()) ) {
					if ((*ePrev)->times.size() > 1) {
						t = (*ePrev)->times.begin();
						while ( (t != (*ePrev)->times.end()) && (t->startzeit <
								(*eNext)->times.begin()->startzeit) ) {
							if (t->startzeit > zeit.startzeit) {
								zeit = *t;
								return *(*ePrev);
							}
							t++;
						}
					}
					ePrev++;
				}
				zeit = *((*eNext)->times.begin());
				return *(*eNext);
			}
			else if (t != eFirst->second->times.end()) {
				zeit = *t;
				return *(eFirst->second);
			}
			else
				return nullEvt;
		}
		else if (t != eFirst->second->times.end()) {
			zeit = *t;
			return *(eFirst->second);
		}
	}

	return nullEvt;
}
*/
// Sucht das naechste UND vorhergehende Event anhand unique key und Startzeit
static void findPrevNextSIevent(const event_id_t uniqueKey, SItime &zeit, SIevent &prev, SItime &prev_zeit, SIevent &next, SItime &next_zeit)
{
	prev = nullEvt;
	next = nullEvt;
	bool prev_ok = false;
	bool next_ok = false;

	MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end())
	{
		if (eFirst->second->times.size() > 1)
		{
			// Wir haben ein NVOD-Event
			// d.h. wir suchen die aktuelle Zeit und nehmen die naechste davon, falls existent

			for (SItimes::iterator t = eFirst->second->times.begin(); t != eFirst->second->times.end(); ++t)
				if (t->startzeit == zeit.startzeit)
				{
					if (t != eFirst->second->times.begin())
					{
						--t;
						prev_zeit = *t;
						prev = *(eFirst->second);
						prev_ok = true;
						++t;
					}

					++t;

					if (t != eFirst->second->times.end())
					{
						next_zeit = *t;
						next = *(eFirst->second);
						next_ok = true;
					}

					if ( prev_ok && next_ok )
						return ; // beide gefunden...
					else
						break;
				}
		}

		MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator eNext = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.find(eFirst->second);

		if ( (!prev_ok) && (eNext != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin() ) )
		{
			--eNext;

			if ((*eNext)->get_channel_id() == eFirst->second->get_channel_id())
			{
				prev_zeit = *((*eNext)->times.begin());
				prev = *(*eNext);
			}

			++eNext;
		}

		++eNext;

		if ( (!next_ok) && (eNext != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end()) )
		{
			if ((*eNext)->get_channel_id() == eFirst->second->get_channel_id())
			{
				next_zeit = *((*eNext)->times.begin());
				next = *(*eNext);
			}
		}

		// printf("evt_id >%llx<, time %x - evt_id >%llx<, time %x\n", prev.uniqueKey(), prev_zeit.startzeit, next.uniqueKey(), next_zeit.startzeit);
	}
}

//---------------------------------------------------------------------
//			connection-thread
// handles incoming requests
//---------------------------------------------------------------------

struct connectionData
{
	int connectionSocket;

	struct sockaddr_in clientAddr;
};

static void commandPauseScanning(int connfd, char *data, const unsigned dataLength)
{
	if (dataLength != 4)
		return ;

	int pause = *(int *)data;

	if (pause && pause != 1)
		return ;

	dprintf("Request of %s scanning.\n", pause ? "stop" : "continue" );

	if (scanning && pause)
	{
		dmxCN.request_pause();
		dmxEIT.request_pause();
#ifdef ENABLE_FREESATEPG
		dmxFSEIT.request_pause();
#endif
#ifdef UPDATE_NETWORKS
		dmxNIT.request_pause();
		dmxSDT.request_pause();
#endif
#ifdef ENABLE_PPT
		dmxPPT.request_pause();
#endif
		scanning = 0;
	}
	else if (!pause && !scanning)
	{
		dmxCN.request_unpause();
#ifdef UPDATE_NETWORKS
		dmxNIT.request_unpause();
		dmxSDT.request_unpause();
#endif
		dmxEIT.request_unpause();
#ifdef ENABLE_FREESATEPG
		dmxFSEIT.request_unpause();
#endif
#ifdef ENABLE_PPT
		dmxPPT.request_unpause();
#endif
		writeLockEvents();
		if (myCurrentEvent) {
			delete myCurrentEvent;
			myCurrentEvent = NULL;
		}
		if (myNextEvent) {
			delete myNextEvent;
			myNextEvent = NULL;
		}
		unlockEvents();
		writeLockMessaging();
		messaging_have_CN = 0x00;
		messaging_got_CN = 0x00;
		unlockMessaging();
		scanning = 1;
		if (!bTimeCorrect && !ntpenable)
		{
			pthread_mutex_lock(&timeThreadSleepMutex);
			pthread_cond_broadcast(&timeThreadSleepCond);
			pthread_mutex_unlock(&timeThreadSleepMutex);
		}

		scanning = 1;
		dmxCN.change(0);
		dmxEIT.change(0);
#ifdef ENABLE_FREESATEPG
		dmxFSEIT.change(0);
#endif
	}

	struct sectionsd::msgResponseHeader msgResponse;

	msgResponse.dataLength = 0;

	writeNbytes(connfd, (const char *)&msgResponse, sizeof(msgResponse), WRITE_TIMEOUT_IN_SECONDS);

	return ;
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

static void commandDumpAllServices(int connfd, char* /*data*/, const unsigned /*dataLength*/)
{
	dputs("Request of service list.\n");
	long count=0;
#define MAX_SIZE_SERVICELIST	64*1024
	char *serviceList = new char[MAX_SIZE_SERVICELIST]; // 65kb should be enough and dataLength is unsigned short

	if (!serviceList)
	{
		fprintf(stderr, "low on memory!\n");
		return ;
	}

	*serviceList = 0;
	readLockServices();
#define MAX_SIZE_DATEN	200
	char daten[MAX_SIZE_DATEN];

	for (MySIservicesOrderUniqueKey::iterator s = mySIservicesOrderUniqueKey.begin(); s != mySIservicesOrderUniqueKey.end(); ++s)
	{
		count += 1 + snprintf(daten, MAX_SIZE_DATEN,
				PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				" %hu %hhu %d %d %d %d %u ",
			        s->first,
			        s->second->service_id, s->second->serviceTyp,
			        s->second->eitScheduleFlag(), s->second->eitPresentFollowingFlag(),
		        	s->second->runningStatus(), s->second->freeCAmode(),
		        	s->second->nvods.size());
/**	soll es in count ?
			+ strlen(s->second->serviceName.c_str()) + 1
 			+ strlen(s->second->providerName.c_str()) + 1
 			+ 3;  **/
		if (count < MAX_SIZE_SERVICELIST)
		{
			strcat(serviceList, daten);
			strcat(serviceList, "\n");
			strcat(serviceList, s->second->serviceName.c_str());
			strcat(serviceList, "\n");
			strcat(serviceList, s->second->providerName.c_str());
			strcat(serviceList, "\n");
		} else {
			dprintf("warning: commandDumpAllServices: serviceList cut\n");
			break;
		}
	}

	unlockServices();
	struct sectionsd::msgResponseHeader msgResponse;
	msgResponse.dataLength = strlen(serviceList) + 1;

	if (msgResponse.dataLength > MAX_SIZE_SERVICELIST)
		printf("warning: commandDumpAllServices: length=%d\n", msgResponse.dataLength);

	if (msgResponse.dataLength == 1)
		msgResponse.dataLength = 0;

	if (writeNbytes(connfd, (const char *)&msgResponse, sizeof(msgResponse), WRITE_TIMEOUT_IN_SECONDS) == true)
	{
		if (msgResponse.dataLength)
			writeNbytes(connfd, serviceList, msgResponse.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");

	delete[] serviceList;

	return ;
}

#if 0
static void commandSetEventsAreOldInMinutes(int connfd, char *data, const unsigned dataLength)
{
	if (dataLength != 2)
		return ;

	dprintf("Set events are old after minutes: %hd\n", *((unsigned short*)data));

	oldEventsAre = *((unsigned short*)data)*60L;

	struct sectionsd::msgResponseHeader responseHeader;

	responseHeader.dataLength = 0;

	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);

	return ;
}

static void commandSetHoursToCache(int connfd, char *data, const unsigned dataLength)
{
	if (dataLength != 2)
		return ;

	dprintf("Set hours to cache: %hd\n", *((unsigned short*)data));

	secondsToCache = *((unsigned short*)data)*60L*60L;

	struct sectionsd::msgResponseHeader responseHeader;

	responseHeader.dataLength = 0;

	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);

	return ;
}

static void commandSetHoursExtendedCache(int connfd, char *data, const unsigned dataLength)
{
	if (dataLength != 2)
		return ;

	dprintf("Set hours to cache extended text: %hd\n", *((unsigned short*)data));

	secondsExtendedTextCache = *((unsigned short*)data)*60L*60L;

	struct sectionsd::msgResponseHeader responseHeader;

	responseHeader.dataLength = 0;

	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);

	return ;
}

#endif

static void sendAllEvents(int connfd, t_channel_id serviceUniqueKey, bool oldFormat = true, char search = 0, std::string search_text = "")
{
#define MAX_SIZE_EVENTLIST	64*1024
	char *evtList = new char[MAX_SIZE_EVENTLIST]; // 64kb should be enough and dataLength is unsigned short
	char *liste;
	long count=0;
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
//	int laststart = 0;

	if (!evtList)
	{
		fprintf(stderr, "low on memory!\n");
		goto out;
	}

	dprintf("sendAllEvents for " PRINTF_CHANNEL_ID_TYPE "\n", serviceUniqueKey);
	*evtList = 0;
	liste = evtList;

	if (serviceUniqueKey != 0)
	{
		// service Found
		readLockEvents();
		int serviceIDfound = 0;

		if (search_text.length()) std::transform(search_text.begin(), search_text.end(), search_text.begin(), tolower);
		for (MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin(); e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end(); ++e)
		{
			if ((*e)->get_channel_id() == serviceUniqueKey)
			{
				serviceIDfound = 1;

				bool copy = true;
				if(search == 0); // nothing to do here
				else if(search == 1)
				{
					std::string eName = (*e)->getName();
					std::transform(eName.begin(), eName.end(), eName.begin(), tolower);
					if(eName.find(search_text) == std::string::npos)
						copy = false;
				}
				else if(search == 2)
				{
					std::string eText = (*e)->getText();
					std::transform(eText.begin(), eText.end(), eText.begin(), tolower);
					if(eText.find(search_text) == std::string::npos)
						copy = false;
				}
				else if(search == 3)
				{
					std::string eExtendedText = (*e)->getExtendedText();
					std::transform(eExtendedText.begin(), eExtendedText.end(), eExtendedText.begin(), tolower);
					if(eExtendedText.find(search_text) == std::string::npos)
						copy = false;
				}

				if(copy)
				{
					for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t)
					{
//						if (t->startzeit > laststart) {
//						laststart = t->startzeit;
						if ( oldFormat )
						{
#define MAX_SIZE_STRTIME	50
							char strZeit[MAX_SIZE_STRTIME];
							char strZeit2[MAX_SIZE_STRTIME];
							struct tm *tmZeit;

							tmZeit = localtime(&(t->startzeit));
							count += snprintf(strZeit, MAX_SIZE_STRTIME, "%012llx ", (*e)->uniqueKey());
							count += snprintf(strZeit2, MAX_SIZE_STRTIME, "%02d.%02d %02d:%02d %u ",
									tmZeit->tm_mday, tmZeit->tm_mon + 1, tmZeit->tm_hour, tmZeit->tm_min, (*e)->times.begin()->dauer / 60);
							count += (*e)->getName().length() + 1;

							if (count < MAX_SIZE_EVENTLIST) {
								strcat(liste, strZeit);
								strcat(liste, strZeit2);
								strcat(liste, (*e)->getName().c_str());
								strcat(liste, "\n");
							} else {
								dprintf("warning: sendAllEvents eventlist cut\n");
								break;
							}
						}
						else
						{
							count += sizeof(event_id_t) + 4 + 4 + (*e)->getName().length() + 1;
							if (((*e)->getText()).empty())
							{
								count += (*e)->getExtendedText().substr(0, 50).length();
							}
							else
							{
								count += (*e)->getText().length();
							}
							count++;

							if (count < MAX_SIZE_EVENTLIST) {
								*((event_id_t *)liste) = (*e)->uniqueKey();
								liste += sizeof(event_id_t);
								*((unsigned *)liste) = t->startzeit;
								liste += 4;
								*((unsigned *)liste) = t->dauer;
								liste += 4;
								strcpy(liste, (*e)->getName().c_str());
								liste += (*e)->getName().length();
								liste++;

								if (((*e)->getText()).empty())
								{
									strcpy(liste, (*e)->getExtendedText().substr(0, 50).c_str());
									liste += strlen(liste);
								}
								else
								{
									strcpy(liste, (*e)->getText().c_str());
									liste += (*e)->getText().length();
								}
								liste++;
							} else {
								dprintf("warning: sendAllEvents eventlist cut\n");
								break;
							}
						}
	//					}
					}
				} // if = serviceID
			}
			else if ( serviceIDfound )
				break; // sind nach serviceID und startzeit sortiert -> nicht weiter suchen
		}

		unlockEvents();
	}

	//printf("warning: [sectionsd] all events - response-size: 0x%x, count = %lx\n", liste - evtList, count);
	if (liste - evtList > MAX_SIZE_EVENTLIST)
		printf("warning: [sectionsd] all events - response-size: 0x%x\n", liste - evtList);
	responseHeader.dataLength = liste - evtList;

	dprintf("[sectionsd] all events - response-size: 0x%x\n", responseHeader.dataLength);

	if ( responseHeader.dataLength == 1 )
		responseHeader.dataLength = 0;

 out:
	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true)
	{
		if (responseHeader.dataLength)
			writeNbytes(connfd, evtList, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");

	if (evtList)
		delete[] evtList;

	return ;
}
/*
static void commandAllEventsChannelName(int connfd, char *data, const unsigned dataLength)
{
	data[dataLength - 1] = 0; // to be sure it has an trailing 0
	dprintf("Request of all events for '%s'\n", data);
	lockServices();
	t_channel_id uniqueServiceKey = findServiceUniqueKeyforServiceName(data);
	unlockServices();
	sendAllEvents(connfd, uniqueServiceKey);
	return ;
}
*/
static void commandAllEventsChannelID(int connfd, char *data, const unsigned dataLength)
{
	if (dataLength != sizeof(t_channel_id))
		return ;

	t_channel_id serviceUniqueKey = *(t_channel_id *)data;

	dprintf("Request of all events for " PRINTF_CHANNEL_ID_TYPE "\n", serviceUniqueKey);

	sendAllEvents(connfd, serviceUniqueKey, false);

	return ;
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

	struct mallinfo speicherinfo = mallinfo();

	//  struct rusage resourceUsage;
	//  getrusage(RUSAGE_CHILDREN, &resourceUsage);
	//  getrusage(RUSAGE_SELF, &resourceUsage);
	time_t zeit = time(NULL);

#define MAX_SIZE_STATI	2024
	char stati[MAX_SIZE_STATI];

	snprintf(stati, MAX_SIZE_STATI,
		"$Id: sectionsd.cpp,v 1.305 2009/07/30 12:41:39 seife Exp $\n"
		"Current time: %s"
		"Hours to cache: %ld\n"
		"Hours to cache extended text: %ld\n"
		"Events are old %ldmin after their end time\n"
		"Number of cached services: %u\n"
		"Number of cached nvod-services: %u\n"
		"Number of cached events: %u\n"
		"Number of cached nvod-events: %u\n"
		"Number of cached meta-services: %u\n"
		//    "Resource-usage: maxrss: %ld ixrss: %ld idrss: %ld isrss: %ld\n"
		"Total size of memory occupied by chunks\n"
		"handed out by malloc: %d (%dkb)\n"
		"Total bytes memory allocated with `sbrk' by malloc,\n"
		"in bytes: %d (%dkb)\n",
		ctime(&zeit),
		secondsToCache / (60*60L), secondsExtendedTextCache / (60*60L), oldEventsAre / 60, anzServices, anzNVODservices, anzEvents, anzNVODevents, anzMetaServices,
		//    resourceUsage.ru_maxrss, resourceUsage.ru_ixrss, resourceUsage.ru_idrss, resourceUsage.ru_isrss,
		speicherinfo.uordblks, speicherinfo.uordblks / 1024,
		speicherinfo.arena, speicherinfo.arena / 1024
		);
	printf("%s\n", stati);
#if 0
	struct sectionsd::msgResponseHeader responseHeader;

	responseHeader.dataLength = strlen(stati) + 1;

	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true)
	{
		if (responseHeader.dataLength)
			writeNbytes(connfd, stati, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");
#endif
	return ;
}
/*
static void commandCurrentNextInfoChannelName(int connfd, char *data, const unsigned dataLength)
{
	int nResultDataSize = 0;
	char* pResultData = 0;

	data[dataLength - 1] = 0; // to be sure it has an trailing 0
	dprintf("Request of current/next information for '%s'\n", data);

	if (EITThreadsPause()) // -> lock
		return ;

	lockServices();

	lockEvents();

	SItime zeitEvt1(0, 0);

	const SIevent &evt = findActualSIeventForServiceName(data, zeitEvt1);

	unlockServices();

	if (evt.service_id != 0)
	{ //Found
		dprintf("current EPG found.\n");
		SItime zeitEvt2(zeitEvt1);
		const SIevent &nextEvt = findNextSIevent(evt.uniqueKey(), zeitEvt2);

		if (nextEvt.service_id != 0)
		{
			dprintf("next EPG found.\n");
			// Folgendes ist grauenvoll, habs aber einfach kopiert aus epgd
			// und keine Lust das grossartig zu verschoenern
			nResultDataSize =
			    12 + 1 + 					// Unique-Key + del
			    strlen(evt.getName().c_str()) + 1 + 		//Name + del
			    3 + 2 + 1 + 					//std:min + del
			    4 + 1 + 					//dauer (mmmm) + del
			    3 + 1 + 					//100 + del
			    12 + 1 + 					// Unique-Key + del
			    strlen(nextEvt.getName().c_str()) + 1 + 		//Name + del
			    3 + 2 + 1 + 					//std:min + del
			    4 + 1 + 1;					//dauer (mmmm) + del + 0
			pResultData = new char[nResultDataSize];

			if (!pResultData)
			{
				fprintf(stderr, "low on memory!\n");
				unlockEvents();
				EITThreadsUnPause();
				return ;
			}

			struct tm *pStartZeit = localtime(&zeitEvt1.startzeit);

			int nSH(pStartZeit->tm_hour), nSM(pStartZeit->tm_min);

			unsigned dauer = zeitEvt1.dauer / 60;

			unsigned nProcentagePassed = (unsigned)((float)(time(NULL) - zeitEvt1.startzeit) / (float)zeitEvt1.dauer * 100.);

			pStartZeit = localtime(&zeitEvt2.startzeit);

			int nSH2(pStartZeit->tm_hour), nSM2(pStartZeit->tm_min);

			unsigned dauer2 = zeitEvt2.dauer / 60;

			sprintf(pResultData,
			        "%012llx\n%s\n%02d:%02d\n%04u\n%03u\n%012llx\n%s\n%02d:%02d\n%04u\n",
			        evt.uniqueKey(),
			        evt.getName().c_str(),
			        nSH, nSM, dauer, nProcentagePassed,
			        nextEvt.uniqueKey(),
			        nextEvt.getName().c_str(),
			        nSH2, nSM2, dauer2);
		}
	}

	unlockEvents();
	EITThreadsUnPause(); // -> unlock

	// response

	struct sectionsd::msgResponseHeader pmResponse;
	pmResponse.dataLength = nResultDataSize;
	bool rc = writeNbytes(connfd, (const char *)&pmResponse, sizeof(pmResponse), WRITE_TIMEOUT_IN_SECONDS);

	if ( nResultDataSize > 0 )
	{
		if (rc == true)
			writeNbytes(connfd, pResultData, nResultDataSize, WRITE_TIMEOUT_IN_SECONDS);
		else
			dputs("[sectionsd] Fehler/Timeout bei write");

		delete[] pResultData;
	}
	else
	{
		dprintf("current/next EPG not found!\n");
	}

	return ;
}
*/
static void commandComponentTagsUniqueKey(int connfd, char *data, const unsigned dataLength)
{
	int nResultDataSize = 0;
	char *pResultData = 0;
	char *p;
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	MySIeventsOrderUniqueKey::iterator eFirst;

	if (dataLength != 8)
		return ;

	event_id_t uniqueKey = *(event_id_t *)data;

	dprintf("Request of ComponentTags for 0x%llx\n", uniqueKey);

	readLockEvents();

	nResultDataSize = sizeof(int);    // num. Component-Tags

	eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end())
	{
		//Found
		dprintf("ComponentTags found.\n");
		dprintf("components.size %d \n", eFirst->second->components.size());

		for (SIcomponents::iterator cmp = eFirst->second->components.begin(); cmp != eFirst->second->components.end(); ++cmp)
		{
			dprintf(" %s \n", cmp->component.c_str());
			nResultDataSize += cmp->component.length() + 1 +	// name
			                   sizeof(unsigned char) +		// componentType
			                   sizeof(unsigned char) +		// componentTag
			                   sizeof(unsigned char);		// streamContent
		}
	}

	pResultData = new char[nResultDataSize];

	if (!pResultData)
	{
		fprintf(stderr, "low on memory!\n");
		unlockEvents();
		goto out;
	}

	p = pResultData;

	if (eFirst != mySIeventsOrderUniqueKey.end())
	{
		*((int *)p) = eFirst->second->components.size();
		p += sizeof(int);

		for (SIcomponents::iterator cmp = eFirst->second->components.begin(); cmp != eFirst->second->components.end(); ++cmp)
		{

			strcpy(p, cmp->component.c_str());
			p += cmp->component.length() + 1;
			*((unsigned char *)p) = cmp->componentType;
			p += sizeof(unsigned char);
			*((unsigned char *)p) = cmp->componentTag;
			p += sizeof(unsigned char);
			*((unsigned char *)p) = cmp->streamContent;
			p += sizeof(unsigned char);
		}
	}
	else
	{
		*((int *)p) = 0;
		p += sizeof(int);
	}

	unlockEvents();

	responseHeader.dataLength = nResultDataSize;

 out:
	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true)
	{
		if (responseHeader.dataLength)
			writeNbytes(connfd, pResultData, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");

	if (pResultData)
		delete[] pResultData;

	return ;
}

static void commandLinkageDescriptorsUniqueKey(int connfd, char *data, const unsigned dataLength)
{
	int nResultDataSize = 0;
	char *pResultData = 0;
	char *p;
	MySIeventsOrderUniqueKey::iterator eFirst;
	int countDescs = 0;
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	event_id_t uniqueKey;

	if (dataLength != 8)
		goto out;

	uniqueKey = *(event_id_t *)data;

	dprintf("Request of LinkageDescriptors for 0x%llx\n", uniqueKey);

	readLockEvents();

	nResultDataSize = sizeof(int);    // num. Component-Tags

	eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end())
	{
		//Found
		dprintf("LinkageDescriptors found.\n");
		dprintf("linkage_descs.size %d \n", eFirst->second->linkage_descs.size());


		for (SIlinkage_descs::iterator linkage_desc = eFirst->second->linkage_descs.begin(); linkage_desc != eFirst->second->linkage_descs.end(); ++linkage_desc)
		{
			if (linkage_desc->linkageType == 0xB0)
			{
				countDescs++;
				dprintf(" %s \n", linkage_desc->name.c_str());
				nResultDataSize += linkage_desc->name.length() + 1 +	// name
				                   sizeof(t_transport_stream_id) +	//transportStreamId
				                   sizeof(t_original_network_id) +	//originalNetworkId
				                   sizeof(t_service_id);		//serviceId
			}
		}
	}

	pResultData = new char[nResultDataSize];

	if (!pResultData)
	{
		fprintf(stderr, "low on memory!\n");
		unlockEvents();
		goto out;
	}

	p = pResultData;

	*((int *)p) = countDescs;
	p += sizeof(int);

	if (eFirst != mySIeventsOrderUniqueKey.end())
	{
		for (SIlinkage_descs::iterator linkage_desc = eFirst->second->linkage_descs.begin(); linkage_desc != eFirst->second->linkage_descs.end(); ++linkage_desc)
		{
			if (linkage_desc->linkageType == 0xB0)
			{
				strcpy(p, linkage_desc->name.c_str());
				p += linkage_desc->name.length() + 1;
				*((t_transport_stream_id *)p) = linkage_desc->transportStreamId;
				p += sizeof(t_transport_stream_id);
				*((t_original_network_id *)p) = linkage_desc->originalNetworkId;
				p += sizeof(t_original_network_id);
				*((t_service_id *)p) = linkage_desc->serviceId;
				p += sizeof(t_service_id);
			}
		}
	}

	unlockEvents();

	responseHeader.dataLength = nResultDataSize;

 out:
	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader),  WRITE_TIMEOUT_IN_SECONDS) == true)
	{
		if (responseHeader.dataLength)
			writeNbytes(connfd, pResultData, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");

	if (pResultData)
		delete[] pResultData;

	return ;
}
/*
std::vector<int64_t>	messaging_skipped_sections_ID [0x22];			// 0x4e .. 0x6f
static int64_t 	messaging_sections_max_ID [0x22];			// 0x4e .. 0x6f
static int 		messaging_sections_got_all [0x22];			// 0x4e .. 0x6f
*/
//static unsigned char 	messaging_current_version_number = 0xff;
//static unsigned char 	messaging_current_section_number = 0;
/* messaging_eit_is_busy does not need locking, it is only written to from CN-Thread */
static bool		messaging_eit_is_busy = false;
static bool		messaging_need_eit_version = false;

//std::vector<int64_t>	messaging_sdt_skipped_sections_ID [2];			// 0x42, 0x46
//static int64_t 	messaging_sdt_sections_max_ID [2];			// 0x42, 0x46
//static int 		messaging_sdt_sections_got_all [2];			// 0x42, 0x46
/*
static bool 		messaging_sdt_actual_sections_got_all;						// 0x42
static bool		messaging_sdt_actual_sections_so_far [MAX_SECTIONS];				// 0x42
static t_transponder_id	messaging_sdt_other_sections_got_all [MAX_OTHER_SDT];				// 0x46
static bool		messaging_sdt_other_sections_so_far [MAX_CONCURRENT_OTHER_SDT] [MAX_SECTIONS];	// 0x46
static t_transponder_id	messaging_sdt_other_tid [MAX_CONCURRENT_OTHER_SDT];				// 0x46
*/
#ifdef UPDATE_NETWORKS
static int		messaging_bat_last_section [MAX_BAT];						// 0x4A
static int		messaging_bat_sections_so_far [MAX_BAT] [MAX_SECTIONS];				// 0x4A
static t_bouquet_id	messaging_bat_bouquet_id [MAX_BAT];						// 0x4A

//static bool		sdt_backoff = true;
//static bool		new_services = false;
/*
static bool		nit_backoff = true;
static bool 		messaging_nit_actual_sections_got_all;						// 0x40
static bool		messaging_nit_actual_sections_so_far [MAX_SECTIONS];				// 0x40
static t_network_id	messaging_nit_other_sections_got_all [MAX_OTHER_NIT];				// 0x41
static bool		messaging_nit_other_sections_so_far [MAX_CONCURRENT_OTHER_NIT] [MAX_SECTIONS];	// 0x41
static t_network_id	messaging_nit_other_nid [MAX_CONCURRENT_OTHER_NIT];				// 0x41
*/
/* nessaging_nit_nid does not need locking, because it is only used in one thread (nit thread). */
static t_network_id 	messaging_nit_nid[MAX_NIDs];							// 0x40,0x41
/* nessaging_sdt_tid does not need locking, because it is only used in one thread (sdt thread). */
static t_transponder_id messaging_sdt_tid[MAX_SDTs];							// 0x42,0x46
static int		auto_scanning = 0;
#endif
std::string		epg_dir("");

static void commandserviceChanged(int connfd, char *data, const unsigned dataLength)
{
	t_channel_id *uniqueServiceKey;
	if (dataLength != sizeof(sectionsd::commandSetServiceChanged))
		goto out;

	uniqueServiceKey = &(((sectionsd::commandSetServiceChanged *)data)->channel_id);

	dprintf("[sectionsd] commandserviceChanged: Service changed to " PRINTF_CHANNEL_ID_TYPE "\n", *uniqueServiceKey);

	messaging_last_requested = time_monotonic();

	if(checkBlacklist(*uniqueServiceKey))
	{
		if (!channel_is_blacklisted) {
			channel_is_blacklisted = true;
			dmxCN.request_pause();
			dmxEIT.request_pause();
#ifdef UPDATE_NETWORKS
			dmxNIT.request_pause();
			dmxSDT.request_pause();
#endif
#ifdef ENABLE_PPT
			dmxPPT.request_pause();
#endif
		}
		xprintf("[sectionsd] commandserviceChanged: service is filtered!\n");
	}
	else
	{
		if (channel_is_blacklisted) {
			channel_is_blacklisted = false;
			dmxCN.request_unpause();
			dmxEIT.request_unpause();
#ifdef UPDATE_NETWORKS
			dmxNIT.request_unpause();
			dmxSDT.request_unpause();
#endif
#ifdef ENABLE_PPT
			dmxPPT.request_unpause();
#endif
			xprintf("[sectionsd] commandserviceChanged: service is no longer filtered!\n");
		}
	}

	if(checkNoDVBTimelist(*uniqueServiceKey))
	{
		if (dvb_time_update) {
			dvb_time_update = false;
		}
		xprintf("[sectionsd] commandserviceChanged: DVB time update is blocked!\n");
	}
	else
	{
		if (!dvb_time_update) {
			dvb_time_update = true;
			xprintf("[sectionsd] commandserviceChanged: DVB time update is allowed!\n");
		}
	}

	if (messaging_current_servicekey != *uniqueServiceKey)
	{
		//if (debug) showProfiling("[sectionsd] commandserviceChanged: before events lock");
		writeLockEvents();
		//if (debug) showProfiling("[sectionsd] commandserviceChanged: after events lock");
		if (myCurrentEvent) {
			delete myCurrentEvent;
			myCurrentEvent = NULL;
		}
		if (myNextEvent) {
			delete myNextEvent;
			myNextEvent = NULL;
		}
		unlockEvents();
		writeLockMessaging();
		messaging_current_servicekey = *uniqueServiceKey;
		messaging_have_CN = 0x00;
		messaging_got_CN = 0x00;
		messaging_zap_detected = true;
#ifdef UPDATE_NETWORKS
		for ( int i = 0; i < MAX_BAT; i++) {
			messaging_bat_bouquet_id[i] = 0;
			messaging_bat_last_section[i] = 0;
			for ( int j= 0; j < MAX_SECTIONS; j++)
				messaging_bat_sections_so_far[i][j] = 0;
		}
#endif
		messaging_need_eit_version = false;
		unlockMessaging();
		dmxCN.setCurrentService(messaging_current_servicekey & 0xffff);
		dmxEIT.setCurrentService(messaging_current_servicekey & 0xffff);
#ifdef ENABLE_FREESATEPG
		dmxFSEIT.setCurrentService(messaging_current_servicekey & 0xffff);
#endif
	}
	else
		dprintf("[sectionsd] commandserviceChanged: no change...\n");

 out:
	struct sectionsd::msgResponseHeader msgResponse;
	msgResponse.dataLength = 0;
	writeNbytes(connfd, (const char *)&msgResponse, sizeof(msgResponse), WRITE_TIMEOUT_IN_SECONDS);

	dprintf("[sectionsd] commandserviceChanged: END!!\n");
	return ;
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
static void commandCurrentNextInfoChannelID(int connfd, char *data, const unsigned dataLength)
{
	int nResultDataSize = 0;
	char* pResultData = 0;
	char* p;
	SIevent currentEvt;
	SIevent nextEvt;
	unsigned flag = 0, flag2=0;
	/* ugly hack: retry fetching current/next by restarting dmxCN if this is true */
	bool change = false;
	struct sectionsd::msgResponseHeader pmResponse;

	t_channel_id * uniqueServiceKey = (t_channel_id *)data;

	if (dataLength != sizeof(t_channel_id))
		goto out;

	dprintf("[sectionsd] Request of current/next information for " PRINTF_CHANNEL_ID_TYPE "\n", *uniqueServiceKey);

	readLockEvents();
	/* if the currently running program is requested... */
	if (*uniqueServiceKey == messaging_current_servicekey) {
		/* ...check for myCurrentEvent and myNextEvent */
		if (!myCurrentEvent) {
			dprintf("!myCurrentEvent ");
			change = true;
			flag |= CSectionsdClient::epgflags::not_broadcast;
		} else {
			currentEvt = *myCurrentEvent;
			flag |= CSectionsdClient::epgflags::has_current; // aktuelles event da...
			flag |= CSectionsdClient::epgflags::has_anything;
		}
		if (!myNextEvent) {
			dprintf("!myNextEvent ");
			change = true;
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
			currentEvt = findActualSIeventForServiceUniqueKey(*uniqueServiceKey, zeitEvt1, 0, &flag2);
		} else {
			zeitEvt1.startzeit = currentEvt.times.begin()->startzeit;
			zeitEvt1.dauer = currentEvt.times.begin()->dauer;
		}
		SItime zeitEvt2(zeitEvt1);

		if (currentEvt.getName().empty() && flag2 != 0)
		{
			dprintf("commandCurrentNextInfoChannelID change1\n");
			change = true;
		}

		if (currentEvt.service_id != 0)
		{ //Found
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
		{ // no current event...
			readLockServices();

			MySIservicesOrderUniqueKey::iterator si = mySIservicesOrderUniqueKey.end();
			si = mySIservicesOrderUniqueKey.find(*uniqueServiceKey);

			if (si != mySIservicesOrderUniqueKey.end())
			{
				dprintf("[sectionsd] current service has%s scheduled events, and has%s present/following events\n", si->second->eitScheduleFlag() ? "" : " no", si->second->eitPresentFollowingFlag() ? "" : " no" );

				if ( /*( !si->second->eitScheduleFlag() ) || */
				    ( !si->second->eitPresentFollowingFlag() ) )
				{
					flag |= CSectionsdClient::epgflags::not_broadcast;
				}
			}
			unlockServices();

			if ( flag2 & CSectionsdClient::epgflags::has_anything )
			{
				flag |= CSectionsdClient::epgflags::has_anything;
				if (!(flag & CSectionsdClient::epgflags::has_next)) {
					dprintf("*nextEvt not from cur/next V2!\n");
					nextEvt = findNextSIeventForServiceUniqueKey(*uniqueServiceKey, zeitEvt2);
				}

				if (nextEvt.service_id != 0)
				{
					MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(*uniqueServiceKey);

					if (eFirst != mySIeventsOrderUniqueKey.end())
					{
						// this is a race condition if first entry found is == mySIeventsOrderUniqueKey.begin()
						// so perform a check
						if (eFirst != mySIeventsOrderUniqueKey.begin())
							--eFirst;

						if (eFirst != mySIeventsOrderUniqueKey.begin())
						{
							time_t azeit = time(NULL);

							if (eFirst->second->times.begin()->startzeit < azeit &&
							    eFirst->second->uniqueKey() == nextEvt.uniqueKey() - 1)
								flag |= CSectionsdClient::epgflags::has_no_current;
						}
					}
				}
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
			change = true;
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

	nResultDataSize =
	    sizeof(event_id_t) +                        // Unique-Key
	    sizeof(CSectionsdClient::sectionsdTime) +  	// zeit
	    currentEvt.getName().length() + 1 + 	// name + '\0'
	    sizeof(event_id_t) +                        // Unique-Key
	    sizeof(CSectionsdClient::sectionsdTime) +  	// zeit
	    nextEvt.getName().length() + 1 +    	// name + '\0'
	    sizeof(unsigned) + 				// flags
	    1						// CurrentFSK
	    ;

	pResultData = new char[nResultDataSize];
	time_t now;

	if (!pResultData)
	{
		fprintf(stderr, "low on memory!\n");
		unlockEvents();
		nResultDataSize = 0; // send empty response
		goto out;
	}

	dprintf("currentEvt: '%s' (%04x) nextEvt: '%s' (%04x) flag: 0x%02x\n",
		currentEvt.getName().c_str(), currentEvt.eventID,
		nextEvt.getName().c_str(), nextEvt.eventID, flag);

	CSectionsdClient::sectionsdTime time_cur;
	CSectionsdClient::sectionsdTime time_nxt;
	now = time(NULL);
	time_cur.startzeit = currentEvt.times.begin()->startzeit;
	time_cur.dauer = currentEvt.times.begin()->dauer;
	time_nxt.startzeit = nextEvt.times.begin()->startzeit;
	time_nxt.dauer = nextEvt.times.begin()->dauer;
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

	p = pResultData;
	*((event_id_t *)p) = currentEvt.uniqueKey();
	p += sizeof(event_id_t);
	*((CSectionsdClient::sectionsdTime *)p) = time_cur;
	p += sizeof(CSectionsdClient::sectionsdTime);
	strcpy(p, currentEvt.getName().c_str());
	p += currentEvt.getName().length() + 1;
	*((event_id_t *)p) = nextEvt.uniqueKey();
	p += sizeof(event_id_t);
	*((CSectionsdClient::sectionsdTime *)p) = time_nxt;
	p += sizeof(CSectionsdClient::sectionsdTime);
	strcpy(p, nextEvt.getName().c_str());
	p += nextEvt.getName().length() + 1;
	*((unsigned*)p) = flag;
	p += sizeof(unsigned);
	*p = currentEvt.getFSK();
	p++;

	unlockEvents();

	//dprintf("change: %s, messaging_eit_busy: %s, last_request: %d\n", change?"true":"false", messaging_eit_is_busy?"true":"false",(time_monotonic() - messaging_last_requested));
	if (change && !messaging_eit_is_busy && (time_monotonic() - messaging_last_requested) < 11) {
		/* restart dmxCN, but only if it is not already running, and only for 10 seconds */
		dprintf("change && !messaging_eit_is_busy => dmxCN.change(0)\n");
		dmxCN.change(0);
	}

	// response

 out:
	pmResponse.dataLength = nResultDataSize;
	bool rc = writeNbytes(connfd, (const char *)&pmResponse, sizeof(pmResponse), WRITE_TIMEOUT_IN_SECONDS);

	if ( nResultDataSize > 0 )
	{
		if (rc == true)
			writeNbytes(connfd, pResultData, nResultDataSize, WRITE_TIMEOUT_IN_SECONDS);
		else
			dputs("[sectionsd] Fehler/Timeout bei write");

		delete[] pResultData;
	}
	else
	{
		dprintf("[sectionsd] current/next EPG not found!\n");
	}

	return ;
}

// Sendet ein EPG, unlocked die events, unpaused dmxEIT

static void sendEPG(int connfd, const SIevent& e, const SItime& t, int shortepg = 0)
{

	struct sectionsd::msgResponseHeader responseHeader;

	if (!shortepg)
	{
		// new format - 0 delimiters
		responseHeader.dataLength =
		    sizeof(event_id_t) +                        // Unique-Key
		    e.getName().length() + 1 + 			// Name + del
		    e.getText().length() + 1 + 			// Text + del
		    e.getExtendedText().length() + 1 +		// ext + del
			// 21.07.2005 - rainerk
			// Send extended events
		    e.itemDescription.length() + 1 +		// Item Description + del
		    e.item.length() + 1 +			// Item + del
		    e.contentClassification.length() + 1 + 	// Text + del
		    e.userClassification.length() + 1 + 	// ext + del
		    1 +                                   	// fsk
		    sizeof(CSectionsdClient::sectionsdTime); 	// zeit
	}
	else
		responseHeader.dataLength =
		    e.getName().length() + 1 + 			// Name + del
		    e.getText().length() + 1 + 			// Text + del
		    e.getExtendedText().length() + 1 + 1;	// ext + del + 0

	char* msgData = new char[responseHeader.dataLength];

	if (!msgData)
	{
		fprintf(stderr, "sendEPG: low on memory!\n");
		unlockEvents();
		responseHeader.dataLength = 0;
		goto out;
	}

	if (!shortepg)
	{
		char *p = msgData;
		*((event_id_t *)p) = e.uniqueKey();
		p += sizeof(event_id_t);

		strcpy(p, e.getName().c_str());
		p += e.getName().length() + 1;
		strcpy(p, e.getText().c_str());
		p += e.getText().length() + 1;
		strcpy(p, e.getExtendedText().c_str());
		p += e.getExtendedText().length() + 1;
		// 21.07.2005 - rainerk
		// Send extended events
		strcpy(p, e.itemDescription.c_str());
		p += e.itemDescription.length() + 1;
		strcpy(p, e.item.c_str());
		p += e.item.length() + 1;

//		strlen(userClassification.c_str()) is not equal to e.userClassification.length()
//		because of binary data same is with contentClassification
		// add length
		*p = (unsigned char)e.contentClassification.length();
		p++;
		memmove(p, e.contentClassification.data(), e.contentClassification.length());
		p += e.contentClassification.length();

		*p = (unsigned char)e.userClassification.length();
		p++;
		memmove(p, e.userClassification.data(), e.userClassification.length());
		p += e.userClassification.length();

		*p = e.getFSK();
		p++;

		CSectionsdClient::sectionsdTime zeit;
		zeit.startzeit = t.startzeit;
		zeit.dauer = t.dauer;
		*((CSectionsdClient::sectionsdTime *)p) = zeit;
		p += sizeof(CSectionsdClient::sectionsdTime);

	}
	else
		sprintf(msgData,
		        "%s\xFF%s\xFF%s\xFF",
		        e.getName().c_str(),
		        e.getText().c_str(),
		        e.getExtendedText().c_str()
		       );

	unlockEvents();

 out:
	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS)) {
		if (responseHeader.dataLength)
			writeNbytes(connfd, msgData, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");

	if (msgData)
		delete[] msgData;
}

static void commandGetNextEPG(int connfd, char *data, const unsigned dataLength)
{
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;

	if (dataLength != 8 + 4) {
		writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
		return ;
	}

	event_id_t * uniqueEventKey = (event_id_t *)data;

	time_t *starttime = (time_t *)(data + 8);

	dprintf("Request of next epg for 0x%llx %s", *uniqueEventKey, ctime(starttime));

	readLockEvents();

	SItime zeit(*starttime, 0);

	const SIevent &nextEvt = findNextSIevent(*uniqueEventKey, zeit);

	if (nextEvt.service_id != 0)
	{
		dprintf("next epg found.\n");
		sendEPG(connfd, nextEvt, zeit);
// this call is made in sendEPG()
//		unlockEvents();
		return;
	}

	unlockEvents();
	dprintf("next epg not found!\n");

	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
	return ;
}

static void commandActualEPGchannelID(int connfd, char *data, const unsigned dataLength)
{
	if (dataLength != sizeof(t_channel_id))
		return ;

	t_channel_id * uniqueServiceKey = (t_channel_id *)data;
	SIevent evt;
	SItime zeit(0, 0);

	dprintf("[commandActualEPGchannelID] Request of current EPG for " PRINTF_CHANNEL_ID_TYPE "\n", * uniqueServiceKey);

	readLockEvents();
	if (*uniqueServiceKey == messaging_current_servicekey) {
		if (myCurrentEvent) {
			evt = *myCurrentEvent;
			zeit.startzeit = evt.times.begin()->startzeit;
			zeit.dauer = evt.times.begin()->dauer;
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
		evt = findActualSIeventForServiceUniqueKey(*uniqueServiceKey, zeit);
	}

	if (evt.service_id != 0)
	{
		dprintf("EPG found.\n");
		sendEPG(connfd, evt, zeit);
		return;
	}

	unlockEvents();
	dprintf("EPG not found!\n");

// out:
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);

	return ;
}

static void commandGetEPGPrevNext(int connfd, char *data, const unsigned dataLength)
{
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	char* msgData = NULL;

	if (dataLength != 8 + 4) {
		writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
		return;
	}

	event_id_t * uniqueEventKey = (event_id_t *)data;

	time_t *starttime = (time_t *)(data + 8);
	SItime zeit(*starttime, 0);
	SItime prev_zeit(0, 0);
	SItime next_zeit(0, 0);
	SIevent prev_evt;
	SIevent next_evt;

	dprintf("Request of Prev/Next EPG for 0x%llx %s", *uniqueEventKey, ctime(starttime));

	readLockEvents();

	findPrevNextSIevent(*uniqueEventKey, zeit, prev_evt, prev_zeit, next_evt, next_zeit);

	responseHeader.dataLength =
	    12 + 1 + 				// Unique-Key + del
	    8 + 1 + 				// start time + del
	    12 + 1 + 				// Unique-Key + del
	    8 + 1 + 1;				// start time + del

	msgData = new char[responseHeader.dataLength];

	if (!msgData)
	{
		fprintf(stderr, "low on memory!\n");
		unlockEvents();
		responseHeader.dataLength = 0; // empty response
		goto out;
	}

	sprintf(msgData, "%012llx\xFF%08lx\xFF%012llx\xFF%08lx\xFF",
	        prev_evt.uniqueKey(),
	        prev_zeit.startzeit,
	        next_evt.uniqueKey(),
	        next_zeit.startzeit
	       );
	unlockEvents();

 out:
	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS)) {
		if (responseHeader.dataLength)
			writeNbytes(connfd, msgData, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	} else
		dputs("[sectionsd] Fehler/Timeout bei write");

	if (msgData)
		delete[] msgData;

	return ;
}

// Mostly copied from epgd (something bugfixed ;) )
/*
static void commandActualEPGchannelName(int connfd, char *data, const unsigned dataLength)
{
	int nResultDataSize = 0;
	char* pResultData = 0;

	data[dataLength - 1] = 0; // to be sure it has an trailing 0
	dprintf("Request of actual EPG for '%s'\n", data);

	if (EITThreadsPause()) // -> lock
		return ;

	lockServices();

	lockEvents();

	SItime zeitEvt(0, 0);

	const SIevent &evt = findActualSIeventForServiceName(data, zeitEvt);

	unlockServices();

	if (evt.service_id != 0)
	{ //Found
		dprintf("EPG found.\n");
		nResultDataSize =
		    12 + 1 + 					// Unique-Key + del
		    strlen(evt.getName().c_str()) + 1 + 		//Name + del
		    strlen(evt.getText().c_str()) + 1 + 		//Text + del
		    strlen(evt.getExtendedText().c_str()) + 1 + 	//ext + del
		    3 + 3 + 4 + 1 + 					//dd.mm.yyyy + del
		    3 + 2 + 1 + 					//std:min + del
		    3 + 2 + 1 + 					//std:min+ del
		    3 + 1 + 1;					//100 + del + 0
		pResultData = new char[nResultDataSize];

		if (!pResultData)
		{
			fprintf(stderr, "low on memory!\n");
			unlockEvents();
			EITThreadsUnPause();
			return ;
		}

		struct tm *pStartZeit = localtime(&zeitEvt.startzeit);

		int nSDay(pStartZeit->tm_mday), nSMon(pStartZeit->tm_mon + 1), nSYear(pStartZeit->tm_year + 1900),
		nSH(pStartZeit->tm_hour), nSM(pStartZeit->tm_min);

		long int uiEndTime(zeitEvt.startzeit + zeitEvt.dauer);

		struct tm *pEndeZeit = localtime((time_t*) & uiEndTime);

		int nFH(pEndeZeit->tm_hour), nFM(pEndeZeit->tm_min);

		unsigned nProcentagePassed = (unsigned)((float)(time(NULL) - zeitEvt.startzeit) / (float)zeitEvt.dauer * 100.);

		sprintf(pResultData, "%012llx\xFF%s\xFF%s\xFF%s\xFF%02d.%02d.%04d\xFF%02d:%02d\xFF%02d:%02d\xFF%03u\xFF",
		        evt.uniqueKey(),
		        evt.getName().c_str(),
		        evt.getText().c_str(),
		        evt.getExtendedText().c_str(), nSDay, nSMon, nSYear, nSH, nSM, nFH, nFM, nProcentagePassed );
	}
	else
		dprintf("actual EPG not found!\n");

	unlockEvents();

	EITThreadsUnPause(); // -> unlock

	// response

	struct sectionsd::msgResponseHeader pmResponse;

	pmResponse.dataLength = nResultDataSize;

	bool rc = writeNbytes(connfd, (const char *)&pmResponse, sizeof(pmResponse), WRITE_TIMEOUT_IN_SECONDS);

	if ( nResultDataSize > 0 )
	{
		if (rc == true)
			writeNbytes(connfd, pResultData, nResultDataSize, WRITE_TIMEOUT_IN_SECONDS);
		else
			dputs("[sectionsd] Fehler/Timeout bei write");

		delete[] pResultData;
	}
}
*/

bool channel_in_requested_list(t_channel_id * clist, t_channel_id chid, int len)
{
        if(len == 0) return true;
        for(int i = 0; i < len; i++) {
                if(clist[i] == chid)
                        return true;
        }
        return false;
}
#if 0
bool channel_in_requested_list(std::vector <t_channel_id> *chidlist, t_channel_id chid)
{
	if (chidlist->empty()) return true;
	for (std::vector <t_channel_id>::iterator i=chidlist->begin(); i!=chidlist->end(); i++) {
		if (*i == chid) return true;
	}
	return false;
}
#endif
static void sendEventList(int connfd, const unsigned char serviceTyp1, const unsigned char serviceTyp2 = 0, int sendServiceName = 1, t_channel_id * chidlist = NULL, int clen = 0)
{
#define MAX_SIZE_BIGEVENTLIST	128*1024

	char *evtList = new char[MAX_SIZE_BIGEVENTLIST]; // 128k mssen reichen... schaut euch mal das Ergebnis fr loop an, jedesmal wenn die Senderliste aufgerufen wird
	char *liste;
	long count=0;
	t_channel_id uniqueNow = 0;
	t_channel_id uniqueOld = 0;
	bool found_already = false;
	time_t azeit = time(NULL);
	std::string sname;
	struct sectionsd::msgResponseHeader msgResponse;
	msgResponse.dataLength = 0;

	if (!evtList)
	{
		fprintf(stderr, "low on memory!\n");
		goto out;
	}

	*evtList = 0;
	liste = evtList;

	readLockEvents();

	/* !!! FIX ME: if the box starts on a channel where there is no EPG sent, it hangs!!!	*/
	for (MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin(); e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end(); ++e)
	{
		uniqueNow = (*e)->get_channel_id();
		if (!channel_in_requested_list(chidlist, uniqueNow, clen)) continue;
		if ( uniqueNow != uniqueOld )
		{
			found_already = true;
			readLockServices();
			// new service, check service- type
			MySIservicesOrderUniqueKey::iterator s = mySIservicesOrderUniqueKey.find(uniqueNow);

			if (s != mySIservicesOrderUniqueKey.end())
			{
				if (s->second->serviceTyp == serviceTyp1 || (serviceTyp2 && s->second->serviceTyp == serviceTyp2))
				{
					sname = s->second->serviceName;
					found_already = false;
				}
			}
			else
			{
				// wenn noch nie hingetuned wurde, dann gibts keine Info ber den ServiceTyp...
				// im Zweifel mitnehmen
				found_already = false;
			}
			unlockServices();

			uniqueOld = uniqueNow;
		}

		if ( !found_already )
		{
			std::string eName = (*e)->getName();
			std::string eText = (*e)->getText();
			std::string eExtendedText = (*e)->getExtendedText();

			for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t)
			{
				if (t->startzeit <= azeit && azeit <= (long)(t->startzeit + t->dauer))
				{
					if (sendServiceName)
					{
						count += 13 + sname.length() + 1 + eName.length() + 1;
						if (count < MAX_SIZE_BIGEVENTLIST) {
							sprintf(liste, "%012llx\n", (*e)->uniqueKey());
							liste += 13;
							strcpy(liste, sname.c_str());
							liste += sname.length();
							*liste = '\n';
							liste++;
							strcpy(liste, eName.c_str());
							liste += eName.length();
							*liste = '\n';
							liste++;
						} else {
							dprintf("warning: sendEventList - eventlist cut\n");
							break;
						}

					} // if sendServiceName
					else
					{
						count += sizeof(event_id_t) + 4 + 4 + eName.length() + 1;
						if (eText.empty())
						{
							count += eExtendedText.substr(0, 50).length();
						}
						else
						{
							count += eText.length();
						}
						count++;

						if (count < MAX_SIZE_BIGEVENTLIST) {
							*((event_id_t *)liste) = (*e)->uniqueKey();
							liste += sizeof(event_id_t);
							*((unsigned *)liste) = t->startzeit;
							liste += 4;
							*((unsigned *)liste) = t->dauer;
							liste += 4;
							strcpy(liste, eName.c_str());
							liste += eName.length();
							liste++;

							if (eText.empty())
							{
								strcpy(liste, eExtendedText.substr(0, 50).c_str());
								liste += strlen(liste);
							}
							else
							{
								strcpy(liste, eText.c_str());
								liste += eText.length();
							}
							liste++;
						} else {
							dprintf("warning: sendEventList - eventlist cut\n");
							break;
						}
					} // else !sendServiceName

					found_already = true;

					break;
				}
			}
		}
	}

	if (sendServiceName && (count+1 < MAX_SIZE_BIGEVENTLIST))
	{
		*liste = 0;
		liste++;
		count++;
	}

	unlockEvents();

	//printf("warning: [sectionsd] sendEventList - response-size: 0x%x, count = %lx\n", liste - evtList, count);
	if (liste - evtList > MAX_SIZE_BIGEVENTLIST)
		printf("warning: [sectionsd] sendEventList- response-size: 0x%x\n", liste - evtList);
	msgResponse.dataLength = liste - evtList;
	dprintf("[sectionsd] sendEventList - response-size: 0x%x\n", msgResponse.dataLength);

	if ( msgResponse.dataLength == 1 )
		msgResponse.dataLength = 0;

 out:
	if (writeNbytes(connfd, (const char *)&msgResponse, sizeof(msgResponse), WRITE_TIMEOUT_IN_SECONDS) == true)
	{
		if (msgResponse.dataLength)
			writeNbytes(connfd, evtList, msgResponse.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");

	if (evtList)
		delete[] evtList;
}

// Sendet ein short EPG, unlocked die events, unpaused dmxEIT

static void sendShort(int connfd, const SIevent& e, const SItime& t)
{

	struct sectionsd::msgResponseHeader responseHeader;

	responseHeader.dataLength =
	    12 + 1 + 				// Unique-Key + del
	    e.getName().length() + 1 + 		// name + del
	    8 + 1 + 				// start time + del
	    8 + 1 + 1;				// duration + del + 0
	char* msgData = new char[responseHeader.dataLength];

	if (!msgData)
	{
		fprintf(stderr, "low on memory!\n");
		unlockEvents();
		responseHeader.dataLength = 0;
		goto out;
	}

	sprintf(msgData,
	        "%012llx\n%s\n%08lx\n%08x\n",
	        e.uniqueKey(),
	        e.getName().c_str(),
	        t.startzeit,
	        t.dauer
	       );
	unlockEvents();

 out:
	if(writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS)) {
		if (responseHeader.dataLength)
			writeNbytes(connfd, msgData, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	} else
		dputs("[sectionsd] Fehler/Timeout bei write");

	if (msgData)
		delete[] msgData;
}

static void commandGetNextShort(int connfd, char *data, const unsigned dataLength)
{
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	if (dataLength != 8 + 4) {
		writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
		return;
	}

	event_id_t * uniqueEventKey = (event_id_t *)data;

	time_t *starttime = (time_t *)(data + 8);
	SItime zeit(*starttime, 0);

	dprintf("Request of next short for 0x%llx %s", *uniqueEventKey, ctime(starttime));

	readLockEvents();

	const SIevent &nextEvt = findNextSIevent(*uniqueEventKey, zeit);

	if (nextEvt.service_id != 0)
	{
		dprintf("next short found.\n");
		sendShort(connfd, nextEvt, zeit);
// this call is made in sendShort()
//		unlockEvents();
		return;
	}
	unlockEvents();
	dprintf("next short not found!\n");

	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
}

static void commandEventListTV(int connfd, char* /*data*/, const unsigned /*dataLength*/)
{
	dputs("Request of TV event list.\n");
	sendEventList(connfd, 0x01, 0x04);
}

static void commandEventListTVids(int connfd, char* data, const unsigned dataLength)
{
        dputs("Request of TV event list (IDs).\n");
#if 0
        std::vector <t_channel_id> chidlist;
        if (dataLength>0) {
                t_channel_id *tmp = (t_channel_id*)data;
                for (uint32_t i=0; i<dataLength/sizeof(t_channel_id); i++)
                        chidlist.push_back(tmp[i]);
        }
        sendEventList(connfd, 0x01, 0x04, 0, &chidlist);
#endif
        sendEventList(connfd, 0x01, 0x04, 0, (t_channel_id *) data, dataLength/sizeof(t_channel_id));
}

static void commandEventListRadio(int connfd, char* /*data*/, const unsigned /*dataLength*/)
{
	dputs("Request of radio event list.\n");
	sendEventList(connfd, 0x02);
}

static void commandEventListRadioIDs(int connfd, char* data, const unsigned dataLength)
{
#if 0
	std::vector <t_channel_id> chidlist;
	dputs("Request of radio event list (IDs).\n");
	if (dataLength>0) {
		t_channel_id *tmp = (t_channel_id*)data;
		for (uint i=0; i<dataLength/sizeof(t_channel_id); i++){
			chidlist.push_back(tmp[i]);
		}
	}
	sendEventList(connfd, 0x02, 0, 0, &chidlist);
#endif
	sendEventList(connfd, 0x02, 0, 0, (t_channel_id *) data, dataLength/sizeof(t_channel_id));
}

static void commandEPGepgID(int connfd, char *data, const unsigned dataLength)
{
	struct sectionsd::msgResponseHeader pmResponse;
	pmResponse.dataLength = 0;

	if (dataLength != 8 + 4) {
		writeNbytes(connfd, (const char *)&pmResponse, sizeof(pmResponse), WRITE_TIMEOUT_IN_SECONDS);
		return;
	}

	event_id_t * epgID = (event_id_t *)data;

	time_t* startzeit = (time_t *)(data + 8);

	dprintf("Request of current EPG for 0x%llx 0x%lx\n", *epgID, *startzeit);

	readLockEvents();

	const SIevent& evt = findSIeventForEventUniqueKey(*epgID);

	if (evt.service_id != 0)
	{ // Event found
		SItimes::iterator t = evt.times.begin();

		for (; t != evt.times.end(); ++t)
			if (t->startzeit == *startzeit)
				break;

		if (t != evt.times.end())
		{
			dputs("EPG found.");
			// Sendet ein EPG, unlocked die events, unpaused dmxEIT
			sendEPG(connfd, evt, *t);
// this call is made in sendEPG()
//			unlockEvents();
			return;
		}
	}

	dputs("EPG not found!");
	unlockEvents();
	// response

	writeNbytes(connfd, (const char *)&pmResponse, sizeof(pmResponse), WRITE_TIMEOUT_IN_SECONDS);
}

static void commandEPGepgIDshort(int connfd, char *data, const unsigned dataLength)
{
	struct sectionsd::msgResponseHeader pmResponse;
	pmResponse.dataLength = 0;

	if (dataLength != 8) {
		writeNbytes(connfd, (const char *)&pmResponse, sizeof(pmResponse), WRITE_TIMEOUT_IN_SECONDS);
		return;
	}

	event_id_t * epgID = (event_id_t *)data;

	dprintf("Request of current EPG for 0x%llx\n", *epgID);

	readLockEvents();

	const SIevent& evt = findSIeventForEventUniqueKey(*epgID);

	if (evt.service_id != 0)
	{ // Event found
		dputs("EPG found.");
		sendEPG(connfd, evt, SItime(0, 0), 1);
// this call is made in sendEPG()
//			unlockEvents();
		return;
	}

	dputs("EPG not found!");
	unlockEvents();
	// response

	writeNbytes(connfd, (const char *)&pmResponse, sizeof(pmResponse), WRITE_TIMEOUT_IN_SECONDS);
}

static void commandTimesNVODservice(int connfd, char *data, const unsigned dataLength)
{
	MySIservicesNVODorderUniqueKey::iterator si;
	char *msgData = 0;
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	t_channel_id uniqueServiceKey;

	if (dataLength != sizeof(t_channel_id))
		goto out;

	uniqueServiceKey = *(t_channel_id *)data;

	dprintf("Request of NVOD times for " PRINTF_CHANNEL_ID_TYPE "\n", uniqueServiceKey);

	readLockServices();
	readLockEvents();

	si = mySIservicesNVODorderUniqueKey.find(uniqueServiceKey);

	if (si != mySIservicesNVODorderUniqueKey.end())
	{
		dprintf("NVODServices: %u\n", si->second->nvods.size());

		if (si->second->nvods.size())
		{
			responseHeader.dataLength = (sizeof(t_service_id) + sizeof(t_original_network_id) + sizeof(t_transport_stream_id) + 4 + 4) * si->second->nvods.size();
			msgData = new char[responseHeader.dataLength];

			if (!msgData)
			{
				fprintf(stderr, "low on memory!\n");
				unlockEvents();
				unlockServices();
				responseHeader.dataLength = 0; // empty response
				goto out;
			}

			char *p = msgData;
			//      time_t azeit=time(NULL);

			for (SInvodReferences::iterator ni = si->second->nvods.begin(); ni != si->second->nvods.end(); ++ni)
			{
				// Zeiten sind erstmal dummy, d.h. pro Service eine Zeit
				ni->toStream(p); // => p += sizeof(t_service_id) + sizeof(t_original_network_id) + sizeof(t_transport_stream_id);

				SItime zeitEvt1(0, 0);
				//        const SIevent &evt=
				findActualSIeventForServiceUniqueKey(ni->uniqueKey(), zeitEvt1, 15*60);
				*(time_t *)p = zeitEvt1.startzeit;
				p += 4;
				*(unsigned *)p = zeitEvt1.dauer;
				p += 4;

				/*        MySIeventUniqueKeysMetaOrderServiceUniqueKey::iterator ei=mySIeventUniqueKeysMetaOrderServiceUniqueKey.find(ni->uniqueKey());
				        if(ei!=mySIeventUniqueKeysMetaOrderServiceUniqueKey.end())
				        {
				            dprintf("found NVod - Service: %0llx\n", ei->second);
				            MySIeventsOrderUniqueKey::iterator e=mySIeventsOrderUniqueKey.find(ei->second);
				            if(e!=mySIeventsOrderUniqueKey.end())
				            {
				                // ist ein MetaEvent, d.h. mit Zeiten fuer NVOD-Event
				                for(SItimes::iterator t=e->second->times.begin(); t!=e->second->times.end(); t++)
				                if(t->startzeit<=azeit && azeit<=(long)(t->startzeit+t->dauer))
				                {
				                    *(time_t *)p=t->startzeit;
				                    break;
				                }
				            }
				        }
				*/

			}
		}
	}
	unlockEvents();
	unlockServices();

	dprintf("data bytes: %u\n", responseHeader.dataLength);

 out:
	if (writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS))
	{
		if (responseHeader.dataLength)
			writeNbytes(connfd, msgData, responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");

	if (msgData)
		delete[] msgData;
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


#ifdef ENABLE_PPT
static void commandSetPrivatePid(int connfd, char *data, const unsigned dataLength)
{
	unsigned short pid;

	if (dataLength != 2)
		goto out;

	pid = *((unsigned short*)data);
//	if (privatePid != pid)
	{
		privatePid = pid;
		if (pid != 0) {
			dprintf("[sectionsd] wakeup PPT Thread, pid=%x\n", pid);
			dmxPPT.change( 0 );
		}
	}

 out:
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
	return ;
}
#endif

#ifdef UPDATE_NETWORKS
static void commandSetSectionsdScanMode(int connfd, char *data, const unsigned dataLength)
{
	if (dataLength != 4)
		goto out;

	writeLockMessaging();
	auto_scanning = *((int*)data);
	unlockMessaging();

 out:
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;

	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
	return ;

}
#endif

static void commandSetConfig(int connfd, char *data, const unsigned /*dataLength*/)
{
	struct sectionsd::msgResponseHeader responseHeader;
	struct sectionsd::commandSetConfig *pmsg;

	pmsg = (struct sectionsd::commandSetConfig *)data;
#ifdef UPDATE_NETWORKS
	if (pmsg->scanMode != auto_scanning) {
		dprintf("new scanMode = %d\n", pmsg->scanMode);
		writeLockMessaging();
		auto_scanning = pmsg->scanMode;
		unlockMessaging();
	}
#endif

	if (secondsToCache != (long)(pmsg->epg_cache)*24*60L*60L) {
		dprintf("new epg_cache = %d\n", pmsg->epg_cache);
		writeLockEvents();
		secondsToCache = (long)(pmsg->epg_cache)*24*60L*60L;
		unlockEvents();
	}

	if (oldEventsAre != (long)(pmsg->epg_old_events)*60L*60L) {
		dprintf("new epg_old_events = %d\n", pmsg->epg_old_events);
		writeLockEvents();
		oldEventsAre = (long)(pmsg->epg_old_events)*60L*60L;
		unlockEvents();
	}
	if (secondsExtendedTextCache != (long)(pmsg->epg_extendedcache)*60L*60L) {
		dprintf("new epg_extendedcache = %d\n", pmsg->epg_extendedcache);
//		lockEvents();
		writeLockEvents();
		secondsExtendedTextCache = (long)(pmsg->epg_extendedcache)*60L*60L;
		unlockEvents();
	}
	if (max_events != pmsg->epg_max_events) {
		dprintf("new epg_max_events = %d\n", pmsg->epg_max_events);
		writeLockEvents();
		max_events = pmsg->epg_max_events;
		unlockEvents();
	}

	if (ntprefresh != pmsg->network_ntprefresh) {
		dprintf("new network_ntprefresh = %d\n", pmsg->network_ntprefresh);
		pthread_mutex_lock(&timeThreadSleepMutex);
		ntprefresh = pmsg->network_ntprefresh;
		if (timeset) {
			// wake up time thread
			pthread_cond_broadcast(&timeThreadSleepCond);
		}
		pthread_mutex_unlock(&timeThreadSleepMutex);
	}

	if (ntpenable ^ (pmsg->network_ntpenable == 1))	{
		dprintf("new network_ntpenable = %d\n", pmsg->network_ntpenable);
		pthread_mutex_lock(&timeThreadSleepMutex);
		ntpenable = (pmsg->network_ntpenable == 1);
		if (timeset) {
			// wake up time thread
			pthread_cond_broadcast(&timeThreadSleepCond);
		}
		pthread_mutex_unlock(&timeThreadSleepMutex);
	}

	if (ntpserver.compare((std::string)&data[sizeof(struct sectionsd::commandSetConfig)])) {
		ntpserver = (std::string)&data[sizeof(struct sectionsd::commandSetConfig)];
		dprintf("new network_ntpserver = %s\n", ntpserver.c_str());
		ntp_system_cmd = ntp_system_cmd_prefix + ntpserver;
	}

	if (epg_dir.compare((std::string)&data[sizeof(struct sectionsd::commandSetConfig) + strlen(&data[sizeof(struct sectionsd::commandSetConfig)]) + 1])) {
		epg_dir= (std::string)&data[sizeof(struct sectionsd::commandSetConfig) + strlen(&data[sizeof(struct sectionsd::commandSetConfig)]) + 1];
		dprintf("new epg_dir = %s\n", epg_dir.c_str());
	}

	responseHeader.dataLength = 0;
	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
	return ;
}

static void deleteSIexceptEPG()
{
	writeLockServices();
	mySIservicesOrderUniqueKey.clear();
	unlockServices();
#ifdef UPDATE_NETWORKS
	writeLockTransponders();
	mySItranspondersOrderUniqueKey.clear();
	unlockTransponders();
	writeLockBouquets();
	mySIbouquetsOrderUniqueKey.clear();
	unlockBouquets();
	dmxNIT.dropCachedSectionIDs();
	dmxSDT.dropCachedSectionIDs();
#endif
	dmxEIT.dropCachedSectionIDs();
}

static void commandFreeMemory(int connfd, char * /*data*/, const unsigned /*dataLength*/)
{
	deleteSIexceptEPG();

	writeLockEvents();
	mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.clear();
	mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.clear();
	mySIeventsOrderUniqueKey.clear();
	mySIeventsNVODorderUniqueKey.clear();
	unlockEvents();

	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
	return ;
}

std::string UTF8_to_Latin1(const char * s)
{
	std::string r;

	while ((*s) != 0)
	{
		if (((*s) & 0xf0) == 0xf0)      /* skip (can't be encoded in Latin1) */
		{
			s++;
			if ((*s) == 0)
				return r;
			s++;
			if ((*s) == 0)
				return r;
			s++;
			if ((*s) == 0)
				return r;
		}
		else if (((*s) & 0xe0) == 0xe0) /* skip (can't be encoded in Latin1) */
		{
			s++;
			if ((*s) == 0)
				return r;
			s++;
			if ((*s) == 0)
				return r;
		}
		else if (((*s) & 0xc0) == 0xc0)
		{
			char c = (((*s) & 3) << 6);
			s++;
			if ((*s) == 0)
				return r;
			r += (c | ((*s) & 0x3f));
		}
		else r += *s;
		s++;
	}
	return r;
}

static void *insertEventsfromFile(void *)
{
	xmlDocPtr event_parser = NULL;
	xmlNodePtr eventfile = NULL;
	xmlNodePtr service = NULL;
	xmlNodePtr event = NULL;
	xmlNodePtr node = NULL;
	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;
	char cclass[20];
	char cuser[20];
	std::string indexname;
	std::string filename;
	std::string epgname;

	indexname = epg_dir + "index.xml";

	printdate_ms(stdout); printf("[sectionsd] Reading Information from file %s:\n", indexname.c_str());
	xmlDocPtr index_parser = parseXmlFile(indexname.c_str());

	if (index_parser != NULL) {

		eventfile = xmlDocGetRootElement(index_parser)->xmlChildrenNode;

		while (eventfile) {
			filename = xmlGetAttribute(eventfile, "name");
			epgname = epg_dir + filename;
			if (!(event_parser = parseXmlFile(epgname.c_str()))) {
				dprintf("unable to open %s for reading\n", epgname.c_str());
			}
			else {
				service = xmlDocGetRootElement(event_parser)->xmlChildrenNode;

				while (service) {
					onid = xmlGetNumericAttribute(service, "original_network_id", 16);
					tsid = xmlGetNumericAttribute(service, "transport_stream_id", 16);
					sid = xmlGetNumericAttribute(service, "service_id", 16);

					event = service->xmlChildrenNode;

					while (event) {

						SIevent e(onid,tsid,sid,xmlGetNumericAttribute(event, "id", 16));

						node = event->xmlChildrenNode;

						while (xmlGetNextOccurence(node, "name") != NULL) {
							e.setName(	std::string(UTF8_to_Latin1(xmlGetAttribute(node, "lang"))),
									std::string(xmlGetAttribute(node, "string")));
							node = node->xmlNextNode;
						}
						while (xmlGetNextOccurence(node, "text") != NULL) {
							e.setText(	std::string(UTF8_to_Latin1(xmlGetAttribute(node, "lang"))),
									std::string(xmlGetAttribute(node, "string")));
							node = node->xmlNextNode;
						}
						while (xmlGetNextOccurence(node, "item") != NULL) {
							e.item = std::string(xmlGetAttribute(node, "string"));
							node = node->xmlNextNode;
						}
						while (xmlGetNextOccurence(node, "item_description") != NULL) {
							e.itemDescription = std::string(xmlGetAttribute(node, "string"));
							node = node->xmlNextNode;
						}
						while (xmlGetNextOccurence(node, "extended_text") != NULL) {
							e.appendExtendedText(	std::string(UTF8_to_Latin1(xmlGetAttribute(node, "lang"))),
										std::string(xmlGetAttribute(node, "string")));
							node = node->xmlNextNode;
						}
						/*
						if (xmlGetNextOccurence(node, "description") != NULL) {
							if (xmlGetAttribute(node, "name") != NULL) {
								e.langName = std::string(UTF8_to_Latin1(xmlGetAttribute(node, "name")));
							}
							//printf("Name: %s\n", e->name);
							if (xmlGetAttribute(node, "text") != NULL) {
								e.langText = std::string(UTF8_to_Latin1(xmlGetAttribute(node, "text")));
							}
							if (xmlGetAttribute(node, "item") != NULL) {
								e.item = std::string(UTF8_to_Latin1(xmlGetAttribute(node, "item")));
							}
							if (xmlGetAttribute(node, "item_description") != NULL) {
								e.itemDescription = std::string(UTF8_to_Latin1(xmlGetAttribute(node,"item_description")));
							}
							if (xmlGetAttribute(node, "extended_text") != NULL) {
								e.langExtendedText = std::string(UTF8_to_Latin1(xmlGetAttribute(node, "extended_text")));
							}
							node = node->xmlNextNode;
						}
*/
						while (xmlGetNextOccurence(node, "time") != NULL) {
							e.times.insert(SItime(xmlGetNumericAttribute(node, "start_time", 10),
										xmlGetNumericAttribute(node, "duration", 10)));
							node = node->xmlNextNode;
						}

						int count = 0;
						while (xmlGetNextOccurence(node, "content") != NULL) {
							cclass[count] = xmlGetNumericAttribute(node, "class", 16);
							cuser[count] = xmlGetNumericAttribute(node, "user", 16);
							node = node->xmlNextNode;
							count++;
						}
						e.contentClassification = std::string(cclass, count);
						e.userClassification = std::string(cuser, count);

						while (xmlGetNextOccurence(node, "component") != NULL) {
							SIcomponent c;
							c.streamContent = xmlGetNumericAttribute(node, "stream_content", 16);
							c.componentType = xmlGetNumericAttribute(node, "type", 16);
							c.componentTag = xmlGetNumericAttribute(node, "tag", 16);
							c.component = std::string(xmlGetAttribute(node, "text"));
							e.components.insert(c);
							node = node->xmlNextNode;
						}
						while (xmlGetNextOccurence(node, "parental_rating") != NULL) {
							e.ratings.insert(SIparentalRating(std::string(UTF8_to_Latin1(xmlGetAttribute(node, "country"))), (unsigned char) xmlGetNumericAttribute(node, "rating", 10)));
							node = node->xmlNextNode;
						}
						while (xmlGetNextOccurence(node, "linkage") != NULL) {
							SIlinkage l;
							l.linkageType = xmlGetNumericAttribute(node, "type", 16);
							l.transportStreamId = xmlGetNumericAttribute(node, "transport_stream_id", 16);
							l.originalNetworkId = xmlGetNumericAttribute(node, "original_network_id", 16);
							l.serviceId = xmlGetNumericAttribute(node, "service_id", 16);
							l.name = std::string(xmlGetAttribute(node, "linkage_descriptor"));
							e.linkage_descs.insert(e.linkage_descs.end(), l);

							node = node->xmlNextNode;
						}
						//lockEvents();
						//writeLockEvents();
						addEvent(e, 0, 0);
						//unlockEvents();

						event = event->xmlNextNode;
					}

					service = service->xmlNextNode;
				}
				xmlFreeDoc(event_parser);
			}

			eventfile = eventfile->xmlNextNode;
		}

		printdate_ms(stdout); printf("[sectionsd] Reading Information finished\n");
		xmlFreeDoc(index_parser);
	}

	pthread_exit(NULL);
}

static void commandReadSIfromXML(int connfd, char *data, const unsigned dataLength)
{
	pthread_t thrInsert;

	if (dataLength > 100)
		return ;

	writeLockMessaging();
	data[dataLength] = '\0';
	epg_dir = (std::string)data + "/";
	unlockMessaging();

	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (pthread_create (&thrInsert, &attr, insertEventsfromFile, 0 ))
	{
		perror("sectionsd: pthread_create()");
	}

	pthread_attr_destroy(&attr);

	return ;
}

static void write_epg_xml_header(FILE * fd, const t_original_network_id onid, const t_transport_stream_id tsid, const t_service_id sid)
{
	fprintf(fd,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!--\n"
		"  This file was automatically generated by the sectionsd.\n"
		"  It contains all event entries which have been cached\n"
		"  at time the box was shut down.\n"
		"-->\n"
	       "<dvbepg>\n");
	fprintf(fd,"\t<service original_network_id=\"%04x\" transport_stream_id=\"%04x\" service_id=\"%04x\">\n",onid,tsid,sid);
}

static void write_index_xml_header(FILE * fd)
{
	fprintf(fd,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!--\n"
		"  This file was automatically generated by the sectionsd.\n"
		"  It contains all event entries which have been cached\n"
		"  at time the box was shut down.\n"
		"-->\n"
	       "<dvbepgfiles>\n");
}

static void write_epgxml_footer(FILE *fd)
{
	fprintf(fd, "\t</service>\n");
	fprintf(fd, "</dvbepg>\n");
}

static void write_indexxml_footer(FILE *fd)
{
	fprintf(fd, "</dvbepgfiles>\n");
}

void cp(char * from, char * to)
{
        char cmd[256];
        snprintf(cmd, 256, "cp -f %s %s", from, to);
        system(cmd);
}

static void commandWriteSI2XML(int connfd, char *data, const unsigned dataLength)
{
        FILE * indexfile = NULL;
        FILE * eventfile =NULL;
        char filename[100] = "";
        char tmpname[100] = "";
        char epgdir[100] = "";
        char eventname[17] = "";
        t_original_network_id onid = 0;
        t_transport_stream_id tsid = 0;
        t_service_id sid = 0;

        struct sectionsd::msgResponseHeader responseHeader;
        responseHeader.dataLength = 0;
        writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);

        if (dataLength > 100)
                goto _ret ;

        strncpy(epgdir, data, dataLength);
        epgdir[dataLength] = '\0';
        sprintf(tmpname, "%s/index.tmp", epgdir);

        if (!(indexfile = fopen(tmpname, "w"))) {
                printf("[sectionsd] unable to open %s for writing\n", tmpname);
		goto _ret;
        }
        else {

                printf("[sectionsd] Writing Information to file: %s\n", tmpname);

                write_index_xml_header(indexfile);

		readLockEvents();

                MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e =
                                        mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin();
                if (e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end()) {
                        onid = (*e)->original_network_id;
                        tsid = (*e)->transport_stream_id;
                        sid = (*e)->service_id;
                        snprintf(eventname,17,"%04x%04x%04x.xml",onid,tsid,sid);
                        sprintf(filename, "%s/%s", epgdir, eventname);
                        if (!(eventfile = fopen(filename, "w"))) {
                                write_indexxml_footer(indexfile);
                                fclose(indexfile);
                                goto _done;
                        }
                        fprintf(indexfile, "\t<eventfile name=\"%s\"/>\n",eventname);
                        write_epg_xml_header(eventfile,onid,tsid,sid);

                        while (e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end()) {
                                if ( (onid != (*e)->original_network_id) || (tsid != (*e)->transport_stream_id) || (sid != (*e)->service_id) ) {
                                        onid = (*e)->original_network_id;
                                        tsid = (*e)->transport_stream_id;
                                        sid = (*e)->service_id;
                                        write_epgxml_footer(eventfile);
                                        fclose(eventfile);
                                        snprintf(eventname,17,"%04x%04x%04x.xml",onid,tsid,sid);
                                        sprintf(filename, "%s/%s", epgdir, eventname);
                                        if (!(eventfile = fopen(filename, "w"))) {
                                                goto _done;
                                        }
                                        fprintf(indexfile, "\t<eventfile name=\"%s\"/>\n", eventname);
                                        write_epg_xml_header(eventfile,onid,tsid,sid);
                                }
                                (*e)->saveXML(eventfile);
                                e ++;
                        }
                        write_epgxml_footer(eventfile);
                        fclose(eventfile);

                }
_done:
                unlockEvents();
                write_indexxml_footer(indexfile);
                fclose(indexfile);

                printf("[sectionsd] Writing Information finished\n");
        }
        strncpy(filename, data, dataLength);
        filename[dataLength] = '\0';
        strncat(filename, "/index.xml", 10);

        cp(tmpname, filename);
        unlink(tmpname);
_ret:
        eventServer->sendEvent(CSectionsdClient::EVT_WRITE_SI_FINISHED, CEventServer::INITID_SECTIONSD);
        return ;
}

/* dummy1: do not send back anything */
static void commandDummy1(int, char *, const unsigned)
{
	return;
}

/* dummy2: send back an empty response */
static void commandDummy2(int connfd, char *, const unsigned)
{
	struct sectionsd::msgResponseHeader msgResponse;
	msgResponse.dataLength = 0;
	writeNbytes(connfd, (const char *)&msgResponse, sizeof(msgResponse), WRITE_TIMEOUT_IN_SECONDS);
	return;
}

static void commandAllEventsChannelIDSearch(int connfd, char *data, const unsigned dataLength)
{
	//dprintf("Request of commandAllEventsChannelIDSearch, %d\n",dataLength);
	if (dataLength > 5)
	{
		char *data_ptr = data;
		char search = 0;
		std::string search_text;

		t_channel_id channel_id = *(t_channel_id*)data_ptr;
		data_ptr += sizeof(t_channel_id);
		search = *data_ptr;
		data_ptr += sizeof(char);
		if(search != 0)
			search_text = data_ptr;
		sendAllEvents(connfd, channel_id, false, search, search_text);
	}
	return;
}

static void commandLoadLanguages(int connfd, char* /*data*/, const unsigned /*dataLength*/)
{
	struct sectionsd::msgResponseHeader responseHeader;
	bool retval = SIlanguage::loadLanguages();
	responseHeader.dataLength = sizeof(retval);

	if (writeNbytes(connfd, (const char *)&responseHeader,
									sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true) {
		writeNbytes(connfd, (const char *)&retval,
								responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");
}


static void commandSaveLanguages(int connfd, char* /*data*/, const unsigned /*dataLength*/)
{
	struct sectionsd::msgResponseHeader responseHeader;
	bool retval = SIlanguage::saveLanguages();
	responseHeader.dataLength = sizeof(retval);

	if (writeNbytes(connfd, (const char *)&responseHeader,
									sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true) {
		writeNbytes(connfd, (const char *)&retval,
								responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	}
	else
		dputs("[sectionsd] Fehler/Timeout bei write");
}


static void commandSetLanguages(int connfd, char* data, const unsigned dataLength)
{
	bool retval = true;

	if (dataLength % 3) {
		retval = false;
	} else {
		std::vector<std::string> languages;
		for (unsigned int i = 0 ; i < dataLength ; ) {
			char tmp[4];
			tmp[0] = data[i++];
			tmp[1] = data[i++];
			tmp[2] = data[i++];
			tmp[3] = '\0';
			languages.push_back(tmp);
		}
		SIlanguage::setLanguages(languages);
	}

	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = sizeof(retval);

	if (writeNbytes(connfd, (const char *)&responseHeader,
									sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true) {
		writeNbytes(connfd, (const char *)&retval, responseHeader.dataLength,
								WRITE_TIMEOUT_IN_SECONDS);
	} else {
		dputs("[sectionsd] Fehler/Timeout bei write");
	}
}


static void commandGetLanguages(int connfd, char* /* data */, const unsigned /* dataLength */)
{
	std::string retval;
	std::vector<std::string> languages = SIlanguage::getLanguages();

	for (std::vector<std::string>::iterator it = languages.begin() ;
			 it != languages.end() ; it++) {
		retval.append(*it);
	}

	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = retval.length();

	if (writeNbytes(connfd, (const char *)&responseHeader,
									sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true) {
		writeNbytes(connfd, (const char *)retval.c_str(),
								responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	} else {
		dputs("[sectionsd] Fehler/Timeout bei write");
	}
}


static void commandSetLanguageMode(int connfd, char* data , const unsigned dataLength)
{
	bool retval = true;
	CSectionsdClient::SIlanguageMode_t tmp(CSectionsdClient::ALL);

	if (dataLength != sizeof(tmp)) {
		retval = false;
	} else {
		tmp = *(CSectionsdClient::SIlanguageMode_t *)data;
		SIlanguage::setMode(tmp);
	}

	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = sizeof(retval);

	if (writeNbytes(connfd, (const char *)&responseHeader,
									sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true) {
		writeNbytes(connfd, (const char *)&retval,
								responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	} else {
		dputs("[sectionsd] Fehler/Timeout bei write");
	}
}


static void commandGetLanguageMode(int connfd, char* /* data */, const unsigned /* dataLength */)
{
	CSectionsdClient::SIlanguageMode_t retval(CSectionsdClient::ALL);

	retval = SIlanguage::getMode();

	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = sizeof(retval);

	if (writeNbytes(connfd, (const char *)&responseHeader,
									sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS) == true) {
		writeNbytes(connfd, (const char *)&retval,
								responseHeader.dataLength, WRITE_TIMEOUT_IN_SECONDS);
	} else {
		dputs("[sectionsd] Fehler/Timeout bei write");
	}
}

#if 0
#define SETENVI(var) { \
	sprintf(val,"%d",var); \
	if (setenv("SD_"#var, val, 1)<0) \
		perror("SETENVI("#var") "); }
#define SETENVB(var) { \
	sprintf(val,"%s",var?"1":"0"); \
	if (setenv("SD_"#var, val, 1)<0) \
		perror("SETENVB("#var") "); }
#define SETENVL(var) { \
	sprintf(val,"%ld",var); \
	if (setenv("SD_"#var, val, 1)<0) \
		perror("SETENVL("#var") "); }
#define SETENVS(var) { \
	if (setenv("SD_"#var, ((std::string)var).c_str(), 1)< 0) \
	perror("SETENVS("#var") "); }
// restart sectionsd....
static void commandRestart(int connfd, char * /*data*/, const unsigned /*dataLength*/)
{
	struct sectionsd::msgResponseHeader responseHeader;
	responseHeader.dataLength = 0;
	char * buf = (char*)malloc(64);
	char *val = (char*)malloc(32);	// needed for SETENV?-macros
	int count;
	if (val && buf && (count = readlink("/proc/self/exe", buf, 63)) >= 0) {
		buf[count] = '\0';
		printf("re-starting %s\n", buf);
        } else {
		fprintf(stderr, "[sectionsd] commandRestart: cannot determine who i am\n");
		writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
		return;
	}
	/* if we close filedescriptors here, the 2.4 kernel hangs hard when we
	   close the two pipe fds probably created by the old threading
	   implementation. We close them instead at startup.
	for (int i = 3; i < 256; i++)
		close(i);
	 */
#ifdef UPDATE_NETWORKS
	SETENVI(auto_scanning);
#endif
	SETENVL(secondsToCache);
	SETENVL(oldEventsAre);
	SETENVL(secondsExtendedTextCache);
	SETENVI(max_events);
	SETENVI(ntprefresh);
	SETENVI(ntpenable);
	SETENVS(ntp_system_cmd);
	SETENVS(epg_dir);
	SETENVB(update_eit);
	SETENVB(bTimeCorrect);
	SETENVB(sections_debug);
	writeNbytes(connfd, (const char *)&responseHeader, sizeof(responseHeader), WRITE_TIMEOUT_IN_SECONDS);
	unlink(SECTIONSD_UDS_NAME);
	char* const p[3] = { buf, (char *) "-p", NULL };
	fprintf(stderr,"[sectionsd] starting '%s'\n",buf);
	execv(buf, p);
	perror("[sectionsd] commandRestart execv");
	fprintf(stderr, "[sectionsd] ERROR! This is impossible!\n\n");
	free(buf);
}
#endif

struct s_cmd_table
{
	void (*cmd)(int connfd, char *, const unsigned);
	std::string sCmd;
};

static s_cmd_table connectionCommands[sectionsd::numberOfCommands] = {
        //commandActualEPGchannelName,
{	commandDummy2,				"commandDummy1"				},
{	commandEventListTV,			"commandEventListTV"			},
        //commandCurrentNextInfoChannelName,
{	commandDummy2,				"commandDummy2"				},
{	commandDumpStatusInformation,		"commandDumpStatusInformation"		},
        //commandAllEventsChannelName,
{	commandAllEventsChannelIDSearch,        "commandAllEventsChannelIDSearch"	},
{	commandDummy2,				"commandSetHoursToCache"		},
{	commandDummy2,				"commandSetHoursExtendedCache"		},
{	commandDummy2,				"commandSetEventsAreOldInMinutes"	},
{	commandDumpAllServices,                 "commandDumpAllServices"		},
{	commandEventListRadio,                  "commandEventListRadio"			},
{	commandGetNextEPG,                      "commandGetNextEPG"			},
{	commandGetNextShort,                    "commandGetNextShort"			},
{	commandPauseScanning,                   "commandPauseScanning"			},
{	commandGetIsScanningActive,             "commandGetIsScanningActive"		},
{	commandActualEPGchannelID,              "commandActualEPGchannelID"		},
{	commandEventListTVids,                  "commandEventListTVids"			},
{	commandEventListRadioIDs,               "commandEventListRadioIDs"		},
{	commandCurrentNextInfoChannelID,        "commandCurrentNextInfoChannelID"	},
{	commandEPGepgID,                        "commandEPGepgID"			},
{	commandEPGepgIDshort,                   "commandEPGepgIDshort"			},
{	commandComponentTagsUniqueKey,          "commandComponentTagsUniqueKey"		},
{	commandAllEventsChannelID,              "commandAllEventsChannelID"		},
{	commandTimesNVODservice,                "commandTimesNVODservice"		},
{	commandGetEPGPrevNext,                  "commandGetEPGPrevNext"			},
{	commandGetIsTimeSet,                    "commandGetIsTimeSet"			},
{	commandserviceChanged,                  "commandserviceChanged"			},
{	commandLinkageDescriptorsUniqueKey,     "commandLinkageDescriptorsUniqueKey"	},
{	commandDummy2,                          "commandPauseSorting"			},
{	commandRegisterEventClient,             "commandRegisterEventClient"		},
{	commandUnRegisterEventClient,           "commandUnRegisterEventClient"		},
#ifdef ENABLE_PPT
{	commandSetPrivatePid,                   "commandSetPrivatePid"			},
#else
{	commandDummy2,                          "commandSetPrivatePid"			},
#endif
#ifdef UPDATE_NETWORKS
{	commandSetSectionsdScanMode,            "commandSetSectionsdScanMode"		},
#else
{	commandDummy2,				"commandSetSectionsdScanMode"		},
#endif
{	commandFreeMemory,			"commandFreeMemory"			},
{	commandReadSIfromXML,			"commandReadSIfromXML"			},
{	commandWriteSI2XML,			"commandWriteSI2XML"			},
{	commandLoadLanguages,                   "commandLoadLanguages"			},
{	commandSaveLanguages,                   "commandSaveLanguages"			},
{	commandSetLanguages,                    "commandSetLanguages"			},
{	commandGetLanguages,                    "commandGetLanguages"			},
{	commandSetLanguageMode,                 "commandSetLanguageMode"		},
{	commandGetLanguageMode,                 "commandGetLanguageMode"		},
{	commandSetConfig,			"commandSetConfig"			},
#if 0
{	commandRestart,				"commandRestart"			},
#else
{	commandDummy1,				"commandRestart"			},
#endif
{	commandDummy1,				"commandPing"				}
};

//static void *connectionThread(void *conn)
bool sectionsd_parse_command(CBasicMessage::Header &rmsg, int connfd)
{
	/*
	  pthread_t threadConnection;
	  rc = pthread_create(&threadConnection, &conn_attrs, connectionThread, client);
	  if(rc)
	  {
	  fprintf(stderr, "[sectionsd] failed to create connection-thread (rc=%d)\n", rc);
	  return 4;
	  }
	*/
	// VERSUCH OHNE CONNECTION-THREAD!
	// spart die thread-creation-zeit, und die Locks lassen ohnehin nur ein cmd gleichzeitig zu
	try
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
						connectionCommands[header.command].cmd(connfd, data, header.dataLength);
					}

					delete[] data;
				}
			}
			else
				dputs("Unknown format or version of request!");
		}
	} // try
#ifdef WITH_EXCEPTIONS
	catch (std::exception& e)
	{
		fprintf(stderr, "Caught std-exception in connection-thread %s!\n", e.what());
	}
#endif
	catch (...)
	{
		fprintf(stderr, "Caught exception in connection-thread!\n");
	}

	return true;
}
#ifdef UPDATE_NETWORKS
xmlNodePtr GetProvider(xmlNodePtr provider, xmlNodePtr tp_node)
{
	xmlNodePtr found = NULL;

	while (provider && !found)
	{
		xmlNodePtr transponder = provider->xmlChildrenNode;

		while (transponder && !found)
		{
			if ( (xmlGetNumericAttribute(transponder, "id", 16) == xmlGetNumericAttribute(tp_node, "id", 16)) &&
				(xmlGetNumericAttribute(transponder, "onid", 16) == xmlGetNumericAttribute(tp_node, "onid", 16)) )
				found = provider;
			else
				transponder = transponder->xmlNextNode;
		}
		if (!found)
			provider = provider->xmlNextNode;
	}
	return found;
}

static void write_xml_header(FILE * fd)
{
	fprintf(fd,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!--\n"
		"  This file was automatically generated by the sectionsd.\n"
		"  It contains all differences between the services.xml and\n"
		"  what is currently signalled within the SDTs.\n"
		"  It shall be merged with services.xml when the box shuts down.\n"
		"-->\n"
	       "<zapit>\n");
}

static void write_xml_footer(FILE *fd)
{
	fprintf(fd, "</zapit>\n");
}

static void write_xml_provend(FILE *dst, const bool is_sat)
{
	if (is_sat)
		fprintf(dst,"\t</sat>\n");
	else
		fprintf(dst,"\t</cable>\n");
}

//Writes transponder entry or copies all existing tps of a provider.
static bool write_xml_transponder(FILE *src, FILE *dst, const xmlNodePtr tp_node, const bool is_sat, const bool copy)
{
#define MAX_SIZE_TP_STR	256
	char tp_str[MAX_SIZE_TP_STR] = "";
	char buffer[256] = "";
	bool tp_existed = false;
	if (is_sat) {
		snprintf(tp_str, MAX_SIZE_TP_STR, "\t\t<transponder id=\"%04x\" onid=\"%04x\" frequency=\"%u\" inversion=\"%hu\" symbol_rate=\"%u\" fec_inner=\"%hu\" polarization=\"%hu\">\n",
				(t_transport_stream_id) xmlGetNumericAttribute(tp_node, "id", 16),
				(t_original_network_id) xmlGetNumericAttribute(tp_node, "onid", 16),
				(uint32_t) xmlGetNumericAttribute(tp_node, "frequency", 0),
				(fe_spectral_inversion_t) xmlGetNumericAttribute(tp_node, "inversion", 0),
				(uint32_t) xmlGetNumericAttribute(tp_node, "symbol_rate", 0),
				(fe_code_rate_t) xmlGetNumericAttribute(tp_node, "fec_inner", 0),
				(uint8_t) xmlGetNumericAttribute(tp_node, "polarization", 0));
	}
	else {
		snprintf(tp_str, MAX_SIZE_TP_STR, "\t\t<transponder id=\"%04x\" onid=\"%04x\" frequency=\"%u\" inversion=\"%hu\" symbol_rate=\"%u\" fec_inner=\"%hu\" modulation=\"%hu\">\n",
				(t_transport_stream_id) xmlGetNumericAttribute(tp_node, "id", 16),
				(t_original_network_id) xmlGetNumericAttribute(tp_node, "onid", 16),
				(uint32_t) xmlGetNumericAttribute(tp_node, "frequency", 0),
				(fe_spectral_inversion_t) xmlGetNumericAttribute(tp_node, "inversion", 0),
				(uint32_t) xmlGetNumericAttribute(tp_node, "symbol_rate", 0),
				(fe_code_rate_t) xmlGetNumericAttribute(tp_node, "fec_inner", 0),
				(fe_modulation_t) xmlGetNumericAttribute(tp_node, "modulation", 0));
	}

	if (!copy)
		fprintf(dst, tp_str);
	else {
		if (!feof(src)) {
			fgets(buffer, 255, src);

			if (is_sat) {
				//find tp in currentservices.xml
				while( (!feof(src)) && (strcmp(buffer, "\t</sat>\n") != 0) && (strcmp(buffer, tp_str) != 0) )
				{
					fprintf(dst, buffer);
					fgets(buffer, 255, src);
				}
			}
			else {
				while( (!feof(src)) && (strcmp(buffer, "\t</cable>\n") != 0) && (strcmp(buffer, tp_str) != 0) )
				{
					fprintf(dst, buffer);
					fgets(buffer, 255, src);
				}
			}
			//If the Transponder alredy existed. This isn't reached at the moment because if the transponder
			//didn't exist we don't call the update function. But maybe this is to be changed:
			//We should save if the update came from SDT other and update it once again, if
			//we find SDT ACTUAL. So we leave it here. Save could be done through another node SDT in
			//currentservices.xml. Should be easy to realize...
			if ( (!feof(src)) && (!strcmp(buffer, tp_str)) ) {
				while( (!feof(src)) && (strcmp(buffer, "\t\t</transponder>\n") != 0) ) {
					tp_existed = true;
					fgets(buffer, 255, src);
				}
			}
		}
	}
	return tp_existed;
}

//return true for sat, false for cable
//The function fulfills two purposes. It writes the correct provider entry (if copy = false) or it copies
//all data including the provider entry if it existed
//Otherwise it reads to the end signalling that it didn't find the provider
static bool write_xml_provider(FILE *src, FILE *dst, const xmlNodePtr provider, const bool copy)
{
#define MAX_SIZE_PROV_STR	256
	char prov_str[MAX_SIZE_PROV_STR] = "";
	char buffer[256] = "";
	std::string frontendType;
	std::string provider_name;
	std::string diseqc;
	bool is_sat = false;
	int position = 0;

	frontendType = xmlGetName(provider);
	provider_name = xmlGetAttribute(provider, "name");

	if (!strcmp(frontendType.c_str(), "sat")) {
		diseqc = xmlGetAttribute(provider, "diseqc");
		position = xmlGetSignedNumericAttribute(provider, "position", 16);
		if (position == 0)
			snprintf(prov_str, MAX_SIZE_PROV_STR, "\t<%s name=\"%s\" diseqc=\"%s\">\n", frontendType.c_str(),
				provider_name.c_str(), diseqc.c_str());
		else {
			//east_west = xmlGetNumericAttribute(provider, "east_west", 16);
			snprintf(prov_str, MAX_SIZE_PROV_STR, "\t<%s name=\"%s\" position=\"%04x\" diseqc=\"%s\">\n",
				frontendType.c_str(),
				provider_name.c_str(),
				position,
				//east_west,
				diseqc.c_str());
		}
		is_sat = true;
	}
	else {
		snprintf(prov_str, MAX_SIZE_PROV_STR, "\t<%s name=\"%s\">\n", frontendType.c_str(), provider_name.c_str());
		is_sat = false;
	}

	if (!copy)
		fprintf(dst, prov_str);
	else {
		if (!feof(src)) {
			fgets(buffer, 255, src);
			//find prov in currentservices.xml
			while( (!feof(src)) && (strcmp(buffer, "</zapit>\n") != 0) && (strcmp(buffer, prov_str) != 0) )
			{
				fprintf(dst, buffer);
				fgets(buffer, 255, src);
			}
			if (strcmp(buffer, prov_str) != 0) {
				while (!feof(src))
					fgets(buffer, 255, src);
//				printf("reading to the end!\n");
			} else
				fprintf(dst, buffer);
		}
	}

	return is_sat;
}

//Determines which action (none, add, replace) should be taken for current service
//This funtion considers the entry scanType in scan.conf.
static int get_action(const xmlNodePtr tp_node, const MySIservicesOrderUniqueKey::iterator s, const int scanType)
{
	//And now node points to transponders channels first entry
	xmlNodePtr node = tp_node->xmlChildrenNode;
	std::string name;

	if ( ((s->second->serviceTyp == 1) && (scanType == 1)) ||
	     ((s->second->serviceTyp == 2) && (scanType == 2)) ||
	     (((s->second->serviceTyp == 1) || (s->second->serviceTyp == 2)) && (scanType == 0)) ||
	     (scanType == 3) ) {

		while (xmlGetNextOccurence(node, "channel") != NULL) {
			if (s->second->service_id == xmlGetNumericAttribute(node, "service_id", 16)) {
				name = xmlGetAttribute(node, "name");
				if ( (s->second->serviceTyp == xmlGetNumericAttribute(node, "service_type", 16)) &&
				     (!strcmp(s->second->serviceName.c_str(), name.c_str())) ) {
				     	dprintf("[sectionsd] Service %s okay\n", name.c_str());
					return 0;	//service okay
				}
				else {
					if (s->second->is_actual & 7) {
						dprintf("[sectionsd] Replacing Service %s\n", name.c_str());
						return 2; //replace
					}
					else {
						dprintf("[sectionsd] Service %s changed but signalled from SDT_Other\n", name.c_str());
						return 0; //service not okay, but came from SDT_OTHER - we can't truly trust
					}
				}
			}
			node = node->xmlNextNode;
		}
		dprintf("[sectionsd] Adding Service %s\n", s->second->serviceName.c_str());
		return 1;	//add
	}
	return 0;		//scanType didn't match do not handle in any case
}

//This updates the /tmp/currentservices.xml with the differences between services.xml and SDT content
//It contains two loops. Each is nested. First loop adds and replaces the services which are new in the SDT
//The second loop removes Services which are in services.xml and not in the SDT anymore.
//Returns true if the transponder needs to be updated
bool updateCurrentXML(xmlNodePtr provider, xmlNodePtr tp_node, const int scanType, const bool /*is_current*/)
{
	bool is_needed = false;
	bool newprov = false;
	bool is_sat = false;
	bool tp_existed = false;

	std::string name;

	FILE * src = NULL;
	FILE * dst = NULL;
	char buffer[256] = "";

	readLockServices();
	for (MySIservicesOrderUniqueKey::iterator s = mySIservicesOrderUniqueKey.begin(); s != mySIservicesOrderUniqueKey.end(); s++)
	{
		unlockServices();
		readLockMessaging();
		if (messaging_zap_detected) {
			unlockMessaging();
			return false;
		}
		unlockMessaging();
		if ( (s->second->transport_stream_id == xmlGetNumericAttribute(tp_node, "id", 16)) &&
			(s->second->original_network_id == xmlGetNumericAttribute(tp_node, "onid", 16)) )
		{
			int action = get_action(tp_node, s, scanType);

			if (action > 0) {
				if (!is_needed) {
					is_needed = true;
					//create new currentservices
					if (!(dst = fopen(CURRENTSERVICES_TMP, "w"))) {
						dprintf("unable to open %s for writing\n", CURRENTSERVICES_TMP);
						return false;
					}
					if (!(src = fopen(CURRENTSERVICES_XML, "r"))) {
						//if currentservices doesn't yet exist
						newprov = true;
						write_xml_header(dst);
						is_sat = write_xml_provider(src, dst, provider, false);
					} else {
						//if it exists. copy till provider
						is_sat = write_xml_provider(src, dst, provider, true);
						//if eof provider didn't exist
						if (feof(src)) {
							newprov = true;
							write_xml_provider(src, dst, provider, false);
						}
						else
							//copy all transponders belonging to current prov
							tp_existed = write_xml_transponder(src, dst, tp_node, is_sat, true);
					}
					// write new transponder node
					write_xml_transponder(src, dst, tp_node, is_sat, false);
				}
				//check which action is necessary for current service
				//0 = nothing / 1 = add / 2 = replace
				switch (action)
				{
					case 1:
						fprintf(dst,
						"\t\t\t<channel action=\"%s\" service_id=\"%04x\" name=\"%s\" service_type=\"%02x\"/>\n",
						"add",
						s->second->service_id,
						UTF8_to_UTF8XML(s->second->serviceName.c_str()).c_str(),
						s->second->serviceTyp);
						break;
					case 2:
						fprintf(dst,
						"\t\t\t<channel action=\"%s\" service_id=\"%04x\" name=\"%s\" service_type=\"%02x\"/>\n",
						"replace",
						s->second->service_id,
						UTF8_to_UTF8XML(s->second->serviceName.c_str()).c_str(),
						s->second->serviceTyp);
						break;
					default:
						break;
				}

			}
		}
		readLockServices();
	}
	unlockServices();
	//Second loop to detect services which are not longer in SDT
	//Only remove if Actual SDT. This could be changed, if all providers would send correct data
	//It is pretty much the same as the first loop. Check there. Later: merge them together?
	readLockServices();
	MySIservicesOrderUniqueKey::iterator s = mySIservicesOrderUniqueKey.begin();
	if (s->second->is_actual == 2) {
		xmlNodePtr node = tp_node->xmlChildrenNode;

		while (xmlGetNextOccurence(node, "channel") != NULL) {

			readLockMessaging();
			if (messaging_zap_detected) {
				unlockMessaging();
				return false;
			}
			unlockMessaging();

			s = mySIservicesOrderUniqueKey.begin();
			while ( (s != mySIservicesOrderUniqueKey.end()) &&
				(s->second->service_id != xmlGetNumericAttribute(node, "service_id", 16)) )
				s++;
			if (s == mySIservicesOrderUniqueKey.end()) {
				if (!is_needed) {

					is_needed = true;
					//create new currentservices
					if (!(dst = fopen(CURRENTSERVICES_TMP, "w"))) {
						dprintf("unable to open %s for writing\n", CURRENTSERVICES_TMP);
						unlockServices();
						return false;
					}
					if (!(src = fopen(CURRENTSERVICES_XML, "r"))) {
						newprov = true;
						write_xml_header(dst);
						is_sat = write_xml_provider(src, dst, provider, false);
					} else {

						is_sat = write_xml_provider(src, dst, provider, true);
						if (feof(src)) {
							newprov = true;
							write_xml_provider(src, dst, provider, false);
						}
						tp_existed = write_xml_transponder(src, dst, tp_node, is_sat, true);
					}
					write_xml_transponder(src, dst, tp_node, is_sat, false);
				}
				name = xmlGetAttribute(node, "name");

				dprintf("[sectionsd] Removing Service %s\n", name.c_str());
				fprintf(dst,
					"\t\t\t<channel action=\"%s\" service_id=\"%04lx\" name=\"%s\" service_type=\"%02lx\"/>\n",
					"remove",
					xmlGetNumericAttribute(node, "service_id", 16),
					UTF8_to_UTF8XML(name.c_str()).c_str(),
					xmlGetNumericAttribute(node, "service_type", 16));
			}
			node = node->xmlNextNode;
		}
	}

	unlockServices();

	//If we chnged some services write the closing tags.
	if (is_needed) {

		fprintf(dst,"\t\t</transponder>\n");

		if (!tp_existed)
			write_xml_provend(dst, is_sat);

		if (newprov) {
			write_xml_footer(dst);
		}
		else {
			fgets(buffer, 255, src);
			while(!feof(src))
			{
				fprintf(dst, buffer);
				fgets(buffer, 255, src);
			}
			fclose(src);
		}

		fclose(dst);
	}

	return is_needed;
}

xmlNodePtr getProviderFromSatellitesXML(xmlNodePtr node, const int position)
{
	struct stat buf;
	const char *filename = ZAPITCONFIGDIR "/" SATELLITES_XML;
	if ((stat(filename, &buf) == -1) && (errno == ENOENT))
		filename = DATADIR "/" SATELLITES_XML;

	xmlDocPtr satellites_parser = parseXmlFile(filename);
	if (satellites_parser == NULL)
		return NULL;
	xmlNodePtr satellite = xmlDocGetRootElement(satellites_parser)->xmlChildrenNode;
	while (satellite) {
		if (xmlGetSignedNumericAttribute(satellite, "position", 16) == position) {
			while (node) {
				if (!strcmp(xmlGetAttribute(satellite, "name"), xmlGetAttribute(node, "name")))
				{
					xmlFreeDoc(satellites_parser);
					return node;
				}
				node = node->xmlNextNode;
			}
		}
		satellite = satellite->xmlNextNode;
	}
	xmlFreeDoc(satellites_parser);
	return NULL;
}

xmlNodePtr getProviderbyName(xmlNodePtr current_provider, xmlNodePtr provider) {
	while (current_provider) {
		if (!strcmp(xmlGetAttribute(current_provider, "name"), xmlGetAttribute(provider, "name")))
			return current_provider;
		current_provider = current_provider->xmlNextNode;
	}
	return NULL;
}

xmlNodePtr findTransponderFromProv(xmlNodePtr transponder, const t_original_network_id onid, const t_transport_stream_id tsid) {
	while (transponder) {
		if ((xmlGetNumericAttribute(transponder, "onid", 16) == onid) && (xmlGetNumericAttribute(transponder, "id", 16) == tsid))
			return transponder;
		transponder = transponder->xmlNextNode;
	}
	return NULL;
}

//SDT-Thread calls this function if it found a complete Service Description Table (SDT). Overwrite for actual = true - for other = false
//static bool updateTP(const t_original_network_id onid, const t_transport_stream_id tsid, const int scanType, const bool overwrite)
static bool updateTP(const int scanType)
{
	xmlDocPtr service_parser = parseXmlFile(SERVICES_XML);
	xmlDocPtr current_parser = NULL;
	bool need_update = false;
	FILE * tmp = NULL;
	xmlNodePtr provider = NULL;
	xmlNodePtr current_provider = NULL;
	t_transport_stream_id tsid = 0;
	t_original_network_id onid = 0;

	if (service_parser == NULL)
		return false;

	int i = 0;
	while ((i < MAX_SDTs) && (messaging_sdt_tid[i] != 0)) {
		readLockMessaging();
		if (messaging_zap_detected) {
			unlockMessaging();
			need_update = false;
			if (current_parser != NULL)
				xmlFreeDoc(current_parser);
			unlink(CURRENTSERVICES_TMP);
			break;
		}
		unlockMessaging();
		onid = (t_original_network_id) (messaging_sdt_tid[i] >> 16) & 0xffff;
		tsid = (t_transport_stream_id) messaging_sdt_tid[i] & 0xffff;

//GET_ORIGINAL_NETWORK_ID_FROM_CHANNEL_ID(channel_id) ((t_original_network_id)((channel_id) >> 16))
//GET_SERVICE_ID_FROM_CHANNEL_ID(channel_id) ((t_service_id)(channel_id))

		xmlNodePtr services_tp = FindTransponder(xmlDocGetRootElement(service_parser)->xmlChildrenNode, onid, tsid);

		if (services_tp)
			provider = GetProvider(xmlDocGetRootElement(service_parser)->xmlChildrenNode, services_tp);
		else
			provider = NULL;

		tmp = fopen(CURRENTSERVICES_XML, "r");
		if (tmp) {
			fclose(tmp);
			current_parser= parseXmlFile(CURRENTSERVICES_XML);
		}

		xmlNodePtr current_tp = NULL;

		if (current_parser != NULL) {
			current_tp = FindTransponder(xmlDocGetRootElement(current_parser)->xmlChildrenNode, onid, tsid);
			if (provider) {
				//printf("getProvbyname\n");
				current_provider = getProviderbyName(xmlDocGetRootElement(current_parser)->xmlChildrenNode, provider);

			}
			else {
				if (current_tp)
					current_provider = GetProvider(xmlDocGetRootElement(current_parser)->xmlChildrenNode, current_tp);
			}
		}

		if (!current_tp) {
			if (provider) {
				if (current_provider) {
					//printf("update with current\n");
					if (!strcmp(xmlGetAttribute(current_provider, "name"), xmlGetAttribute(provider, "name")))
						if (updateCurrentXML(current_provider, services_tp, scanType, false))
							need_update = true;

				}
				else {
					//printf("update with prov\n");
					if (updateCurrentXML(provider, services_tp, scanType, false))
						need_update = true;

				}
			}
			else
				dprintf("[sectionsd] No Transponder with ONID: %04x TSID: %04x found in services.xml!\n", onid, tsid);
		}
		else {
			if (!provider) {
				//printf("update with current / current\n");

				if (updateCurrentXML(current_provider, current_tp, scanType, false))
					need_update = true;

			}
			else
				dprintf("[sectionsd] No Update needed for Transponder with ONID: %04x TSID: %04x!\n", onid, tsid);
		}
		if (current_parser != NULL)
			xmlFreeDoc(current_parser);
		current_parser = NULL;

		i++;
	}

	xmlFreeDoc(service_parser);

	if (need_update)
	{
		rename(CURRENTSERVICES_TMP, CURRENTSERVICES_XML);

		dprintf("[sectionsd] We updated at least one Transponder in currentservices.xml!\n");

	} else
		dprintf("[sectionsd] No new services found!\n");
//printf("Finishing updateTP\n");
	return need_update;
}
//stolen from frontend.cpp please fix.
fe_code_rate_t getCodeRate(const uint8_t fec_inner)
{
	switch (fec_inner & 0x0F) {
	case 0x01:
		return FEC_1_2;
	case 0x02:
		return FEC_2_3;
	case 0x03:
		return FEC_3_4;
	case 0x04:
		return FEC_5_6;
	case 0x05:
		return FEC_7_8;
	case 0x0F:
		return FEC_NONE;
	default:
		return FEC_AUTO;
	}
}
//also stolen from frontend.cpp. please fix.
fe_modulation_t getModulation(const uint8_t modulation)
{
	switch (modulation) {
	case 0x00:
		return QPSK;
	case 0x01:
		return QAM_16;
	case 0x02:
		return QAM_32;
	case 0x03:
		return QAM_64;
	case 0x04:
		return QAM_128;
	case 0x05:
		return QAM_256;
	default:
#if 1
		return QAM_AUTO;
#else
		// i do not know how to do it correctly for old API -- seife
		return QAM_256;
#endif
	}
}

static void writeTransponderFromDescriptor(FILE *dst, const t_original_network_id onid, const t_transport_stream_id tsid, const char *ddp, const bool is_sat)
{
	struct satellite_delivery_descriptor *sdd;
	struct cable_delivery_descriptor *cdd;

	if (is_sat) {
		sdd = (struct satellite_delivery_descriptor *)ddp;
		fprintf(dst,"\t\t<transponder id=\"%04x\" onid=\"%04x\" frequency=\"%08u\" inversion=\"%hu\" symbol_rate=\"%08u\" fec_inner=\"%hu\" polarization=\"%hu\">\n",
		tsid,
		onid,
		((sdd->frequency_1 >> 4)	* 100000000) +
		((sdd->frequency_1 & 0x0F)	* 10000000) +
		((sdd->frequency_2 >> 4)	* 1000000) +
		((sdd->frequency_2 & 0x0F)	* 100000) +
		((sdd->frequency_3 >> 4)	* 10000) +
		((sdd->frequency_3 & 0x0F)	* 1000) +
		((sdd->frequency_4 >> 4)	* 100) +
		((sdd->frequency_4 & 0x0F)	* 10),
//		sdd->modulation,
		INVERSION_AUTO,
		((sdd->symbol_rate_1 >> 4)	* 100000000) +
		((sdd->symbol_rate_1 & 0x0F)	* 10000000) +
		((sdd->symbol_rate_2 >> 4)	* 1000000) +
		((sdd->symbol_rate_2 & 0x0F)	* 100000) +
		((sdd->symbol_rate_3 >> 4)	* 10000) +
		((sdd->symbol_rate_3 & 0x0F)	* 1000) +
		((sdd->symbol_rate_4 >> 4)	* 100),
		(fe_code_rate_t) getCodeRate(sdd->fec_inner & 0x0F),
		sdd->polarization);
	}
	else {
		cdd = (struct cable_delivery_descriptor *)ddp;
		fprintf(dst,"\t\t<transponder id=\"%04x\" onid=\"%04x\" frequency=\"%09u\" inversion=\"%hu\" symbol_rate=\"%07u\" fec_inner=\"%hu\" modulation=\"%hu\">\n",
		tsid,
		onid,
		((cdd->frequency_1 >> 4)	* 1000000000) +
		((cdd->frequency_1 & 0x0F)	* 100000000) +
		((cdd->frequency_2 >> 4)	* 10000000) +
		((cdd->frequency_2 & 0x0F)	* 1000000) +
		((cdd->frequency_3 >> 4)	* 100000) +
		((cdd->frequency_3 & 0x0F)	* 10000) +
		((cdd->frequency_4 >> 4)	* 1000) +
		((cdd->frequency_4 & 0x0F)	* 100),
//		cdd->fec_outer,
		INVERSION_AUTO,
		((cdd->symbol_rate_1 >> 4)	* 100000000) +
		((cdd->symbol_rate_1 & 0x0F)	* 10000000) +
		((cdd->symbol_rate_2 >> 4)	* 1000000) +
		((cdd->symbol_rate_2 & 0x0F)	* 100000) +
		((cdd->symbol_rate_3 >> 4)	* 10000) +
		((cdd->symbol_rate_3 & 0x0F)	* 1000) +
		((cdd->symbol_rate_4 >> 4)	* 100),
		(fe_code_rate_t) getCodeRate(cdd->fec_inner & 0x0F),
		(fe_modulation_t) getModulation(cdd->modulation));
	}
	fprintf(dst,"\t\t</transponder>\n");
}

static void updateXMLnet(xmlNodePtr provider, const t_original_network_id onid, const t_transport_stream_id tsid,
				const char *ddp, const int position)
{
	FILE * src = NULL;
	FILE * dst = NULL;
	bool is_new = false;
	bool is_sat = false;

#define MAX_SIZE_PROV_STR	256
	char prov_str_neu[MAX_SIZE_PROV_STR] = "";
	char buffer[256] = "";

	std::string frontendType;
	std::string provider_name;
	std::string diseqc;

	if (!(dst = fopen(CURRENTSERVICES_TMP, "w"))) {
		dprintf("unable to open %s for writing\n", CURRENTSERVICES_TMP);
		return;
	}

	frontendType = xmlGetName(provider);
	provider_name = xmlGetAttribute(provider, "name");

	if (!strcmp(frontendType.c_str(), "sat")) {
		diseqc = xmlGetAttribute(provider, "diseqc");
		snprintf(prov_str_neu, MAX_SIZE_PROV_STR, "\t<%s name=\"%s\" position=\"%04x\" diseqc=\"%s\">\n", frontendType.c_str(), provider_name.c_str(),
			position, diseqc.c_str());
		is_sat = true;
	}
	else {
		snprintf(prov_str_neu, MAX_SIZE_PROV_STR, "\t<%s name=\"%s\">\n", frontendType.c_str(), provider_name.c_str());
		is_sat = false;
	}

	if (!(src = fopen(CURRENTSERVICES_XML, "r"))) {
		is_new = true;
		write_xml_header(dst);
		fprintf(dst, prov_str_neu);
		if (ddp != NULL)
			writeTransponderFromDescriptor(dst, onid, tsid, ddp, is_sat);
		write_xml_provend(dst, is_sat);
		write_xml_footer(dst);
	}
	else {
		if (!feof(src)) {
			fgets(buffer, 255, src);
			//find prov in currentservices.xml
			while( (!feof(src)) && (strcmp(buffer, "</zapit>\n") != 0) && (strcmp(buffer, prov_str_neu) != 0) )
			{
				fprintf(dst, buffer);
				fgets(buffer, 255, src);
			}
			if (strcmp(buffer, prov_str_neu) != 0)
				fprintf(dst, prov_str_neu);
			if (ddp != NULL) {
				while( (!feof(src)) && (strcmp(buffer, "</zapit>\n") != 0) &&
					(strcmp(buffer, "\t</sat>\n") != 0) && (strcmp(buffer, "\t</cable>\n")) )
				{
					fprintf(dst, buffer);
					fgets(buffer, 255, src);
				}
				//if (strcmp(buffer, "</zapit>\n") == 0)
				writeTransponderFromDescriptor(dst, onid, tsid, ddp, is_sat);
			}
			if (strcmp(buffer, "</zapit>\n") == 0)
				write_xml_provend(dst, is_sat);

			while (!feof(src))
			{
				fprintf(dst, buffer);
				fgets(buffer, 255, src);
			}
		}

		fclose(src);
	}
	fclose(dst);

	rename(CURRENTSERVICES_TMP, CURRENTSERVICES_XML);

	return;
}

static bool updateNetwork()
{
	t_transport_stream_id tsid;
	t_original_network_id onid;
	t_network_id network_id;

	int position = 0;
	struct satellite_delivery_descriptor *sdd;
	const char *ddp;
	std::string frontendType;

	bool need_update = false;
	bool needs_fix = false;

	xmlNodePtr provider;
	xmlNodePtr tp;

	FILE * tmp;

	xmlDocPtr service_parser = parseXmlFile(SERVICES_XML);

	if (service_parser == NULL)
		return false;

	xmlDocPtr current_parser = NULL;
	xmlNodePtr current_tp = NULL;
	xmlNodePtr current_provider = NULL;

	tmp = fopen(CURRENTSERVICES_XML, "r");
	if (tmp) {
		fclose(tmp);
		current_parser= parseXmlFile(CURRENTSERVICES_XML);
	}

	int i = 0;
	readLockMessaging();
	while ((i < MAX_NIDs) && (messaging_nit_nid[i] != 0) && (!messaging_zap_detected)) {
		unlockMessaging();

		network_id = messaging_nit_nid[i];

		// go through all transpopnders currently cached by neutrino - I won't need them after this loop. They COULD be cleared.
     		for (MySItranspondersOrderUniqueKey::iterator s = mySItranspondersOrderUniqueKey.begin(); s !=
		 mySItranspondersOrderUniqueKey.end(); s++)
		{
			readLockMessaging();
			if (messaging_zap_detected) {
				unlockMessaging();
				break;
			}
			unlockMessaging();
			if (s->second->network_id == network_id) {
				needs_fix = false;
				tsid = s->second->transport_stream_id;
				onid = s->second->original_network_id;
				ddp = &s->second->delivery_descriptor[0];

				//printf("Descriptor_type: %02x\n", s->second->delivery_type);
				frontendType = xmlGetName(xmlDocGetRootElement(service_parser)->xmlChildrenNode);
				switch (s->second->delivery_type) {
					case 0x43:
						if (!strcmp(frontendType.c_str(), "sat")) {
							sdd = (struct satellite_delivery_descriptor *)ddp;
							position = (sdd->orbital_pos_hi << 8) | sdd->orbital_pos_lo;
							if (!sdd->west_east_flag)
								position = -position;
							provider = getProvbyPosition(xmlDocGetRootElement(service_parser)->xmlChildrenNode, position);
						}
						else {
							provider = NULL;
							position = 1000;
						}
						break;
					case 0x44:
						if (!strcmp(frontendType.c_str(), "cable")) {
							provider = xmlDocGetRootElement(service_parser)->xmlChildrenNode;
							position = 0;
						}
						else {
							position = 1000;
							provider = NULL;
						}
						break;
					default:
						position = 1000;
						provider = NULL;
						break;
				}

				//provider with satellite position does not exist in services.xml
				if ((!provider) && (position != 1000)) {
					provider = getProviderFromSatellitesXML(xmlDocGetRootElement(service_parser)->xmlChildrenNode, position);
					if (provider)
						needs_fix = true; //backward compatibility - add position node
				}
				//provider also not found in satellites.xml...
				if ((!provider) && (position != 1000)) {
					if (current_parser != NULL) {
		 				provider = getProvbyPosition(xmlDocGetRootElement(current_parser)->xmlChildrenNode, position);
					}
				}
				//and finally provider not found in currentservices.xml - we give up
				if (!provider) {
					dprintf("[sectionsd::updateNetwork] Provider not found for Transponder ONID: %04x TSID: %04x.\n", onid,
					tsid);
				}
				else {
					//we found a valid provider node
					tp = findTransponderFromProv(provider->xmlChildrenNode, onid, tsid);
					if (!tp) {
						dprintf("[sectionsd::updateNetwork] Transponder ONID: %04x TSID: %04x not found.\n", onid, tsid);
					 	if (current_parser != NULL) {

							switch (s->second->delivery_type) {
								 case 0x43: //satellite descriptor
									current_provider =
									getProvbyPosition(xmlDocGetRootElement(current_parser)->xmlChildrenNode,
														position);
									break;
								case 0x44: //cable
									current_provider = xmlDocGetRootElement(current_parser)->xmlChildrenNode;
									break;
								default:
									break;
							}
							if (current_provider)
								current_tp = findTransponderFromProv(current_provider->xmlChildrenNode, onid,
								tsid);
						}
						//write the new transponder to currentservices.xml
						if (!current_tp) {
							updateXMLnet(provider, onid, tsid, ddp, position);
							xmlFreeDoc(current_parser);
							current_parser= parseXmlFile(CURRENTSERVICES_XML);
						}

					} else {
						dprintf("[sectionsd::updateNetwork] Transponder ONID: %04x TSID: %04x found.\n", onid, tsid);
						if ( (s->second->is_actual & 7) && (needs_fix) ) {
							//if(!(tmp = fopen(CURRENTSERVICES_XML, "r")))
							if (current_parser == NULL) {
								dprintf("[sectionsd::updateNetwork] services.xml provider needs update\n");
								updateXMLnet(provider, onid, tsid, NULL, position);
								current_parser= parseXmlFile(CURRENTSERVICES_XML);
							}
							else {
						 		current_provider =
									getProvbyPosition(xmlDocGetRootElement(current_parser)->xmlChildrenNode,
														position);
								if (!current_provider) {
									updateXMLnet(provider, onid, tsid, NULL, position);
									xmlFreeDoc(current_parser);
									current_parser= parseXmlFile(CURRENTSERVICES_XML);
								}
							}
						}
					}
				}
			}
			//sleep(10);
		}
		i++;
		readLockMessaging();
	}
	unlockMessaging();
	if (current_parser != NULL)
		xmlFreeDoc(current_parser);
	xmlFreeDoc(service_parser);

	return need_update;
}

xmlNodePtr findBouquet(xmlDocPtr parser,t_bouquet_id bouquet_id)
{
	xmlNodePtr bouquet = xmlDocGetRootElement(parser)->xmlChildrenNode;
	while (xmlGetNextOccurence(bouquet, "Bouquet") != NULL) {
		//printf("Checking: %04x\n", xmlGetNumericAttribute(bouquet, "bouquet_id", 16));
		if (xmlGetNumericAttribute(bouquet, "bouquet_id", 16) == bouquet_id)
			return bouquet;
		bouquet = bouquet->xmlNextNode;
	}
	return NULL;
}

static bool compareBouquet(xmlNodePtr channel, t_bouquet_id bouquet_id)
{
	MySIbouquetsOrderUniqueKey::iterator s = mySIbouquetsOrderUniqueKey.begin();
	while (s != mySIbouquetsOrderUniqueKey.end()) {
		if (s->second->bouquet_id == bouquet_id) {
			if (channel) {
				if (	(xmlGetNumericAttribute(channel, "serviceID", 16) != s->second->service_id) ||
					(xmlGetNumericAttribute(channel, "tsid", 16) != s->second->transport_stream_id) ||
					(xmlGetNumericAttribute(channel, "onid", 16) != s->second->original_network_id) ) {
					//printf("Service: %04x\n",s->second->service_id);
					return true;
					}
				channel = channel->xmlNextNode;
			} else
				return true;
		}
		s++;
	}
	if ((!channel) && (s == mySIbouquetsOrderUniqueKey.end()))
		return false;
	else
		return true;
}

static void write_bouquet_xml_node(FILE *fd, t_bouquet_id bouquet_id)
{
	bool found = false;

	MySIbouquetsOrderUniqueKey::iterator s = mySIbouquetsOrderUniqueKey.begin();
	while ((!found) && (s != mySIbouquetsOrderUniqueKey.end())) {
		if ((s->second->bouquet_id == bouquet_id) && (s->second->bouquetName.length() != 0))
			found = true;
		else
			s++;
	}
	if (found)
		fprintf(fd, "\t<Bouquet type=\"1\" bouquet_id=\"%04x\" name=\"%s\" hidden=\"0\" locked=\"0\">\n",
			bouquet_id,
			s->second->bouquetName.c_str());
	else
		fprintf(fd, "\t<Bouquet type=\"1\" bouquet_id=\"%04x\" name=\"NoBouquetName\" hidden=\"0\" locked=\"0\">\n",
			bouquet_id);

}

static void addBouquetToCurrentXML(xmlNodePtr bouquet, t_bouquet_id bouquet_id)
{
	FILE * dst = NULL;
	std::string name;
	xmlNodePtr node;

	if (!(dst = fopen(CURRENTBOUQUETS_TMP, "w"))) {
		dprintf("unable to open %s for writing\n", CURRENTBOUQUETS_TMP);
		return;
	}

	write_bouquet_xml_header(dst);

	while (bouquet) {
		name = xmlGetAttribute(bouquet, "name");

		fprintf(dst, "\t<Bouquet type=\"1\" bouquet_id=\"%04x\" name=\"%s\" hidden=\"0\" locked=\"0\">\n",
			xmlGetNumericAttribute(bouquet, "bouquet_id", 16),
			UTF8_to_UTF8XML(name.c_str()).c_str());

		node = bouquet->xmlChildrenNode;
		while (xmlGetNextOccurence(node, "channel") != NULL) {
			fprintf(dst, "\t\t<channel serviceID=\"%04x\" name=\"\" tsid=\"%04x\" onid=\"%04x\"/>\n",
				xmlGetNumericAttribute(node, "serviceID", 16),
				xmlGetNumericAttribute(node, "tsid", 16),
				xmlGetNumericAttribute(node, "onid", 16));
			node = node->xmlNextNode;
		}

		fprintf(dst, "\t</Bouquet>\n");

		bouquet = bouquet->xmlNextNode;
	}

	write_bouquet_xml_node(dst, bouquet_id);
	for (MySIbouquetsOrderUniqueKey::iterator s = mySIbouquetsOrderUniqueKey.begin(); s !=
 		mySIbouquetsOrderUniqueKey.end(); s++)
	{
		if (s->second->bouquet_id == bouquet_id) {
		/*
			dprintf("Bouquet ID: %04x Name: %s ONID: %04x TSID: %04x SID: %04x Type: %hhu\n",
					s->second->bouquet_id,
					s->second->bouquetName.c_str(),
					s->second->original_network_id,
					s->second->transport_stream_id,
					s->second->service_id,
					s->second->serviceTyp);
		*/
			//printf("Position: %04x Typ: %hhu\n",s->second->position, s->second->serviceTyp);
			fprintf(dst, "\t\t<channel serviceID=\"%04x\" name=\"\" tsid=\"%04x\" onid=\"%04x\"/>\n",
				s->second->service_id,
				s->second->transport_stream_id,
				s->second->original_network_id);
		}
	}
	fprintf(dst, "\t</Bouquet>\n");
	write_bouquet_xml_footer(dst);
	fclose(dst);
}

static bool updateBouquets()
{
	bool need_update = false;
	bool at_least_one_update = false;
	t_bouquet_id bouquet_id;
	xmlNodePtr bouquet;

	FILE * tmp;

	xmlDocPtr bouquet_parser = parseXmlFile(BOUQUETS_XML);

	if (bouquet_parser == NULL)
		return false;

	xmlDocPtr current_parser = NULL;

	tmp = fopen(CURRENTBOUQUETS_XML, "r");
	if (tmp) {
		fclose(tmp);
		current_parser= parseXmlFile(CURRENTBOUQUETS_XML);
	}

	int i = 0;
	while ((i < MAX_BAT) && (messaging_bat_bouquet_id[i] != 0)) {
		//dprintf("Bouquet ID: %04x consisting of %d sections\n", messaging_bat_bouquet_id[i], messaging_bat_last_section[i]+1);

		bool is_complete = true;
		for (int j = 0; j <= messaging_bat_last_section[i]; j++) {
			if (!messaging_bat_sections_so_far[i][j])
				is_complete = false;
		}
		if (is_complete) {
			bouquet_id = messaging_bat_bouquet_id[i];
			bouquet = NULL;
			need_update = false;

			if (current_parser != NULL)
				bouquet = findBouquet(current_parser, bouquet_id);

			if (bouquet == NULL) {
				//printf("Bouquet not found in CurrentBouquet\n");
				bouquet = findBouquet(bouquet_parser, bouquet_id);

				if (bouquet != NULL)
				{
					//printf("Bouquet found in Bouquet\n");
					if (!xmlGetNumericAttribute(bouquet, "hidden", 16))
						need_update = compareBouquet(bouquet->xmlChildrenNode, bouquet_id);
				}
				else
					need_update = true;

				if (need_update) {
					//printf("We need to update\n");
					at_least_one_update = true;
					if (current_parser != NULL)
						addBouquetToCurrentXML(xmlDocGetRootElement(current_parser)->xmlChildrenNode, bouquet_id);
					else
						addBouquetToCurrentXML(NULL, bouquet_id);
					rename(CURRENTBOUQUETS_TMP, CURRENTBOUQUETS_XML);
					if (current_parser != NULL)
						xmlFreeDoc(current_parser);
					current_parser= parseXmlFile(CURRENTBOUQUETS_XML);
				}
			}
			//else
			//	printf("Bouquet found in CurrentBouquet\n");
		}
		i++;
	}
	if (current_parser != NULL)
		xmlFreeDoc(current_parser);
	xmlFreeDoc(bouquet_parser);

	return at_least_one_update;
}

//This has to be rewritten. I don't know how neutrino accesses its conf files...
static int getscanType()
{
	FILE * scanconf = NULL;
	char buffer[256] = "";
	int ret = 3;

	if (!(scanconf = fopen(NEUTRINO_SCAN_SETTINGS_FILE, "r"))) {
		printf("unable to open %s for reading\n", NEUTRINO_SCAN_SETTINGS_FILE);
	} else {
		while (!feof(scanconf)) {
			fgets(buffer, 255, scanconf);
			if (!strncmp(buffer, "scanType=", 9)) {
				if (buffer[9] >= '0' && buffer[9] <='3')
					ret= buffer[9] - '0';
				break;
			}
		}
		fclose(scanconf);
	}

	return ret;
}

//---------------------------------------------------------------------
//			nit-thread
// reads nit for transponder list
//---------------------------------------------------------------------
static void *nitThread(void *)
{

	struct SI_section_header *header;
	const unsigned timeoutInMSeconds = 2500;
	bool is_new;
	t_network_id nid = 0;
	time_t lastData = 0;
	time_t zeit = 0;
	int rs = 0;
	int i = 0;
	bool is_actual = false;

	dmxNIT.addfilter(0x40, 0xfe );		//NIT actual = 0x40 + NIT other = 0x41

	try
	{
		dprintf("[%sThread] pid %d (%lu) start\n", "nit", getpid(), pthread_self());

		int timeoutsDMX = 0;
		char *static_buf = new char[MAX_SECTION_LENGTH];
		int rc;

		if (static_buf == NULL)
			throw std::bad_alloc();

		for ( i = 0; i < MAX_NIDs; i++)
			messaging_nit_nid[i] = 0;
		dmxNIT.start(); // -> unlock
		if (!scanning)
			dmxNIT.request_pause();

		bool startup = true;

		waitForTimeset();

		for (;;)
		{
			while (!scanning) {
				if(sectionsd_stop)
					break;
				sleep(1);
			}
			if(sectionsd_stop)
				break;
			zeit = time_monotonic();

			readLockMessaging();
			if (messaging_zap_detected)
				startup = true;
			unlockMessaging();
			if ((zeit > lastData + TIME_NIT_NONEWDATA) || (startup))
			{
				struct timespec abs_wait;
				struct timeval now;

				gettimeofday(&now, NULL);
				TIMEVAL_TO_TIMESPEC(&now, &abs_wait);
				abs_wait.tv_sec += (TIME_NIT_SCHEDULED_PAUSE);

				dmxNIT.real_pause();
				pthread_mutex_lock( &dmxNIT.start_stop_mutex );
				dprintf("dmxNIT: going to sleep...\n");

				if ((auto_scanning > 0) && (!startup)) {
					 if (messaging_nit_nid[0] != 0)
						updateNetwork();
					pthread_mutex_unlock( &dmxSDT.start_stop_mutex );
					dmxSDT.change( 0 );
				}

				for ( i = 0; i < MAX_NIDs; i++)
					messaging_nit_nid[i] = 0;

				rs = pthread_cond_timedwait( &dmxNIT.change_cond, &dmxNIT.start_stop_mutex, &abs_wait );
				pthread_mutex_unlock( &dmxNIT.start_stop_mutex );

				if (rs == ETIMEDOUT)
				{
					dprintf("dmxNIT: waking up again - looking for new transponders :)\n");
					dmxNIT.change( 0 ); // -> restart
				}
				else if (rs == 0)
				{
					dprintf("dmxNIT: waking up again - requested from .change()\n");
				}
				else
				{
					dprintf("dmxNIT:  waking up again - unknown reason?!\n");
					dmxNIT.real_unpause();
				}
				// update zeit after sleep
				startup = false;
				zeit = time_monotonic();
				timeoutsDMX = 0;
				lastData = zeit;
			}

			if (timeoutsDMX >= RESTART_DMX_AFTER_TIMEOUTS && scanning)
			{
				timeoutsDMX = 0;
				dmxNIT.stop();
				dmxNIT.start(); // leaves unlocked
				dputs("\n !!! dmxNIT restarted !!!\n");
			}

			rc = dmxNIT.getSection(static_buf, timeoutInMSeconds, timeoutsDMX);

			if (rc < 0)
				continue;

			if (rc < (int)sizeof(struct SI_section_header))
			{
				xprintf("%s rc < sizeof(SI_section_header) (%d < %d)\n", __FUNCTION__, rc, sizeof(struct SI_section_header));
				continue;
			}

			header = (SI_section_header*)static_buf;
			unsigned short section_length = header->section_length_hi << 8 | header->section_length_lo;

			if (header->current_next_indicator)
			{
				if ((header->table_id == 0x40) || (header->table_id == 0x41))
				{
					// Wir wollen nur aktuelle sections
					SIsectionNIT nit(section_length + 3, static_buf);

					is_actual = (header->table_id == 0x40) ? true : false;

					is_new = false;

					for (SInetworks::iterator s = nit.networks().begin(); s != nit.networks().end(); s++)
						if (addTransponder(*s, is_actual))
							is_new = true;

					if (is_new) {
						nid = header->table_id_extension_hi << 8 | header->table_id_extension_lo;
						lastData = time_monotonic();
						dprintf("[nitThread] adding %d transponders [table 0x%x]\n",
								nit.networks().size(), header->table_id);
						i = 0;
						while ((i < MAX_NIDs) && (messaging_nit_nid[i] != 0) && (messaging_nit_nid[i] != nid))
							i++;
						if (i < MAX_NIDs)
							messaging_nit_nid[i] = nid;
					}
				}
			} // if
		} // for

		dmxNIT.closefd();
		delete[] static_buf;
	} // try
	catch (std::exception& e)
	{
		fprintf(stderr, "Caught std-exception in connection-thread %s!\n", e.what());
	}
	catch (...)
	{
		fprintf(stderr, "Caught exception in nit-thread!\n");
	}

	dprintf("nit-thread ended\n");

	pthread_exit(NULL);
}

//little helper for sdt-thread
static int get_bat_slot( t_bouquet_id bouquet_id, int last_section)
{
	for (int i = 0; i < MAX_BAT; i++) {
		if ( (messaging_bat_bouquet_id[i] == 0) || (messaging_bat_bouquet_id[i] == bouquet_id) ) {
			messaging_bat_bouquet_id[i] = bouquet_id;
			messaging_bat_last_section[i] = last_section;
			return i;
		}
	}
	return -1;
}

//---------------------------------------------------------------------
// sdt-thread
// reads sdt for service list
//---------------------------------------------------------------------
static void *sdtThread(void *)
{
	struct SI_section_header *header;
	const unsigned timeoutInMSeconds = 2500;
	bool is_new;
	t_transponder_id tid = 0;
	time_t lastData = 0;
	time_t zeit = 0;
	int rs = 0;
	int i = 0;
	int j = 0;
	int scanType = 3;	//default scan all
	bool bouquet_filtered = false;
	int is_actual = 0;

	dmxSDT.addfilter(0x42, 0xf3 );		//SDT actual = 0x42 + SDT other = 0x46 + BAT = 0x4A

	dprintf("[%sThread] pid %d (%lu) start\n", "sdt", getpid(), pthread_self());

	int timeoutsDMX = 0;
	char *static_buf = new char[MAX_SECTION_LENGTH];
	int rc;

	if (static_buf == NULL)
	{
		xprintf("%s: could not allocate static_buf\n", __FUNCTION__);
		pthread_exit(NULL);
		//throw std::bad_alloc();
	}
	for ( i = 0; i < MAX_SDTs; i++)
		messaging_sdt_tid[i] = 0;
	writeLockMessaging();
	for ( i = 0; i < MAX_BAT; i++) {
		messaging_bat_bouquet_id[i] = 0;
		messaging_bat_last_section[i] = 0;
		for ( j= 0; j < MAX_SECTIONS; j++)
			messaging_bat_sections_so_far[i][j] = 0;
	}
	unlockMessaging();
	scanType = getscanType();
	dmxSDT.start(); // -> unlock
	if (!scanning)
		dmxSDT.request_pause();

	bool startup = true;

	waitForTimeset();

	while (!sectionsd_stop) {
		while (!scanning) {
			if(sectionsd_stop)
				break;
			sleep(1);
		}
		zeit = time_monotonic();

		if(sectionsd_stop)
			break;

		readLockMessaging();
		if (messaging_zap_detected)
			startup = true;
		unlockMessaging();
		if ((zeit > lastData + TIME_SDT_NONEWDATA) || (startup))
		{
			struct timespec abs_wait;
			struct timeval now;

			gettimeofday(&now, NULL);
			TIMEVAL_TO_TIMESPEC(&now, &abs_wait);
			abs_wait.tv_sec += (TIME_SDT_SCHEDULED_PAUSE);

			dmxSDT.real_pause();
			pthread_mutex_lock( &dmxSDT.start_stop_mutex );
			/* this is the "last" thread. Means: if this one goes to sleep, sectionsd
			   sleeps mostly. Worth printing. */
			printdate_ms(stdout);printf("dmxSDT: going to sleep...\n");
			if ((auto_scanning > 0) && (!startup)) {
				if ((auto_scanning == 1) || (auto_scanning == 3)) {
					if (updateTP(scanType)) {
						eventServer->sendEvent(CSectionsdClient::EVT_SERVICES_UPDATE, CEventServer::INITID_SECTIONSD);
					}
				}
				is_new = false;
				i = 0;
				readLockMessaging();
				while ((i < MAX_BAT) && (messaging_bat_bouquet_id[i] != 0)) {
					for (j = 0; j <= messaging_bat_last_section[i]; j++) {
						if (messaging_bat_sections_so_far[i][j] == 1)
							is_new = true;
					}
					i++;
				}
				unlockMessaging();
				if (is_new) {
					if (auto_scanning == 1) {
						if (updateBouquets()) {
							eventServer->sendEvent(CSectionsdClient::EVT_BOUQUETS_UPDATE, CEventServer::INITID_SECTIONSD);
						}
					}
				}
			}

			for ( i = 0; i < MAX_SDTs; i++)
				messaging_sdt_tid[i] = 0;
			writeLockMessaging();
			for ( i = 0; i < MAX_BAT; i++) {
				messaging_bat_bouquet_id[i] = 0;
				messaging_bat_last_section[i] = 0;
				for ( j= 0; j < MAX_SECTIONS; j++)
					messaging_bat_sections_so_far[i][j] = 0;
			}
			messaging_zap_detected = false;
			unlockMessaging();

			rs = pthread_cond_timedwait( &dmxSDT.change_cond, &dmxSDT.start_stop_mutex, &abs_wait );
			pthread_mutex_unlock( &dmxSDT.start_stop_mutex );

			if(sectionsd_stop)
				break;

			if (rs == ETIMEDOUT)
			{
				dprintf("dmxSDT: waking up again - looking for new services :)\n");
				dmxSDT.change( 0 ); // -> restart
			}
			else if (rs == 0)
			{
				dprintf("dmxSDT: waking up again - requested from .change()\n");
			}
			else
			{
				dprintf("dmxSDT:  waking up again - unknown reason?!\n");
				dmxSDT.real_unpause();
			}
			// update zeit after sleep
			startup = false;
			zeit = time_monotonic();
			timeoutsDMX = 0;
			lastData = zeit;
		}

		if (timeoutsDMX >= RESTART_DMX_AFTER_TIMEOUTS && scanning)
		{
			timeoutsDMX = 0;
			dmxSDT.stop();
			dmxSDT.start(); // leaves unlocked
			dputs("\n !!! dmxSDT restarted !!!\n");
		}

		rc = dmxSDT.getSection(static_buf, timeoutInMSeconds, timeoutsDMX);

		if (rc < 0)
			continue;

		if (rc <= (int)sizeof(struct SI_section_header))
		{
			xprintf("%s rc < sizeof(SI_Section_header) (%d < %d)\n", __FUNCTION__, rc, sizeof(struct SI_section_header));
			continue;
		}

		header = (SI_section_header*)static_buf;
		unsigned short section_length = header->section_length_hi << 8 | header->section_length_lo;

		if (header->current_next_indicator)
		{
			if ((header->table_id == 0x42) || (header->table_id == 0x46))
			{
				// Wir wollen nur aktuelle sections
				SIsectionSDT sdt(section_length + 3, static_buf);

				is_actual = (header->table_id == 0x42) ? 1 : 0;

				if (is_actual && !header->last_section_number)
					is_actual = 2;

				is_new = false;
				is_actual = (is_actual | 8);

				for (SIservices::iterator s = sdt.services().begin(); s != sdt.services().end(); s++)
					if (addService(*s, is_actual)) {
						is_new = true;
						tid = CREATE_TRANSPONDER_ID_FROM_ORIGINALNETWORK_TRANSPORTSTREAM_ID(s->original_network_id,
								s->transport_stream_id);
					}

				if (is_new) {
					lastData = time_monotonic();

					dprintf("[sdtThread] added %d services [table 0x%x TID: %08x]\n",
							sdt.services().size(), header->table_id, tid);
					i = 0;
					while ((i < MAX_SDTs) && (messaging_sdt_tid[i] != 0) && (messaging_sdt_tid[i] != tid))
						i++;
					if (i < MAX_SDTs)
						messaging_sdt_tid[i] = tid;
				}
			}
			else if (header->table_id == 0x4a) {
				t_bouquet_id bid = header->table_id_extension_hi << 8 | header->table_id_extension_lo;

				bouquet_filtered = checkBouquetFilter(bid);

				if (((!bouquet_filtered) && (!bouquet_filter_is_whitelist)) ||
						((bouquet_filtered) && (bouquet_filter_is_whitelist))) {

					// This is 0 .. MAX_BAT - 1 if already started or new and free or -1 if no free slot available.
					int current_bouquet = get_bat_slot(bid, (int)header->last_section_number);

					if (current_bouquet != -1 && !messaging_bat_sections_so_far[current_bouquet][header->section_number]) {
						SIsectionBAT bat(section_length + 3, static_buf);

						dprintf("[sdtThread] adding %d bouquet entries\n", bat.bouquets().size());

						is_new = false;

						int new_bouquet_id = 0;

						//int count = 0;
						for (SIbouquets::iterator s = bat.bouquets().begin(); s != bat.bouquets().end(); s++) {
							//(*s)->position = (uint16_t) (((header.section_number & 0x1f) << 11) + (count & 0x7ff));
							new_bouquet_id = addBouquetEntry(*s/*, (int) header.section_number, count*/);
							if (new_bouquet_id & 1)
								is_new = true;
							//	bid = new_bouquet_id >> 1;
							//	count++;
						}
						//Only necessary due to BouquetID-misuse of Providers
						if (new_bouquet_id >> 1)
							messaging_bat_bouquet_id[current_bouquet] = new_bouquet_id >> 1;
						//printf("Bouquet ID: %04x\n", bid);

						writeLockMessaging();

						if (is_new) {
							lastData = time_monotonic();
							messaging_bat_sections_so_far[current_bouquet][header->section_number] = 1;
						} else
							messaging_bat_sections_so_far[current_bouquet][header->section_number] = 2;

						unlockMessaging();

						//dprintf("current bouquet: %d, current section: %d code: %d\n", current_bouquet, header.section_number, messaging_bat_sections_so_far[current_bouquet][header.section_number]);
					}
				}
			}
		} // if
	} // for

	dmxSDT.closefd();
	delete[] static_buf;
	printf("[sectionsd] sdt-thread ended\n");

	pthread_exit(NULL);
}
#endif

//---------------------------------------------------------------------
//			Time-thread
// updates system time according TOT every 30 minutes
//---------------------------------------------------------------------


/*
// BR schickt falschen Time-Offset, daher per TZ und Rest hier auskommentiert

struct descr_gen_struct {
  unsigned char descriptor_tag : 8;
  unsigned char descriptor_length : 8;
} __attribute__ ((packed)) ;

struct local_time_offset {
  char country_code1 : 8;
  char country_code2 : 8;
  char country_code3 : 8;
  unsigned char country_region_id : 6;
  unsigned char reserved : 1;
  unsigned char local_time_offset_polarity : 1;
  unsigned short local_time_offs : 16;
  uint64_t time_of_chng : 40;
  unsigned short next_time_offset : 8;
} __attribute__ ((packed)) ;

static int timeOffsetMinutes=0; // minutes
static int timeOffsetFound=0;

static void parseLocalTimeOffsetDescriptor(const char *buf, const char *countryCode)
{
  struct descr_gen_struct *desc=(struct descr_gen_struct *)buf;
  buf+=2;
  while(buf<((char *)desc)+2+desc->descriptor_length-sizeof(struct local_time_offset)) {
    struct local_time_offset *lto=(struct local_time_offset *)buf;
    if(!strncmp(countryCode, buf, 3)) {
      timeOffsetMinutes=(((lto->local_time_offs)>>12)&0x0f)*10*60L+(((lto->local_time_offs)>>8)&0x0f)*60L+
	(((lto->local_time_offs)>>4)&0x0f)*10+((lto->local_time_offs)&0x0f);
      if(lto->local_time_offset_polarity)
        timeOffsetMinutes=-timeOffsetMinutes;
      timeOffsetFound=1;
      break;
    }
//    else
//      printf("Code: %c%c%c\n", lto->country_code1, lto->country_code2, lto->country_code3);
    buf+=sizeof(struct local_time_offset);
  }
}

static void parseDescriptors(const char *des, unsigned len, const char *countryCode)
{
  struct descr_gen_struct *desc;
  while(len>=sizeof(struct descr_gen_struct)) {
    desc=(struct descr_gen_struct *)des;
    if(desc->descriptor_tag==0x58) {
//      printf("Found time descriptor\n");
      parseLocalTimeOffsetDescriptor((const char *)desc, countryCode);
      if(timeOffsetFound)
        break;
    }
    len-=desc->descriptor_length+2;
    des+=desc->descriptor_length+2;
  }
}

*/

static void *timeThread(void *)
{
	UTC_t UTC;
	time_t tim;
	unsigned int seconds;
	bool first_time = true; /* we don't sleep the first time (we try to get a TOT header) */
	struct timespec restartWait;
	struct timeval now;
	bool time_ntp = false;
	bool success = true;

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	dprintf("[%sThread] pid %d (%lu) start\n", "time", getpid(), pthread_self());

	while(!sectionsd_stop)
	{
		while (!scanning) {
			if(sectionsd_stop)
				break;
			sleep(1);
		}
		if (bTimeCorrect == true){		// sectionsd started with parameter "-tc"
			if (first_time == true) {	// only do this once!
				time_t actTime;
				actTime=time(NULL);
				pthread_mutex_lock(&timeIsSetMutex);
				timeset = true;
				pthread_cond_broadcast(&timeIsSetCond);
				pthread_mutex_unlock(&timeIsSetMutex );
				eventServer->sendEvent(CSectionsdClient::EVT_TIMESET, CEventServer::INITID_SECTIONSD, &actTime, sizeof(actTime) );
				printf("[timeThread] Time is already set by system, no further timeThread work!\n");
				break;
			}
		}

		else if ( ntpenable && system( ntp_system_cmd.c_str() ) == 0)
		{
			time_t actTime;
			actTime=time(NULL);
			first_time = false;
			pthread_mutex_lock(&timeIsSetMutex);
			timeset = true;
			time_ntp = true;
			pthread_cond_broadcast(&timeIsSetCond);
			pthread_mutex_unlock(&timeIsSetMutex );
			eventServer->sendEvent(CSectionsdClient::EVT_TIMESET, CEventServer::INITID_SECTIONSD, &actTime, sizeof(actTime) );
		} else {
			if (dvb_time_update) {
				success = getUTC(&UTC, first_time); // for first time, get TDT, then TOT
				if (success)
				{
					tim = changeUTCtoCtime((const unsigned char *) &UTC);

					if (tim) {
						if ((!messaging_neutrino_sets_time) && (geteuid() == 0)) {
							struct timeval tv;
							tv.tv_sec = tim;
							tv.tv_usec = 0;
							if (settimeofday(&tv, NULL) < 0) {
								perror("[sectionsd] settimeofday");
								pthread_exit(NULL);
							}
						}
					}

					time_t actTime;
					struct tm *tmTime;
					actTime=time(NULL);
					tmTime = localtime(&actTime);
					xprintf("[%sThread] - %02d.%02d.%04d %02d:%02d:%02d, tim: %s", "time", tmTime->tm_mday, tmTime->tm_mon+1, tmTime->tm_year+1900, tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec, ctime(&tim));
					pthread_mutex_lock(&timeIsSetMutex);
					timeset = true;
					time_ntp= false;
					pthread_cond_broadcast(&timeIsSetCond);
					pthread_mutex_unlock(&timeIsSetMutex );
					eventServer->sendEvent(CSectionsdClient::EVT_TIMESET, CEventServer::INITID_SECTIONSD, &tim, sizeof(tim));
				}
			}
		}

		if (timeset && dvb_time_update) {
			if (first_time)
				seconds = 5; /* retry a second time immediately */
			else
				seconds = ntprefresh * 60;

			if(time_ntp){
				xprintf("[%sThread] Time set via NTP, going to sleep for %d seconds.\n", "time", seconds);
			}
			else {
				xprintf("[%sThread] Time %sset via DVB(%s), going to sleep for %d seconds.\n",
					"time", success?"":"not ", first_time?"TDT":"TOT", seconds);
			}
			first_time = false;
		}
		else {
			if (!first_time){
				/* time was already set, no need to do it again soon when DVB time-blocked channel is tuned */
				seconds = ntprefresh * 60;
			}
			else if (!scanning){
				seconds = 60;
			}
			else {
				seconds = 1;
			}
			if (!dvb_time_update && !first_time) {
				xprintf("[%sThread] Time NOT set via DVB due to blocked channel, going to sleep for %d seconds.\n", "time", seconds);
			}
		}
		if(sectionsd_stop)
			break;

		gettimeofday(&now, NULL);
		TIMEVAL_TO_TIMESPEC(&now, &restartWait);
		restartWait.tv_sec += seconds;
		pthread_mutex_lock( &timeThreadSleepMutex );
		int ret = pthread_cond_timedwait( &timeThreadSleepCond, &timeThreadSleepMutex, &restartWait );
		if (ret == ETIMEDOUT)
		{
			dprintf("TDT-Thread sleeping is over - no signal received\n");
		}
		else if (ret == EINTR)
		{
			dprintf("TDT-Thread sleeping interrupted\n");
		}
		// else if (ret == 0) //everything is fine :) e.g. timeThreadSleepCond maybe signalled @zap time to get a valid time
		pthread_mutex_unlock( &timeThreadSleepMutex );
	}

	printf("[sectionsd] timeThread ended\n");
	pthread_exit(NULL);
}

static cDemux * eitDmx;
int eit_set_update_filter(int *fd)
{
        dprintf("eit_set_update_filter\n");

        unsigned char cur_eit = dmxCN.get_eit_version();
	xprintf("eit_set_update_filter, servicekey = 0x"
		PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
		", current version 0x%x got events %d\n",
		messaging_current_servicekey, cur_eit, messaging_have_CN);

        if (cur_eit == 0xff) {
                *fd = -1;
                return -1;
        }

        if(eitDmx == NULL) {
                eitDmx = new cDemux(2);
                eitDmx->Open(DMX_PSI_CHANNEL);
        }

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];
	unsigned char mode[DMX_FILTER_SIZE];
	memset(&filter, 0, DMX_FILTER_SIZE);
	memset(&mask, 0, DMX_FILTER_SIZE);
	memset(&mode, 0, DMX_FILTER_SIZE);

	filter[0] = 0x4e;   /* table_id */
	filter[1] = (unsigned char)(messaging_current_servicekey >> 8);
	filter[2] = (unsigned char)messaging_current_servicekey;

	mask[0] = 0xFF;
	mask[1] = 0xFF;
	mask[2] = 0xFF;

	int timeout = 0;
#if !HAVE_COOL_HARDWARE
	filter[3] = (cur_eit << 1) | 0x01;
	mask[3] = (0x1F << 1) | 0x01;
	mode[3] = 0x1F << 1;
	eitDmx->sectionFilter(0x12, filter, mask, 4, timeout, mode);
#else
	/* coolstream drivers broken? */
	filter[3] = (((cur_eit + 1) & 0x01) << 1) | 0x01;
	mask[3] = (0x01 << 1) | 0x01;
	eitDmx->sectionFilter(0x12, filter, mask, 4, timeout, NULL);
#endif

        //printf("[sectionsd] start EIT update filter: current version %02X, filter %02X %02X %02X %02X, mask %02X mode %02X \n", cur_eit, dsfp.filter.filter[0], dsfp.filter.filter[1], dsfp.filter.filter[2], dsfp.filter.filter[3], dsfp.filter.mask[3], dsfp.filter.mode[3]);
        *fd = 1;
        return 0;
}

int eit_stop_update_filter(int *fd)
{
printf("[sectionsd] stop eit update filter\n");
        if(eitDmx)
                eitDmx->Stop();

        *fd = -1;
        return 0;
}

#ifdef ENABLE_FREESATEPG
//---------------------------------------------------------------------
//			Freesat EIT-thread
// reads Freesat EPG-data
//---------------------------------------------------------------------
static void *fseitThread(void *)
{

	struct SI_section_header *header;
	/* we are holding the start_stop lock during this timeout, so don't
	   make it too long... */
	unsigned timeoutInMSeconds = EIT_READ_TIMEOUT;
	bool sendToSleepNow = false;
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	dmxFSEIT.addfilter(0x60, 0xfe); //other TS, scheduled, freesat epg is only broadcast using table_ids 0x60 (scheduled) and 0x61 (scheduled later)

	if (debug) {
		int policy;
		struct sched_param parm;
		int rc = pthread_getschedparam(pthread_self(), &policy, &parm);
		dprintf("freesatEitThread getschedparam: %d pol %d, prio %d\n", rc, policy, parm.sched_priority);
	}

	dprintf("[%sThread] pid %d (%lu) start\n", "fseit", getpid(), pthread_self());
	int timeoutsDMX = 0;
	char *static_buf = new char[MAX_SECTION_LENGTH];
	int rc;

	if (static_buf == NULL)
		throw std::bad_alloc();

	dmxFSEIT.start(); // -> unlock
	if (!scanning)
		dmxFSEIT.request_pause();

	waitForTimeset();
	dmxFSEIT.lastChanged = time_monotonic();

	while(!sectionsd_stop) {
		while (!scanning) {
			if(sectionsd_stop)
				break;
			sleep(1);
		}
		if(sectionsd_stop)
			break;
		time_t zeit = time_monotonic();

		rc = dmxFSEIT.getSection(static_buf, timeoutInMSeconds, timeoutsDMX);

		if (rc < 0)
			continue;

		if (timeoutsDMX < 0)
		{
			if ( dmxFSEIT.filter_index + 1 < (signed) dmxFSEIT.filters.size() )
			{
				if (timeoutsDMX == -1)
					dprintf("[freesatEitThread] skipping to next filter(%d) (> DMX_HAS_ALL_SECTIONS_SKIPPING)\n", dmxFSEIT.filter_index+1 );
				if (timeoutsDMX == -2)
					dprintf("[freesatEitThread] skipping to next filter(%d) (> DMX_HAS_ALL_CURRENT_SECTIONS_SKIPPING)\n", dmxFSEIT.filter_index+1 );
				timeoutsDMX = 0;
				dmxFSEIT.change(dmxFSEIT.filter_index + 1);
			}
			else {
				sendToSleepNow = true;
				timeoutsDMX = 0;
			}
		}

		if (timeoutsDMX >= CHECK_RESTART_DMX_AFTER_TIMEOUTS - 1)
		{
			readLockServices();
			readLockMessaging();

			MySIservicesOrderUniqueKey::iterator si = mySIservicesOrderUniqueKey.end();
			//dprintf("timeoutsDMX %x\n",currentServiceKey);

			if ( messaging_current_servicekey )
				si = mySIservicesOrderUniqueKey.find( messaging_current_servicekey );

			if (si != mySIservicesOrderUniqueKey.end())
			{
				// 1 and 3 == scheduled
				// 2 == current/next
				if ((dmxFSEIT.filter_index == 2 && !si->second->eitPresentFollowingFlag()) ||
						((dmxFSEIT.filter_index == 1 || dmxFSEIT.filter_index == 3) && !si->second->eitScheduleFlag()))
				{
					timeoutsDMX = 0;
					dprintf("[freesatEitThread] timeoutsDMX for 0x"
							PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
							" reset to 0 (not broadcast)\n", messaging_current_servicekey );

					dprintf("New Filterindex: %d (ges. %d)\n", dmxFSEIT.filter_index + 1, (signed) dmxFSEIT.filters.size() );
					dmxFSEIT.change( dmxFSEIT.filter_index + 1 );
				}
				else
					if (dmxFSEIT.filter_index >= 1)
					{
						if (dmxFSEIT.filter_index + 1 < (signed) dmxFSEIT.filters.size() )
						{
							dprintf("New Filterindex: %d (ges. %d)\n", dmxFSEIT.filter_index + 1, (signed) dmxFSEIT.filters.size() );
							dmxFSEIT.change(dmxFSEIT.filter_index + 1);
							//dprintf("[eitThread] timeoutsDMX for 0x%x reset to 0 (skipping to next filter)\n" );
							timeoutsDMX = 0;
						}
						else
						{
							sendToSleepNow = true;
							dputs("sendToSleepNow = true");
						}
					}
			}
			unlockMessaging();
			unlockServices();
		}

		if (timeoutsDMX >= CHECK_RESTART_DMX_AFTER_TIMEOUTS && scanning)
		{
			if ( dmxFSEIT.filter_index + 1 < (signed) dmxFSEIT.filters.size() )
			{
				dprintf("[freesatEitThread] skipping to next filter(%d) (> DMX_TIMEOUT_SKIPPING)\n", dmxFSEIT.filter_index+1 );
				dmxFSEIT.change(dmxFSEIT.filter_index + 1);
			}
			else
				sendToSleepNow = true;

			timeoutsDMX = 0;
		}

		if (sendToSleepNow)
		{
			sendToSleepNow = false;

			if(sectionsd_stop)
				break;
			dmxFSEIT.real_pause();
			pthread_mutex_lock( &dmxFSEIT.start_stop_mutex );
			writeLockMessaging();
			messaging_zap_detected = false;
			unlockMessaging();

			if (auto_scanning) {
				pthread_mutex_unlock( &dmxNIT.start_stop_mutex );
				dmxNIT.change( 0 );
			}

			struct timespec abs_wait;
			struct timeval now;
			gettimeofday(&now, NULL);
			TIMEVAL_TO_TIMESPEC(&now, &abs_wait);
			abs_wait.tv_sec += TIME_EIT_SCHEDULED_PAUSE;
			dprintf("dmxFSEIT: going to sleep for %d seconds...\n", TIME_EIT_SCHEDULED_PAUSE);

			int rs = pthread_cond_timedwait( &dmxFSEIT.change_cond, &dmxFSEIT.start_stop_mutex, &abs_wait );

			pthread_mutex_unlock( &dmxFSEIT.start_stop_mutex );

			if (rs == ETIMEDOUT)
			{
				dprintf("dmxFSEIT: waking up again - timed out\n");
				// must call dmxFSEIT.change after! unpause otherwise dev is not open,
				// dmxFSEIT.lastChanged will not be set, and filter is advanced the next iteration
				// maybe .change should imply .real_unpause()? -- seife
				dprintf("New Filterindex: %d (ges. %d)\n", 2, (signed) dmxFSEIT.filters.size() );
				dmxFSEIT.change(1); // -> restart
			}
			else if (rs == 0)
			{
				dprintf("dmxFSEIT: waking up again - requested from .change()\n");
			}
			else
			{
				dprintf("dmxFSEIT:  waking up again - unknown reason %d\n",rs);
			}
			// update zeit after sleep
			zeit = time_monotonic();
		}
		else if (zeit > dmxFSEIT.lastChanged + TIME_FSEIT_SKIPPING )
		{
			readLockMessaging();

			if ( dmxFSEIT.filter_index + 1 < (signed) dmxFSEIT.filters.size() )
			{
				dprintf("[freesatEitThread] skipping to next filter(%d) (> TIME_FSEIT_SKIPPING)\n", dmxFSEIT.filter_index+1 );
				dmxFSEIT.change(dmxFSEIT.filter_index + 1);
			}
			else
				sendToSleepNow = true;

			unlockMessaging();
		}

		if (rc <= (int)sizeof(struct SI_section_header))
		{
			xprintf("%s rc < sizeof(SI_Section_header) (%d < %d)\n", __FUNCTION__, rc, sizeof(struct SI_section_header));
			continue;
		}

		header = (SI_section_header*)static_buf;
		unsigned short section_length = header->section_length_hi << 8 | header->section_length_lo;



		if ((header->current_next_indicator) && (!dmxFSEIT.real_pauseCounter ))
		{
			// Wir wollen nur aktuelle sections

			// Houdini: added new constructor where the buffer is given as a parameter and must be allocated outside
			// -> no allocation and copy of data into a 2nd buffer
			//				SIsectionEIT eit(SIsection(section_length + 3, buf));
			SIsectionEIT eit(section_length + 3, static_buf);
			// Houdini: if section is not parsed (too short) -> no need to check events
			if (eit.is_parsed() && eit.header())
			{
				// == 0 -> kein event

				//dprintf("[eitThread] adding %d events [table 0x%x] (begin)\n", eit.events().size(), header.table_id);
				zeit = time(NULL);
				// Nicht alle Events speichern
				for (SIevents::iterator e = eit.events().begin(); e != eit.events().end(); e++)
				{
					if (!(e->times.empty()))
					{
						if ( ( e->times.begin()->startzeit < zeit + secondsToCache ) &&
								( ( e->times.begin()->startzeit + (long)e->times.begin()->dauer ) > zeit - oldEventsAre ) )
						{
							//fprintf(stderr, "%02x ", header.table_id);
							addEvent(*e, header->table_id, zeit);
						}
					}
					else
					{
						// pruefen ob nvod event
						readLockServices();
						MySIservicesNVODorderUniqueKey::iterator si = mySIservicesNVODorderUniqueKey.find(e->get_channel_id());

						if (si != mySIservicesNVODorderUniqueKey.end())
						{
							// Ist ein nvod-event
							writeLockEvents();

							for (SInvodReferences::iterator i = si->second->nvods.begin(); i != si->second->nvods.end(); i++)
								mySIeventUniqueKeysMetaOrderServiceUniqueKey.insert(std::make_pair(i->uniqueKey(), e->uniqueKey()));

							unlockEvents();
							addNVODevent(*e);
						}
						unlockServices();
					}
				} // for
				//dprintf("[eitThread] added %d events (end)\n",  eit.events().size());
			} // if
		} // if
		else
		{
			delete[] static_buf;

			//dprintf("[eitThread] skipped sections for table 0x%x\n", header.table_id);
		}
	} // for
	dputs("[freesatEitThread] end");

	pthread_exit(NULL);
}
#endif

//---------------------------------------------------------------------
//			EIT-thread
// reads EPG-datas
//---------------------------------------------------------------------
static void *eitThread(void *)
{

	struct SI_section_header *header;
	/* we are holding the start_stop lock during this timeout, so don't
	   make it too long... */
	unsigned timeoutInMSeconds = EIT_READ_TIMEOUT;
	bool sendToSleepNow = false;

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	/* These filters are a bit tricky (index numbers):
	   - 0   Dummy filter, to make this thread sleep for some seconds
	   - 1   then get other TS's current/next (this TS's cur/next are
	   handled in dmxCN)
	   - 2/3 then get scheduled events on this TS
	   - 4   then get the other TS's scheduled events,
	   - 4ab (in two steps to reduce the POLLERRs on the DMX device)
	   */
	// -- set EIT filter  0x4e-0x6F
	dmxEIT.addfilter(0x00, 0x00); //0 dummy filter
	dmxEIT.addfilter(0x50, 0xf0); //1  current TS, scheduled
	dmxEIT.addfilter(0x4f, 0xff); //2  other TS, current/next
#if 1
	dmxEIT.addfilter(0x60, 0xf1); //3a other TS, scheduled, even
	dmxEIT.addfilter(0x61, 0xf1); //3b other TS, scheduled, odd
#else
	dmxEIT.addfilter(0x60, 0xf0); //3  other TS, scheduled
#endif

	if (sections_debug) {
		int policy;
		struct sched_param parm;
		int rc = pthread_getschedparam(pthread_self(), &policy, &parm);
		dprintf("eitThread getschedparam: %d pol %d, prio %d\n", rc, policy, parm.sched_priority);
	}
	dprintf("[%sThread] pid %d (%lu) start\n", "eit", getpid(), pthread_self());
	int timeoutsDMX = 0;
	char *static_buf = new char[MAX_SECTION_LENGTH];
	int rc;

	if (static_buf == NULL)
	{
		xprintf("%s: could not allocate static_buf\n", __FUNCTION__);
		pthread_exit(NULL);
		//throw std::bad_alloc();
	}

	dmxEIT.start(); // -> unlock
	if (!scanning)
		dmxEIT.request_pause();

	waitForTimeset();
	dmxEIT.lastChanged = time_monotonic();

	while (!sectionsd_stop) {
		while (!scanning) {
			if(sectionsd_stop)
				break;
			sleep(1);
		}
		if(sectionsd_stop)
			break;
		time_t zeit = time_monotonic();

		rc = dmxEIT.getSection(static_buf, timeoutInMSeconds, timeoutsDMX);
		if(sectionsd_stop)
			break;
#if 0
		/* comment out for now - will be removed later -- seife */
		if (update_eit) {
			writeLockMessaging();
			messaging_current_version_number = dmxEIT.get_eit_version();
			unlockMessaging();

			readLockMessaging();
			if ((dmxEIT.filter_index == (signed)dmxEIT.filters.size() - 1) &&
					(messaging_current_version_number == 0xff))
			{
				if (!wait_for_eit_version) {
					dprintf("waiting for eit_version...\n");
					eit_waiting_since = zeit;
				}
				if (zeit - eit_waiting_since > TIME_EIT_VERSION_WAIT) {
					dprintf("waiting for more than %d seconds - bail out...\n", TIME_EIT_VERSION_WAIT);
					wait_for_eit_version = false;
					sendToSleepNow = true;
				} else if (timeoutsDMX >= TIMEOUTS_EIT_VERSION_WAIT) {
					dprintf("more than %d EIT DMX timeouts - bail out...\n", TIMEOUTS_EIT_VERSION_WAIT);
					wait_for_eit_version = false;
					sendToSleepNow = true;
				} else
					wait_for_eit_version = true;
			} else
				wait_for_eit_version = false;

			unlockMessaging();

			if (wait_for_eit_version)
			{
				sched_yield();
				continue;
			}
		} // if (update_eit)
#endif

		if (timeoutsDMX < 0 && !channel_is_blacklisted)
		{
#if 0
			writeLockMessaging();
			if (update_eit){
				if (dmxEIT.filters[dmxEIT.filter_index].filter == 0x4e) {
					dprintf("[eitThread] got all current_next - sending event!\n");
					messaging_wants_current_next_Event = false;
					eventServer->sendEvent(CSectionsdClient::EVT_GOT_CN_EPG, CEventServer::INITID_SECTIONSD, &messaging_current_servicekey, sizeof(messaging_current_servicekey) );
				}
			} else {
				if ( messaging_wants_current_next_Event )
				{
					dprintf("[eitThread] got all current_next - sending event!\n");
					messaging_wants_current_next_Event = false;
					eventServer->sendEvent(CSectionsdClient::EVT_GOT_CN_EPG, CEventServer::INITID_SECTIONSD, &messaging_current_servicekey, sizeof(messaging_current_servicekey) );
				}
			}
			unlockMessaging();
#endif

			if (timeoutsDMX == -1)
				dprintf("[eitThread] skipping to next filter(%d) (> DMX_HAS_ALL_SECTIONS_SKIPPING)\n", dmxEIT.filter_index+1 );
			else if (timeoutsDMX == -2)
				dprintf("[eitThread] skipping to next filter(%d) (> DMX_HAS_ALL_CURRENT_SECTIONS_SKIPPING)\n", dmxEIT.filter_index+1 );
			else
				dprintf("[eitThread] skipping to next filter(%d) (timeouts %d)\n", dmxEIT.filter_index+1, timeoutsDMX);
			if ( dmxEIT.filter_index + 1 < (signed) dmxEIT.filters.size() )
			{
				timeoutsDMX = 0;
				dmxEIT.change(dmxEIT.filter_index + 1);
			}
			else {
				sendToSleepNow = true;
				timeoutsDMX = 0;
			}
		}

		if (timeoutsDMX >= CHECK_RESTART_DMX_AFTER_TIMEOUTS - 1 && !channel_is_blacklisted)
		{
			readLockServices();
			MySIservicesOrderUniqueKey::iterator si = mySIservicesOrderUniqueKey.end();
			//dprintf("timeoutsDMX %x\n",currentServiceKey);

			if ( messaging_current_servicekey )
				si = mySIservicesOrderUniqueKey.find( messaging_current_servicekey );

			if (si != mySIservicesOrderUniqueKey.end())
			{
				/* I'm not 100% sure what this is good for... */
				// 1 == scheduled
				// 2 == current/next
				if ((dmxEIT.filter_index == 2 && !si->second->eitPresentFollowingFlag()) ||
						(dmxEIT.filter_index == 1 && !si->second->eitScheduleFlag()))
				{
					timeoutsDMX = 0;
					dprintf("[eitThread] timeoutsDMX for 0x"
							PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
							" reset to 0 (not broadcast)\n", messaging_current_servicekey );

					dprintf("New Filterindex: %d (ges. %d)\n", dmxEIT.filter_index + 1, (signed) dmxEIT.filters.size() );
					dmxEIT.change( dmxEIT.filter_index + 1 );
				}
				else
					if (dmxEIT.filter_index >= 1)
					{
						if (dmxEIT.filter_index + 1 < (signed) dmxEIT.filters.size() )
						{
							dprintf("[eitThread] New Filterindex: %d (ges. %d)\n", dmxEIT.filter_index + 1, (signed) dmxEIT.filters.size() );
							dmxEIT.change(dmxEIT.filter_index + 1);
							//dprintf("[eitThread] timeoutsDMX for 0x%x reset to 0 (skipping to next filter)\n" );
							timeoutsDMX = 0;
						}
						else
						{
							sendToSleepNow = true;
							dputs("sendToSleepNow = true");
						}
					}
			}
			unlockServices();
		}

		if (timeoutsDMX >= CHECK_RESTART_DMX_AFTER_TIMEOUTS && scanning && !channel_is_blacklisted)
		{
			dprintf("[eitThread] skipping to next filter(%d) (> DMX_TIMEOUT_SKIPPING %d)\n", dmxEIT.filter_index+1, timeoutsDMX);
			if ( dmxEIT.filter_index + 1 < (signed) dmxEIT.filters.size() )
			{
				dmxEIT.change(dmxEIT.filter_index + 1);
			}
			else
				sendToSleepNow = true;

			timeoutsDMX = 0;
		}

		if (sendToSleepNow || channel_is_blacklisted)
		{
			sendToSleepNow = false;

			dmxEIT.real_pause();
			writeLockMessaging();
			messaging_zap_detected = false;
			unlockMessaging();
#ifdef UPDATE_NETWORKS
			if (auto_scanning) {
				pthread_mutex_unlock( &dmxNIT.start_stop_mutex );
				dmxNIT.change( 0 );
			}
#endif
			int rs = 0;
			do {
				struct timespec abs_wait;
				struct timeval now;
				gettimeofday(&now, NULL);
				TIMEVAL_TO_TIMESPEC(&now, &abs_wait);
				abs_wait.tv_sec += TIME_EIT_SCHEDULED_PAUSE;
				dprintf("dmxEIT: going to sleep for %d seconds...\n", TIME_EIT_SCHEDULED_PAUSE);
				if(sectionsd_stop)
					break;

				pthread_mutex_lock( &dmxEIT.start_stop_mutex );
				rs = pthread_cond_timedwait( &dmxEIT.change_cond, &dmxEIT.start_stop_mutex, &abs_wait );
				pthread_mutex_unlock( &dmxEIT.start_stop_mutex );
			} while (channel_is_blacklisted);

			if (rs == ETIMEDOUT)
			{
				dprintf("dmxEIT: waking up again - timed out\n");
				dprintf("New Filterindex: %d (ges. %d)\n", 2, (signed) dmxEIT.filters.size() );
				dmxEIT.change(1); // -> restart
			}
			else if (rs == 0)
			{
				dprintf("dmxEIT: waking up again - requested from .change()\n");
			}
			else
			{
				dprintf("dmxEIT:  waking up again - unknown reason %d\n",rs);
				dmxEIT.real_unpause();
			}
			// update zeit after sleep
			zeit = time_monotonic();
		}
		else if (zeit > dmxEIT.lastChanged + TIME_EIT_SKIPPING )
		{
			readLockMessaging();

			dprintf("[eitThread] skipping to next filter(%d) (> TIME_EIT_SKIPPING)\n", dmxEIT.filter_index+1 );
			if ( dmxEIT.filter_index + 1 < (signed) dmxEIT.filters.size() )
			{
				dmxEIT.change(dmxEIT.filter_index + 1);
			}
			else
				sendToSleepNow = true;

			unlockMessaging();
		}

		if (rc < 0)
			continue;

		if (rc < (int)sizeof(struct SI_section_header))
		{
			xprintf("%s rc < sizeof(SI_Section_header) (%d < %d)\n", __FUNCTION__, rc, sizeof(struct SI_section_header));
			continue;
		}

		header = (SI_section_header*)static_buf;
		unsigned short section_length = header->section_length_hi << 8 | header->section_length_lo;

		if(sectionsd_stop)
			break;
		if (header->current_next_indicator)
		{
			// Wir wollen nur aktuelle sections

			// Houdini: added new constructor where the buffer is given as a parameter and must be allocated outside
			// -> no allocation and copy of data into a 2nd buffer
			//				SIsectionEIT eit(SIsection(section_length + 3, buf));
			SIsectionEIT eit(section_length + 3, static_buf);
			// Houdini: if section is not parsed (too short) -> no need to check events
			if (eit.is_parsed() && eit.header())
			{
				// == 0 -> kein event

				/*					dprintf("[eitThread] adding %d events [table 0x%x] (begin)\n", eit.events().size(), header.table_id);*/
				zeit = time(NULL);
				// Nicht alle Events speichern
				for (SIevents::iterator e = eit.events().begin(); e != eit.events().end(); e++)
				{
					if (!(e->times.empty()))
					{
						if ( ( e->times.begin()->startzeit < zeit + secondsToCache ) &&
								( ( e->times.begin()->startzeit + (long)e->times.begin()->dauer ) > zeit - oldEventsAre ) )
						{
							//fprintf(stderr, "%02x ", header.table_id);
							if(sectionsd_stop)
								break;
//printf("Adding event 0x%llx table %x version %x running %d\n", e->uniqueKey(), header->table_id, header->version_number, e->runningStatus());
							addEvent(*e, header->table_id, zeit);
						}
					}
					else
					{
						// pruefen ob nvod event
						readLockServices();
						MySIservicesNVODorderUniqueKey::iterator si = mySIservicesNVODorderUniqueKey.find(e->get_channel_id());

						if (si != mySIservicesNVODorderUniqueKey.end())
						{
							// Ist ein nvod-event
							writeLockEvents();

							for (SInvodReferences::iterator i = si->second->nvods.begin(); i != si->second->nvods.end(); i++)
								mySIeventUniqueKeysMetaOrderServiceUniqueKey.insert(std::make_pair(i->uniqueKey(), e->uniqueKey()));

							unlockEvents();
							addNVODevent(*e);
						}
						unlockServices();
					}
				} // for
				//dprintf("[eitThread] added %d events (end)\n",  eit.events().size());
			} // if
		} // if
		else
		{
			dprintf("[eitThread] skipped sections for table 0x%x\n", header->table_id);
		}
	} // for
	delete[] static_buf;

	printf("[sectionsd] eitThread ended\n");

	pthread_exit(NULL);
}

//---------------------------------------------------------------------
// CN-thread: eit thread, but only current/next
//---------------------------------------------------------------------
static void *cnThread(void *)
{

	struct SI_section_header *header;
	/* we are holding the start_stop lock during this timeout, so don't
	   make it too long... */
	unsigned timeoutInMSeconds = EIT_READ_TIMEOUT;
	bool sendToSleepNow = false;
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	// -- set EIT filter  0x4e
	dmxCN.addfilter(0x4e, 0xff); //0  current TS, current/next

	dprintf("[%sThread] pid %d (%lu) start\n", "cn", getpid(), pthread_self());
	t_channel_id time_trigger_last = 0;
	int timeoutsDMX = 0;
	char *static_buf = new char[MAX_SECTION_LENGTH];
	int rc;

	if (static_buf == NULL)
	{
		xprintf("%s: could not allocate static_buf\n", __FUNCTION__);
		pthread_exit(NULL);
		//throw std::bad_alloc();
	}

	dmxCN.start(); // -> unlock
	if (!scanning)
		dmxCN.request_pause();

	writeLockMessaging();
	messaging_eit_is_busy = true;
	messaging_need_eit_version = false;
	unlockMessaging();

	waitForTimeset();

	time_t eit_waiting_since = time_monotonic();
	dmxCN.lastChanged = eit_waiting_since;

	while(!sectionsd_stop)
	{
		while (!scanning) {
			sleep(1);
			if(sectionsd_stop)
				break;
		}
		if(sectionsd_stop)
			break;

		rc = dmxCN.getSection(static_buf, timeoutInMSeconds, timeoutsDMX);
		time_t zeit = time_monotonic();
		if (update_eit) {
			if (dmxCN.get_eit_version() != 0xff) {
				writeLockMessaging();
				messaging_need_eit_version = false;
				unlockMessaging();
			} else {
				readLockMessaging();
				if (!messaging_need_eit_version) {
					unlockMessaging();
					dprintf("waiting for eit_version...\n");
					zeit = time_monotonic();  /* reset so that we don't get negative */
					eit_waiting_since = zeit; /* and still compensate for getSection */
					dmxCN.lastChanged = zeit; /* this is ugly - needs somehting better */
					sendToSleepNow = false;   /* reset after channel change */
					writeLockMessaging();
					messaging_need_eit_version = true;
				}
				unlockMessaging();
				if (zeit - eit_waiting_since > TIME_EIT_VERSION_WAIT) {
					dprintf("waiting for more than %d seconds - bail out...\n", TIME_EIT_VERSION_WAIT);
					/* send event anyway, so that we know there is no EPG */
					eventServer->sendEvent(CSectionsdClient::EVT_GOT_CN_EPG,
						CEventServer::INITID_SECTIONSD,
						&messaging_current_servicekey,
						sizeof(messaging_current_servicekey));
					writeLockMessaging();
					messaging_need_eit_version = false;
					unlockMessaging();
					sendToSleepNow = true;
				}
#if 0 // 2009-09-14
				else if (timeoutsDMX >= TIMEOUTS_EIT_VERSION_WAIT) {
					dprintf("more than %d EIT DMX timeouts - bail out...\n", TIMEOUTS_EIT_VERSION_WAIT);
					writeLockMessaging();
					messaging_need_eit_version = false;
					unlockMessaging();
					sendToSleepNow = true;
				}
#endif
			}

		} // if (update_eit)

		readLockMessaging();
		if (messaging_got_CN != messaging_have_CN) // timeoutsDMX < -1)
		{
			unlockMessaging();
			writeLockMessaging();
			messaging_have_CN = messaging_got_CN;
			unlockMessaging();
			dprintf("[cnThread] got current_next (0x%x) - sending event!\n", messaging_have_CN);
			eventServer->sendEvent(CSectionsdClient::EVT_GOT_CN_EPG,
					CEventServer::INITID_SECTIONSD,
					&messaging_current_servicekey,
					sizeof(messaging_current_servicekey));
			/* we received an event => reset timeout timer... */
			eit_waiting_since = zeit;
			dmxCN.lastChanged = zeit; /* this is ugly - needs somehting better */
			readLockMessaging();
		}
		if (messaging_have_CN == 0x03) // current + next
		{
			unlockMessaging();
			sendToSleepNow = true;
			//timeoutsDMX = 0;
		}
		else {
			unlockMessaging();
		}

#if 0
		if (timeoutsDMX >= CHECK_RESTART_DMX_AFTER_TIMEOUTS - 1)
		{
			fprintf(stderr, "!!!timeoutsDMX >= CHECK_RESTART_DMX_AFTER_TIMEOUTS!!!!!!\n");
			readLockServices();
			readLockMessaging();

			MySIservicesOrderUniqueKey::iterator si = mySIservicesOrderUniqueKey.end();
			//dprintf("timeoutsDMX %x\n",currentServiceKey);

			if (messaging_current_servicekey)
				si = mySIservicesOrderUniqueKey.find(messaging_current_servicekey);

			if (si != mySIservicesOrderUniqueKey.end())
			{
				if (!si->second->eitPresentFollowingFlag())
				{
					timeoutsDMX = 0;
					printf("[cnThread] timeoutsDMX for 0x"
							PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
							" reset to 0 (not broadcast)\n", messaging_current_servicekey );
				}
				else
				{
					sendToSleepNow = true;
					dputs("sendToSleepNow = true");
				}
			}
			unlockMessaging();
			unlockServices();
		}
		else {
#endif
#if 0 // 2009-09-14
			if (timeoutsDMX > 0)
				usleep(timeoutsDMX*1000);
			//			}

		if (timeoutsDMX >= CHECK_RESTART_DMX_AFTER_TIMEOUTS && scanning)
		{
			dprintf("timeoutsDMX (%d) >= CHECK_RESTART_DMX_AFTER_TIMEOUTS (%d) && scanning => sendtosleepnow=true\n",
					timeoutsDMX, CHECK_RESTART_DMX_AFTER_TIMEOUTS);
			sendToSleepNow = true;
			timeoutsDMX = 0;
		}
#endif
		if ((sendToSleepNow && !messaging_need_eit_version) || channel_is_blacklisted)
		{
			sendToSleepNow = false;

			dmxCN.real_pause();
			dprintf("dmxCN: going to sleep...\n");

			writeLockMessaging();
			messaging_eit_is_busy = false;
			unlockMessaging();

			/* re-fetch time if transponder changed
			   Why I'm doing this here and not from commandserviceChanged?
			   commandserviceChanged is called on zap *start*, not after zap finished
			   this would lead to often actually fetching the time on the transponder
			   you are switching away from, not the one you are switching onto.
			   Doing it here at least gives us a good chance to have actually tuned
			   to the channel we want to get the time from...
			 */
			if (time_trigger_last != (messaging_current_servicekey & 0xFFFFFFFF0000ULL))
			{
				time_trigger_last = messaging_current_servicekey & 0xFFFFFFFF0000ULL;
				pthread_mutex_lock(&timeThreadSleepMutex);
				pthread_cond_broadcast(&timeThreadSleepCond);
				pthread_mutex_unlock(&timeThreadSleepMutex);
			}

			int rs;
			do {
				pthread_mutex_lock( &dmxCN.start_stop_mutex );
				if (!channel_is_blacklisted)
					eit_set_update_filter(&eit_update_fd);
				rs = pthread_cond_wait(&dmxCN.change_cond, &dmxCN.start_stop_mutex);
				eit_stop_update_filter(&eit_update_fd);
				pthread_mutex_unlock(&dmxCN.start_stop_mutex);
			} while (channel_is_blacklisted);

			writeLockMessaging();
			messaging_need_eit_version = false;
			messaging_eit_is_busy = true;
			unlockMessaging();

			if (rs == 0)
			{
				dprintf("dmxCN: waking up again - requested from .change()\n");
				// fix EPG problems on IPBox
				// http://tuxbox-forum.dreambox-fan.de/forum/viewtopic.php?p=367937#p367937
#if HAVE_IPBOX_HARDWARE
				dmxCN.change(0);
#endif
			}
			else
			{
				printf("dmxCN:  waking up again - unknown reason %d\n",rs);
				dmxCN.real_unpause();
			}
			zeit = time_monotonic();
		}
		else if (zeit > dmxCN.lastChanged + TIME_EIT_VERSION_WAIT && !messaging_need_eit_version)
		{
			xprintf("zeit > dmxCN.lastChanged + TIME_EIT_VERSION_WAIT\n");
			sendToSleepNow = true;
			/* we can get here if we got the EIT version but no events */
			/* send a "no epg" event anyway before going to sleep */
			if (messaging_have_CN == 0x00)
				eventServer->sendEvent(CSectionsdClient::EVT_GOT_CN_EPG,
						CEventServer::INITID_SECTIONSD,
						&messaging_current_servicekey,
						sizeof(messaging_current_servicekey));
			continue;
		}

		if (rc < 0)
			continue;

		if (rc < (int)sizeof(struct SI_section_header))
		{
			xprintf("%s: rc < sizeof(SI_Section_header) (%d < %d)\n", __FUNCTION__, rc, sizeof(struct SI_section_header));
			continue;
		}

		header = (SI_section_header *)static_buf;
		unsigned short section_length = (header->section_length_hi << 8) | header->section_length_lo;
		unsigned table_id = header->table_id;

		if (!header->current_next_indicator)
		{
			// Wir wollen nur aktuelle sections
			//dprintf("[cnThread] skipped sections for table 0x%x\n", table_id);
			continue;
		}

		SIsectionEIT eit(section_length + 3, static_buf);
		// Houdini: if section is not parsed (too short) -> no need to check events
		if (!eit.is_parsed() || !eit.header())
			continue;

		// == 0 -> kein event
		//dprintf("[cnThread] adding %d events [table 0x%x] (begin)\n", eit.events().size(), table_id);
		zeit = time(NULL);
		// Nicht alle Events speichern
		for (SIevents::iterator e = eit.events().begin(); e != eit.events().end(); e++)
		{
			if (!(e->times.empty()))
			{
				addEvent(*e, table_id, zeit, true); /* cn = true => fill in current / next event */
			}
#if 0
			/* I don't think there are NVOD events in CN tables, so we can skip that */
			else
			{
				/* this actually happens sometimes... */
				dprintf("e->times.empty in CN-THREAD!!\n");
				// pruefen ob nvod event
				readLockServices();
				MySIservicesNVODorderUniqueKey::iterator si = mySIservicesNVODorderUniqueKey.find(e->get_channel_id());
				if (si != mySIservicesNVODorderUniqueKey.end())
				{
					/* if this never happens in reality, we can remove the whole "else" clause here */
					printdate_ms(stderr); fprintf(stderr, "NVOD-EVENT in CN-THREAD!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
					// Ist ein nvod-event
					writeLockEvents();
					for (SInvodReferences::iterator i = si->second->nvods.begin(); i != si->second->nvods.end(); i++)
						mySIeventUniqueKeysMetaOrderServiceUniqueKey.insert(std::make_pair(i->uniqueKey(), e->uniqueKey()));
					unlockEvents();
					addNVODevent(*e);
				}
				unlockServices();
			}
#endif
		} // for
		//dprintf("[cnThread] added %d events (end)\n",  eit.events().size());
	} // for
	delete[] static_buf;

	printf("[sectionsd] cnThread ended\n");

	pthread_exit(NULL);
}

#ifdef ENABLE_PPT

//---------------------------------------------------------------------
// Premiere Private EPG Thread
// reads EPG-datas
//---------------------------------------------------------------------

static void *pptThread(void *)
{
	struct SI_section_header *header;
	unsigned timeoutInMSeconds = EIT_READ_TIMEOUT;
	bool sendToSleepNow = false;
	unsigned short start_section = 0;
	unsigned short pptpid=0;
	long first_content_id = 0;
	long previous_content_id = 0;
	long current_content_id = 0;
	bool already_exists = false;

	//	dmxPPT.addfilter( 0xa0, (0xff - 0x01) );
	dmxPPT.addfilter( 0xa0, (0xff));
	dprintf("[%sThread] pid %d (%lu) start\n", "ppt", getpid(), pthread_self());
	int timeoutsDMX = 0;
	char *static_buf = new char[MAX_SECTION_LENGTH];
	int rc;

	if (static_buf == NULL)
		throw std::bad_alloc();

	time_t lastRestarted = time_monotonic();
	time_t lastData = time_monotonic();

	dmxPPT.start(); // -> unlock
	if (!scanning)
		dmxPPT.request_pause();

	waitForTimeset();
	dmxPPT.lastChanged = time_monotonic();

	while (!sectionsd_stop) {
		time_t zeit = time_monotonic();

		if (timeoutsDMX >= CHECK_RESTART_DMX_AFTER_TIMEOUTS && scanning && !channel_is_blacklisted)
		{
			if (zeit > lastRestarted + 3) // last restart older than 3secs, therefore do NOT decrease cache
			{
				dmxPPT.stop(); // -> lock
				dmxPPT.start(); // -> unlock
				dprintf("[pptThread] dmxPPT restarted, cache NOT decreased (dt=%ld)\n", (int)zeit - lastRestarted);
			}
			else
			{

				// sectionsd ist zu langsam, da zu viele events -> cache kleiner machen
				dmxPPT.stop(); // -> lock
				/*                    lockEvents();
						      if(secondsToCache>24*60L*60L && mySIeventsOrderUniqueKey.size()>3000)
						      {
				// kleiner als 1 Tag machen wir den Cache nicht,
				// da die timeouts ja auch von einem Sender ohne EPG kommen koennen
				// Die 3000 sind ne Annahme und beruhen auf (wenigen) Erfahrungswerten
				// Man koennte auch ab 3000 Events nur noch jedes 3 Event o.ae. einsortieren
				dmxSDT.real_pause();
				lockServices();
				unsigned anzEventsAlt=mySIeventsOrderUniqueKey.size();
				secondsToCache-=5*60L*60L; // 5h weniger
				dprintf("[eitThread] decreasing cache 5h (now %ldh)\n", secondsToCache/(60*60L));
				removeNewEvents();
				removeOldEvents(oldEventsAre);
				if(anzEventsAlt>mySIeventsOrderUniqueKey.size())
				dprintf("[eitThread] Removed %u Events (%u -> %u)\n", anzEventsAlt-mySIeventsOrderUniqueKey.size(), anzEventsAlt, mySIeventsOrderUniqueKey.size());
				unlockServices();
				dmxSDT.real_unpause();
				}
				unlockEvents();
				*/
				dmxPPT.start(); // -> unlock
				//					dputs("[pptThread] dmxPPT restarted");

			}

			lastRestarted = zeit;
			timeoutsDMX = 0;
			lastData = zeit;
		}

		if (sendToSleepNow || !scanning || channel_is_blacklisted)
		{
			sendToSleepNow = false;

			dmxPPT.real_pause();

			int rs;
			do {
				pthread_mutex_lock( &dmxPPT.start_stop_mutex );

				if (0 != privatePid)
				{
					struct timespec abs_wait;
					struct timeval now;

					gettimeofday(&now, NULL);
					TIMEVAL_TO_TIMESPEC(&now, &abs_wait);
					abs_wait.tv_sec += (TIME_EIT_SCHEDULED_PAUSE);
					dprintf("[pptThread] going to sleep for %d seconds...\n", TIME_EIT_SCHEDULED_PAUSE);
					rs = pthread_cond_timedwait(&dmxPPT.change_cond, &dmxPPT.start_stop_mutex, &abs_wait);
				}
				else
				{
					dprintf("[pptThread] going to sleep until wakeup...\n");
					rs = pthread_cond_wait(&dmxPPT.change_cond, &dmxPPT.start_stop_mutex);
				}

				pthread_mutex_unlock( &dmxPPT.start_stop_mutex );
			} while (channel_is_blacklisted);

			if (rs == ETIMEDOUT)
			{
				dprintf("dmxPPT: waking up again - looking for new events :)\n");
				if (0 != privatePid)
				{
					dmxPPT.change( 0 ); // -> restart
				}
			}
			else if (rs == 0)
			{
				dprintf("dmxPPT: waking up again - requested from .change()\n");
			}
			else
			{
				dprintf("dmxPPT: waking up again - unknown reason?!\n");
				dmxPPT.real_unpause();
			}
			// after sleeping get current time
			zeit = time_monotonic();
			start_section = 0; // fetch new? events
			lastData = zeit; // restart timer
			first_content_id = 0;
			previous_content_id = 0;
			current_content_id = 0;
		}

		if (0 == privatePid)
		{
			sendToSleepNow = true; // if there is no valid pid -> sleep
			dprintf("dmxPPT: no valid pid 0\n");
			sleep(1);
			continue;
		}

		if (!scanning)
			continue; // go to sleep again...

		if (pptpid != privatePid)
		{
			pptpid = privatePid;
			dprintf("Setting PrivatePid %x\n", pptpid);
			dmxPPT.setPid(pptpid);
		}

		rc = dmxPPT.getSection(static_buf, timeoutInMSeconds, timeoutsDMX);

		if (rc < 0) {
			if (zeit > lastData + 5)
			{
				sendToSleepNow = true; // if there are no data for 5 seconds -> sleep
				dprintf("dmxPPT: no data for 5 seconds\n");
			}
			continue;
		}

		if (rc < (int)sizeof(struct SI_section_header))
		{
			xprintf("%s: ret < sizeof(SI_Section_header) (%d < %d)\n", __FUNCTION__, rc, sizeof(struct SI_section_header));
			continue;
		}

		lastData = zeit;

		header = (SI_section_header*)static_buf;
		unsigned short section_length = header->section_length_hi << 8 | header->section_length_lo;

		if (header->current_next_indicator)
		{
			// Wir wollen nur aktuelle sections
			if (start_section == 0)
				start_section = header->section_number;
			else if (start_section == header->section_number)
			{
				sendToSleepNow = true; // no more scanning
				dprintf("[pptThread] got all sections\n");
				continue;
			}

			//				SIsectionPPT ppt(SIsection(section_length + 3, buf));
			SIsectionPPT ppt(section_length + 3, static_buf);
			if (ppt.is_parsed())
				if (ppt.header())
				{
					// == 0 -> kein event
					//					dprintf("[pptThread] adding %d events [table 0x%x] (begin)\n", ppt.events().size(), header.table_id);
					//					dprintf("got %d: ", header.section_number);
					zeit = time(NULL);

					// Hintereinander vorkommende sections mit gleicher contentID herausfinden
					current_content_id = ppt.content_id();
					if (first_content_id == 0)
					{
						// aktuelle section ist die erste
						already_exists = false;
						first_content_id = current_content_id;
					}
					else if ((first_content_id == current_content_id) || (previous_content_id == current_content_id))
					{
						// erste und aktuelle bzw. vorherige und aktuelle section sind gleich
						already_exists = true;
					}
					else
					{
						// erste und aktuelle bzw. vorherige und aktuelle section sind nicht gleich
						already_exists = false;
						previous_content_id = current_content_id;
					}

					// Nicht alle Events speichern
					for (SIevents::iterator e = ppt.events().begin(); e != ppt.events().end(); e++)
					{
						if (!(e->times.empty()))
						{
							for (SItimes::iterator t = e->times.begin(); t != e->times.end(); t++) {
								//								if ( ( e->times.begin()->startzeit < zeit + secondsToCache ) &&
								//								        ( ( e->times.begin()->startzeit + (long)e->times.begin()->dauer ) > zeit - oldEventsAre ) )
								// add the event if at least one starttime matches
								if ( ( t->startzeit < zeit + secondsToCache ) &&
										( ( t->startzeit + (long)t->dauer ) > zeit - oldEventsAre ) )
								{
									//									dprintf("chId: " PRINTF_CHANNEL_ID_TYPE " Dauer: %ld, Startzeit: %s", e->get_channel_id(),  (long)e->times.begin()->dauer, ctime(&e->times.begin()->startzeit));
									//									writeLockEvents();

									if (already_exists)
									{
										// Zusaetzliche Zeiten in ein Event einfuegen
										addEventTimes(*e, header->table_id);
									}
									else
									{
										// Ein Event in alle Mengen einfuegen
										addEvent(*e, header->table_id, zeit);
									}

									//									unlockEvents();
									break; // only add the event once
								}
#if 0
								// why is the following not compiling, fuXX STL
								else {
									// remove unusable times in event
									SItimes::iterator kt = t;
									t--; // the iterator t points to the last element
									e->times.erase(kt);
								}
#endif
							}
						}
						else
						{
							// pruefen ob nvod event
							readLockServices();
							MySIservicesNVODorderUniqueKey::iterator si = mySIservicesNVODorderUniqueKey.find(e->get_channel_id());

							if (si != mySIservicesNVODorderUniqueKey.end())
							{
								// Ist ein nvod-event
								writeLockEvents();

								for (SInvodReferences::iterator i = si->second->nvods.begin(); i != si->second->nvods.end(); i++)
									mySIeventUniqueKeysMetaOrderServiceUniqueKey.insert(std::make_pair(i->uniqueKey(), e->uniqueKey()));

								addNVODevent(*e);
								unlockEvents();
							}
							unlockServices();
						}
					} // for
					//dprintf("[pptThread] added %d events (end)\n",  ppt.events().size());
				} // if
		} // if
	} // for
	delete[] static_buf;

	dputs("[pptThread] end");

	pthread_exit(NULL);
}

#endif

/* helper function for the housekeeping-thread */
static void print_meminfo(void)
{
	if (!sections_debug)
		return;

	struct mallinfo meminfo = mallinfo();
	dprintf("total size of memory occupied by chunks handed out by malloc: %d\n"
		"total bytes memory allocated with `sbrk' by malloc, in bytes: %d (%dkB)\n",
		meminfo.uordblks, meminfo.arena, meminfo.arena / 1024);
}

//---------------------------------------------------------------------
// housekeeping-thread
// does cleaning on fetched datas
//---------------------------------------------------------------------
static void *houseKeepingThread(void *)
{
	int count = 0;
#ifdef UPDATE_NETWORKS
	char servicename[MAX_SIZE_SERVICENAME];
#endif
	dprintf("housekeeping-thread started.\n");
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	while (!sectionsd_stop)
	{
		if (count == META_HOUSEKEEPING) {
			dprintf("meta housekeeping - deleting all transponders, services, bouquets.\n");
			deleteSIexceptEPG();
			count = 0;
		}

		int rc = HOUSEKEEPING_SLEEP;

		while (rc)
			rc = sleep(rc);

		while (!scanning) {
			sleep(1);	// wait for streaming to end...
			if(sectionsd_stop)
				break;
		}

		dprintf("housekeeping.\n");

		// TODO: maybe we need to stop scanning here?...

		readLockEvents();

		unsigned anzEventsAlt = mySIeventsOrderUniqueKey.size();
		dprintf("before removeoldevents\n");
		unlockEvents();
		removeOldEvents(oldEventsAre); // alte Events
		dprintf("after removeoldevents\n");
		readLockEvents();
printf("[sectionsd] Removed %d old events.\n", anzEventsAlt - mySIeventsOrderUniqueKey.size());
		if (mySIeventsOrderUniqueKey.size() != anzEventsAlt)
		{
			print_meminfo();
			dprintf("Removed %d old events.\n", anzEventsAlt - mySIeventsOrderUniqueKey.size());
		}
		anzEventsAlt = mySIeventsOrderUniqueKey.size();
		unlockEvents();
		//			usleep(100);
		//			lockEvents();
		/* this is currently broken */
		removeDupEvents();
		readLockEvents();
printf("[sectionsd] Removed %d dup events.\n", anzEventsAlt - mySIeventsOrderUniqueKey.size());
		anzEventsAlt = mySIeventsOrderUniqueKey.size();
		unlockEvents();
		dprintf("before removewasteepg\n");
#ifdef UPDATE_NETWORKS
		removeWasteEvents(); // Events for channels not in services.xml
		dprintf("after removewasteepg\n");
#endif
		readLockEvents();
		if (mySIeventsOrderUniqueKey.size() != anzEventsAlt)
		{
			print_meminfo();
			dprintf("Removed %d waste events.\n", anzEventsAlt - mySIeventsOrderUniqueKey.size());
		}

		dprintf("Number of sptr events (event-ID): %u\n", mySIeventsOrderUniqueKey.size());
		dprintf("Number of sptr events (service-id, start time, event-id): %u\n", mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.size());
		dprintf("Number of sptr events (end time, service-id, event-id): %u\n", mySIeventsOrderFirstEndTimeServiceIDEventUniqueKey.size());
		dprintf("Number of sptr nvod events (event-ID): %u\n", mySIeventsNVODorderUniqueKey.size());
		dprintf("Number of cached meta-services: %u\n", mySIeventUniqueKeysMetaOrderServiceUniqueKey.size());

		unlockEvents();

#ifdef UPDATE_NETWORKS
		readLockServices();
		dprintf("Number of services: %u\n", mySIservicesOrderUniqueKey.size());
		//dprintf("Number of services (name): %u\n", mySIservicesOrderServiceName.size());
		dprintf("Number of cached nvod-services: %u\n", mySIservicesNVODorderUniqueKey.size());

		xmlDocPtr bouquet_parser = NULL;
		xmlDocPtr current_parser = NULL;
		for (MySIservicesOrderUniqueKey::iterator s = mySIservicesOrderUniqueKey.begin(); s !=  mySIservicesOrderUniqueKey.end(); s++) {
			dprintf("ONID: %04x TSID: %04x SID: %04x Prov: %s Name: %s actual: %d\n",
					s->second->original_network_id,
					s->second->transport_stream_id,
					s->second->service_id,
					s->second->providerName.c_str(),
					s->second->serviceName.c_str(),
					(int) s->second->is_actual);
			readLockMessaging();
			if (((s->second->is_actual & 8) == 8) && (!messaging_zap_detected)) {
				unlockMessaging();

				if (!bouquet_parser) {
					if (!access(CURRENTBOUQUETS_XML, R_OK)) {
						if (current_parser)
							xmlFreeDoc(current_parser);
						current_parser =
							parseXmlFile(CURRENTBOUQUETS_XML);
					}
					bouquet_parser = parseXmlFile(BOUQUETS_XML);
				}
				snprintf(servicename, MAX_SIZE_SERVICENAME,
						s->second->providerName.c_str());
				if (AddServiceToAutoBouquets(&servicename[0],
							s->second->original_network_id,
							s->second->transport_stream_id,
							s->second->service_id,
							bouquet_parser,
							current_parser)) {
					readLockMessaging();
					if (!messaging_zap_detected) {
						unlockMessaging();
						if (current_parser)
							xmlFreeDoc(current_parser);
						current_parser =
							parseXmlFile(CURRENTBOUQUETS_XML);
					}
					else
						unlockMessaging();
				}
				unlockServices();
				writeLockServices();
				s->second->is_actual = (s->second->is_actual & 7);
				unlockServices();
				readLockServices();
			}
			else
				unlockMessaging();

		}
		unlockServices();
		if (bouquet_parser)
			xmlFreeDoc(bouquet_parser);
		if (current_parser)
			xmlFreeDoc(current_parser);
#endif
		/*
		   lockTransponders();
		   for (MySItranspondersOrderUniqueKey::iterator s = mySItranspondersOrderUniqueKey.begin(); s != mySItranspondersOrderUniqueKey.end(); s++)
		   {
		//				tsid = s->second->transport_stream_id;
		//				onid = s->second->original_network_id;
		const char *ddp = &s->second->delivery_descriptor[0];

		//printf("Descriptor_type: %02x\n", s->second->delivery_type);

		switch (s->second->delivery_type) {
		case 0x43:
		struct satellite_delivery_descriptor *sdd = (struct satellite_delivery_descriptor *)ddp;
		printf("Sat TP - Orbital %04x ONID: %04x TSID: %04x\n",
		(sdd->orbital_pos_hi << 8) | sdd->orbital_pos_lo,
		s->second->original_network_id,
		s->second->transport_stream_id);

		//	if (!sdd->west_east_flag)
		//		position = -position;
		//	provider = getProvbyPosition(xmlDocGetRootElement(service_parser)->xmlChildrenNode, //position);
		break;
		case 0x44:
		//provider = xmlDocGetRootElement(service_parser)->xmlChildrenNode;
		break;
		default:
		break;
		}
		}
		unlockTransponders();
		*/

		print_meminfo();

		count++;

	} // for endlos
	dprintf("housekeeping-thread ended.\n");

	pthread_exit(NULL);
}

static void readEPGFilter(void)
{
	xmlDocPtr filter_parser = parseXmlFile(epg_filter_dir.c_str());

	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;

	if (filter_parser != NULL)
	{
		dprintf("Reading EPGFilters\n");

		xmlNodePtr filter = xmlDocGetRootElement(filter_parser);
		if (xmlGetNumericAttribute(filter, "is_whitelist", 10) == 1)
			epg_filter_is_whitelist = true;
		if (xmlGetNumericAttribute(filter, "except_current_next", 10) == 1)
			epg_filter_except_current_next = true;
		filter = filter->xmlChildrenNode;

		while (filter) {

			onid = xmlGetNumericAttribute(filter, "onid", 16);
			tsid = xmlGetNumericAttribute(filter, "tsid", 16);
			sid  = xmlGetNumericAttribute(filter, "serviceID", 16);
			if (xmlGetNumericAttribute(filter, "blacklist", 10) == 1)
				addBlacklist(onid, tsid, sid);
			else
				addEPGFilter(onid, tsid, sid);

			filter = filter->xmlNextNode;
		}
	}
	xmlFreeDoc(filter_parser);
}

static void readDVBTimeFilter(void)
{
	xmlDocPtr filter_parser = parseXmlFile(dvbtime_filter_dir.c_str());

	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;

	if (filter_parser != NULL)
	{
		dprintf("Reading DVBTimeFilters\n");

		xmlNodePtr filter = xmlDocGetRootElement(filter_parser);
		filter = filter->xmlChildrenNode;

		while (filter) {

			onid = xmlGetNumericAttribute(filter, "onid", 16);
			tsid = xmlGetNumericAttribute(filter, "tsid", 16);
			sid  = xmlGetNumericAttribute(filter, "serviceID", 16);
			addNoDVBTimelist(onid, tsid, sid);

			filter = filter->xmlNextNode;
		}
		xmlFreeDoc(filter_parser);
	}
	else
	{
		dvb_time_update = true;
	}
}

#if 0
static void readBouquetFilter(void)
{
	xmlDocPtr filter_parser = parseXmlFile("/var/tuxbox/config/mybouquets.xml");

	t_bouquet_id bid = 0;

	if (filter_parser != NULL)
	{
		dprintf("Reading Bouquet Filters\n");

		xmlNodePtr mybouquets = xmlDocGetRootElement(filter_parser);
		mybouquets = mybouquets->xmlChildrenNode;

		while ((xmlGetNextOccurence(mybouquets, "filter") != NULL) ||
			(xmlGetNextOccurence(mybouquets, "adder") != NULL)) {
			if (strcmp(xmlGetName(mybouquets), "filter") == 0) {
				if (xmlGetNumericAttribute(mybouquets, "is_whitelist", 10) == 1)
					bouquet_filter_is_whitelist = true;
				xmlNodePtr filter = mybouquets->xmlChildrenNode;

				while (filter) {

					bid  = xmlGetNumericAttribute(filter, "bouquet_id", 16);

					addBouquetFilter(bid);

					filter = filter->xmlNextNode;
				}
			}
			if (strcmp(xmlGetName(mybouquets), "adder") == 0) {
				if (mybouquets->xmlChildrenNode)
				{
					BouquetAdder *bouquet = new BouquetAdder;
					snprintf(bouquet->BouquetName, MAX_SIZE_MYBOUQUETS_STR,
							xmlGetAttribute(mybouquets, "name"));
					bouquet->bid = xmlGetNumericAttribute(mybouquets, "bouquet_id", 16);
					bouquet->bae = NULL;
       					bouquet->next = CurrentBouquetAdder;
					CurrentBouquetAdder = bouquet;
					xmlNodePtr entry = mybouquets->xmlChildrenNode;
					while (entry) {
						BouquetAdderEntry *adderentry = new BouquetAdderEntry;
						snprintf(adderentry->ProviderName, MAX_SIZE_MYBOUQUETS_STR,
							xmlGetAttribute(entry, "provider"));
						adderentry->onid =
							xmlGetNumericAttribute(entry, "onid", 16);
						adderentry->tsid =
							xmlGetNumericAttribute(entry, "tsid", 16);
						adderentry->es = NULL;
       						adderentry->next = bouquet->bae;
						bouquet->bae = adderentry;
						if (entry->xmlChildrenNode)
						{
							xmlNodePtr excepts = entry->xmlChildrenNode;
							while (excepts) {
								ExceptService *eservice = new
											ExceptService;
								eservice->sid =
									xmlGetNumericAttribute(entry,
									"service_id", 16);
								eservice->next = adderentry->es;
								adderentry->es = eservice;
								excepts = excepts->xmlNextNode;
							}
						}
						entry = entry->xmlNextNode;
					}
				}
			}
			mybouquets = mybouquets->xmlNextNode;
		}
		if (CurrentBouquetAdder) {
			BouquetAdder *bouquet;
			bouquet = CurrentBouquetAdder;
			while (bouquet) {
				dprintf("New Automatic Bouquet: Name:%s BouquetID:0x%04x\n",
					bouquet->BouquetName,
					bouquet->bid);
				BouquetAdderEntry *currententry;
				currententry = bouquet->bae;
				while (currententry) {
					dprintf("Entry found: Provider:%s ONID:0x%04x TSID:0x%04x\n",
					currententry->ProviderName,
					currententry->onid,
					currententry->tsid);
					currententry = currententry->next;
				}
				bouquet = bouquet->next;
			}
		}
	}
	xmlFreeDoc(filter_parser);
}

static void printHelp(void)
{
	printf("\nUsage: sectionsd [-d][-nu]\n\n");
}

// Just to get our listen socket closed cleanly
static void signalHandler(int signum)
{
	switch (signum)
	{

	case SIGHUP:
		break;

	default:
		exit(0);
	}
}

#define GETENVI(var) \
	env = getenv("SD_"#var); \
	if (env) { \
		var = atoi(env); \
		fprintf(stderr, "GETENVI("#var") = %d\n",var); \
	} else	fprintf(stderr, "GETENVI("#var") failed\n");
#define GETENVB(var) \
	env = getenv("SD_"#var); \
	if (env) { \
		var = !!atoi(env); \
		fprintf(stderr, "GETENVB("#var") = %d\n",var); \
	} else	fprintf(stderr, "GETENVB("#var") failed\n");
#define GETENVL(var) \
	env = getenv("SD_"#var); \
	if (env) { \
		var = atol(env); \
		fprintf(stderr, "GETENVL("#var") = %ld\n",var); \
	} else	fprintf(stderr, "GETENVL("#var") failed\n");
#define GETENVS(var) \
	env = getenv("SD_"#var); \
	if (env) { \
		var = env; \
		fprintf(stderr, "GETENVS("#var") = %s\n",env); \
	} else	fprintf(stderr, "GETENVS("#var") failed\n");
#endif
//int main(int argc, char **argv)
extern cDemux * dmxUTC;

void sectionsd_main_thread(void */*data*/)
{
	pthread_t threadTOT, threadEIT, threadCN, threadHouseKeeping;
#ifdef UPDATE_NETWORKS
	pthread_t threadSDT, threadNIT;
#endif
#ifdef ENABLE_FREESATEPG
	pthread_t threadFSEIT;
#endif
#ifdef ENABLE_PPT
	pthread_t threadPPT;
#endif
	int rc;

	struct sched_param parm;

	printf("$Id: sectionsd.cpp,v 1.305 2009/07/30 12:41:39 seife Exp $\n");

	SIlanguage::loadLanguages();

	tzset(); // TZ auswerten

	CBasicServer sectionsd_server;

	//NTP-Config laden
	if (!ntp_config.loadConfig(CONF_FILE))
	{
		/* set defaults if no configuration file exists */
		printf("[sectionsd] %s not found\n", CONF_FILE);
	}

	ntpserver = ntp_config.getString("network_ntpserver", "de.pool.ntp.org");
	ntprefresh = atoi(ntp_config.getString("network_ntprefresh","30").c_str() );
	ntpenable = ntp_config.getBool("network_ntpenable", false);
	ntp_system_cmd = ntp_system_cmd_prefix + ntpserver;

	//EPG Einstellungen laden
	secondsToCache = (atoi(ntp_config.getString("epg_cache_time","14").c_str() ) *24*60L*60L); //Tage
	secondsExtendedTextCache = (atoi(ntp_config.getString("epg_extendedcache_time","360").c_str() ) *60L*60L); //Stunden
	oldEventsAre = (atoi(ntp_config.getString("epg_old_events","1").c_str() ) *60L*60L); //Stunden
	max_events= atoi(ntp_config.getString("epg_max_events","50000").c_str() );

	printf("[sectionsd] Caching max %d events\n", max_events);
	printf("[sectionsd] Caching %ld days\n", secondsToCache / (24*60*60L));
	printf("[sectionsd] Caching %ld hours Extended Text\n", secondsExtendedTextCache / (60*60L));
	printf("[sectionsd] Events are old %ldmin after their end time\n", oldEventsAre / 60);

	readEPGFilter();
	readDVBTimeFilter();
#if 0
	readBouquetFilter();
#endif
	readEncodingFile();

	if (!sectionsd_server.prepare(SECTIONSD_UDS_NAME)) {
		fprintf(stderr, "[sectionsd] failed to prepare basic server\n");
		return;
	}
	// from here on forked
	//signal(SIGHUP, signalHandler);
	eventServer = new CEventServer;

	// time-Thread starten
	rc = pthread_create(&threadTOT, 0, timeThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create time-thread (rc=%d)\n", rc);
		return;
	}

	// EIT-Thread starten
	rc = pthread_create(&threadEIT, 0, eitThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create eit-thread (rc=%d)\n", rc);
		return;
	}

	// EIT-Thread2 starten
	rc = pthread_create(&threadCN, 0, cnThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create eit-thread (rc=%d)\n", rc);
		return;
	}

#ifdef ENABLE_FREESATEPG
	// EIT-Thread3 starten
	rc = pthread_create(&threadFSEIT, 0, fseitThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create fseit-thread (rc=%d)\n", rc);
		return;
	}
#endif

#ifdef ENABLE_PPT
	// premiere private epg -Thread starten
	rc = pthread_create(&threadPPT, 0, pptThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create ppt-thread (rc=%d)\n", rc);
		return;
	}
#endif
#ifdef UPDATE_NETWORKS
	// NIT-Thread starten
	rc = pthread_create(&threadNIT, 0, nitThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create nit-thread (rc=%d)\n", rc);
		return;
	}
	// SDT-Thread starten
	rc = pthread_create(&threadSDT, 0, sdtThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create sdt-thread (rc=%d)\n", rc);
		return;
	}
#endif

	// housekeeping-Thread starten
	rc = pthread_create(&threadHouseKeeping, 0, houseKeepingThread, 0);

	if (rc) {
		fprintf(stderr, "[sectionsd] failed to create housekeeping-thread (rc=%d)\n", rc);
		return;
	}

	if (sections_debug) {
		int policy;
		rc = pthread_getschedparam(pthread_self(), &policy, &parm);
		dprintf("mainloop getschedparam %d policy %d prio %d\n", rc, policy, parm.sched_priority);
	}

	while (sectionsd_server.run(sectionsd_parse_command, sectionsd::ACTVERSION, true)) {
		sched_yield();
		if (eit_update_fd != -1) {
			unsigned char buf[4096];
			int ret = eitDmx->Read(buf, 4095, 10);

			if (ret > 0) {
				//printf("[sectionsd] EIT update: len %d, %02X %02X %02X %02X %02X %02X version %02X\n", ret, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], ((SI_section_header*)buf)->version_number);

				printdate_ms(stdout);printf("EIT Update Filter: new version 0x%x, Activate cnThread\n", ((SI_section_header*)buf)->version_number);
				writeLockMessaging();
				//						messaging_skipped_sections_ID[0].clear();
				//						messaging_sections_max_ID[0] = -1;
				//						messaging_sections_got_all[0] = false;
				messaging_have_CN = 0x00;
				messaging_got_CN = 0x00;
				messaging_last_requested = time_monotonic();
				unlockMessaging();
				sched_yield();
				dmxCN.change(0);
				sched_yield();
			}
		}
		if(sectionsd_stop)
			break;

		sched_yield();
		/* 10 ms is the minimal timeslice anyway (HZ = 100), so let's
		   wait 20 ms at least to lower the CPU load */
		usleep(20000);
	}

        printf("[sectionsd] stopping...\n");
        scanning = 0;
        timeset = true;
printf("broadcasting...\n");
	pthread_mutex_lock(&timeIsSetMutex);
	pthread_cond_broadcast(&timeIsSetCond);
	pthread_mutex_unlock(&timeIsSetMutex);
	pthread_mutex_lock(&timeThreadSleepMutex);
	pthread_cond_broadcast(&timeThreadSleepCond);
	pthread_mutex_unlock(&timeThreadSleepMutex);
	pthread_mutex_lock(&dmxEIT.start_stop_mutex);
	pthread_cond_broadcast(&dmxEIT.change_cond);
	pthread_mutex_unlock(&dmxEIT.start_stop_mutex);
	pthread_mutex_lock(&dmxCN.start_stop_mutex);
	pthread_cond_broadcast(&dmxCN.change_cond);
	pthread_mutex_unlock(&dmxCN.start_stop_mutex);
#ifdef ENABLE_PPT
	pthread_mutex_lock(&dmxPPT.start_stop_mutex);
	pthread_cond_broadcast(&dmxPPT.change_cond);
	pthread_mutex_unlock(&dmxPPT.start_stop_mutex);
#endif
#ifdef UPDATE_NETWORKS
	pthread_mutex_lock(&dmxSDT.start_stop_mutex);
	pthread_cond_broadcast(&dmxSDT.change_cond);
	pthread_mutex_unlock(&dmxSDT.start_stop_mutex);
#endif
printf("pausing...\n");
        dmxEIT.request_pause();
	dmxCN.request_pause();
#ifdef ENABLE_PPT
        dmxPPT.request_pause();
#endif
#ifdef ENABLE_FREESATEPG
        dmxFSEIT.request_pause();
#endif
#ifdef UPDATE_NETWORKS
	dmxSDT.request_pause();
	dmxNIT.request_pause();
#endif
        pthread_cancel(threadHouseKeeping);

        if(dmxUTC) dmxUTC->Stop();

	pthread_cancel(threadTOT);

printf("join 1\n");
        pthread_join(threadTOT, NULL);
        if(dmxUTC) delete dmxUTC;
printf("join 2\n");
        pthread_join(threadEIT, NULL);
printf("join 3\n");
        pthread_join(threadCN, NULL);
#ifdef ENABLE_PPT
printf("join 3\n");
        pthread_join(threadPPT, NULL);
#endif
#ifdef UPDATE_NETWORKS
printf("join 4\n");
        pthread_join(threadSDT, NULL);
#endif

	eit_stop_update_filter(&eit_update_fd);
	if(eitDmx)
		delete eitDmx;

printf("close 1\n");
        dmxEIT.close();
printf("close 3\n");
        dmxCN.close();
#ifdef ENABLE_FREESATEPG
        dmxFSEIT.close();
#endif
#ifdef ENABLE_PPT
        dmxPPT.close();
#endif
#ifdef UPDATE_NETWORKS
	dmxSDT.close();
        dmxNIT.close();
#endif

        printf("[sectionsd] ended\n");

	return;
}
/* was: commandAllEventsChannelID sendAllEvents */
void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "")
{
	dprintf("sendAllEvents for " PRINTF_CHANNEL_ID_TYPE "\n", serviceUniqueKey);

	if ((serviceUniqueKey& 0xFFFFFFFFFFFFULL) != 0) { //0xFFFFFFFFFFFFULL for CREATE_CHANNEL_ID64
		// service Found
		readLockEvents();
		int serviceIDfound = 0;

		if (search_text.length()) std::transform(search_text.begin(), search_text.end(), search_text.begin(), tolower);
		for (MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin(); e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end(); ++e)
		{
			if ((*e)->get_channel_id() == (serviceUniqueKey& 0xFFFFFFFFFFFFULL)) { //0xFFFFFFFFFFFFULL for CREATE_CHANNEL_ID64
				serviceIDfound = 1;

				bool copy = true;
				if(search == 0); // nothing to do here
				else if(search == 1) {
					std::string eName = (*e)->getName();
					std::transform(eName.begin(), eName.end(), eName.begin(), tolower);
					if(eName.find(search_text) == std::string::npos)
						copy = false;
				}
				else if(search == 2) {
					std::string eText = (*e)->getText();
					std::transform(eText.begin(), eText.end(), eText.begin(), tolower);
					if(eText.find(search_text) == std::string::npos)
						copy = false;
				}
				else if(search == 3) {
					std::string eExtendedText = (*e)->getExtendedText();
					std::transform(eExtendedText.begin(), eExtendedText.end(), eExtendedText.begin(), tolower);
					if(eExtendedText.find(search_text) == std::string::npos)
						copy = false;
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
}
/* was: commandCurrentNextInfoChannelID */
void sectionsd_getCurrentNextServiceKey(t_channel_id uniqueServiceKey, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next )
{
	dprintf("[sectionsd] Request of current/next information for " PRINTF_CHANNEL_ID_TYPE "\n", uniqueServiceKey);

	SIevent currentEvt;
	SIevent nextEvt;
	unsigned flag = 0, flag2=0;
	/* ugly hack: retry fetching current/next by restarting dmxCN if this is true */
	bool change = false;

	//t_channel_id * uniqueServiceKey = (t_channel_id *)data;

	readLockEvents();
	/* if the currently running program is requested... */
	if (uniqueServiceKey == messaging_current_servicekey) {
		/* ...check for myCurrentEvent and myNextEvent */
		if (!myCurrentEvent) {
			dprintf("!myCurrentEvent ");
			change = true;
			flag |= CSectionsdClient::epgflags::not_broadcast;
		} else {
			currentEvt = *myCurrentEvent;
			flag |= CSectionsdClient::epgflags::has_current; // aktuelles event da...
			flag |= CSectionsdClient::epgflags::has_anything;
		}
		if (!myNextEvent) {
			dprintf("!myNextEvent ");
			change = true;
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
		} else {
			zeitEvt1.startzeit = currentEvt.times.begin()->startzeit;
			zeitEvt1.dauer = currentEvt.times.begin()->dauer;
		}
		SItime zeitEvt2(zeitEvt1);

		if (currentEvt.getName().empty() && flag2 != 0)
		{
			dprintf("commandCurrentNextInfoChannelID change1\n");
			change = true;
		}

		if (currentEvt.service_id != 0)
		{ //Found
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
		{ // no current event...
			readLockServices();

			MySIservicesOrderUniqueKey::iterator si = mySIservicesOrderUniqueKey.end();
			si = mySIservicesOrderUniqueKey.find(uniqueServiceKey);

			if (si != mySIservicesOrderUniqueKey.end())
			{
				dprintf("[sectionsd] current service has%s scheduled events, and has%s present/following events\n", si->second->eitScheduleFlag() ? "" : " no", si->second->eitPresentFollowingFlag() ? "" : " no" );

				if ( /*( !si->second->eitScheduleFlag() ) || */
				    ( !si->second->eitPresentFollowingFlag() ) )
				{
					flag |= CSectionsdClient::epgflags::not_broadcast;
				}
			}
			unlockServices();

			if ( flag2 & CSectionsdClient::epgflags::has_anything )
			{
				flag |= CSectionsdClient::epgflags::has_anything;
				if (!(flag & CSectionsdClient::epgflags::has_next)) {
					dprintf("*nextEvt not from cur/next V2!\n");
					nextEvt = findNextSIeventForServiceUniqueKey(uniqueServiceKey, zeitEvt2);
				}

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

							if (eFirst->second->times.begin()->startzeit < azeit &&
							    eFirst->second->uniqueKey() == nextEvt.uniqueKey() - 1)
								flag |= CSectionsdClient::epgflags::has_no_current;
						}
					}
				}
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
			change = true;
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
	time_cur.startzeit = currentEvt.times.begin()->startzeit;
	time_cur.dauer = currentEvt.times.begin()->dauer;
	time_nxt.startzeit = nextEvt.times.begin()->startzeit;
	time_nxt.dauer = nextEvt.times.begin()->dauer;
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

	//dprintf("change: %s, messaging_eit_busy: %s, last_request: %d\n", change?"true":"false", messaging_eit_is_busy?"true":"false",(time_monotonic() - messaging_last_requested));
	if (change && !messaging_eit_is_busy && (time_monotonic() - messaging_last_requested) < 11) {
		/* restart dmxCN, but only if it is not already running, and only for 10 seconds */
		dprintf("change && !messaging_eit_is_busy => dmxCN.change(0)\n");
		dmxCN.change(0);
	}
}
/* commandEPGepgIDshort */
bool sectionsd_getEPGidShort(event_id_t epgID, CShortEPGData * epgdata)
{
	bool ret = false;
	dprintf("Request of current EPG for 0x%llx\n", epgID);

	readLockEvents();

	const SIevent& e = findSIeventForEventUniqueKey(epgID);

	if (e.service_id != 0)
	{ // Event found
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

#if 0
char * parseExtendedEvents(char * dp, CEPGData * epgdata) {
        char * pItemDescriptions = dp, * pItemDescriptionStart;
        dp+=strlen(dp)+1;
        char * pItems = dp, * pItemStart;
        dp+=strlen(dp)+1;
        /* Clear vector since epgdata seems to be reused */
        epgdata->itemDescriptions.clear();
        while (*pItemDescriptions) {
                pItemDescriptionStart = pItemDescriptions;
                while (*pItemDescriptions && '\n' != *pItemDescriptions) {
                        pItemDescriptions++;
                }
                char pp = *pItemDescriptions;
                *pItemDescriptions = 0;
                epgdata->itemDescriptions.push_back(pItemDescriptionStart);
/*printf("CSectionsdClient::parseExtendedEvents: desc %s\n", pItemDescriptionStart);*/
                if(!pp)
                        break;
                pItemDescriptions++;
        }
        /* Clear vector since epgdata seems to be reused */
        epgdata->items.clear();
        while (*pItems) {
                pItemStart = pItems;
                while (*pItems && '\n' != *pItems) {
                        pItems++;
                }
                char pp = *pItemDescriptions;
                *pItems = 0;
                epgdata->items.push_back(pItemStart);
/*printf("CSectionsdClient::parseExtendedEvents: item %s\n", pItemStart);*/
                if(!pp)
                        break;
                pItems++;
        }
        return dp;
}
#endif

/*was getEPGid commandEPGepgID(int connfd, char *data, const unsigned dataLength) */
bool sectionsd_getEPGid(const event_id_t epgID, const time_t startzeit, CEPGData * epgdata)
{
	bool ret = false;
	dprintf("Request of actual EPG for 0x%llx 0x%lx\n", epgID, startzeit);

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
			epgdata->contentClassification = std::string(evt.contentClassification.data(), evt.contentClassification.length());
			epgdata->userClassification = std::string(evt.userClassification.data(), evt.userClassification.length());
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
bool sectionsd_getActualEPGServiceKey(const t_channel_id uniqueServiceKey, CEPGData * epgdata)
{
	bool ret = false;
	SIevent evt;
	SItime zeit(0, 0);

	dprintf("[commandActualEPGchannelID] Request of current EPG for " PRINTF_CHANNEL_ID_TYPE "\n", uniqueServiceKey);

	readLockEvents();
	if (uniqueServiceKey == messaging_current_servicekey) {
		if (myCurrentEvent) {
			evt = *myCurrentEvent;
			zeit.startzeit = evt.times.begin()->startzeit;
			zeit.dauer = evt.times.begin()->dauer;
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
                epgdata->contentClassification = std::string(evt.contentClassification.data(), evt.contentClassification.length());
                epgdata->userClassification = std::string(evt.userClassification.data(), evt.userClassification.length());
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
/* was static void sendEventList(int connfd, const unsigned char serviceTyp1, const unsigned char serviceTyp2 = 0, int sendServiceName = 1, t_channel_id * chidlist = NULL, int clen = 0) */
void sectionsd_getChannelEvents(CChannelEventList &eList, const bool tv_mode = true, t_channel_id *chidlist = NULL, int clen = 0)
{
	unsigned char serviceTyp1, serviceTyp2;
	clen = clen / sizeof(t_channel_id);

	t_channel_id uniqueNow = 0;
	t_channel_id uniqueOld = 0;
	bool found_already = false;
	time_t azeit = time(NULL);
	std::string sname;

        if(tv_mode) {
                serviceTyp1 = 0x01;
                serviceTyp2 = 0x04;
        } else {
                serviceTyp1 = 0x02;
                serviceTyp2 = 0x00;
        }

	readLockEvents();

	/* !!! FIX ME: if the box starts on a channel where there is no EPG sent, it hangs!!!	*/
	for (MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e = mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin(); e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end(); ++e)
	{
		uniqueNow = (*e)->get_channel_id();
		if (!channel_in_requested_list(chidlist, uniqueNow, clen)) continue;
		if ( uniqueNow != uniqueOld )
		{
			found_already = true;
			readLockServices();
			// new service, check service- type
			MySIservicesOrderUniqueKey::iterator s = mySIservicesOrderUniqueKey.find(uniqueNow);

			if (s != mySIservicesOrderUniqueKey.end())
			{
				if (s->second->serviceTyp == serviceTyp1 || (serviceTyp2 && s->second->serviceTyp == serviceTyp2))
				{
					sname = s->second->serviceName;
					found_already = false;
				}
			}
			else
			{
				// wenn noch nie hingetuned wurde, dann gibts keine Info ber den ServiceTyp...
				// im Zweifel mitnehmen
				found_already = false;
			}
			unlockServices();

			uniqueOld = uniqueNow;
		}

		if ( !found_already )
		{
			std::string eName = (*e)->getName();
			std::string eText = (*e)->getText();
			std::string eExtendedText = (*e)->getExtendedText();

			for (SItimes::iterator t = (*e)->times.begin(); t != (*e)->times.end(); ++t)
			{
				if (t->startzeit <= azeit && azeit <= (long)(t->startzeit + t->dauer))
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
                                        eList.push_back(aEvent);

                                        found_already = true;
                                        break;
				}
			}
		}
	}

	unlockEvents();
}
/*was static void commandComponentTagsUniqueKey(int connfd, char *data, const unsigned dataLength) */
bool sectionsd_getComponentTagsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::ComponentTagList& tags)
{
	bool ret = false;
	dprintf("Request of ComponentTags for 0x%llx\n", uniqueKey);

	tags.clear();

	readLockEvents();

	MySIeventsOrderUniqueKey::iterator eFirst = mySIeventsOrderUniqueKey.find(uniqueKey);

	if (eFirst != mySIeventsOrderUniqueKey.end()) {
		CSectionsdClient::responseGetComponentTags response;
		ret = true;

		for (SIcomponents::iterator cmp = eFirst->second->components.begin(); cmp != eFirst->second->components.end(); ++cmp) {
			response.component = cmp->component;
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
bool sectionsd_getLinkageDescriptorsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::LinkageDescriptorList& descriptors)
{
        bool ret = false;
        dprintf("Request of LinkageDescriptors for 0x%llx\n", uniqueKey);

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
bool sectionsd_getNVODTimesServiceKey(const t_channel_id uniqueServiceKey, CSectionsdClient::NVODTimesList& nvod_list)
{
        bool ret = false;
        dprintf("Request of NVOD times for " PRINTF_CHANNEL_ID_TYPE "\n", uniqueServiceKey);

        nvod_list.clear();

	readLockServices();
	readLockEvents();

        MySIservicesNVODorderUniqueKey::iterator si = mySIservicesNVODorderUniqueKey.find(uniqueServiceKey);
        if (si != mySIservicesNVODorderUniqueKey.end())
        {
                dprintf("NVODServices: %u\n", si->second->nvods.size());

                if (si->second->nvods.size()) {
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

void sectionsd_setPrivatePid(unsigned short /*pid*/)
{
#ifdef ENABLE_PPT
                privatePid = pid;
                if (pid != 0) {
                        dprintf("[sectionsd] wakeup PPT Thread, pid=%x\n", pid);
                        dmxPPT.change( 0 );
                }
#endif
}

void sectionsd_set_languages(const std::vector<std::string>& newLanguages)
{
	SIlanguage::setLanguages(newLanguages);
	SIlanguage::saveLanguages();
}
