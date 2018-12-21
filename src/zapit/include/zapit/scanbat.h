/*
 * Neutrino-GUI  -   DBoxII-Project
 *
 * Copyright (C) 2011 CoolStream International Ltd
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

#ifndef __zapit_scan_bat_h__
#define __zapit_scan_bat_h__

#include <OpenThreads/Thread>
#include <hardware/dmx.h>
#include <dvbsi++/bouquet_association_section.h>
#include <dvbsi++/service_list_descriptor.h>
#include <dvbsi++/logical_channel_descriptor.h>

#define BAT_SECTION_SIZE 4098

typedef std::map <t_channel_id, int> channel_number_map_t;
typedef std::map <std::string, std::set<t_channel_id> > bouquet_map_t;

class CBat : public OpenThreads::Thread
{
	private:
		int dmxnum;

		t_satellite_position satellitePosition;
		freq_id_t freq_id;
		channel_number_map_t logical_map;
		bouquet_map_t bouquet_map;
		std::string bouquetName;

		BouquetAssociationSectionList sections;

		void run();
		bool Read();
		bool ParseServiceList(ServiceListDescriptor * sd, BouquetAssociation * b);
		bool ParseLogicalChannels(LogicalChannelDescriptor * ld, BouquetAssociation * b);

	public:
		CBat(t_satellite_position spos, freq_id_t frq, int dnum = 0);
		~CBat();
		bool Start();
		bool Stop();
		bool Parse();
		channel_number_map_t & getLogicalMap() { return logical_map; }
		bouquet_map_t & getBouquets() { return bouquet_map; }
};

#endif
