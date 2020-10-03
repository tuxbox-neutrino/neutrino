/*
 * DMX class (sectionsd) - d-box2 linux project
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
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

#ifndef __sectionsd__dmx_h__
#define __sectionsd__dmx_h__

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <vector>
#include <config.h>

#include <hardware/dmx.h>

#include <zapit/types.h>
#include <set>
#include <map>
#include <string>

#define MAX_SECTION_LENGTH (0x0fff + 3)

typedef uint64_t sections_id_t;
typedef unsigned char version_number_t;

typedef std::set<sections_id_t> section_map_t;
typedef std::map<sections_id_t, version_number_t, std::less<sections_id_t> > MyDMXOrderUniqueKey;

class DMX
{
protected:

	cDemux *	dmx;
	int		dmx_num;
	unsigned short  pID;
	unsigned short  dmxBufferSizeInKB;
	sections_id_t	first_skipped;
	t_channel_id	current_service;
	unsigned char	eit_version;
	bool		cache; /* should read sections be cached? true for all but dmxCN */
	int		real_pauseCounter;
	std::string 	name;

	/* flag to check if there was section for that table, if yes, dont increase timeouts */
	bool		seen_section;

	inline bool isOpen(void) { return (dmx != NULL); }

	int immediate_start(void); /* mutex must be locked before and unlocked after this method */
	int immediate_stop(void);  /* mutex must be locked before and unlocked after this method */

	bool next_filter();
	void init();

	section_map_t seenSections;
	section_map_t calcedSections;
	MyDMXOrderUniqueKey myDMXOrderUniqueKey;
	bool check_complete(sections_id_t sectionNo, uint8_t number, uint8_t last, uint8_t segment_last);
public:
	struct s_filters
	{
		unsigned char filter;
		unsigned char mask;
	};

	std::vector<s_filters> filters;
	int                    filter_index;
	time_t                 lastChanged;

	pthread_cond_t         change_cond;
	pthread_mutex_t        start_stop_mutex;

	DMX();
	DMX(const unsigned short p, const unsigned short bufferSizeInKB, const bool cache = true, int dmx_source = 0);
	~DMX();

	int start(void);
	void closefd(void);
	void addfilter(const unsigned char filter, const unsigned char mask);
	int stop(void);
	void close(void);

	int real_pause(void);
	int real_unpause(void);
#if 0
	int request_pause(void);
	int request_unpause(void);
#endif
	int change(const int new_filter_index, const t_channel_id new_current_service = 0); // locks while changing

	void lock(void);
	void unlock(void);

	// section with size < 3 + 5 are skipped !
	int getSection(uint8_t *buf, const unsigned timeoutInMSeconds, int &timeouts);
	int setPid(const unsigned short new_pid);
	int setCurrentService(t_channel_id new_current_service);
	int dropCachedSectionIDs();

	unsigned char get_eit_version(void) { return eit_version; }
	// was useful for debugging...
	t_channel_id get_current_service(void) { return current_service; }
	void setDemux(int dnum) { dmx_num = dnum; }
};

#endif /* __sectionsd__dmx_h__ */

