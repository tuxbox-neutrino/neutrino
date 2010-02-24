#ifndef __nhttpd_neutrinoapi_h__
#define __nhttpd_neutrinoapi_h__

// c++
#include <map>
#include <string>

// tuxbox
#include <controldclient/controldclient.h>
#include <eventserver.h>
#include <sectionsdclient/sectionsdclient.h>
#include <timerdclient/timerdclient.h>
#include <zapit/client/zapitclient.h>

// nhttpd
#include "helper.h"
#include "lcdapi.h"
#include "neutrinoyparser.h"
#include "controlapi.h"
#include "lcdapi.h"

//-------------------------------------------------------------------------
// No Class Helpers
const char * _getISO639Description(const char * const iso);
bool _initialize_iso639_map(void);

//-------------------------------------------------------------------------
class CNeutrinoAPI
{
	// Clientlibs
	CControldClient		*Controld;
	CSectionsdClient	*Sectionsd;
	CZapitClient		*Zapit;
	CTimerdClient		*Timerd;
	CLCDAPI                 *LcdAPI;
	CEventServer		*EventServer;

	// complete channellists
	CZapitClient::BouquetChannelList RadioChannelList,TVChannelList;
	// events of actual channel
	std::map<unsigned, CChannelEvent *> ChannelListEvents;
	// List of available tv bouquets
	std::map<int, CZapitClient::BouquetChannelList> TVBouquetsList;
	// List of available radio bouquets
	std::map<int, CZapitClient::BouquetChannelList> RadioBouquetsList;
	// List of bouquets
	CZapitClient::BouquetList BouquetList;

	//bool standby_mode;

	// some constants
	static std::string Dbox_Hersteller[4];
	static std::string videooutput_names[5];
	static std::string videoformat_names[4];
	static std::string audiotype_names[5];

	// get functions to collect data
	bool GetChannelEvents(void);
	bool GetStreamInfo(int bitinfo[10]);
	std::string GetServiceName(t_channel_id channel_id);
	CZapitClient::BouquetChannelList *GetBouquet(unsigned int BouquetNr, int Mode);
	CZapitClient::BouquetChannelList *GetChannelList(int Mode);

	// support functions
	void ZapTo          (const char * const target);
	void ZapToSubService(const char * const target);
	void ZapToChannelId (t_channel_id channel_id);
	t_channel_id ChannelNameToChannelId(std::string search_channel_name);

	void UpdateBouquet(unsigned int BouquetNr);
	void UpdateChannelList(void);
	void UpdateBouquets(void);

	std::string timerEventType2Str(CTimerd::CTimerEventTypes type);
	std::string timerEventRepeat2Str(CTimerd::CTimerEventRepeat rep);

public:
	CNeutrinoAPI();
	~CNeutrinoAPI(void);

	CChannelEventList	eList;
	CNeutrinoYParser	*NeutrinoYParser;
	CControlAPI		*ControlAPI;

	friend class CNeutrinoYParser; // Backreference
	friend class CNeutrinoYParserCoolstream; // Backreference
	friend class CControlAPI;
};

#endif /*__nhttpd_neutrinoapi_h__*/
