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

#ifndef __zapit_pat_h__
#define __zapit_pat_h__

#include <zapit/channel.h>
#include <hardware/dmx.h>

#define PAT_SECTION_SIZE 1024

typedef std::map<int,int> sidpmt_map_t;
typedef sidpmt_map_t::iterator sidpmt_map_iterator_t;
typedef std::pair<int,int> sidpmt_map_pair_t;

class CPat
{
	private:
		int dmxnum;
		t_transport_stream_id ts_id;
		bool parsed;
		sidpmt_map_t sidpmt;

	public:
		CPat(int dnum = 0);
		~CPat();
		void Reset();
		bool Parse();
		t_transport_stream_id GetPatTransportStreamId(){ return ts_id;}
		unsigned short GetPmtPid(t_service_id sid);
		bool Parse(CZapitChannel * const channel);
		sidpmt_map_t &getSids() { return sidpmt; };
};

#endif /* __zapit_pat_h__ */
