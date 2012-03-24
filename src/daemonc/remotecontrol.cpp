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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <daemonc/remotecontrol.h>

#include <global.h>
#include <neutrino.h>
#include <gui/infoviewer.h>

#include <driver/encoding.h>
#include <driver/record.h>
#include "libdvbsub/dvbsub.h"
#include "libtuxtxt/teletext.h"

#include <zapit/channel.h>
#include <zapit/bouquets.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>

extern CBouquetManager *g_bouquetManager;

extern uint32_t scrambled_timer;

bool sectionsd_getComponentTagsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::ComponentTagList& tags);
bool sectionsd_getLinkageDescriptorsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::LinkageDescriptorList& descriptors);
bool sectionsd_getNVODTimesServiceKey(const t_channel_id uniqueServiceKey, CSectionsdClient::NVODTimesList& nvod_list);
void sectionsd_setPrivatePid(unsigned short pid);

CSubService::CSubService(const t_original_network_id anoriginal_network_id, const t_service_id aservice_id, const t_transport_stream_id atransport_stream_id, const std::string &asubservice_name)
{
	service.original_network_id = anoriginal_network_id;
	service.service_id          = aservice_id;
	service.transport_stream_id = atransport_stream_id;
	startzeit                   = 0;
	dauer                       = 0;
	subservice_name             = asubservice_name;

	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	satellitePosition = channel ? channel->getSatellitePosition() : 0;
}

CSubService::CSubService(const t_original_network_id anoriginal_network_id, const t_service_id aservice_id, const t_transport_stream_id atransport_stream_id, const time_t astartzeit, const unsigned adauer)
{
	service.original_network_id = anoriginal_network_id;
	service.service_id          = aservice_id;
	service.transport_stream_id = atransport_stream_id;
	startzeit                   = astartzeit;
	dauer                       = adauer;
	subservice_name             = "";

	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	satellitePosition = channel ? channel->getSatellitePosition() : 0;
}

t_channel_id CSubService::getChannelID(void) const
{
	return ((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000+ abs(satellitePosition))) << 48) |
		(uint64_t) CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(service.service_id, service.original_network_id, service.transport_stream_id);
}


CRemoteControl::CRemoteControl()
{
	current_channel_id = 	CZapit::getInstance()->GetCurrentChannelID();;
	current_sub_channel_id = 0;
	current_channel_name = 	"";

	zap_completion_timeout = 0;

	current_EPGid =	0;
	next_EPGid = 	0;
	memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );
	has_ac3 = 	false;
	selected_subchannel = -1;
	needs_nvods = 	false;
	director_mode = 0;
//	current_programm_timer = 0;
	is_video_started = true;
}


int CRemoteControl::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
//printf("[neutrino] MSG %x\n", msg);
	if ( zap_completion_timeout != 0 ) {
    		if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) || (msg == NeutrinoMessages::EVT_ZAP_FAILED  ) ||
		    (msg == NeutrinoMessages::EVT_ZAP_ISNVOD  )) {
//printf("[neutrino] timeout EVT_ZAP current %llx data %llx\n", current_channel_id, *(t_channel_id *)data);
			if ((*(t_channel_id *)data) != current_channel_id) {
				g_InfoViewer->chanready = 0;
				g_Zapit->zapTo_serviceID_NOWAIT(current_channel_id );
				g_Sectionsd->setServiceChanged(current_channel_id &0xFFFFFFFFFFFFULL, false);

				zap_completion_timeout = getcurrenttime() + 2 * (int64_t) 1000000;

				return messages_return::handled;
			}
			else {
				zap_completion_timeout = 0;
				g_InfoViewer->chanready = 1;
			}
			/* for CHANGETOLOCKED, we don't need to wait for EPG to arrive... */
			if ((!is_video_started) &&
			    (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED))
				g_RCInput->postMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false);
		}
	} else {
		if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) || (msg == NeutrinoMessages::EVT_ZAP_FAILED  ) ||
		    (msg == NeutrinoMessages::EVT_ZAP_ISNVOD  ))
		{
//printf("[neutrino] EVT_ZAP current %llx data %llx\n", current_channel_id, *(t_channel_id *)data);
			g_InfoViewer->chanready = 1;
			// warte auf keine Meldung vom ZAPIT -> jemand anderer hat das zappen ausgelöst...
			if ((*(t_channel_id *)data) != current_channel_id) {
				t_channel_id new_id = *(t_channel_id *)data;
				CZapitChannel* channel = CServiceManager::getInstance()->FindChannel(new_id);
				is_video_started = true;
				if (channel) {
					current_channel_name = channel->getName();
					if (channel->bAlwaysLocked != g_settings.parentallock_defaultlocked)
						stopvideo();
				}
				CVFD::getInstance()->showServicename(current_channel_name); // UTF-8
				current_channel_id = new_id;

				current_EPGid = 0;
				next_EPGid = 0;

				memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );

				current_PIDs.APIDs.clear();
				has_ac3 = false;

				subChannels.clear();
				selected_subchannel = -1;
				director_mode = 0;
				needs_nvods = (msg == NeutrinoMessages:: EVT_ZAP_ISNVOD);

				g_Sectionsd->setServiceChanged( current_channel_id&0xFFFFFFFFFFFFULL, true );
				CNeutrinoApp::getInstance()->channelList->adjustToChannelID(current_channel_id);
				if ( g_InfoViewer->is_visible )
					g_RCInput->postMsg( NeutrinoMessages::SHOW_INFOBAR , 0 );
			}
			if ((!is_video_started) &&
			    (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED))
				g_RCInput->postMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false);
		}
		else
			if ((msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE) || (msg == NeutrinoMessages:: EVT_ZAP_SUB_FAILED )) {
//printf("[neutrino] EVT_ZAP_SUB current %llx data %llx\n", current_sub_channel_id, *(t_channel_id *)data);
				if ((*(t_channel_id *)data) != current_sub_channel_id)
				{
					current_sub_channel_id = *(t_channel_id *)data;

					for(unsigned int i = 0; i < subChannels.size(); i++)
					if (subChannels[i].getChannelID() == (*(t_channel_id *)data))
					{
						selected_subchannel = i;
						break;
					}
				}
			}
	}

	if ( msg == NeutrinoMessages::EVT_CURRENTEPG )
	{
		if ((*(t_channel_id *)data) != (current_channel_id & 0xFFFFFFFFFFFFULL) &&
		    (*(t_channel_id *)data) != (current_sub_channel_id & 0xFFFFFFFFFFFFULL))
			return messages_return::handled;

		const CSectionsdClient::CurrentNextInfo info_CN = g_InfoViewer->getCurrentNextInfo();
		if ((info_CN.current_uniqueKey >> 16) == (current_channel_id & 0xFFFFFFFFFFFFULL) || (info_CN.current_uniqueKey >> 16) == (current_sub_channel_id & 0xFFFFFFFFFFFFULL))
		{
			//CURRENT-EPG for current channel arrived!;
			CVFD::getInstance()->setEPGTitle(info_CN.current_name);
			if (info_CN.current_uniqueKey != current_EPGid)
			{
				if ( current_EPGid != 0 )
				{
					// ist nur ein neues Programm, kein neuer Kanal
					// PIDs neu holen
					g_Zapit->getPIDS( current_PIDs );
					// APID Bearbeitung neu anstossen
					has_unresolved_ctags = true;
					// infobar indicate on epg change
					g_InfoViewer->showEpgInfo();
				}

				current_EPGid = info_CN.current_uniqueKey;

				if ( has_unresolved_ctags )
					processAPIDnames();

				if (selected_subchannel <= 0 && info_CN.flags & CSectionsdClient::epgflags::current_has_linkagedescriptors)
				{
					subChannels.clear();
					getSubChannels();
				}

				if ( needs_nvods )
					getNVODs();
			}

			// is_video_started is only false if channel is locked
			if ((!is_video_started) &&
			    (info_CN.current_fsk == 0 || g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED))
				g_RCInput->postMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false);
			else
				g_RCInput->postMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, info_CN.current_fsk, false);
		}
		return messages_return::handled;
	}
	else if ( msg == NeutrinoMessages::EVT_NEXTEPG )
	{
		if ((*(t_channel_id *)data) != current_channel_id)
			return messages_return::handled;

		const CSectionsdClient::CurrentNextInfo info_CN = g_InfoViewer->getCurrentNextInfo();
		if ((info_CN.next_uniqueKey >> 16) == (current_channel_id&0xFFFFFFFFFFFFULL) )
		{
			// next-EPG for current channel arrived. no current-EPG?!
			if (info_CN.next_uniqueKey != next_EPGid)
				next_EPGid = info_CN.next_uniqueKey;
		}
		if ( !is_video_started )
			g_RCInput->postMsg( NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false );
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_NOEPG_YET)
	{
		if ((*(t_channel_id *)data) == (current_channel_id&0xFFFFFFFFFFFFULL))
		{
			if ( !is_video_started )
				g_RCInput->postMsg( NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false );
		}
		return messages_return::handled;
	}
	else if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE)||
                 (msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE)) {

		if ((*(t_channel_id *)data) == ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) ? current_channel_id : current_sub_channel_id))
		{
			CVFD::getInstance()->showServicename(current_channel_name); // UTF-8
			g_Zapit->getPIDS( current_PIDs );
			//g_Sectionsd->setPrivatePid( current_PIDs.PIDs.privatepid );
			sectionsd_setPrivatePid( current_PIDs.PIDs.privatepid );
			//tuxtxt
#if 1
			tuxtxt_stop();
			if(g_settings.cacheTXT) {
				printf("TuxTXT pid: %X\n", current_PIDs.PIDs.vtxtpid);
				if(current_PIDs.PIDs.vtxtpid != 0)
					tuxtxt_start(current_PIDs.PIDs.vtxtpid);
			}
#endif
			char *p = new char[sizeof(t_channel_id)];
			memcpy(p, &current_channel_id, sizeof(t_channel_id));
			g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOTPIDS, (const neutrino_msg_data_t)p, false); 

			processAPIDnames();
		}
	    return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_PMT_CHANGED) {
		g_Zapit->getPIDS(current_PIDs);
		processAPIDnames();
		return messages_return::unhandled;
	}
	else if (msg == NeutrinoMessages::EVT_ZAP_ISNVOD)
	{
		if ((*(t_channel_id *)data) == current_channel_id)
		{
			needs_nvods = true;
			CVFD::getInstance()->showServicename(std::string("[") + current_channel_name + ']'); // UTF-8
			if ( current_EPGid != 0)
			{
				getNVODs();
				if (subChannels.empty())
					g_Sectionsd->setServiceChanged( current_channel_id&0xFFFFFFFFFFFFULL, true );
			}
			else
				// EVENT anfordern!
				g_Sectionsd->setServiceChanged( current_channel_id&0xFFFFFFFFFFFFULL, true );

		}
	    return messages_return::handled;
	}
#if 0
	else if ( ( msg == NeutrinoMessages::EVT_TIMER ) && ( data == current_programm_timer ) )
	{
		//printf("new program !\n");

		t_channel_id * p = new t_channel_id;
		*p = current_channel_id;
		g_RCInput->postMsg(NeutrinoMessages::EVT_NEXTPROGRAM, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory

 		return messages_return::handled;
	}
#endif
	//else if (msg == NeutrinoMessages::EVT_ZAP_FAILED || msg == NeutrinoMessages::EVT_ZAP_SUB_FAILED)
 		//return messages_return::handled;
	else
		return messages_return::unhandled;
}

void CRemoteControl::getSubChannels()
{
//printf("[neutrino] getSubChannels, current_EPGid %llx\n", current_EPGid);
	if ( subChannels.size() == 0 )
	{
		CSectionsdClient::LinkageDescriptorList	linkedServices;
		//if ( g_Sectionsd->getLinkageDescriptorsUniqueKey( current_EPGid, linkedServices ) )
		if ( sectionsd_getLinkageDescriptorsUniqueKey( current_EPGid, linkedServices ) )
		{
			if ( linkedServices.size()> 1 )
			{
				are_subchannels = true;
//printf("CRemoteControl::getSubChannels linkedServices.size %d\n", linkedServices.size());
				for (unsigned int i=0; i< linkedServices.size(); i++)
				{
//printf("CRemoteControl::getSubChannels %s\n", linkedServices[i].name.c_str());
					subChannels.push_back(CSubService(
								      linkedServices[i].originalNetworkId,
								      linkedServices[i].serviceId,
								      linkedServices[i].transportStreamId,
								      linkedServices[i].name));
					if ((subChannels[i].getChannelID()&0xFFFFFFFFFFFFULL) == (current_channel_id&0xFFFFFFFFFFFFULL))
						selected_subchannel = i;
				}
				copySubChannelsToZapit();

				char *p = new char[sizeof(t_channel_id)];
				memcpy(p, &current_channel_id, sizeof(t_channel_id));
				g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory
			}
		}
	}
}

void CRemoteControl::getNVODs()
{
//printf("[neutrino] getNVODs, current_EPGid %llx\n", current_EPGid);
	if ( subChannels.size() == 0 )
	{
		CSectionsdClient::NVODTimesList	NVODs;
		//if ( g_Sectionsd->getNVODTimesServiceKey( current_channel_id & 0xFFFFFFFFFFFFULL, NVODs ) )
		if ( sectionsd_getNVODTimesServiceKey( current_channel_id & 0xFFFFFFFFFFFFULL, NVODs ) )
		{
			are_subchannels = false;
//printf("CRemoteControl::getNVODs NVODs.size %d\n", NVODs.size());
			for (unsigned int i=0; i< NVODs.size(); i++)
			{
				if ( NVODs[i].zeit.dauer> 0 )
				{
					CSubService newService(
						NVODs[i].original_network_id,
						NVODs[i].service_id,
						NVODs[i].transport_stream_id,
						NVODs[i].zeit.startzeit,
						NVODs[i].zeit.dauer);

					CSubServiceListSorted::iterator e= subChannels.begin();
					for(; e!=subChannels.end(); ++e)
					{
						if ( e->startzeit > newService.startzeit )
							break;
					}
					subChannels.insert( e, newService );
				}

			}

			copySubChannelsToZapit();

			char *p = new char[sizeof(t_channel_id)];
			memcpy(p, &current_channel_id, sizeof(t_channel_id));
			g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory

			if ( selected_subchannel == -1 )
			{
				// beim ersten Holen letzten NVOD-Kanal setzen!
				setSubChannel( subChannels.size()- 1 );
			}
			else
			{
				// sollte nur passieren, wenn die aktuelle Sendung vorbei ist?!
				selected_subchannel = -1;
			}
		}
	}
}

void CRemoteControl::processAPIDnames()
{
	has_unresolved_ctags= false;
	has_ac3 = false; //FIXME what this variable suppoused to do ?? seems unused
	int pref_found = -1;
	int pref_ac3_found = -1;
	int pref_idx = -1;
	int pref_ac3_idx = -1;
	int ac3_found = -1;
	const char *desc;
	char lang[4];

	if(g_settings.auto_lang) {
		/* first we check prefs to find pid according to pref index */
		for(int i = 0; i < 3; i++) {
			for(int j = 0; j < (int) current_PIDs.APIDs.size(); j++) {
				desc = current_PIDs.APIDs[j].desc;
				// In some cases AAC is the only audio system used
				// so accept it here as a 'normal' sound track
				if(strstr(desc, "(AAC)")) {
					strncpy(lang, desc, 3);
					lang[3] = 0;
					desc = lang;
				}
				/* processAPIDnames called 2 times, TODO find better way to detect second call */
				if(strlen( desc ) != 3)
					continue;
				if(strlen(g_settings.pref_lang[i]) == 0)
					continue;

				std::string temp(g_settings.pref_lang[i]);
				std::map<std::string, std::string>::const_iterator it;
				for(it = iso639.begin(); it != iso639.end(); it++) {
					if(temp == it->second && strcasecmp(desc, it->first.c_str()) == 0) {
						/* remember first pref found index and pid*/
						if(pref_found < 0) {
							pref_found = j;
							pref_idx = i;
						}
						if(current_PIDs.APIDs[j].is_ac3 && g_settings.audio_DolbyDigital && (pref_ac3_found < 0)) {
							pref_ac3_found = j;
							pref_ac3_idx = i;
						}
						break;
					}
				}
			} /* for all pids */
		} /*for all prefs*/

		/* reset pref ac3, if it have lower priority */
		if((pref_idx >= 0) && (pref_idx < pref_ac3_idx))
			pref_ac3_found = -1;
	}

	for(unsigned int count=0; count< current_PIDs.APIDs.size(); count++)
	{
		printf("Neutrino: apid name= %s (%s) pid= %X\n", current_PIDs.APIDs[count].desc, getISO639Description( current_PIDs.APIDs[count].desc ), current_PIDs.APIDs[count].pid);
		if ( current_PIDs.APIDs[count].component_tag != 0xFF )
		{
			has_unresolved_ctags= true;
		}
		if ( strlen( current_PIDs.APIDs[count].desc ) == 3 )
		{
			// unaufgeloeste Sprache...
			strcpy( current_PIDs.APIDs[count].desc, getISO639Description( current_PIDs.APIDs[count].desc ) );
		}

		if ( current_PIDs.APIDs[count].is_ac3 )
		{
			if(!strstr(current_PIDs.APIDs[count].desc, " (AC3)"))
				strncat(current_PIDs.APIDs[count].desc, " (AC3)", DESC_MAX_LEN - strlen(current_PIDs.APIDs[count].desc));
			has_ac3 = true;
			if(g_settings.audio_DolbyDigital && (ac3_found < 0))
				ac3_found = count;
		}
		else if (current_PIDs.APIDs[count].is_aac &&  !strstr(current_PIDs.APIDs[count].desc, " (AAC)"))
			strncat(current_PIDs.APIDs[count].desc, " (AAC)", DESC_MAX_LEN - strlen(current_PIDs.APIDs[count].desc));
	}

	if ( has_unresolved_ctags )
	{
		if ( current_EPGid != 0 )
		{
			CSectionsdClient::ComponentTagList tags;
			//if ( g_Sectionsd->getComponentTagsUniqueKey( current_EPGid, tags ) )
			if ( sectionsd_getComponentTagsUniqueKey( current_EPGid, tags ) )
			{
				has_unresolved_ctags = false;

				for (unsigned int i=0; i< tags.size(); i++)
				{
					for (unsigned int j=0; j< current_PIDs.APIDs.size(); j++)
					{
						if ( current_PIDs.APIDs[j].component_tag == tags[i].componentTag )
						{
							// workaround for buggy ZDF ctags / or buggy sectionsd/drivers , who knows...
							if(!tags[i].component.empty())
							{
								strncpy(current_PIDs.APIDs[j].desc, tags[i].component.c_str(), DESC_MAX_LEN);
								if (current_PIDs.APIDs[j].is_ac3 &&  !strstr(current_PIDs.APIDs[j].desc, " (AC3)"))
									strncat(current_PIDs.APIDs[j].desc, " (AC3)", DESC_MAX_LEN - strlen(current_PIDs.APIDs[j].desc));
								else if (current_PIDs.APIDs[j].is_aac &&  !strstr(current_PIDs.APIDs[j].desc, " (AAC)"))
									strncat(current_PIDs.APIDs[j].desc, " (AAC)", DESC_MAX_LEN - strlen(current_PIDs.APIDs[j].desc));
							}
							current_PIDs.APIDs[j].component_tag = -1;
							break;
						}
					}
				}
			}
		}
	}
	printf("Neutrino: pref_found %d pref_ac3_found %d ac3_found %d\n", pref_found, pref_ac3_found, ac3_found);
	if(pref_ac3_found >= 0) {
		printf("Neutrino: set apid name= %s pid= %X\n", current_PIDs.APIDs[pref_ac3_found].desc, current_PIDs.APIDs[pref_ac3_found].pid);
		setAPID(pref_ac3_found);
	} else if(pref_found >= 0) {
		printf("Neutrino: set apid name= %s pid= %X\n", current_PIDs.APIDs[pref_found].desc, current_PIDs.APIDs[pref_found].pid);
		setAPID(pref_found);
	}
	else if(ac3_found >= 0) {
		printf("Neutrino: set apid name= %s pid= %X\n", current_PIDs.APIDs[ac3_found].desc, current_PIDs.APIDs[ac3_found].pid);
		setAPID(ac3_found);
	}
	else if ( current_PIDs.PIDs.selected_apid >= current_PIDs.APIDs.size() )
	{
		setAPID( 0 );
	}

	char *p = new char[sizeof(t_channel_id)];
	memcpy(p, &current_channel_id, sizeof(t_channel_id));
	g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOTAPIDS, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory
}


void CRemoteControl::copySubChannelsToZapit(void)
{
	CZapitClient::subServiceList zapitList;

	for (CSubServiceListSorted::const_iterator e = subChannels.begin(); e != subChannels.end(); e++)
		zapitList.push_back(e->getAsZapitSubService());

	g_Zapit->setSubServices(zapitList);
}


void CRemoteControl::setAPID( uint32_t APID )
{
	if ((current_PIDs.PIDs.selected_apid == APID ) ||
	    (APID >= current_PIDs.APIDs.size()))
		return;

	current_PIDs.PIDs.selected_apid = APID;
	g_Zapit->setAudioChannel( APID );
}

static const std::string empty_string;

const std::string & CRemoteControl::setSubChannel(const int numSub, const bool force_zap)
{
	if ((numSub < 0) || (numSub >= (int)subChannels.size()))
		return empty_string;

	if ((selected_subchannel == numSub ) && (!force_zap))
		return empty_string;

	selected_subchannel = numSub;
	current_sub_channel_id = subChannels[numSub].getChannelID();
	g_InfoViewer->chanready = 0;
	g_RCInput->killTimer(scrambled_timer);

	g_Zapit->zapTo_subServiceID_NOWAIT( current_sub_channel_id );
	// Houdini: to restart reading the private EPG when switching to a new option
	g_Sectionsd->setServiceChanged( current_sub_channel_id , true );

	return subChannels[numSub].subservice_name;
}

const std::string & CRemoteControl::subChannelUp(void)
{
	//return setSubChannel((subChannels.size() == 0) ? -1 : (int)((selected_subchannel + 1) % subChannels.size()));
 	// if there are any NVOD/subchannels switch these else switch audio channel (if any)
  	if (subChannels.size() > 0 || !g_settings.audiochannel_up_down_enable)
  	{
  		return setSubChannel((subChannels.size() == 0) ? -1 : (int)((selected_subchannel + 1) % subChannels.size()));
  	}
  	else
  	{
  		if (current_PIDs.APIDs.size() > 0)
  		{
  			setAPID((current_PIDs.PIDs.selected_apid + 1) % current_PIDs.APIDs.size());
  		}
  		return (empty_string);
  	}
}

const std::string & CRemoteControl::subChannelDown(void)
{
	//return setSubChannel((selected_subchannel <= 0) ? (subChannels.size() - 1) : (selected_subchannel - 1));
	// if there are any NVOD/subchannels switch these else switch audio channel (if any)
  	if (subChannels.size() > 0 || !g_settings.audiochannel_up_down_enable)
  	{
  		return setSubChannel((selected_subchannel <= 0) ? (subChannels.size() - 1) : (selected_subchannel - 1));
  	}
  	else
  	{
  		if (current_PIDs.APIDs.size() > 0)
  		{
  			if (current_PIDs.PIDs.selected_apid <= 0)
  				setAPID(current_PIDs.APIDs.size() - 1);
  			else
  				setAPID((current_PIDs.PIDs.selected_apid - 1));
  		}
  		return (empty_string);
  	}
}

void stopAutoRecord();
void CRemoteControl::zapTo_ChannelID(const t_channel_id channel_id, const std::string & channame, const bool start_video) // UTF-8
{
	current_channel_id = channel_id;
	current_channel_name = channame;
//printf("zapTo_ChannelID: start_video: %d\n", start_video);
	if (start_video)
		startvideo();
	else
		stopvideo();

	current_sub_channel_id = 0;
	current_EPGid = 0;
	next_EPGid = 0;

	memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );

	current_PIDs.APIDs.clear();
	has_ac3 = false;

	subChannels.clear();
	selected_subchannel = -1;
	needs_nvods = false;
	director_mode = 0;

	uint64_t now = getcurrenttime();
	if ( zap_completion_timeout < now )
	{
		g_InfoViewer->chanready = 0;

		CRecordManager::getInstance()->StopAutoRecord();

		g_RCInput->killTimer(scrambled_timer);
		//dvbsub_pause(true);
		CZapit::getInstance()->Abort();
		g_Zapit->zapTo_serviceID_NOWAIT(channel_id);
		g_Sectionsd->setServiceChanged( current_channel_id&0xFFFFFFFFFFFFULL, false );

		zap_completion_timeout = now + 2 * (int64_t) 1000000;
//		g_RCInput->killTimer( current_programm_timer );
	}
}

void CRemoteControl::startvideo()
{
	if ( !is_video_started )
	{
		is_video_started= true;
		//g_Zapit->startPlayBack();
		g_Zapit->unlockPlayBack();
	}
}

void CRemoteControl::stopvideo()
{
	if ( is_video_started )
	{
		is_video_started= false;
#if HAVE_TRIPLEDRAGON
		/* we need stopPlayback to blank video,
		   lockPlayback prevents it from being inadvertently starting */
		g_Zapit->stopPlayBack();
#endif
		g_Zapit->lockPlayBack();
	}
}

void CRemoteControl::radioMode()
{
	g_Zapit->setMode( CZapitClient::MODE_RADIO );
}

void CRemoteControl::tvMode()
{
	g_Zapit->setMode( CZapitClient::MODE_TV );
}
