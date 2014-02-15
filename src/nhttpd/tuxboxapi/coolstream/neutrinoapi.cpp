//=============================================================================
// NHTTPD
// NeutrionAPI
//
// Aggregates: NeutrinoYParser, NeutrinoControlAPI
// Defines Interfaces to:CControldClient, CSectionsdClient, CZapitClient,
//			CTimerdClient,CLCDAPI
// Place for common used Neutrino-functions used by NeutrinoYParser, NeutrinoControlAPI
//=============================================================================

// C
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// C++
#include <string>
#include <fstream>
#include <map>
#include <sstream>

// tuxbox
#include <neutrinoMessages.h>
#include <global.h>
#include <neutrino.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <gui/color.h>
#include <gui/widget/icons.h>
#include <gui/customcolor.h>
#include <daemonc/remotecontrol.h>
#include <zapit/frontend_c.h>
#include <video.h>
#include <audio.h>
#include <dmx.h>
#include <zapit/satconfig.h>
#include <zapit/client/zapitclient.h>
#include <zapit/zapit.h>
#include <zapit/bouquets.h>
#include <zapit/getservices.h>
#include <eitd/sectionsd.h>

extern CBouquetManager *g_bouquetManager;
extern CFrontend * frontend;
extern cVideo * videoDecoder;
extern cAudio * audioDecoder;

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
extern CZapitClient::SatelliteList satList;

// yhttpd
#include <ylogging.h>

// nhttpd
#include "neutrinoapi.h"

//=============================================================================
// No Class Helpers
//=============================================================================

//static std::map<std::string, std::string> iso639;
#ifndef initialize_iso639_map
bool _initialize_iso639_map(void)
{
	std::string s, t, u, v;
	std::ifstream in("/share/iso-codes/iso-639.tab");
	if (in.is_open())
	{
		while (in.peek() == '#')
			getline(in, s);
		while (in >> s >> t >> u >> std::ws)
		{
			getline(in, v);
			iso639[s] = v;
			if (s != t)
				iso639[t] = v;
		}
		in.close();
		return true;
	}
 	else
		return false;
}
#endif
//-----------------------------------------------------------------------------
const char * _getISO639Description(const char * const iso)
{
	std::map<std::string, std::string>::const_iterator it = iso639.find(std::string(iso));
	if (it == iso639.end())
		return iso;
	else
		return it->second.c_str();
}

//=============================================================================
// Initialization of static variables
//=============================================================================
std::string CNeutrinoAPI::Dbox_Hersteller[4]	= {"none", "Nokia", "Philips", "Sagem"};
std::string CNeutrinoAPI::videooutput_names[5]	= {"CVBS", "RGB with CVBS", "S-Video", "YUV with VBS", "YUV with CVBS"};
std::string CNeutrinoAPI::videoformat_names[5]	= {"automatic", "4:3", "14:9", "16:9", "20:9"};
std::string CNeutrinoAPI::audiotype_names[5] 	= {"none", "single channel","dual channel","joint stereo","stereo"};
std::string CNeutrinoAPI::mpegmodes[] 			= { "stereo", "joint_st", "dual_ch", "single_ch" };
std::string CNeutrinoAPI::ddmodes[] 			= { "CH1/CH2", "C", "L/R", "L/C/R", "L/R/S", "L/C/R/S", "L/R/SL/SR", "L/C/R/SL/SR" };

//=============================================================================
// Constructor & Destructor
//=============================================================================
CNeutrinoAPI::CNeutrinoAPI()
{
	Sectionsd = new CSectionsdClient();
	Zapit = new CZapitClient();
	Timerd = new CTimerdClient();

	NeutrinoYParser = new CNeutrinoYParser(this);
	ControlAPI = new CControlAPI(this);

	UpdateBouquets();

	EventServer = new CEventServer;
	EventServer->registerEvent2( NeutrinoMessages::SHUTDOWN, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::REBOOT, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::STANDBY_ON, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::STANDBY_OFF, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::STANDBY_TOGGLE, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::EVT_POPUP, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::EVT_EXTMSG, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::CHANGEMODE, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::EVT_START_PLUGIN, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::LOCK_RC, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::UNLOCK_RC, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::RELOAD_SETUP, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");//reload neutrino conf
	EventServer->registerEvent2( NeutrinoMessages::EVT_HDMI_CEC_VIEW_ON, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::EVT_HDMI_CEC_STANDBY, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::EVT_SET_MUTE, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::EVT_SET_VOLUME, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
}
//-------------------------------------------------------------------------

CNeutrinoAPI::~CNeutrinoAPI(void)
{
	if (NeutrinoYParser)
		delete NeutrinoYParser;
	if (ControlAPI)
		delete ControlAPI;
	if (Sectionsd)
		delete Sectionsd;
	if (Zapit)
		delete Zapit;
	if (Timerd)
		delete Timerd;
	if (EventServer)
		delete EventServer;
}

//-------------------------------------------------------------------------

void CNeutrinoAPI::UpdateBouquets(void)
{
#if 0 //FIXME
	BouquetList.clear();
	Zapit->getBouquets(BouquetList, true, true);
	for (unsigned int i = 1; i <= BouquetList.size(); i++)
		UpdateBouquet(i);

	UpdateChannelList();
#endif
}

//-------------------------------------------------------------------------
void CNeutrinoAPI::ZapTo(const char * const target)
{
	t_channel_id channel_id;

	sscanf(target,
		SCANF_CHANNEL_ID_TYPE,
		&channel_id);

	ZapToChannelId(channel_id);
}
//-------------------------------------------------------------------------
void CNeutrinoAPI::ZapToChannelId(t_channel_id channel_id)
{
	if (channel_id == Zapit->getCurrentServiceID())
	{
		//printf("Kanal ist aktuell\n");
		return;
	}

	if (Zapit->zapTo_serviceID(channel_id) != CZapitClient::ZAP_INVALID_PARAM)
		Sectionsd->setServiceChanged(channel_id, false);
}
//-------------------------------------------------------------------------

void CNeutrinoAPI::ZapToSubService(const char * const target)
{
	t_channel_id channel_id;

	sscanf(target,
		SCANF_CHANNEL_ID_TYPE,
		&channel_id);

	if (Zapit->zapTo_subServiceID(channel_id) != CZapitClient::ZAP_INVALID_PARAM)
		Sectionsd->setServiceChanged(channel_id, false);
}
//-------------------------------------------------------------------------
t_channel_id CNeutrinoAPI::ChannelNameToChannelId(std::string search_channel_name)
{
//FIXME depending on mode missing
	//int mode = Zapit->getMode();
	t_channel_id channel_id = (t_channel_id)-1;
	CStringArray channel_names = ySplitStringVector(search_channel_name, ",");

	for(unsigned int j=0;j<channel_names.size();j++) {
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannelByName(channel_names[j]);
		if(channel) {
			channel_id = channel->getChannelID();
			break;
		}
	}
	return channel_id;
}

//-------------------------------------------------------------------------
// Get functions
//-------------------------------------------------------------------------

bool CNeutrinoAPI::GetStreamInfo(int bitInfo[10])
{
	char /* *key,*/ *tmpptr, buf[100];
	long value;
	int pos = 0;

	memset(bitInfo, 0, sizeof(int[10]));

	FILE *fd = fopen("/proc/bus/bitstream", "rt");

	if (fd == NULL)
	{
		dprintf("error while opening proc-bitstream\n" );
		return false;
	}

	fgets(buf,35,fd);//dummy
	while(!feof(fd))
	{
		if(fgets(buf,35,fd)!=NULL)
		{
			buf[strlen(buf)-1]=0;
			tmpptr=buf;
			//key=strsep(&tmpptr,":");
			value=strtoul(tmpptr,NULL,0);
			bitInfo[pos]= value;
			pos++;
		}
	}
	fclose(fd);

	return true;
}

//-------------------------------------------------------------------------

bool CNeutrinoAPI::GetChannelEvents(void)
{
	eList.clear();
	CEitManager::getInstance()->getChannelEvents(eList);
	CChannelEventList::iterator eventIterator;

	ChannelListEvents.clear();

	if (eList.begin() == eList.end())
		return false;

	for (eventIterator = eList.begin(); eventIterator != eList.end(); ++eventIterator)
		ChannelListEvents[(*eventIterator).get_channel_id()] = &(*eventIterator);

	return true;
}

//-------------------------------------------------------------------------

std::string CNeutrinoAPI::GetServiceName(t_channel_id channel_id)
{
	return CServiceManager::getInstance()->GetServiceName(channel_id);
}

//-------------------------------------------------------------------------

CZapitClient::BouquetChannelList *CNeutrinoAPI::GetBouquet(unsigned int, int)
{
	//FIXME
	printf("CNeutrinoAPI::GetChannelList still used !\n");
	return NULL;
}

//-------------------------------------------------------------------------

CZapitClient::BouquetChannelList *CNeutrinoAPI::GetChannelList(int)
{
//FIXME
	printf("CNeutrinoAPI::GetChannelList still used !\n");
	return NULL;
}

//-------------------------------------------------------------------------
void CNeutrinoAPI::UpdateBouquet(unsigned int)
{
	//FIXME
}

//-------------------------------------------------------------------------
void CNeutrinoAPI::UpdateChannelList(void)
{
	//FIXME
}

//-------------------------------------------------------------------------

std::string CNeutrinoAPI::timerEventType2Str(CTimerd::CTimerEventTypes type)
{
	std::string result;
	switch (type) {
	case CTimerd::TIMER_SHUTDOWN:
		result = "Shutdown";
		break;
#if 0
	case CTimerd::TIMER_NEXTPROGRAM:
		result = "Next program";
		break;
#endif
	case CTimerd::TIMER_ZAPTO:
		result = "Zap to";
		break;
	case CTimerd::TIMER_STANDBY:
		result = "Standby";
		break;
	case CTimerd::TIMER_RECORD:
		result = "Record";
		break;
	case CTimerd::TIMER_REMIND:
		result = "Reminder";
		break;
	case CTimerd::TIMER_EXEC_PLUGIN:
		result = "Execute plugin";
		break;
	case CTimerd::TIMER_SLEEPTIMER:
		result = "Sleeptimer";
		break;
	default:
		result = "Unknown";
		break;
	}
	return result;
}

//-------------------------------------------------------------------------

std::string CNeutrinoAPI::timerEventRepeat2Str(CTimerd::CTimerEventRepeat rep)
{
	std::string result;
	switch (rep) {
	case CTimerd::TIMERREPEAT_ONCE:
		result = "once";
		break;
	case CTimerd::TIMERREPEAT_DAILY:
		result = "daily";
		break;
	case CTimerd::TIMERREPEAT_WEEKLY:
		result = "weekly";
		break;
	case CTimerd::TIMERREPEAT_BIWEEKLY:
		result = "2-weekly";
		break;
	case CTimerd::TIMERREPEAT_FOURWEEKLY:
		result = "4-weekly";
		break;
	case CTimerd::TIMERREPEAT_MONTHLY:
		result = "monthly";
		break;
	case CTimerd::TIMERREPEAT_BYEVENTDESCRIPTION:
		result = "event";
		break;
	case CTimerd::TIMERREPEAT_WEEKDAYS:
		result = "weekdays";
		break;
	default:
		if (rep > CTimerd::TIMERREPEAT_WEEKDAYS)
		{
			if (rep & 0x0200)
				result += "Mo ";
			if (rep & 0x0400)
				result += "Tu ";
			if (rep & 0x0800)
				result += "We ";
			if (rep & 0x1000)
				result += "Th ";
			if (rep & 0x2000)
				result += "Fr ";
			if (rep & 0x4000)
				result += "Sa ";
			if (rep & 0x8000)
				result += "Su ";
		}
		else
			result = "Unknown";
	}
	return result;
}

//-------------------------------------------------------------------------
std::string CNeutrinoAPI::getVideoAspectRatioAsString(void) {
	int aspectRatio = videoDecoder->getAspectRatio();
	if (aspectRatio >= 0 && aspectRatio <= 4)
		return videoformat_names[aspectRatio];
	else
		return "unknown";
}
//-------------------------------------------------------------------------
int CNeutrinoAPI::setVideoAspectRatioAsString(std::string newRatioString) {
	int newRatioInt = -1;
	for(int i=0;i<(int)sizeof(videoformat_names);i++)
		if( videoformat_names[i] == newRatioString){
			newRatioInt = i;
			break;
		}
	if(newRatioInt != -1)
		videoDecoder->setAspectRatio(newRatioInt, -1);
	return newRatioInt;
}
//-------------------------------------------------------------------------
std::string CNeutrinoAPI::getVideoResolutionAsString(void) {
	int xres, yres, framerate;
	videoDecoder->getPictureInfo(xres, yres, framerate);
	std::stringstream out;
	out << xres << "x" << yres;
	return out.str();
}

//-------------------------------------------------------------------------
std::string CNeutrinoAPI::getVideoFramerateAsString(void) {
	int xres, yres, framerate;
	std::string sframerate="unknown";
	videoDecoder->getPictureInfo(xres, yres, framerate);
	switch(framerate){
		case 2:
			sframerate="25fps";break;
		case 5:
			sframerate="50fps";break;
	}
	return sframerate;
}

//-------------------------------------------------------------------------
std::string CNeutrinoAPI::getAudioInfoAsString(void) {
	int type, layer, freq, mode, lbitrate;
	audioDecoder->getAudioInfo(type, layer, freq, lbitrate, mode);
	std::stringstream out;
	if(type == 0)
		out << "MPEG " << mpegmodes[mode] << " (" << freq <<")";
	else
		out << "DD " << ddmodes[mode] << " (" << freq <<")";
	return out.str();
}

//-------------------------------------------------------------------------
std::string CNeutrinoAPI::getCryptInfoAsString(void) {
	std::stringstream out;
	std::string casys[11]=	{"Irdeto:","Betacrypt:","Seca:","Viaccess:","Nagra:","Conax: ","Cryptoworks:","Videoguard:","EBU:","XCrypt:","PowerVU:"};
	int caids[] =		{ 0x600, 0x1700, 0x0100, 0x0500, 0x1800, 0xB00, 0xD00, 0x900, 0x2600, 0x4a00, 0x0E00 };

	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	if(channel) {
                for (unsigned short i = 0; i < 11; i++) {
                        for(casys_map_iterator_t it = channel->camap.begin(); it != channel->camap.end(); ++it) {
                                int caid = (*it) & 0xFF00;
                                if(caid == caids[i])
					out << casys[i] << hex << (*it) << "\n";
                        }

		}
	}
	return out.str();
}

//-------------------------------------------------------------------------
std::string CNeutrinoAPI::getLogoFile(std::string _logoURL, t_channel_id channelId) {
	std::string channelIdAsString = string_printf( PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS , channelId & 0xFFFFFFFFFFFFULL);
	std::string channelName = GetServiceName(channelId);
//	replace(channelName, " ", "_");
	_logoURL+="/";
	if (access((_logoURL + channelName + ".png").c_str(), 4) == 0)
		return _logoURL + channelName + ".png";
	else if (access((_logoURL + channelName + ".jpg").c_str(), 4) == 0)
		return _logoURL + channelName + ".jpg";
	else if (access((_logoURL + channelName + ".gif").c_str(), 4) == 0)
		return _logoURL + channelName + ".gif";
	else if(access((_logoURL + channelIdAsString + ".png").c_str(), 4) == 0)
		return _logoURL + channelIdAsString + ".png";
	else if (access((_logoURL + channelIdAsString + ".jpg").c_str(), 4) == 0)
		return _logoURL + channelIdAsString + ".jpg";
	else if (access((_logoURL + channelIdAsString + ".gif").c_str(), 4) == 0)
		return _logoURL + channelIdAsString + ".gif";
	else
		return "";
}

