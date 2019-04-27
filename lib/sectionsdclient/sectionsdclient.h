#ifndef __sectionsdclient__
#define __sectionsdclient__
/*
  Client-Interface for zapit  -   DBoxII-Project

  $Id: sectionsdclient.h,v 1.42 2007/01/12 22:57:57 houdini Exp $

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

#include <string>
#include <vector>

#include <connection/basicclient.h>

#include <sectionsdclient/sectionsdtypes.h>

class CShortEPGData
{
 public:
	std::string title;
	std::string info1;
	std::string info2;
};

class CEPGData;

class CChannelEvent
{
 public:
	t_channel_id       get_channel_id(void) const { return GET_CHANNEL_ID_FROM_EVENT_ID(eventID); }
	t_event_id         eventID;
	std::string        description;
	std::string        text;
	time_t             startTime;
	unsigned           duration;
	t_channel_id 	   channelID; 
};

typedef std::vector<CChannelEvent> CChannelEventList;

class CSectionsdClient : private CBasicClient
{
 private:
	virtual unsigned char   getVersion   () const;
	virtual const    char * getSocketName() const;

	int readResponse(char* data = NULL, unsigned int size = 0);
	bool send(const unsigned char command, const char* data = NULL, const unsigned int size = 0);
	//char * parseExtendedEvents(char * dp, CEPGData * epgdata);

 public:
	virtual ~CSectionsdClient() {};
	enum SIlanguageMode_t {
		ALL,
		FIRST_FIRST,
		FIRST_ALL,
		ALL_FIRST,
		ALL_ALL,
		LANGUAGE_MODE_OFF
	};

	enum events
		{
			EVT_TIMESET,
			EVT_GOT_CN_EPG,
			EVT_EIT_COMPLETE,
#if 0
			EVT_SERVICES_UPDATE,
			EVT_BOUQUETS_UPDATE,
#endif
			EVT_WRITE_SI_FINISHED
		};

	struct epgflags {
		enum
		{
			has_anything = 0x01,
			has_later = 0x02,
			has_current = 0x04,
			not_broadcast = 0x08,
			has_next = 0x10,
			has_no_current= 0x20,
			current_has_linkagedescriptors= 0x40
		};
	};

	struct responseGetComponentTags
	{
		std::string   component;        // Text aus dem Component Descriptor
		unsigned char componentType;    // Component Descriptor
		unsigned char componentTag;     // Component Descriptor
		unsigned char streamContent;    // Component Descriptor
	};
	typedef std::vector<responseGetComponentTags> ComponentTagList;

	struct responseGetLinkageDescriptors
	{
		std::string           name;
		t_transport_stream_id transportStreamId;
		t_original_network_id originalNetworkId;
		t_service_id          serviceId;
	};
	typedef std::vector<responseGetLinkageDescriptors> LinkageDescriptorList;

	struct sectionsdTime
	{
		time_t startzeit;
		unsigned dauer;
		sectionsdTime() {
			startzeit = 0;
			dauer = 0;
		}
	} __attribute__ ((packed)) ;

	struct responseGetNVODTimes
	{
		t_service_id                    service_id;
		t_original_network_id           original_network_id;
		t_transport_stream_id           transport_stream_id;
		CSectionsdClient::sectionsdTime zeit;
	};
	typedef std::vector<responseGetNVODTimes> NVODTimesList;

	struct responseGetCurrentNextInfoChannelID
	{
		t_event_id                      current_uniqueKey;
		CSectionsdClient::sectionsdTime current_zeit;
		std::string                     current_name;
		char                            current_fsk;
		t_event_id                      next_uniqueKey;
		CSectionsdClient::sectionsdTime next_zeit;
		std::string                     next_name;
		unsigned                        flags;
	};

	struct CurrentNextInfo : public responseGetCurrentNextInfoChannelID
	{
		CurrentNextInfo() {
			current_uniqueKey = 0;
			current_name = "";
			current_fsk = 0x0;
			next_uniqueKey = 0;
			next_name = "";
			flags = 0x0;
		}
	};

	typedef struct
	{
		int epg_cache;
		int epg_old_events;
		int epg_max_events;
		int network_ntprefresh;
		int network_ntpenable;
		int epg_extendedcache;
		std::string network_ntpserver;
		int epg_save_frequently;
		int epg_read_frequently;
		std::string epg_dir;
	} epg_config;

#if 0
	bool getComponentTagsUniqueKey(const t_event_id uniqueKey, CSectionsdClient::ComponentTagList& tags);
	bool getLinkageDescriptorsUniqueKey(const t_event_id uniqueKey, CSectionsdClient::LinkageDescriptorList& descriptors);
	bool getNVODTimesServiceKey(const t_channel_id channel_id, CSectionsdClient::NVODTimesList& nvod_list);
	bool getCurrentNextServiceKey(const t_channel_id channel_id, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next);
	CChannelEventList getChannelEvents(const bool tv_mode = true, t_channel_id* = NULL, int size = 0);
	CChannelEventList getEventsServiceKey(const t_channel_id channel_id);
	bool getEventsServiceKeySearchAdd(CChannelEventList& evtlist,const t_channel_id channel_id,char m_search_typ,std::string& m_search_text);
	bool getEPGid(const t_event_id eventid, const time_t starttime, CEPGData * epgdata);
	bool getActualEPGServiceKey(const t_channel_id channel_id, CEPGData * epgdata);
	bool getEPGidShort(const t_event_id eventid, CShortEPGData * epgdata);
	void setPrivatePid(const unsigned short pid);
#endif

	bool getIsTimeSet();
	void setPauseScanning(const bool doPause);
	void setServiceChanged(const t_channel_id channel_id, const bool requestEvent, int dnum = 0);
	void setServiceStopped();

	bool getIsScanningActive();

	void freeMemory();

	void readSIfromXML(const char * epgxmlname);

	void readSIfromXMLTV(const char * url);

	void writeSI2XML(const char * epgxmlname);

	/*
	  ein beliebiges Event anmelden
	*/
	void registerEvent(const unsigned int eventID, const unsigned int clientID, const char * const udsName);

	/*
	  ein beliebiges Event abmelden
	*/
	void unRegisterEvent(const unsigned int eventID, const unsigned int clientID);

	void setConfig(const epg_config config);
	void dumpStatus(void);

};

class CEPGData
{
 public:
	uint64_t eventID;
	CSectionsdClient::sectionsdTime	epg_times;
	std::string                     title;
	std::string                     info1;
	std::string                     info2;
	// 21.07.2005 - extended event data
	std::vector<std::string>		itemDescriptions;
	std::vector<std::string>		items;
	char                            fsk;
	unsigned char                   table_id;
#ifdef FULL_CONTENT_CLASSIFICATION
	std::string                     contentClassification;
	std::string                     userClassification;
#else
	unsigned char contentClassification;
	unsigned char userClassification;
#endif

	CEPGData()
		{
			eventID               =  0;
			fsk                   =  0;
			table_id              = 0xff;
		};
};

#endif
