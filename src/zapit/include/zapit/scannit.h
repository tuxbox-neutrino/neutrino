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

#ifndef __zapit_scan_nit_h__
#define __zapit_scan_nit_h__

#include <OpenThreads/Thread>
#include <dmx.h>
#include <dvbsi++/network_information_section.h>
#include <dvbsi++/satellite_delivery_system_descriptor.h>
#include <dvbsi++/cable_delivery_system_descriptor.h>
#include <dvbsi++/service_list_descriptor.h>

#define NIT_SECTION_SIZE 1024

class CNit : public OpenThreads::Thread
{
	private:
		int dmxnum;
		bool cable;

		t_satellite_position satellitePosition;
		freq_id_t freq_id;
		unsigned short nid;

		NetworkInformationSectionList sections;

		void run();
		bool Read();
		bool ParseSatelliteDescriptor(SatelliteDeliverySystemDescriptor * sd, TransportStreamInfo * ts);
		bool ParseCableDescriptor(CableDeliverySystemDescriptor * sd, TransportStreamInfo * ts);
		bool ParseServiceList(ServiceListDescriptor * sd, TransportStreamInfo * ts);

	public:
		CNit(t_satellite_position spos, freq_id_t frq, unsigned short pnid, int dnum = 0);
		~CNit();
		bool Start();
		bool Stop();
		bool Parse();
};

#endif
