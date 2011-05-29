/*
 * $Header: /cvs/tuxbox/apps/tuxbox/neutrino/daemons/sectionsd/dmx.cpp,v 1.51 2009/06/14 21:46:03 rhabarber1848 Exp $
 *
 * DMX class (sectionsd) - d-box2 linux project
 *
 * (C) 2001 by fnbrd,
 *     2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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


#include <dmx.h>
#include <dmxapi.h>
#include <debug.h>

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

/*
#define DEBUG_MUTEX 1
#define DEBUG_CACHED_SECTIONS 1
*/

typedef std::map<sections_id_t, version_number_t, std::less<sections_id_t> > MyDMXOrderUniqueKey;
static MyDMXOrderUniqueKey myDMXOrderUniqueKey;

extern void showProfiling(std::string text);

DMX::DMX(const unsigned short p, const unsigned short bufferSizeInKB, const bool c, int dmx_source)
{
	dmx_num = dmx_source;
	fd = -1;
	lastChanged = time_monotonic();
	filter_index = 0;
	pID = p;
	dmxBufferSizeInKB = bufferSizeInKB;
#if HAVE_TRIPLEDRAGON
	/* hack, to keep the TD changes in one place. */
	dmxBufferSizeInKB = 128;	/* 128kB is enough on TD */
	dmx_num = 0;			/* always use demux 0 */
#endif
	pthread_mutex_init(&pauselock, NULL);        // default = fast mutex
#ifdef DEBUG_MUTEX
	pthread_mutexattr_t start_stop_mutex_attr;
	pthread_mutexattr_init(&start_stop_mutex_attr);
	pthread_mutexattr_settype(&start_stop_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&start_stop_mutex, &start_stop_mutex_attr);
#else
	pthread_mutex_init(&start_stop_mutex, NULL); // default = fast mutex
#endif
	pthread_cond_init (&change_cond, NULL);
	real_pauseCounter = 0;
	current_service = 0;

	first_skipped = 0;
	cache = c;
}

DMX::~DMX()
{
	first_skipped = 0;
	myDMXOrderUniqueKey.clear();
	pthread_mutex_destroy(&pauselock);
	pthread_mutex_destroy(&start_stop_mutex);
	pthread_cond_destroy (&change_cond);
	closefd();
}

ssize_t DMX::read(char * const /*buf*/, const size_t /*buflength*/, const unsigned /*timeoutMInSeconds*/)
{
	//FIXME is this used ??
	printf("[sectionsd] ******************************* DMX::read called *******************************\n");
	return 0;
	//return readNbytes(fd, buf, buflength, timeoutMInSeconds);
}

void DMX::close(void)
{
	if(dmx)
		delete dmx;
	dmx = NULL;
}

void DMX::closefd(void)
{
	if (isOpen())
	{
		//close(fd);
#if HAVE_TRIPLEDRAGON
		dmx->Close();
#else
		dmx->Stop();
#endif
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

sections_id_t DMX::create_sections_id(const unsigned char table_id, const unsigned short extension_id, const unsigned char section_number, const unsigned short onid, const unsigned short tsid)
{
	return 	(sections_id_t) (	((sections_id_t) table_id 	<< 56) |
					((sections_id_t) extension_id 	<< 40) |
					((sections_id_t) section_number << 32) |
					((sections_id_t) onid		<< 16) |
					((sections_id_t) tsid));
}

#ifdef USE_CHECK_COMPLETE_EVEN_THOUGH_IT_SEEMS_BROKEN
bool DMX::check_complete(const unsigned char table_id, const unsigned short extension_id, const unsigned short onid, const unsigned short tsid, const unsigned char last)
{
	int current_section_number = 0;

	if (((table_id == 0x4e) || (table_id == 0x50)) && (current_service == extension_id)) {
		if (last == 0)
			return true;
		MyDMXOrderUniqueKey::iterator di = myDMXOrderUniqueKey.find(create_sections_id(
				table_id,
				extension_id,
				current_section_number,
				onid,
				tsid));
		if (di != myDMXOrderUniqueKey.end()) {
			di++;
		}
		while ((di != myDMXOrderUniqueKey.end()) && ((uint8_t) ((di->first >> 56) & 0xff) == table_id) &&
				((uint16_t) ((di->first >> 40) & 0xffff) == extension_id) &&
				(((uint8_t) ((di->first >> 32) & 0xff) == current_section_number + 1) ||
				 ((uint8_t) ((di->first >> 32) & 0xff) == current_section_number + 8)) &&
				((uint16_t) ((di->first >> 16) & 0xffff) == onid) &&
				((uint16_t) (di->first & 0xffff) == tsid))
		{
			if ((uint8_t) ((di->first >> 32) & 0xff) == last) {
				return true;
			}
			else {
				current_section_number = (uint8_t) (di->first >> 32) & 0xff;
				di++;
			}
		}
	}
	return false;
}
#else
/* the above version does not seem to work well for table 0x5[0-f], signalling
 * completeness before it is actually complete
 * additionally it seems to have problems with table version updates
 * finally, it saves only little compared to the "HAVE_ALL_SECTIONS" check,
 * so it is probably cheaper to just skip it
 */
bool DMX::check_complete(const unsigned char, const unsigned short, const unsigned short, const unsigned short, const unsigned char)
{
	return false;
}
#endif

int DMX::getSection(char *buf, const unsigned timeoutInMSeconds, int &timeouts)
{
	struct minimal_section_header {
		unsigned table_id                 : 8;
#if __BYTE_ORDER == __BIG_ENDIAN
		unsigned section_syntax_indicator : 1;
		unsigned reserved_future_use      : 1;
		unsigned reserved1                : 2;
		unsigned section_length_hi        : 4;
#else
		unsigned section_length_hi        : 4;
		unsigned reserved1                : 2;
		unsigned reserved_future_use      : 1;
		unsigned section_syntax_indicator : 1;
#endif
		unsigned section_length_lo        : 8;
	} __attribute__ ((packed));  // 3 bytes total

	struct extended_section_header {
		unsigned table_extension_id_hi    : 8;
		unsigned table_extension_id_lo    : 8;
#if __BYTE_ORDER == __BIG_ENDIAN
		unsigned reserved		  : 2;
		unsigned version_number		  : 5;
		unsigned current_next_indicator	  : 1;
#else
		unsigned current_next_indicator	  : 1;
		unsigned version_number		  : 5;
		unsigned reserved		  : 2;
#endif
		unsigned section_number		  : 8;
		unsigned last_section_number	  : 8;
	} __attribute__ ((packed));  // 5 bytes total

	struct eit_extended_section_header {
		unsigned transport_stream_id_hi	  : 8;
		unsigned transport_stream_id_lo	  : 8;
		unsigned original_network_id_hi   : 8;
		unsigned original_network_id_lo   : 8;
		unsigned segment_last_section_number : 8;
		unsigned last_table_id		  : 8;
	} __attribute__ ((packed));  // 6 bytes total

	minimal_section_header *initial_header;
	extended_section_header *extended_header;
	eit_extended_section_header *eit_extended_header;
	int    rc;
	short section_length = 0;
	unsigned short current_onid = 0;
	unsigned short current_tsid = 0;

	/* filter == 0 && maks == 0 => EIT dummy filter to slow down EIT thread startup */
	if (pID == 0x12 && filters[filter_index].filter == 0 && filters[filter_index].mask == 0)
	{
		//dprintf("dmx: dummy filter, sleeping for %d ms\n", timeoutInMSeconds);
		usleep(timeoutInMSeconds * 1000);
		timeouts++;
		return -1;
	}

	lock();

	//rc = read(buf, 4098, timeoutInMSeconds);
	rc = dmx->Read((unsigned char *) buf, 4098, timeoutInMSeconds);

	if (rc < 3)
	{
		unlock();
		if (rc <= 0)
		{
			dprintf("dmx.read timeout - filter: %x - timeout# %d\n", filters[filter_index].filter, timeouts);
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

	initial_header = (minimal_section_header*)buf;
	section_length = (initial_header->section_length_hi * 256) | initial_header->section_length_lo;

	if (section_length <= 0)
	{
		unlock();
		fprintf(stderr, "[sectionsd] section_length <= 0: %d [%s:%s:%d] please report!\n", section_length, __FILE__,__FUNCTION__,__LINE__);
		return -1;
	}

	timeouts = 0;

	if (rc != section_length + 3)
	{
		xprintf("rc != section_length + 3 (%d != %d + 3)\n", rc, section_length);
		unlock();
		// DMX restart required? This should never happen anyway.
		real_pause();
		real_unpause();
		return -1;
	}

	// check if the filter worked correctly
	if (((initial_header->table_id ^ filters[filter_index].filter) & filters[filter_index].mask) != 0)
	{
		printf("[sectionsd] filter 0x%x mask 0x%x -> skip sections for table 0x%x\n", filters[filter_index].filter, filters[filter_index].mask, initial_header->table_id);
		unlock();
		real_pause();
		real_unpause();
		return -1;
	}

	unlock();
	// skip sections which are too short
	if ((section_length < 5) ||
			(initial_header->table_id >= 0x4e && initial_header->table_id <= 0x6f && section_length < 14))
	{
		dprintf("section too short: table %x, length: %d\n", initial_header->table_id, section_length);
		return -1;
	}

	// check if it's extended syntax, e.g. NIT, BAT, SDT, EIT
	if (initial_header->section_syntax_indicator != 0)
	{
		extended_header = (extended_section_header *)(buf+3);

		// only current sections
		if (extended_header->current_next_indicator != 0) {
			// if ((initial_header.table_id >= 0x4e) && (initial_header.table_id <= 0x6f))
			if (pID == 0x12) {
				eit_extended_header = (eit_extended_section_header *)(buf+8);
				current_onid = 	eit_extended_header->original_network_id_hi * 256 +
						eit_extended_header->original_network_id_lo;
				current_tsid = 	eit_extended_header->transport_stream_id_hi * 256 +
						eit_extended_header->transport_stream_id_lo;
			}
			else {
				current_onid = 0;
				current_tsid = 0;
			}

			int eh_tbl_extension_id = extended_header->table_extension_id_hi * 256
						  + extended_header->table_extension_id_lo;

			/* if we are not caching the already read sections (CN-thread), check EIT version and get out */
			if (!cache)
			{
				if (initial_header->table_id == 0x4e &&
						eh_tbl_extension_id == current_service &&
						extended_header->version_number != eit_version) {
					dprintf("EIT old: %d new version: %d\n",eit_version,extended_header->version_number);
					eit_version = extended_header->version_number;
				}
				return rc;
			}

			// the current section
			sections_id_t s_id = create_sections_id(initial_header->table_id,
								eh_tbl_extension_id,
								extended_header->section_number,
								current_onid,
								current_tsid);
			//find current section in list
			MyDMXOrderUniqueKey::iterator di = myDMXOrderUniqueKey.find(s_id);
			if (di != myDMXOrderUniqueKey.end())
			{
				//the current section was read before
				if (di->second == extended_header->version_number) {
#ifdef DEBUG_CACHED_SECTIONS
					dprintf("[sectionsd] skipped duplicate section for table 0x%02x table_extension 0x%04x section 0x%02x\n",
						initial_header->table_id,
						eh_tbl_extension_id,
						extended_header->section_number);
#endif
					//the version number is still up2date
					if (first_skipped == 0) {
						//the last section was new - this is the 1st dup
						first_skipped = s_id;
					}
					else {
						//this is not the 1st new - check if it's the last
						//or to be more precise only dups occured since
						if (first_skipped == s_id)
							timeouts = -1;
					}
					//since version is still up2date, check if table complete
					if (check_complete(initial_header->table_id,
							   eh_tbl_extension_id,
							   current_onid,
							   current_tsid,
							   extended_header->last_section_number))
						timeouts = -2;
					return -1;
				}
				else {
#ifdef DEBUG_CACHED_SECTIONS
					dprintf("[sectionsd] version update from 0x%02x to 0x%02x for table 0x%02x table_extension 0x%04x section 0x%02x\n",
						di->second,
						extended_header->version_number,
						initial_header->table_id,
						eh_tbl_extension_id,
						extended_header->section_number);
#endif
					//update version number
					di->second = extended_header->version_number;
				}
			}
			else
			{
#ifdef DEBUG_CACHED_SECTIONS
				dprintf("[sectionsd] new section for table 0x%02x table_extension 0x%04x section 0x%02x\n",
					initial_header->table_id,
					eh_tbl_extension_id,
					extended_header->section_number);
#endif
				//section was not read before - insert in list
				myDMXOrderUniqueKey.insert(std::make_pair(s_id, extended_header->version_number));
				//check if table is now complete
				if (check_complete(initial_header->table_id,
						   eh_tbl_extension_id,
						   current_onid,
						   current_tsid,
						   extended_header->last_section_number))
					timeouts = -2;
			}
			//if control comes to here the sections skipped counter must be restarted
			first_skipped = 0;
		}
	}

	return rc;
}

int DMX::immediate_start(void)
{
	if (isOpen())
	{
		xprintf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>DMX::imediate_start: isOpen()<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		closefd();
	}

	if (real_pauseCounter != 0) {
		dprintf("DMX::immediate_start: realPausecounter !=0 (%d)!\n", real_pauseCounter);
		return 0;
	}
#if 0
	if ((fd = open(DEMUX_DEVICE, O_RDWR|O_NONBLOCK)) == -1)
	{
		perror("[sectionsd] open dmx");
		return 2;
	}

	if (ioctl(fd, DMX_SET_BUFFER_SIZE, (unsigned long)(dmxBufferSizeInKB*1024UL)) == -1)
	{
		closefd();
		perror("[sectionsd] DMX: DMX_SET_BUFFER_SIZE");
		return 3;
	}
#endif
	if(dmx == NULL) {
		dmx = new cDemux(dmx_num);
#if !HAVE_TRIPLEDRAGON
		dmx->Open(DMX_PSI_CHANNEL, NULL, dmxBufferSizeInKB*1024UL);
#endif
	}
#if HAVE_TRIPLEDRAGON
	dmx->Open(DMX_PSI_CHANNEL, NULL, dmxBufferSizeInKB*1024UL);
#endif
	fd = 1;

	/* setfilter() only if this is no dummy filter... */
#if 0
	if (filters[filter_index].filter && filters[filter_index].mask &&
			!setfilter(fd, pID, filters[filter_index].filter, filters[filter_index].mask, DMX_IMMEDIATE_START | DMX_CHECK_CRC))
#endif
		if (filters[filter_index].filter && filters[filter_index].mask)
		{
			unsigned char filter[DMX_FILTER_SIZE];
			unsigned char mask[DMX_FILTER_SIZE];

			filter[0] = filters[filter_index].filter;
			mask[0] = filters[filter_index].mask;
			dmx->sectionFilter(pID, filter, mask, 1);
			//FIXME error check
			//closefd();
			//return 4;
		}
	/* this is for dmxCN only... */
	eit_version = 0xff;
	return 0;
}

int DMX::start(void)
{
	int rc;

	lock();

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

#if 0
to be removed....
int DMX::pause(void)
{
	pthread_mutex_lock(&pauselock);

	//dprintf("lock from pc: %d\n", pauseCounter);
	pauseCounter++;

	pthread_mutex_unlock(&pauselock);

	return 0;
}

int DMX::unpause(void)
{
	pthread_mutex_lock(&pauselock);

	//dprintf("unlock from pc: %d\n", pauseCounter);
	--pauseCounter;

	pthread_mutex_unlock(&pauselock);

	return 0;
}
#endif

const char *dmx_filter_types [] = {
	"dummy filter",
	"actual transport stream, scheduled",
	"other transport stream, now/next",
	"other transport stream, scheduled 1",
	"other transport stream, scheduled 2",
	"dummy filter2"
};

int DMX::change(const int new_filter_index, const int new_current_service)
{
	if (sections_debug)
		showProfiling("changeDMX: before pthread_mutex_lock(&start_stop_mutex)");
	lock();

	if (sections_debug)
		showProfiling("changeDMX: after pthread_mutex_lock(&start_stop_mutex)");

	filter_index = new_filter_index;
	first_skipped = 0;

#if 0
	/* i have to think about this. This #if 0 now makes .change() automatically unpause the
	 * demux.  No idea if there are negative side effects - we will find out :)  -- seife
	 */
	if (!isOpen())
	{
		pthread_cond_signal(&change_cond);
		unlock();
		dprintf("DMX::change(%d): not open!\n",new_filter_index);
		return 1;
	}
#endif
	if (new_current_service != -1)
		current_service = new_current_service;

	if (real_pauseCounter > 0)
	{
		printf("changeDMX: for 0x%x not ignored! even though real_pauseCounter> 0 (%d)\n",
		       filters[new_filter_index].filter, real_pauseCounter);
		/* immediate_start() checks for real_pauseCounter again (and
		   does nothing in that case), so we can just continue here. */
	}

	if (1 /*sections_debug*/) { // friendly debug output...
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

// Liest n Bytes aus einem Socket per read
// Liefert 0 bei timeout
// und -1 bei Fehler
// ansonsten die Anzahl gelesener Bytes
/* inline */
ssize_t DMX::readNbytes(int _fd, char *buf, const size_t n, unsigned timeoutInMSeconds)
{
	int rc;
	struct pollfd ufds;
	ufds.fd = _fd;
	ufds.events = POLLIN;
	ufds.revents = 0;

	rc = ::poll(&ufds, 1, timeoutInMSeconds);

	if (!rc)
		return 0; // timeout
	else if (rc < 0)
	{
		/* we consciously ignore EINTR, since it does not happen in practice */
		perror ("[sectionsd] DMX::readNbytes poll");
		return -1;
	}
	if ((ufds.revents & POLLERR) != 0) /* POLLERR means buffer error, i.e. buffer overflow */
	{
		printdate_ms(stderr);
		fprintf(stderr, "[sectionsd] DMX::readNbytes received POLLERR, pid 0x%x, filter[%d] "
			"filter 0x%02x mask 0x%02x\n", pID, filter_index,
			filters[filter_index].filter, filters[filter_index].mask);
		return -1;
	}
	if (!(ufds.revents&POLLIN))
	{
		xprintf("%s: not ufds.revents&POLLIN, please report!\n", __FUNCTION__);
		// POLLHUP, beim dmx bedeutet das DMXDEV_STATE_TIMEDOUT
		// kommt wenn ein Timeout im Filter gesetzt wurde
		// dprintf("revents: 0x%hx\n", ufds.revents);
		// usleep(100*1000UL); // wir warten 100 Millisekunden bevor wir es nochmal probieren
		// if (timeoutInMSeconds <= 200000)
		return 0; // timeout
		// timeoutInMSeconds -= 200000;
		// goto retry;
	}

	int r = ::read(_fd, buf, n);

	if (r >= 0)
		return r;

	perror ("[sectionsd] DMX::readNbytes read");
	return -1;
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

int DMX::setCurrentService(int new_current_service)
{
	return change(0, new_current_service);
}

int DMX::dropCachedSectionIDs()
{
	lock();

	/* i think that those checks are wrong for dropCachedSectionIDs(), since
	   this is called from the housekeeping thread while sectionsd might be
	   idle waiting for the EIT update filter to trigger -- seife
	 */
#if 0
	if (!isOpen())
	{
		pthread_cond_signal(&change_cond);
		unlock();
		return 1;
	}

	if (real_pauseCounter > 0)
	{
		unlock();
		return 0;	// not running (e.g. streaming)
	}
	closefd();
#endif

	myDMXOrderUniqueKey.clear();

#if 0
	int rc = immediate_start();

	if (rc != 0)
	{
		unlock();
		return rc;
	}
#endif

	pthread_cond_signal(&change_cond);

	unlock();

	return 0;
}

unsigned char DMX::get_eit_version()
{
	return eit_version;
}

unsigned int DMX::get_current_service()
{
	return current_service;
}
