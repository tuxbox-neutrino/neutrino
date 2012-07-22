/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef __remotecontrol__
#define __remotecontrol__

#include <driver/rcinput.h> /* neutrino_msg_t, neutrino_msg_data_t */

#include <zapit/client/zapitclient.h>

#include <vector>
#include <string>

struct st_rmsg
{
	unsigned char version;
	unsigned char cmd;
	unsigned char param;
	unsigned short param2;
	char param3[30];
};

class CSubService
{
 private:
	struct CZapitClient::commandAddSubServices service;
	t_satellite_position satellitePosition;

 public:
	time_t      startzeit;
	unsigned    dauer;
	std::string subservice_name;

	CSubService(const t_original_network_id, const t_service_id, const t_transport_stream_id, const std::string &asubservice_name);
	CSubService(const t_original_network_id, const t_service_id, const t_transport_stream_id, const time_t astartzeit, const unsigned adauer);

	t_channel_id                               getChannelID        (void) const;
	inline const struct CZapitClient::commandAddSubServices getAsZapitSubService(void) const { return service;           }
};

typedef std::vector<CSubService> CSubServiceListSorted;

class CRemoteControl
{
//	unsigned int            current_programm_timer;
	uint64_t		zap_completion_timeout;
	std::string             current_channel_name;
	t_channel_id            current_sub_channel_id;

	void getNVODs();
	void getSubChannels();
	void copySubChannelsToZapit(void);

public:
	t_channel_id                  current_channel_id;
	uint64_t		      current_EPGid;
	//uint64_t                      next_EPGid;
	CZapitClient::responseGetPIDs current_PIDs;

	// APID - Details
	bool                          has_ac3;
	bool                          has_unresolved_ctags;

	// SubChannel/NVOD - Details
	CSubServiceListSorted         subChannels;
	int                           selected_subchannel;
	bool                          are_subchannels;
	bool                          needs_nvods;
	int                           director_mode;

	// Video / Parental-Lock
	bool                          is_video_started;

	CRemoteControl();
	void zapTo_ChannelID(const t_channel_id channel_id, const std::string & channame, const bool start_video = true); // UTF-8
	void startvideo();
	void stopvideo();
	void queryAPIDs();
	void setAPID(uint32_t APID);
	void processAPIDnames();
	const std::string & setSubChannel(const int numSub, const bool force_zap = false);
	const std::string & subChannelUp(void);
	const std::string & subChannelDown(void);

	void radioMode();
	void tvMode();

	int handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data);
	inline const std::string & getCurrentChannelName(void) const { return current_channel_name; }
};


#endif
