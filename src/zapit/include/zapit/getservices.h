/*
 * $Id: getservices.h,v 1.51.2.7 2003/06/10 12:37:19 digi_casi Exp $
 *
 * (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
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

#ifndef __getservices_h__
#define __getservices_h__

#include <linux/dvb/frontend.h>

#include <eventserver.h>

#include "ci.h"
#include "descriptors.h"
#include "sdt.h"
#include "types.h"
#include <xmltree/xmlinterface.h>
#include <zapit/channel.h>
#include <zapit/bouquets.h>
#include <zapit/satconfig.h>

#include <map>
#define zapped_chan_is_nvod 0x80

struct transponder
{
	t_transport_stream_id transport_stream_id;
	t_original_network_id original_network_id;
	struct dvb_frontend_parameters feparams;
	unsigned char polarization;
	bool updated;

	transponder (t_transport_stream_id p_transport_stream_id, struct dvb_frontend_parameters p_feparams)
	{
		transport_stream_id = p_transport_stream_id;
		feparams = p_feparams;
		polarization = 0;
		original_network_id = 0;
		updated = 0;
	}

	transponder(const t_transport_stream_id p_transport_stream_id, const t_original_network_id p_original_network_id, const struct dvb_frontend_parameters p_feparams, const uint8_t p_polarization = 0)
        {
                transport_stream_id = p_transport_stream_id;
                original_network_id = p_original_network_id;
                feparams            = p_feparams;
                polarization        = p_polarization;
		updated = 0;
        }

	transponder (t_transport_stream_id p_transport_stream_id, struct dvb_frontend_parameters p_feparams, unsigned short p_polarization, t_original_network_id p_original_network_id)
	{
		transport_stream_id = p_transport_stream_id;
		feparams = p_feparams;
		polarization = p_polarization;
		original_network_id = p_original_network_id;
		updated = 0;
	}
};

typedef std::map <transponder_id_t, transponder> transponder_list_t;
typedef std::map <transponder_id_t, transponder>::iterator stiterator;
extern transponder_list_t scantransponders;
extern transponder_list_t transponders;

typedef map<t_channel_id, CZapitChannel> channel_map_t;
typedef channel_map_t::iterator channel_map_iterator_t;
typedef std::pair<t_channel_id, CZapitChannel> channel_pair_t;
typedef std::pair<channel_map_iterator_t,bool> channel_insert_res_t;

class CServiceManager
{
	private:
		xmlDocPtr scanInputParser;

		int service_count;
		int tp_count;
		uint32_t fake_tid;
		uint32_t fake_nid;
		int newfound;

		tallchans allchans;
		tallchans curchans;
		tallchans nvodchannels;

		bool ParseScanXml();
		void ParseTransponders(xmlNodePtr node, t_satellite_position satellitePosition, bool cable);
		void ParseChannels(xmlNodePtr node, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
		void FindTransponder(xmlNodePtr search);
		void ParseSatTransponders(fe_type_t frontendType, xmlNodePtr search, t_satellite_position satellitePosition);
		int LoadMotorPositions(void);

		void WriteSatHeader(FILE * fd, sat_config_t &config);
		void WriteTransponderHeader(FILE * fd, struct transponder &tp);
		void WriteCurrentService(FILE * fd, bool &satfound, bool &tpdone,
				bool &updated, char * satstr, struct transponder &tp, CZapitChannel &channel, const char * action);

		static CServiceManager * manager;
		CServiceManager();

	public:
		~CServiceManager();
		static CServiceManager * getInstance();

		static void CopyFile(char * from, char * to);

		void InitSatPosition(t_satellite_position position);
		bool LoadServices(bool only_current);
		void SaveServices(bool tocopy);
		void SaveMotorPositions();
		bool SaveCurrentServices(transponder_id_t tpid);

		bool AddChannel(CZapitChannel * &channel);
		bool AddCurrentChannel(CZapitChannel * &channel);
		bool AddNVODChannel(CZapitChannel * &channel);

		void ResetChannelNumbers();
		void RemoveChannel(const t_channel_id channel_id);
		void RemoveAllChannels();
		void RemoveCurrentChannels();
		void RemoveNVODChannels();

		CZapitChannel* FindChannel(const t_channel_id channel_id, bool * current_is_nvod = NULL);
		CZapitChannel* FindChannelByName(std::string name);
		CZapitChannel* FindCurrentChannel(const t_channel_id channel_id);

		std::string GetServiceName(t_channel_id channel_id);

		bool GetAllRadioChannels(ZapitChannelList &list);
		bool GetAllTvChannels(ZapitChannelList &list);
		bool GetAllHDChannels(ZapitChannelList &list);
		bool GetAllSatelliteChannels(ZapitChannelList &list, t_satellite_position position);
		bool GetAllUnusedChannels(ZapitChannelList &list);

		xmlDocPtr ScanXml();
};
#endif /* __getservices_h__ */
