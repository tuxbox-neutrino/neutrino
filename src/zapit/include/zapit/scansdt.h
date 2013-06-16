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

#ifndef __zapit_scan_sdt_h__
#define __zapit_scan_sdt_h__

#include <zapit/pat.h>
#include <dmx.h>
#include <dvbsi++/service_description_section.h>
#include <dvbsi++/service_descriptor.h>

#define SDT_SECTION_SIZE 4096

class CSdt
{
	private:
		int dmxnum;
		bool cable;

		t_transport_stream_id transport_stream_id;
		t_original_network_id original_network_id;
		t_satellite_position satellitePosition;
		freq_id_t freq_id;
		std::string lastProviderName;

		ServiceDescriptionSectionList sections;
		CPat pat;

		/* current sdt to check updates */
		bool current;
		transponder_id_t current_tp_id;

		uint8_t FixServiceType(uint8_t type);
		bool CheckScanType(uint8_t service_type);
		CZapitChannel * CheckChannelId(t_service_id service_id);
		void FixWhiteSpaces(std::string &str);
		bool AddToBouquet(std::string &providerName, CZapitChannel *channel);

		bool Read();
		bool ParseServiceDescriptor(ServiceDescription * service, ServiceDescriptor * sd);

	public:
		CSdt(t_satellite_position spos, freq_id_t frq, bool curr = false, int dnum = 0);
		~CSdt();
		bool Parse(t_transport_stream_id &tsid, t_original_network_id &onid);
};

#endif
