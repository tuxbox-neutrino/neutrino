/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	(C) 2008-2013 Stefan Seyfried

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
#include <gui/movieplayer.h>

#include <driver/record.h>
#include <driver/abstime.h>
#include <driver/display.h>
#include <libdvbsub/dvbsub.h>
#include <libtuxtxt/teletext.h>

#include <zapit/channel.h>
#include <zapit/bouquets.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>
#include <eitd/sectionsd.h>

#define ZAP_GUARD_TIME 2000 // ms

extern CBouquetManager *g_bouquetManager;

extern uint32_t scrambled_timer;

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
		(uint64_t) CREATE_CHANNEL_ID(service.service_id, service.original_network_id, service.transport_stream_id);
}


CRemoteControl::CRemoteControl()
{
	current_channel_id = 	CZapit::getInstance()->GetCurrentChannelID();;
	current_sub_channel_id = 0;
	current_channel_name = 	"";
	current_channel_num = -1;

	zap_completion_timeout = 0;

	current_EPGid =	0;
	memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );
	has_ac3 = 	false;
	selected_subchannel = -1;
	needs_nvods = 	false;
	director_mode = 0;
//	current_programm_timer = 0;
	is_video_started = true;
	//next_EPGid = 	0;
	are_subchannels = false;
	has_unresolved_ctags = false;
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
				if (!IS_WEBCHAN(current_channel_id))
					g_Sectionsd->setServiceStopped();
				CMoviePlayerGui::getInstance().stopPlayBack();
				g_Zapit->zapTo_serviceID_NOWAIT(current_channel_id );

				zap_completion_timeout = time_monotonic_ms() + ZAP_GUARD_TIME;

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

			//check epg fsk in onsignal mode
			if ((!is_video_started) &&
			    (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_ONSIGNAL)){
					CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
					CEitManager::getInstance()->getCurrentNextServiceKey(current_channel_id, currentNextInfo);
					if(currentNextInfo.current_fsk  && currentNextInfo.current_fsk >= g_settings.parentallock_lockage){
						g_RCInput->postMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false);
					}
			}
		}
	} else {
		if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) || (msg == NeutrinoMessages::EVT_ZAP_FAILED  ) ||
		    (msg == NeutrinoMessages::EVT_ZAP_ISNVOD  ))
		{
//printf("[neutrino] EVT_ZAP current %llx data %llx\n", current_channel_id, *(t_channel_id *)data);
			g_InfoViewer->chanready = 1;
			// wait for no message from ZAPIT -> someone else has triggered zapping
			if ((*(t_channel_id *)data) != current_channel_id) {
				t_channel_id new_id = *(t_channel_id *)data;
				CZapitChannel* channel = CServiceManager::getInstance()->FindChannel(new_id);
				is_video_started = true;
				if (channel) {
					current_channel_name = channel->getName();
					current_channel_num = channel->number;
					if (channel->Locked() != g_settings.parentallock_defaultlocked)
						stopvideo();
				}
				CVFD::getInstance()->showServicename(current_channel_name, current_channel_num); // UTF-8
				current_channel_id = new_id;

				current_EPGid = 0;
				//next_EPGid = 0;

				memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );

				current_PIDs.APIDs.clear();
				has_ac3 = false;

				subChannels.clear();
				selected_subchannel = -1;
				director_mode = 0;
				needs_nvods = (msg == NeutrinoMessages:: EVT_ZAP_ISNVOD);

				CNeutrinoApp::getInstance()->adjustToChannelID(current_channel_id);
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
#if 0
		const CSectionsdClient::CurrentNextInfo info_CN = g_InfoViewer->getCurrentNextInfo();
		if ((info_CN.next_uniqueKey >> 16) == (current_channel_id&0xFFFFFFFFFFFFULL) )
		{
			// next-EPG for current channel arrived. no current-EPG?!
			if (info_CN.next_uniqueKey != next_EPGid)
				next_EPGid = info_CN.next_uniqueKey;
		}
#endif
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
			CVFD::getInstance()->showServicename(current_channel_name, current_channel_num); // UTF-8
			g_Zapit->getPIDS( current_PIDs );
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
			g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOTPIDS, (neutrino_msg_data_t)p, false);

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
			CVFD::getInstance()->showServicename(std::string("[") + current_channel_name + ']', current_channel_num); // UTF-8
			if ( current_EPGid != 0)
			{
				getNVODs();
				if (subChannels.empty())
					g_Sectionsd->setServiceChanged( current_channel_id, false);
			}
			else
				// EVENT anfordern!
				g_Sectionsd->setServiceChanged( current_channel_id, false);

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
	else if (msg == NeutrinoMessages::EVT_TUNE_COMPLETE) {
		t_channel_id chid = *(t_channel_id *)data;
		printf("CRemoteControl::handleMsg: EVT_TUNE_COMPLETE (%016" PRIx64 ")\n", chid);
		if(chid && !IS_WEBCHAN(chid))
			g_Sectionsd->setServiceChanged(chid, false);
 		return messages_return::handled;
	}
	//else if (msg == NeutrinoMessages::EVT_ZAP_FAILED || msg == NeutrinoMessages::EVT_ZAP_SUB_FAILED)
 		//return messages_return::handled;
	else
		return messages_return::unhandled;
}

void CRemoteControl::getSubChannels()
{
//printf("[neutrino] getSubChannels, current_EPGid %llx\n", current_EPGid);
	if ( subChannels.empty() )
	{
		CSectionsdClient::LinkageDescriptorList	linkedServices;
		if (CEitManager::getInstance()->getLinkageDescriptorsUniqueKey( current_EPGid, linkedServices))
		{
			if ( linkedServices.size()> 1 )
			{
				are_subchannels = true;
//printf("CRemoteControl::getSubChannels linkedServices.size %d\n", linkedServices.size());
				for (unsigned int i=0; i< linkedServices.size(); i++)
				{
					subChannels.push_back(CSubService(
								      linkedServices[i].originalNetworkId,
								      linkedServices[i].serviceId,
								      linkedServices[i].transportStreamId,
								      linkedServices[i].name));
//printf("CRemoteControl::getSubChannels %s: %016llx\n", linkedServices[i].name.c_str(), subChannels[i].getChannelID());
					if ((subChannels[i].getChannelID()&0xFFFFFFFFFFFFULL) == (current_channel_id&0xFFFFFFFFFFFFULL))
						selected_subchannel = i;
				}
				copySubChannelsToZapit();

				char *p = new char[sizeof(t_channel_id)];
				memcpy(p, &current_channel_id, sizeof(t_channel_id));
				g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES, (neutrino_msg_data_t)p, false); // data is pointer to allocated memory
			}
		}
	}
}

void CRemoteControl::getNVODs()
{
//printf("[neutrino] getNVODs, current_EPGid %llx\n", current_EPGid);
	if ( subChannels.empty() )
	{
		CSectionsdClient::NVODTimesList	NVODs;
		if (CEitManager::getInstance()->getNVODTimesServiceKey( current_channel_id, NVODs))
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
			g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES, (neutrino_msg_data_t)p, false); // data is pointer to allocated memory

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
	has_ac3 = false; //use in infoviewer
	int pref_found = -1;
	int pref_ac3_found = -1;
	int pref_idx = -1;
	int pref_ac3_idx = -1;
	int ac3_found = -1;
	const char *desc;
	char lang[4];

	if(g_settings.auto_lang &&  (current_PIDs.APIDs.size() > 1)) {
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
				if(g_settings.pref_lang[i].empty())
					continue;

				std::string temp(g_settings.pref_lang[i]);
				std::map<std::string, std::string>::const_iterator it;
				for(it = iso639.begin(); it != iso639.end(); ++it) {
					if(temp == it->second && strcasecmp(desc, it->first.c_str()) == 0) {
						/* remember first pref found index and pid*/
						if(pref_found < 0) {
							pref_found = j;
							pref_idx = i;
						}
						if((current_PIDs.APIDs[j].is_ac3 || current_PIDs.APIDs[j].is_eac3)
								&& g_settings.audio_DolbyDigital && (pref_ac3_found < 0)) {
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
#ifdef APID_DEBUG
	if (! current_PIDs.APIDs.empty())
		printf("Neutrino: ");
#endif
	for(unsigned int count=0; count< current_PIDs.APIDs.size(); count++)
	{
		const char *iso = getISO639Description(current_PIDs.APIDs[count].desc);
#ifdef APID_DEBUG
		printf("apid=%04x/%s/%s ", current_PIDs.APIDs[count].pid, current_PIDs.APIDs[count].desc, iso);
#endif
		if ( current_PIDs.APIDs[count].component_tag != 0xFF )
		{
			has_unresolved_ctags= true;
		}
		std::string tmp_desc = current_PIDs.APIDs[count].desc;
		if ( tmp_desc.size() == 3 )
		{
			// unresolved language
			tmp_desc = iso;
		}

		if ( current_PIDs.APIDs[count].is_ac3 )
		{
			if(!strstr(current_PIDs.APIDs[count].desc, " (AC3)"))
				tmp_desc += " (AC3)";
			has_ac3 = true;
			if(g_settings.audio_DolbyDigital && (ac3_found < 0))
				ac3_found = count;
		}
		else if (current_PIDs.APIDs[count].is_aac &&  !strstr(current_PIDs.APIDs[count].desc, " (AAC)"))
			tmp_desc += " (AAC)";
		else if (current_PIDs.APIDs[count].is_eac3 &&  !strstr(current_PIDs.APIDs[count].desc, " (EAC3)"))
			tmp_desc += " (EAC3)";

		if(!tmp_desc.empty()){
			strncpy(current_PIDs.APIDs[count].desc, tmp_desc.c_str(), DESC_MAX_LEN -1);
		}

	}
#ifdef APID_DEBUG
	if (! current_PIDs.APIDs.empty())
		printf("\n");
#endif
	if ( has_unresolved_ctags )
	{
		if ( current_EPGid == 0 ){
			const CSectionsdClient::CurrentNextInfo info_CN = g_InfoViewer->getCurrentNextInfo();
			if ((info_CN.current_uniqueKey >> 16) == (current_channel_id & 0xFFFFFFFFFFFFULL)){
				current_EPGid = info_CN.current_uniqueKey;
			}
		}
		if ( current_EPGid != 0 )
		{
			CSectionsdClient::ComponentTagList tags;
			if (CEitManager::getInstance()->getComponentTagsUniqueKey(current_EPGid, tags))
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
								std::string tmp_desc2 = tags[i].component.c_str();
								if (current_PIDs.APIDs[j].is_ac3 &&  tmp_desc2.find(" (AC3)"))
									tmp_desc2 += " (AC3)";
								else if (current_PIDs.APIDs[j].is_aac && tmp_desc2.find(" (AAC)"))
									tmp_desc2 += " (AAC)";
								else if (current_PIDs.APIDs[j].is_eac3 &&  tmp_desc2.find(" (EAC3)"))
									tmp_desc2 += " (EAC3)";

								if(!tmp_desc2.empty()){
									strncpy(current_PIDs.APIDs[j].desc, tmp_desc2.c_str(), DESC_MAX_LEN -1);
								}
							}
							current_PIDs.APIDs[j].component_tag = -1;
							break;
						}
					}
				}
			}
		}
	}
#ifdef APID_DEBUG
	printf("Neutrino: pref_found %d pref_ac3_found %d ac3_found %d\n", pref_found, pref_ac3_found, ac3_found);
#endif
	if(pref_ac3_found >= 0) {
#ifdef APID_DEBUG
		printf("Neutrino: set apid name= %s pid= %X\n", current_PIDs.APIDs[pref_ac3_found].desc, current_PIDs.APIDs[pref_ac3_found].pid);
#endif
		setAPID(pref_ac3_found);
	} else if(pref_found >= 0) {
#ifdef APID_DEBUG
		printf("Neutrino: set apid name= %s pid= %X\n", current_PIDs.APIDs[pref_found].desc, current_PIDs.APIDs[pref_found].pid);
#endif
		setAPID(pref_found);
	}
	else if(ac3_found >= 0) {
#ifdef APID_DEBUG
		printf("Neutrino: set apid name= %s pid= %X\n", current_PIDs.APIDs[ac3_found].desc, current_PIDs.APIDs[ac3_found].pid);
#endif
		setAPID(ac3_found);
	}
	else if ( current_PIDs.PIDs.selected_apid >= current_PIDs.APIDs.size() )
	{
		setAPID( 0 );
	}

	char *p = new char[sizeof(t_channel_id)];
	memcpy(p, &current_channel_id, sizeof(t_channel_id));
	g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOTAPIDS, (neutrino_msg_data_t)p, false); // data is pointer to allocated memory
}


void CRemoteControl::copySubChannelsToZapit(void)
{
	CZapitClient::subServiceList zapitList;

	for (CSubServiceListSorted::const_iterator e = subChannels.begin(); e != subChannels.end(); ++e)
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

	CMoviePlayerGui::getInstance().stopPlayBack();
	g_Zapit->zapTo_subServiceID_NOWAIT( current_sub_channel_id );

	return subChannels[numSub].subservice_name;
}

const std::string & CRemoteControl::subChannelUp(void)
{
	//return setSubChannel((subChannels.empty()) ? -1 : (int)((selected_subchannel + 1) % subChannels.size()));
 	// if there are any NVOD/subchannels switch these else switch audio channel (if any)
  	if ( !subChannels.empty() || !g_settings.audiochannel_up_down_enable)
  	{
  		return setSubChannel( subChannels.empty() ? -1 : (int)((selected_subchannel + 1) % subChannels.size()));
  	}
  	else
  	{
  		if ( !current_PIDs.APIDs.empty() )
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
  	if ( !subChannels.empty() || !g_settings.audiochannel_up_down_enable)
  	{
  		return setSubChannel((selected_subchannel <= 0) ? (subChannels.size() - 1) : (selected_subchannel - 1));
  	}
  	else
  	{
  		if ( !current_PIDs.APIDs.empty() )
  		{
  			if (current_PIDs.PIDs.selected_apid <= 0)
  				setAPID(current_PIDs.APIDs.size() - 1);
  			else
  				setAPID((current_PIDs.PIDs.selected_apid - 1));
  		}
  		return (empty_string);
  	}
}

void CRemoteControl::zapTo_ChannelID(const t_channel_id channel_id, const std::string & channame, int channum, const bool start_video) // UTF-8
{
//printf("zapTo_ChannelID: start_video: %d\n", start_video);
	if (start_video)
		startvideo();
	else
		stopvideo();

	current_sub_channel_id = 0;
	current_EPGid = 0;
	//next_EPGid = 0;

	memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );

	current_PIDs.APIDs.clear();
	has_ac3 = false;

	subChannels.clear();
	selected_subchannel = -1;
	needs_nvods = false;
	director_mode = 0;

	uint64_t now = time_monotonic_ms();
	if ( zap_completion_timeout < now )
	{
		g_InfoViewer->chanready = 0;

		CRecordManager::getInstance()->StopAutoTimer();
		if (channel_id != current_channel_id)
			CRecordManager::getInstance()->StopAutoRecord();

		g_RCInput->killTimer(scrambled_timer);
		//dvbsub_pause(true);
		CZapit::getInstance()->Abort();
		if (!IS_WEBCHAN(channel_id))
			g_Sectionsd->setServiceStopped();
		CMoviePlayerGui::getInstance().stopPlayBack();
		g_Zapit->zapTo_serviceID_NOWAIT(channel_id);

		zap_completion_timeout = now + ZAP_GUARD_TIME;
//		g_RCInput->killTimer( current_programm_timer );
	}
	current_channel_id = channel_id;
	current_channel_name = channame;
	current_channel_num = channum;
}

void CRemoteControl::startvideo()
{
	if ( !is_video_started )
	{
		is_video_started= true;
		//g_Zapit->startPlayBack();
		g_Zapit->unlockPlayBack(true); /* TODO: check if sendpmt=false is correct in stopvideo() */
	}
}

void CRemoteControl::stopvideo()
{
	if ( is_video_started )
	{
		is_video_started= false;
		/* we need stopPlayback to blank video,
		   lockPlayback prevents it from being inadvertently starting */
		g_Zapit->stopPlayBack(false);
		g_Zapit->lockPlayBack(false);
	}
}

void CRemoteControl::radioMode()
{
printf("CRemoteControl::radioMode\n");
	g_Zapit->setMode( CZapitClient::MODE_RADIO );
}

void CRemoteControl::tvMode()
{
printf("CRemoteControl::tvMode\n");
	g_Zapit->setMode( CZapitClient::MODE_TV );
}
