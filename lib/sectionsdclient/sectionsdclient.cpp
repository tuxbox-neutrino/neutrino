/*
  Client-Interface für zapit  -   DBoxII-Project

  $Id: sectionsdclient.cpp,v 1.53 2007/01/12 22:57:57 houdini Exp $

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

#include <cstdio>
#include <cstring>
#include <eventserver.h>

#include <sectionsdclient/sectionsdclient.h>
#include <sectionsdclient/sectionsdMsg.h>

#ifdef PEDANTIC_VALGRIND_SETUP
#define VALGRIND_PARANOIA(x) memset(&x, 0, sizeof(x))
#else
#define VALGRIND_PARANOIA(x) {}
#endif

unsigned char   CSectionsdClient::getVersion   () const
{
	return sectionsd::ACTVERSION;
}

const          char * CSectionsdClient::getSocketName() const
{
	return SECTIONSD_UDS_NAME;
}

int CSectionsdClient::readResponse(char* data,unsigned int size)
{
	struct sectionsd::msgResponseHeader responseHeader;
	if (!receive_data((char*)&responseHeader, sizeof(responseHeader)))
		return 0;

	if ( data != NULL )
	{
		if ( responseHeader.dataLength != (unsigned)size )
			return -1;
		else
			return receive_data(data, size);
	}
	else
		return responseHeader.dataLength;
}

bool CSectionsdClient::send(const unsigned char command, const char* data, const unsigned int size)
{
	sectionsd::msgRequestHeader msgHead;
	VALGRIND_PARANOIA(msgHead);

	msgHead.version    = getVersion();
	msgHead.command    = command;
	msgHead.dataLength = size;

	open_connection(); // if the return value is false, the next send_data call will return false, too

        if (!send_data((char*)&msgHead, sizeof(msgHead)))
            return false;

        if (size != 0)
            return send_data(data, size);

        return true;
}

void CSectionsdClient::registerEvent(const unsigned int eventID, const unsigned int clientID, const char * const udsName)
{
	CEventServer::commandRegisterEvent msg2;
	VALGRIND_PARANOIA(msg2);

	msg2.eventID = eventID;
	msg2.clientID = clientID;

	strcpy(msg2.udsName, udsName);

	send(sectionsd::CMD_registerEvents, (char*)&msg2, sizeof(msg2));

	close_connection();
}

void CSectionsdClient::unRegisterEvent(const unsigned int eventID, const unsigned int clientID)
{
	CEventServer::commandUnRegisterEvent msg2;
	VALGRIND_PARANOIA(msg2);

	msg2.eventID = eventID;
	msg2.clientID = clientID;

	send(sectionsd::CMD_unregisterEvents, (char*)&msg2, sizeof(msg2));

	close_connection();
}

bool CSectionsdClient::getIsTimeSet()
{
	sectionsd::responseIsTimeSet rmsg;

	if (send(sectionsd::getIsTimeSet))
	{
		readResponse((char*)&rmsg, sizeof(rmsg));
		close_connection();

		return rmsg.IsTimeSet;
	}
	else
	{
		close_connection();
		return false;
	}
}

void CSectionsdClient::setPauseScanning(const bool doPause)
{
	int PauseIt = (doPause) ? 1 : 0;

	send(sectionsd::pauseScanning, (char*)&PauseIt, sizeof(PauseIt));

	readResponse();
	close_connection();
}

bool CSectionsdClient::getIsScanningActive()
{
	int scanning;

	if (send(sectionsd::getIsScanningActive))
	{
		readResponse((char*)&scanning, sizeof(scanning));
		close_connection();

		return scanning;
	}
	else
	{
		close_connection();
		return false;
	}
}

void CSectionsdClient::setServiceChanged(const t_channel_id channel_id, const bool requestEvent, int dnum)
{
	sectionsd::commandSetServiceChanged msg;
	VALGRIND_PARANOIA(msg);

	msg.channel_id   = channel_id;
	msg.requestEvent = requestEvent;
	msg.dnum = dnum;

	send(sectionsd::serviceChanged, (char *)&msg, sizeof(msg));

	readResponse();
	close_connection();
}

void CSectionsdClient::setServiceStopped()
{
	send(sectionsd::serviceStopped);

	readResponse();
	close_connection();
}

void CSectionsdClient::freeMemory()
{
	send(sectionsd::freeMemory);

	readResponse();
	close_connection();
}

void CSectionsdClient::readSIfromXML(const char * epgxmlname)
{
	send(sectionsd::readSIfromXML, (char*) epgxmlname, strlen(epgxmlname));

	readResponse();
	close_connection();
}

void CSectionsdClient::readSIfromXMLTV(const char * url)
{
	send(sectionsd::readSIfromXMLTV, (char*) url, strlen(url));

	readResponse();
	close_connection();
}

void CSectionsdClient::writeSI2XML(const char * epgxmlname)
{
	send(sectionsd::writeSI2XML, (char*) epgxmlname, strlen(epgxmlname));

	readResponse();
	close_connection();
}

void CSectionsdClient::setConfig(const epg_config config)
{
	sectionsd::commandSetConfig *msg;
	char* pData = new char[sizeof(sectionsd::commandSetConfig) + config.network_ntpserver.length() + 1 + config.epg_dir.length() + 1];
	msg = (sectionsd::commandSetConfig *)pData;

	msg->epg_cache		= config.epg_cache;
	msg->epg_old_events	= config.epg_old_events;
	msg->epg_max_events	= config.epg_max_events;
	msg->network_ntprefresh	= config.network_ntprefresh;
	msg->network_ntpenable	= config.network_ntpenable;
	msg->epg_extendedcache	= config.epg_extendedcache;
	msg->epg_save_frequently= config.epg_save_frequently;
	msg->epg_read_frequently= config.epg_read_frequently;
//	config.network_ntpserver:
	strcpy(&pData[sizeof(sectionsd::commandSetConfig)], config.network_ntpserver.c_str());
//	config.epg_dir:
	strcpy(&pData[sizeof(sectionsd::commandSetConfig) + config.network_ntpserver.length() + 1], config.epg_dir.c_str());

	send(sectionsd::setConfig, (char*)pData, sizeof(sectionsd::commandSetConfig) + config.network_ntpserver.length() + 1 + config.epg_dir.length() + 1);
	readResponse();
	close_connection();
	delete[] pData;
}

void CSectionsdClient::dumpStatus()
{
	send(sectionsd::dumpStatusinformation);
	close_connection();
}

#if 0
bool CSectionsdClient::getComponentTagsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::ComponentTagList& tags)
{
	if (send(sectionsd::ComponentTagsUniqueKey, (char*)&uniqueKey, sizeof(uniqueKey)))
	{
		tags.clear();

		int nBufSize = readResponse();

		char* pData = new char[nBufSize];
		receive_data(pData, nBufSize);
		char* dp = pData;

		int	count= *(int *) pData;
		dp+= sizeof(int);

		CSectionsdClient::responseGetComponentTags response;
		for (int i= 0; i<count; i++)
		{
			response.component = dp;
			dp+= strlen(dp)+1;
			response.componentType = *(unsigned char *) dp;
			dp+=sizeof(unsigned char);
			response.componentTag = *(unsigned char *) dp;
			dp+=sizeof(unsigned char);
			response.streamContent = *(unsigned char *) dp;
			dp+=sizeof(unsigned char);

			tags.insert( tags.end(), response);
		}
		delete[] pData;
		close_connection();

		return true;
	}
	else
	{
		close_connection();
		return false;
	}
}

bool CSectionsdClient::getLinkageDescriptorsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::LinkageDescriptorList& descriptors)
{
	if (send(sectionsd::LinkageDescriptorsUniqueKey, (char*)&uniqueKey, sizeof(uniqueKey)))
	{
		descriptors.clear();

		int nBufSize = readResponse();

		char* pData = new char[nBufSize];
		receive_data(pData, nBufSize);
		char* dp = pData;

		int count= *(int *) pData;
		dp+= sizeof(int);

		CSectionsdClient::responseGetLinkageDescriptors response;
		for (int i= 0; i<count; i++)
		{
			response.name = dp;
			dp+= strlen(dp)+1;
			response.transportStreamId = *(t_transport_stream_id *) dp;
			dp+=sizeof(t_transport_stream_id);
			response.originalNetworkId = *(t_original_network_id *) dp;
			dp+=sizeof(t_original_network_id);
			response.serviceId = *(t_service_id *) dp;
			dp+=sizeof(t_service_id);

			descriptors.insert( descriptors.end(), response);
		}
		delete[] pData;
		close_connection();
		return true;
	}
	else
	{
		close_connection();
		return false;
	}
}

bool CSectionsdClient::getNVODTimesServiceKey(const t_channel_id channel_id, CSectionsdClient::NVODTimesList& nvod_list)
{
	if (send(sectionsd::timesNVODservice, (char*)&channel_id, sizeof(channel_id)))
	{
		nvod_list.clear();

		int nBufSize = readResponse();

		char* pData = new char[nBufSize];
		receive_data(pData, nBufSize);
		char* dp = pData;

		CSectionsdClient::responseGetNVODTimes response;

		while( dp< pData+ nBufSize )
		{
			response.service_id = *(t_service_id *) dp;			dp += sizeof(t_service_id);
			response.original_network_id = *(t_original_network_id *) dp;	dp += sizeof(t_original_network_id);
			response.transport_stream_id = *(t_transport_stream_id *) dp;	dp += sizeof(t_transport_stream_id);
			response.zeit = *(CSectionsdClient::sectionsdTime*) dp;		dp += sizeof(CSectionsdClient::sectionsdTime);

			nvod_list.insert( nvod_list.end(), response);
		}
		delete[] pData;
		close_connection();
		return true;
	}
	else
	{
		close_connection();
		return false;
	}
}

bool CSectionsdClient::getCurrentNextServiceKey(const t_channel_id channel_id, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next)
{
	if (send(sectionsd::currentNextInformationID, (char*)&channel_id, sizeof(channel_id)))
	{
		int nBufSize = readResponse();

		char* pData = new char[nBufSize];
		receive_data(pData, nBufSize);
		char* dp = pData;

		// current
		current_next.current_uniqueKey = *((event_id_t *)dp);
		dp+= sizeof(event_id_t);
		current_next.current_zeit = *(CSectionsdClient::sectionsdTime*) dp;
		dp+= sizeof(CSectionsdClient::sectionsdTime);
		current_next.current_name = dp;
		dp+=strlen(dp)+1;

		// next
		current_next.next_uniqueKey = *((event_id_t *)dp);
		dp+= sizeof(event_id_t);
		current_next.next_zeit = *(CSectionsdClient::sectionsdTime*) dp;
		dp+= sizeof(CSectionsdClient::sectionsdTime);
		current_next.next_name = dp;
		dp+=strlen(dp)+1;

		current_next.flags = *(unsigned*) dp;
		dp+= sizeof(unsigned);

		current_next.current_fsk = *(char*) dp;

		delete[] pData;
		close_connection();
		return true;
	}
	else
	{
		current_next.flags = 0;
		close_connection();
		return false;
	}
}

CChannelEventList CSectionsdClient::getChannelEvents(const bool tv_mode, t_channel_id *p_requested_channels, int size_requested_channels)
{
	CChannelEventList eList;

	if (send(tv_mode ? sectionsd::actualEventListTVshortIDs : sectionsd::actualEventListRadioShortIDs, (char*)p_requested_channels, size_requested_channels))
	{
		int nBufSize = readResponse();

		if( nBufSize > 0)
		{
			char* pData = new char[nBufSize];
			receive_data(pData, nBufSize);

			char* dp = pData;

			while(dp < pData + nBufSize)
			{
				CChannelEvent aEvent;

				aEvent.eventID = *((event_id_t *) dp);
				dp+=sizeof(aEvent.eventID);

				aEvent.startTime = *((time_t *) dp);
				dp+=sizeof(aEvent.startTime);

				aEvent.duration = *((unsigned *) dp);
				dp+=sizeof(aEvent.duration);

				aEvent.description= dp;
				dp+=strlen(dp)+1;

				aEvent.text= dp;
				dp+=strlen(dp)+1;

				eList.push_back(aEvent);
			}
			delete[] pData;
		}
	}
	close_connection();
	return eList;
}

//GU:EPG
/* This function does initiate a search for a keyword in all EPG Event of the Channel channel_id in sectionsd.
   The parameter search_typ does specify the search mode
	 0: none 			-> all EPG events of the channel are returned
	 1: keyword search in EPG Title
	 2: keyword search in EPG short description (INFO1)
	 3: keyword search in EPG description (INFO2)
  In case of a match, the EPG event is added to the Eventlist eList.
  */
bool CSectionsdClient::getEventsServiceKeySearchAdd(CChannelEventList& eList,const t_channel_id channel_id,char search_typ,std::string& search_text)
{
	int nBufSize=0;

	nBufSize += sizeof(t_channel_id);
	nBufSize += sizeof(char);
	nBufSize += search_text.size()+1;

	char* pSData = new char[nBufSize];
	char* pSData_ptr = pSData;

	*(t_channel_id*)pSData_ptr = channel_id;
	pSData_ptr += sizeof(t_channel_id);
	*pSData_ptr = search_typ;
	pSData_ptr += sizeof(char);
	strcpy(pSData_ptr,search_text.c_str());

	if (send(sectionsd::allEventsChannelIDSearch, pSData, nBufSize))
	{
		int nBufSize2 = readResponse();

		if( nBufSize2 > 0)
		{
			char* pData = new char[nBufSize2];
			receive_data(pData, nBufSize2);

			char* dp = pData;

//			int a = eList.size();

			while(dp < pData + nBufSize2)
			{
				CChannelEvent aEvent;

				aEvent.eventID = *((event_id_t *) dp);
				dp+=sizeof(aEvent.eventID);

				aEvent.startTime = *((time_t *) dp);
				dp+=sizeof(aEvent.startTime);

				aEvent.duration = *((unsigned *) dp);
				dp+=sizeof(aEvent.duration);

				aEvent.description= dp;
				dp+=aEvent.description.length()+1;

				aEvent.text= dp;
				dp+=aEvent.text.length()+1;

				eList.push_back(aEvent);
			}
//			int b = eList.size() -a;
			delete[] pData;
		}
	}
	delete[] pSData;

	close_connection();
	return true;
}

CChannelEventList CSectionsdClient::getEventsServiceKey(const t_channel_id channel_id)
{
	CChannelEventList eList;

	if (send(sectionsd::allEventsChannelID_, (char*)&channel_id, sizeof(channel_id)))
	{
		int nBufSize = readResponse();

		if( nBufSize > 0)
		{
			char* pData = new char[nBufSize];
			receive_data(pData, nBufSize);

			char* dp = pData;

			while(dp < pData + nBufSize)
			{
				CChannelEvent aEvent;

				aEvent.eventID = *((event_id_t *) dp);
				dp+=sizeof(aEvent.eventID);

				aEvent.startTime = *((time_t *) dp);
				dp+=sizeof(aEvent.startTime);

				aEvent.duration = *((unsigned *) dp);
				dp+=sizeof(aEvent.duration);

				aEvent.description= dp;
				dp+=strlen(dp)+1;

				aEvent.text= dp;
				dp+=strlen(dp)+1;

				eList.push_back(aEvent);
			}
			delete[] pData;
		}
	}

	close_connection();
	return eList;
}

//never used
void showhexdumpa (char *label, unsigned char * from, int len)
{
  int i, j, k;
  char buf[128];
  char abuf[128];
  unsigned char fl, ol;

  fl = len / 16;
  ol = len % 16;
  if (label) {
        time_t tt = time (0);
        printf("\n%s -- %s", label, ctime (&tt));
        printf("----------------------------------------------------\n");
  }

  for (i = 0; i < fl; i++) {
        j = i * 16;
        sprintf (buf, "%03X: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", i * 16, from[j + 0], from[j + 1], from[j + 2], from[j + 3], from[j + 4], from[j + 5], from[j + 6], from[j + 7], from[j + 8], from[j + 9], from[j + 10], from[j + 11], from[j + 12], from[j + 13], from[j + 14], from[j + 15]);
        printf ("%s  ", buf);
        for (k = 0; k < 16; k++) {
          abuf[k] = (from[j + k] >= 0x20 && from[j + k] <= 0x7b) ? from[j + k] : 0x2E;
        }
        abuf[16] = 0;
        printf("%s\n", abuf);
  }
  if (ol) {
        j = fl * 16;
        sprintf (buf, "%03X: ", j);
        for (i = 0; i < ol; i++) {
          sprintf (&buf[5 + i * 3], "%02x ", from[j + i]);
          abuf[i] = (from[j + i] >= 0x20 && from[j + i] <= 0x7b) ? from[j + i] : 0x2E;
        }
        abuf[ol] = 0;
        for (i = ol; i < 16; i++)
          sprintf (&buf[5 + i * 3], "   ");
        printf ("%s ", buf);
        printf ("%s\n", abuf);
  }
  printf ("\n");
}

// 21.07.2005 - rainerk
// Convert line-terminated extended events to vector of strings
char * CSectionsdClient::parseExtendedEvents(char * dp, CEPGData * epgdata) {
	char * pItemDescriptions = dp, * pItemDescriptionStart;
	dp+=strlen(dp)+1;
	char * pItems = dp, * pItemStart;
	dp+=strlen(dp)+1;
	/* Clear vector since epgdata seems to be reused */
	epgdata->itemDescriptions.clear();
	while (*pItemDescriptions) {
		pItemDescriptionStart = pItemDescriptions;
		while (*pItemDescriptions && '\n' != *pItemDescriptions) {
			pItemDescriptions++;
		}
		char pp = *pItemDescriptions;
		*pItemDescriptions = 0;
		epgdata->itemDescriptions.push_back(pItemDescriptionStart);
/*printf("CSectionsdClient::parseExtendedEvents: desc %s\n", pItemDescriptionStart);*/
		if(!pp)
			break;
		pItemDescriptions++;
	}
	/* Clear vector since epgdata seems to be reused */
	epgdata->items.clear();
	while (*pItems) {
		pItemStart = pItems;
		while (*pItems && '\n' != *pItems) {
			pItems++;
		}
		char pp = *pItemDescriptions;
		*pItems = 0;
		epgdata->items.push_back(pItemStart);
/*printf("CSectionsdClient::parseExtendedEvents: item %s\n", pItemStart);*/
		if(!pp)
			break;
		pItems++;
	}
	return dp;
}

bool CSectionsdClient::getActualEPGServiceKey(const t_channel_id channel_id, CEPGData * epgdata)
{
	epgdata->title = "";

	if (send(sectionsd::actualEPGchannelID, (char*)&channel_id, sizeof(channel_id)))
	{
		int nBufSize = readResponse();
		if( nBufSize > 0)
		{
			char* pData = new char[nBufSize];
			receive_data(pData, nBufSize);
			close_connection();

			char* dp = pData;

			epgdata->eventID = *((event_id_t *)dp);
			dp+= sizeof(epgdata->eventID);

			epgdata->title = dp;
			dp+=strlen(dp)+1;
			epgdata->info1 = dp;
			dp+=strlen(dp)+1;
			epgdata->info2 = dp;
			dp+=strlen(dp)+1;
			// 21.07.2005 - rainerk
			// Convert line-terminated extended events to vector of strings
//showhexdumpa("Data:", (unsigned char *)pData, nBufSize);
			dp = parseExtendedEvents(dp, epgdata);

			// *dp is the length, dp+1 is the chararray[]
			epgdata->contentClassification = std::string(dp+1, *dp);
			dp+=*dp+1;
			epgdata->userClassification = std::string(dp+1, *dp);
			dp+=*dp+1;

			epgdata->fsk = *dp++;

			epgdata->epg_times.startzeit = ((CSectionsdClient::sectionsdTime *) dp)->startzeit;
			epgdata->epg_times.dauer = ((CSectionsdClient::sectionsdTime *) dp)->dauer;
			dp+= sizeof(CSectionsdClient::sectionsdTime);

			delete[] pData;
			return true;
		}
		else
			printf("no response from sectionsd\n");
	}

	close_connection();

	return false;
}

bool CSectionsdClient::getEPGid(const event_id_t eventid, const time_t starttime, CEPGData * epgdata)
{
	sectionsd::commandGetEPGid msg;

	msg.eventid   = eventid;
	msg.starttime = starttime;

	if (send(sectionsd::epgEPGid, (char *)&msg, sizeof(msg)))
	{
		int nBufSize = readResponse();
		if (nBufSize > 0)
		{
			char* pData = new char[nBufSize];
			receive_data(pData, nBufSize);
			close_connection();

			char* dp = pData;

			epgdata->eventID = *((event_id_t *)dp);
			dp+= sizeof(epgdata->eventID);

			epgdata->title = dp;
			dp+=strlen(dp)+1;
			epgdata->info1 = dp;
			dp+=strlen(dp)+1;
			epgdata->info2 = dp;
			dp+=strlen(dp)+1;
			// 21.07.2005 - rainerk
			// Convert line-terminated extended events to vector of strings
			dp = parseExtendedEvents(dp, epgdata);

			// *dp is the length, dp+1 is the chararray[]
			epgdata->contentClassification = std::string(dp+1, *dp);
			dp+=*dp+1;
			epgdata->userClassification = std::string(dp+1, *dp);
			dp+=*dp+1;

			epgdata->fsk = *dp++;

			epgdata->epg_times.startzeit = ((CSectionsdClient::sectionsdTime *) dp)->startzeit;
			epgdata->epg_times.dauer = ((CSectionsdClient::sectionsdTime *) dp)->dauer;
			dp+= sizeof(CSectionsdClient::sectionsdTime);

			delete[] pData;
			return true;
		}
		else
			printf("no response from sectionsd\n");
	}

	close_connection();

	return false;
}

bool CSectionsdClient::getEPGidShort(const event_id_t eventid, CShortEPGData * epgdata)
{
	if (send(sectionsd::epgEPGidShort, (char*)&eventid, sizeof(eventid)))
	{
		int nBufSize = readResponse();
		if( nBufSize > 0)
		{
			char* pData = new char[nBufSize];
			receive_data(pData, nBufSize);

			close_connection();

			char* dp = pData;

			for(int i = 0; i < nBufSize;i++)
				if(((unsigned char)pData[i]) == 0xff)
					pData[i] = 0;

			epgdata->title = dp;
			dp+=strlen(dp)+1;
			epgdata->info1 = dp;
			dp+=strlen(dp)+1;
			epgdata->info2 = dp;
			dp+=strlen(dp)+1;
//			printf("titel: %s\n",epgdata->title.c_str());


			delete[] pData;
			return true;
		}
		else
			printf("no response from sectionsd\n");
	}

	close_connection();

	return false;
}
#ifdef ENABLE_PPT
void CSectionsdClient::setPrivatePid(const unsigned short pid)
{
	send(sectionsd::setPrivatePid, (char*)&pid, sizeof(pid));

	readResponse();
	close_connection();
}
#endif
#endif

