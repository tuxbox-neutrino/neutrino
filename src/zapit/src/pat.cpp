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

#include <zapit/debug.h>
#include <zapit/pat.h>
#include <hardware/dmx.h>

CPat::CPat(int dnum)
{
	parsed = false;
	dmxnum = dnum;
}

CPat::~CPat()
{
	sidpmt.clear();
}

void CPat::Reset()
{
	sidpmt.clear();
	parsed = false;
	ts_id  = 0;
}

bool CPat::Parse()
{
	//INFO("parsed: %s", parsed ? "yes" : "no");
	if(parsed)
		return true;

        dmx = new cDemux(dmxnum);
	dmx->Open(DMX_PSI_CHANNEL);

	/* buffer for program association table */
	unsigned char buffer[PAT_SECTION_SIZE];

	/* current positon in buffer */
	unsigned short i;

	const short crc_len = 4;

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	mask[0] = 0xFF;
	mask[4] = 0xFF;

	do {
		/* set filter for program association section */
		/* read section */
		if ((!dmx->sectionFilter(0, filter, mask, 5)) || (i = dmx->Read(buffer, PAT_SECTION_SIZE) < 0))
		{
			delete dmx;
			printf("[pat.cpp] dmx read failed\n");
			return false;
		}
		/* set Transport_Stream_ID from pat */
		ts_id = ((buffer[3] << 8) | buffer[4]);
		/* loop over service id / program map table pid pairs */
		for (i = 8; i < (((buffer[1] & 0x0F) << 8) | buffer[2]) + 3 - crc_len; i += 4) {
			/* store program map table pid */
			int service_id	= ((buffer[i] << 8) | buffer[i+1]);
			int pmt_pid	= (((buffer[i+2] & 0x1F) << 8) | buffer[i+3]);
			sidpmt.insert(sidpmt_map_pair_t(service_id, pmt_pid)); 
		}
	} while (filter[4]++ != buffer[7]);
	parsed = true;
	delete dmx;
	return true;
}

unsigned short CPat::GetPmtPid(t_service_id sid)
{
	unsigned short pid = 0;

	if(Parse()) {
		sidpmt_map_iterator_t it = sidpmt.find(sid);
		if(it != sidpmt.end())
			pid = it->second;
	}
	if(!pid)
		printf("[pat] sid %04x not found\n", sid);
	return pid;
}

bool CPat::Parse(CZapitChannel * const channel)
{
	unsigned short pid = GetPmtPid(channel->getServiceId());
	if(pid > 0) {
		channel->setPmtPid(pid);
		return true;
	}
	return false;
}
