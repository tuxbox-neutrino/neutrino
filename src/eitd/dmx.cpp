/*
 * DMX class (sectionsd) - d-box2 linux project
 *
 * (C) 2001 by fnbrd (fnbrd@gmx.de),
 *     2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 * Copyright (C) 2007-2009, 2011-2012 Stefan Seyfried
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include <string>
#include <cstring>
#include <map>

#include <driver/abstime.h>
#include <dvbsi++/long_section.h>

#include "dmx.h"
#include "debug.h"

//#define DEBUG_MUTEX 1
//#define DEBUG_DEMUX 1 // debug start/close/change
//#define DEBUG_CACHED_SECTIONS 1
//#define DEBUG_COMPLETE_SECTIONS 1
#define DEBUG_COMPLETE 1

//static MyDMXOrderUniqueKey myDMXOrderUniqueKey;

DMX::DMX(const unsigned short p, const unsigned short bufferSizeInKB, const bool c, int dmx_source)
{
	dmx_num = dmx_source;
	dmxBufferSizeInKB = bufferSizeInKB;
	pID = p;
	cache = c;
	init();
}

DMX::DMX()
{
	pID = 0;
	cache = true;
	dmx_num = 0;
	dmxBufferSizeInKB = 512;
	init();
	eit_version = 0;
	dmx = 0;
}

void DMX::init()
{
	fd = -1;
	lastChanged = time_monotonic();
	filter_index = 0;
	real_pauseCounter = 0;
	current_service = 0;
	first_skipped = 0;

#if HAVE_TRIPLEDRAGON
	/* hack, to keep the TD changes in one place. */
	dmxBufferSizeInKB = 128;	/* 128kB is enough on TD */
	dmx_num = 0;			/* always use demux 0 */
#endif
#ifdef DEBUG_MUTEX
	pthread_mutexattr_t start_stop_mutex_attr;
	pthread_mutexattr_init(&start_stop_mutex_attr);
	pthread_mutexattr_settype(&start_stop_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&start_stop_mutex, &start_stop_mutex_attr);
#else
	pthread_mutex_init(&start_stop_mutex, NULL); // default = fast mutex
#endif
	pthread_cond_init (&change_cond, NULL);
	seen_section = false;
}

DMX::~DMX()
{
	first_skipped = 0;
	myDMXOrderUniqueKey.clear();
	pthread_mutex_destroy(&start_stop_mutex);
	pthread_cond_destroy (&change_cond);
	//closefd();
	close();
}

void DMX::close(void)
{
	if(dmx)
		delete dmx;
	dmx = NULL;
}

void DMX::closefd(void)
{
#ifdef DEBUG_DEMUX
	xcprintf("	%s: DMX::closefd, isOpen %d demux #%d", name.c_str(), isOpen(), dmx_num);
#endif
	if (isOpen())
	{
		//dmx->Stop();
		delete dmx;
		dmx = NULL;
		fd = -1;
	}
}

void DMX::addfilter(const unsigned char filter, const unsigned char mask)
{
	s_filters tmp;
	tmp.filter = filter;
	tmp.mask   = mask;
	filters.push_back(tmp);
}

int DMX::immediate_stop(void)
{
	if (!isOpen())
		return 1;

	closefd();

	return 0;
}

int DMX::stop(void)
{
	int rc;

	lock();

	rc = immediate_stop();

	unlock();

	return rc;
}

void DMX::lock(void)
{
	//dprintf("DMX::lock, thread %lu\n", pthread_self());
#ifdef DEBUG_MUTEX
	int rc = pthread_mutex_lock(&start_stop_mutex);
	if (rc != 0)
	{
		fprintf(stderr, "[sectionsd] mutex_lock: %d %d %d\n", rc, EINVAL, EDEADLK);
		fflush(stderr);
		fprintf(stderr, "[sectionsd] pid: %d\n", getpid());
		fflush(stderr);
	}
#else
	pthread_mutex_lock(&start_stop_mutex);
#endif
}

void DMX::unlock(void)
{
	//dprintf("DMX::unlock, thread %lu\n", pthread_self());
#ifdef DEBUG_MUTEX
	int rc = pthread_mutex_unlock(&start_stop_mutex);
	if (rc != 0)
	{
		fprintf(stderr, "[sectionsd] mutex_unlock: %d %d %d\n", rc, EINVAL, EPERM);
		fflush(stderr);
		fprintf(stderr, "[sectionsd] pid: %d\n", getpid());
		fflush(stderr);
	}
#else
	pthread_mutex_unlock(&start_stop_mutex);
#endif
	sched_yield();
}

inline sections_id_t create_sections_id(const uint8_t table_id, const uint16_t extension_id,
		const uint16_t onid, const uint16_t tsid, const uint8_t section_number)
{
	return 	(sections_id_t) (	((sections_id_t) table_id 	<< 56) |
					((sections_id_t) extension_id 	<< 40) |
					((sections_id_t) onid		<< 24) |
					((sections_id_t) tsid		<< 8) |
					((sections_id_t) section_number));
}

bool DMX::check_complete(sections_id_t s_id, uint8_t number, uint8_t last, uint8_t segment_last) 
{
	bool ret = false;

	section_map_t::iterator it = seenSections.find(s_id);

	if (it == seenSections.end()) {
		seenSections.insert(s_id);
		calcedSections.insert(s_id);
		uint64_t tmpval = s_id & 0xFFFFFFFFFFFFFF00ULL;

		uint8_t tid = (s_id >> 56);
		uint8_t incr = ((tid >> 4) == 4) ? 1 : 8;
		for ( int i = 0; i <= last; i+=incr )
		{
			if ( i == number )
			{
				for (int x = i; x <= segment_last; ++x)
					calcedSections.insert((sections_id_t) tmpval | (sections_id_t) (x&0xFF));
			}
			else
				calcedSections.insert((sections_id_t) tmpval | (sections_id_t)(i&0xFF));
		}
#ifdef DEBUG_COMPLETE_SECTIONS
printf("	[%s cache] new section for table 0x%02x sid 0x%04x section 0x%02x last 0x%02x slast 0x%02x seen %d calc %d\n", name.c_str(),
		(int)(s_id >> 56), (int) ((s_id >> 40) & 0xFFFF), (int)(s_id & 0xFF), last,
		segment_last, seenSections.size(), calcedSections.size());
#endif
	}
#ifdef DEBUG_COMPLETE_SECTIONS
	else {
printf("	[%s cache] old section for table 0x%02x sid 0x%04x section 0x%02x last 0x%02x slast 0x%02x seen %d calc %d\n", name.c_str(),
		(int)(s_id >> 56), (int) ((s_id >> 40) & 0xFFFF), (int)(s_id & 0xFF), last,
		segment_last, seenSections.size(), calcedSections.size());
	}
#endif
	if(seenSections == calcedSections) {
#ifdef DEBUG_COMPLETE
		xcprintf("	%s cache %02x complete: %d", name.c_str(), filters[filter_index].filter, (int)seenSections.size());
#endif
		/* FIXME this algo fail sometimes:
		*	[cnThread cache] new section for table 0x4e sid 0x0a39 section 0x00 last 0x00 slast 0x00 seen 1 calc 1
		* 16:18:25.136   cnThread cache 4e complete: 1
		* 	[cnThread cache] new section for table 0x4e sid 0x0a29 section 0x00 last 0x00 slast 0x00 seen 2 calc 2
		* 16:18:25.214   cnThread cache 4e complete: 2
		*	[cnThread cache] new section for table 0x4e sid 0x0a2a section 0x00 last 0x00 slast 0x00 seen 3 calc 3
		* 16:18:25.220   cnThread cache 4e complete: 3
		*	[cnThread cache] new section for table 0x4e sid 0x0a2b section 0x00 last 0x00 slast 0x00 seen 4 calc 4
		*/
		if(seenSections.size() > 10)
			ret = true;
	}
	return ret;
}

int DMX::getSection(uint8_t *buf, const unsigned timeoutInMSeconds, int &timeouts)
{
	struct eit_extended_section_header {
		unsigned transport_stream_id_hi	  : 8;
		unsigned transport_stream_id_lo	  : 8;
		unsigned original_network_id_hi   : 8;
		unsigned original_network_id_lo   : 8;
		unsigned segment_last_section_number : 8;
		unsigned last_table_id		  : 8;
	} __attribute__ ((packed));  // 6 bytes total
	static int bad_count = 0;

	eit_extended_section_header *eit_extended_header;

	/* filter == 0 && maks == 0 => EIT dummy filter to slow down EIT thread startup */
	if (pID == 0x12 && filters[filter_index].filter == 0 && filters[filter_index].mask == 0)
	{
		//dprintf("dmx: dummy filter, sleeping for %d ms\n", timeoutInMSeconds);
		usleep(timeoutInMSeconds * 1000);
		timeouts++;
		return -1;
	}

	lock();

	int rc = dmx->Read(buf, MAX_SECTION_LENGTH, timeoutInMSeconds);

	if (rc < 3)
	{
		unlock();
		if (rc <= 0)
		{
			dprintf("dmx.read timeout - filter: %x - timeout# %d\n", filters[filter_index].filter, timeouts);
			if(!seen_section)
				timeouts++;
		}
		else
		{
			dprintf("dmx.read rc: %d - filter: %x\n", rc, filters[filter_index].filter);
			// restart DMX
			real_pause();
			real_unpause();
		}
		return -1;
	}

	/* lets assume buffer size never less than 8, and parse LongSection */
	LongSection section(buf);
	uint16_t section_length =  section.getSectionLength();

	if (section_length <= 0)
	{
		unlock();
		fprintf(stderr, "[sectionsd] section_length <= 0: %d [%s:%s:%d] please report!\n", section_length, __FILE__,__FUNCTION__,__LINE__);
		return -1;
	}

	timeouts = 0;

	if (rc != section_length + 3)
	{
		xprintf("	%s: rc != section_length + 3 (%d != %d + 3)\n", name.c_str(), rc, section_length);
		unlock();
		// DMX restart required? This should never happen anyway.
		real_pause();
		real_unpause();
		return -1;
	}

	uint8_t table_id = section.getTableId();
	// check if the filter worked correctly
	if (((table_id ^ filters[filter_index].filter) & filters[filter_index].mask) != 0)
	{
		xcprintf("	%s: filter 0x%x mask 0x%x -> skip sections for table 0x%x", name.c_str(), filters[filter_index].filter, filters[filter_index].mask, table_id);
		unlock();
		bad_count++;
		if(bad_count >= 5) {
			bad_count = 0;
			timeouts = -1;
			return -1;
		}
		real_pause();
		real_unpause();
		return -1;
	}
	bad_count = 0;
	/* there are channels with very low rate, neutrino change filter on timeouts while data not complete */
	seen_section = true;
	//unlock();

	// skip sections which are too short
	if ((section_length < 5) ||
			(table_id >= 0x4e && table_id <= 0x6f && section_length < 14))
	{
		dprintf("section too short: table %x, length: %d\n", table_id, section_length);
		unlock();
		return -1;
	}

	// check if it's extended syntax, e.g. NIT, BAT, SDT, EIT, only current sections
	if (!section.getSectionSyntaxIndicator() || !section.getCurrentNextIndicator()) {
		unlock();
		return rc;
	}

	uint16_t eh_tbl_extension_id = section.getTableIdExtension();
	uint8_t version_number = section.getVersionNumber();

#if 0
	/* if we are not caching the already read sections (CN-thread), check EIT version and get out */
	if (!cache)
	{
		if (table_id == 0x4e &&
				eh_tbl_extension_id == (current_service & 0xFFFF) &&
				version_number != eit_version) {
			dprintf("EIT old: %d new version: %d\n", eit_version, version_number);
			eit_version = version_number;
		}
		return rc;
	}
#endif
	MyDMXOrderUniqueKey::iterator di;

	uint8_t section_number = section.getSectionNumber();
	uint8_t last_section_number = section.getLastSectionNumber();

	unsigned short current_onid = 0;
	unsigned short current_tsid = 0;
	uint8_t segment_last_section_number = last_section_number;

	if (pID == 0x12) {
		eit_extended_header = (eit_extended_section_header *)(buf+8);
		current_onid = 	eit_extended_header->original_network_id_hi * 256 +
			eit_extended_header->original_network_id_lo;
		current_tsid = 	eit_extended_header->transport_stream_id_hi * 256 +
			eit_extended_header->transport_stream_id_lo;
		segment_last_section_number = eit_extended_header->segment_last_section_number;
	}

	sections_id_t s_id = create_sections_id(table_id, eh_tbl_extension_id, current_onid, current_tsid, section_number);

	bool complete = false;
	if (pID == 0x12)
		complete = check_complete(s_id, section_number, last_section_number, segment_last_section_number);

	/* if we are not caching the already read sections (CN-thread), check EIT version and get out */
	if (table_id == 0x4e &&
			eh_tbl_extension_id == (current_service & 0xFFFF) &&
			version_number != eit_version) {
		dprintf("EIT old: %d new version: %d\n", eit_version, version_number);
		eit_version = version_number;
	}
	//find current section in list
	di = myDMXOrderUniqueKey.find(s_id);
	if (di != myDMXOrderUniqueKey.end())
	{
		//the current section was read before
		if (di->second == version_number) {
                        if (first_skipped == 0) {
                                //the last section was new - this is the 1st dup
                                first_skipped = s_id;
			} else {
				//this is not the 1st new - check if it's the last
				//or to be more precise only dups occured since
				if (first_skipped == s_id)
					timeouts = -1;
			}
#ifdef DEBUG_CACHED_SECTIONS
			printf("[%s] skipped duplicate section for table 0x%02x table_extension 0x%04x section 0x%02x last 0x%02x touts %d\n", name.c_str(),
					table_id, eh_tbl_extension_id, section_number,
					last_section_number, timeouts);
#endif
			rc = -1;
		} else {
#ifdef DEBUG_CACHED_SECTIONS
			printf("[%s] version update from 0x%02x to 0x%02x for table 0x%02x table_extension 0x%04x section 0x%02x\n", name.c_str(),
					di->second, version_number, table_id,
					eh_tbl_extension_id, section_number);
#endif
			//update version number
			di->second = version_number;
		}
	}
	else
	{
		//section was not read before - insert in list
		myDMXOrderUniqueKey.insert(std::make_pair(s_id, version_number));
#ifdef DEBUG_CACHED_SECTIONS
		printf("[%s] new section for table 0x%02x table_extension 0x%04x section 0x%02x last 0x%02x slast 0x%02x\n", name.c_str(),
				table_id, eh_tbl_extension_id,
				section_number, last_section_number, segment_last_section_number);
#endif
	}
	//debug
	if(timeouts == -1) {
		xcprintf("	%s: skipped looped", name.c_str());
	}

	if(complete) {
		seenSections.clear();
		calcedSections.clear();
		timeouts = -2;
	}
	if(rc > 0)
		first_skipped = 0;

	unlock();
	return rc;
}

int DMX::immediate_start(void)
{
#ifdef DEBUG_DEMUX
	xprintf("	%s: DMX::immediate_start, isOpen %d\n", name.c_str(), isOpen());
#endif
	if (isOpen())
	{
		xprintf("	%s: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>DMX::imediate_start: isOpen()<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n", name.c_str());
		closefd();
	}

	if (real_pauseCounter != 0) {
		dprintf("DMX::immediate_start: realPausecounter !=0 (%d)!\n", real_pauseCounter);
		return 0;
	}

	if(dmx == NULL) {
#ifdef DEBUG_DEMUX
		xcprintf("	%s: open demux #%d", name.c_str(), dmx_num);
#endif
		dmx = new cDemux(dmx_num);
		dmx->Open(DMX_PSI_CHANNEL, NULL, dmxBufferSizeInKB*1024UL);
	}
	fd = 1;

	/* setfilter() only if this is no dummy filter... */
	if (filters[filter_index].filter && filters[filter_index].mask)
	{
		unsigned char filter[DMX_FILTER_SIZE];
		unsigned char mask[DMX_FILTER_SIZE];

		filter[0] = filters[filter_index].filter;
		mask[0] = filters[filter_index].mask;
		dmx->sectionFilter(pID, filter, mask, 1);
		//FIXME error check
	}
	/* this is for dmxCN only... */
	eit_version = 0xff;
	return 0;
}

int DMX::start(void)
{
	int rc;

	lock();

	lastChanged = time_monotonic();

	rc = immediate_start();

	unlock();

	return rc;
}

int DMX::real_pause(void)
{
	if (!isOpen()) {
		dprintf("DMX::real_pause: (!isOpen())\n");
		return 1;
	}

	lock();

	if (real_pauseCounter == 0)
	{
		immediate_stop();
	}
	//else
	//	dprintf("real_pause: counter %d\n", real_pauseCounter);

	unlock();

	return 0;
}

int DMX::real_unpause(void)
{
	lock();

	if (real_pauseCounter == 0)
	{
		immediate_start();
		//dprintf("real_unpause DONE: %d\n", real_pauseCounter);
	}
	//else
	//	dprintf("real_unpause NOT DONE: %d\n", real_pauseCounter);

	unlock();

	return 0;
}

#if 0
int DMX::request_pause(void)
{
	real_pause(); // unlocked

	lock();
	//dprintf("request_pause: %d\n", real_pauseCounter);

	real_pauseCounter++;

	unlock();

	return 0;
}

int DMX::request_unpause(void)
{
	lock();

	//dprintf("request_unpause: %d\n", real_pauseCounter);
	--real_pauseCounter;

	unlock();

	real_unpause(); // unlocked

	return 0;
}
#endif

bool DMX::next_filter()
{
	if (filter_index + 1 < (signed) filters.size()) {
		change(filter_index + 1);
		return true;
	}
	return false;
}

const char *dmx_filter_types [] = {
	"dummy filter",
	"actual transport stream, scheduled",
	"other transport stream, now/next",
	"other transport stream, scheduled 1",
	"other transport stream, scheduled 2"
};

int DMX::change(const int new_filter_index, const t_channel_id new_current_service)
{
	if (sections_debug)
		showProfiling("changeDMX: before pthread_mutex_lock(&start_stop_mutex)");
	lock();

	if (sections_debug)
		showProfiling("changeDMX: after pthread_mutex_lock(&start_stop_mutex)");

	filter_index = new_filter_index;
	first_skipped = 0;

	eit_version = 0xff;
	seenSections.clear();
	calcedSections.clear();
	seen_section = false;
	if(!cache)
		myDMXOrderUniqueKey.clear();

	if (new_current_service)
		current_service = new_current_service;

#ifdef DEBUG_DEMUX
	xprintf("	%s: switch to filter %02x current_service %016llx\n\n", name.c_str(), filters[filter_index].filter, current_service);
#endif

	if (real_pauseCounter > 0)
	{
		printf("changeDMX: for 0x%x not ignored! even though real_pauseCounter> 0 (%d)\n",
		       filters[new_filter_index].filter, real_pauseCounter);
		/* immediate_start() checks for real_pauseCounter again (and
		   does nothing in that case), so we can just continue here. */
	}

	if (sections_debug) { // friendly debug output...
		if(pID==0x12 && filters[0].filter != 0x4e) { // Only EIT
			printdate_ms(stderr);
			fprintf(stderr, "changeDMX [EIT]-> %d (0x%x/0x%x) %s (%ld seconds)\n",
				new_filter_index, filters[new_filter_index].filter,
				filters[new_filter_index].mask, dmx_filter_types[new_filter_index],
				time_monotonic()-lastChanged);
		} else {
			printdate_ms(stderr);
			fprintf(stderr, "changeDMX [%x]-> %d (0x%x/0x%x) (%ld seconds)\n", pID,
				new_filter_index, filters[new_filter_index].filter,
				filters[new_filter_index].mask, time_monotonic()-lastChanged);
		}
	}

	closefd();

	int rc = immediate_start();

	if (rc != 0)
	{
		unlock();
		return rc;
	}

	if (sections_debug)
		showProfiling("after DMX_SET_FILTER");

	pthread_cond_signal(&change_cond);

	lastChanged = time_monotonic();

	unlock();

	return 0;
}

int DMX::setPid(const unsigned short new_pid)
{
	lock();

	if (!isOpen())
	{
		pthread_cond_signal(&change_cond);
		unlock();
		return 1;
	}

	if (real_pauseCounter > 0)
	{
		dprintf("changeDMX: for 0x%x ignored! because of real_pauseCounter> 0 (%d)\n", new_pid, real_pauseCounter);
		unlock();
		return 0;	// not running (e.g. streaming)
	}
	closefd();

	pID = new_pid;
	int rc = immediate_start();

	if (rc != 0)
	{
		unlock();
		return rc;
	}

	pthread_cond_signal(&change_cond);

	lastChanged = time_monotonic();

	unlock();

	return 0;
}

int DMX::setCurrentService(t_channel_id new_current_service)
{
	return change(0, new_current_service);
}

int DMX::dropCachedSectionIDs()
{
	lock();
	myDMXOrderUniqueKey.clear();
	unlock();

	return 0;
}
