/*
 * $Id: cam.h,v 1.25 2003/02/09 19:22:08 thegoodguy Exp $
 *
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>,
 *               thegoodguy         <thegoodguy@berlios.de>
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

#ifndef __capmt_h__
#define __capmt_h__

#include <basicclient.h>
#include "types.h"
#include <OpenThreads/Mutex>
#include <zapit/channel.h>
#include <dvbsi++/ca_program_map_section.h>
#include <dmx_cs.h>

#define DEMUX_DECODE_0 1
#define DEMUX_DECODE_1 2
#define DEMUX_DECODE_2 4

#define DEMUX_SOURCE_0 0
#define DEMUX_SOURCE_1 1
#define DEMUX_SOURCE_2 2

#define LIVE_DEMUX	0
#define STREAM_DEMUX	1
#define RECORD_DEMUX	2
#define PIP_DEMUX	3


class CCam : public CBasicClient
{
	private:
		virtual unsigned char getVersion(void) const;
		virtual const char *getSocketName(void) const;
		int camask, demuxes[MAX_DMX_UNITS];
		int source_demux;
		uint8_t cabuf[2048];
		int calen;

	public:
		enum ca_pmt_list_management
		{
			CAPMT_MORE	= 0x00,
			CAPMT_FIRST	= 0x01,
			CAPMT_LAST	= 0x02,
			CAPMT_ONLY	= 0x03,
			CAPMT_ADD	= 0x04,
			CAPMT_UPDATE	= 0x05
		};
		CCam();
		virtual ~CCam() {};
		bool sendMessage(const char * const data, const size_t length, bool update = false);
		bool makeCaPmt(CZapitChannel * channel, bool add_private, uint8_t list = CAPMT_ONLY, const CaIdVector &caids = CaIdVector());
		bool setCaPmt(bool update = false);
		bool sendCaPmt(uint64_t tpid, uint8_t *rawpmt, int rawlen);
		int  makeMask(int demux, bool add);
		int  getCaMask(void) { return camask; }
		void setCaMask(int mask) { camask = mask; }
		int  getSource() { return source_demux; }
		void setSource(int demux) { source_demux = demux; }
};

typedef std::map<t_channel_id, CCam*> cammap_t;
typedef cammap_t::iterator cammap_iterator_t;

class CCamManager
{
	public:
		enum runmode {
			PLAY,
			RECORD,
			STREAM,
			PIP
		};
	private:
		cammap_t		channel_map;
		OpenThreads::Mutex	mutex;
		static CCamManager *	manager;
		bool SetMode(t_channel_id id, enum runmode mode, bool enable, bool force_update = false);

	public:
		CCamManager();
		~CCamManager();
		static CCamManager * getInstance(void);
		bool Start(t_channel_id id, enum runmode mode, bool force_update = false) { return SetMode(id, mode, true, force_update); };
		bool Stop(t_channel_id id, enum runmode mode) { return SetMode(id, mode, false); };

};
#endif /* __capmt_h__ */
