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
#include <driver/pictureviewer/pictureviewer.h>
#include <system/httptool.h>
#include <system/helpers.h>
#include <gui/color.h>
#include <gui/widget/icons.h>
#include <gui/movieplayer.h>
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
#include <OpenThreads/ScopedLock>

extern CPictureViewer *g_PicViewer;
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
	EventServer->registerEvent2( NeutrinoMessages::RECORD_START, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");
	EventServer->registerEvent2( NeutrinoMessages::RECORD_STOP, CEventServer::INITID_HTTPD, "/tmp/neutrino.sock");

	pmutex = new OpenThreads::Mutex(OpenThreads::Mutex::MUTEX_RECURSIVE);
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

	delete pmutex;
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
	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
	t_channel_id channel_id;

	sscanf(target,
		SCANF_CHANNEL_ID_TYPE,
		&channel_id);

	ZapToChannelId(channel_id);
}
//-------------------------------------------------------------------------
void CNeutrinoAPI::ZapToChannelId(t_channel_id channel_id)
{
	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
	if (channel_id == Zapit->getCurrentServiceID())
	{
		//printf("Kanal ist aktuell\n");
		return;
	}

	CMoviePlayerGui::getInstance().stopPlayBack();
	Zapit->zapTo_serviceID(channel_id);
}
//-------------------------------------------------------------------------

void CNeutrinoAPI::ZapToSubService(const char * const target)
{
	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
	t_channel_id channel_id;

	sscanf(target,
		SCANF_CHANNEL_ID_TYPE,
		&channel_id);

	CMoviePlayerGui::getInstance().stopPlayBack();
	Zapit->zapTo_subServiceID(channel_id);
}
//-------------------------------------------------------------------------
t_channel_id CNeutrinoAPI::ChannelNameToChannelId(std::string search_channel_name)
{
	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
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
#if 0 /* unused funktion*/
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
#endif
//-------------------------------------------------------------------------

bool CNeutrinoAPI::GetChannelEvents(void)
{
	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
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

void CNeutrinoAPI::GetChannelEvent(t_channel_id channel_id, CChannelEvent &event)
{
	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
	event.eventID = 0;

	t_channel_id epg_id = channel_id;
	CZapitChannel * ch = CServiceManager::getInstance()->FindChannel(channel_id);
	if (ch)
		epg_id = ch->getEpgID();

	CChannelEvent * evt = ChannelListEvents[epg_id];
	if (evt)
		event = *evt;
}

//-------------------------------------------------------------------------

std::string CNeutrinoAPI::GetServiceName(t_channel_id channel_id)
{
	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
	return CServiceManager::getInstance()->GetServiceName(channel_id);
}

//-------------------------------------------------------------------------
#if 0 //never used
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
#endif
//-------------------------------------------------------------------------

std::string CNeutrinoAPI::timerEventType2Str(CTimerd::CTimerEventTypes type)
{
	std::string result;
	switch (type) {
	case CTimerd::TIMER_SHUTDOWN:
		result = "{=L:timerlist.type.shutdown=}";
		break;
#if 0
	case CTimerd::TIMER_NEXTPROGRAM:
		result = "{=L:timerlist.type.nextprogram=}";
		break;
#endif
	case CTimerd::TIMER_ZAPTO:
		result = "{=L:timerlist.type.zapto=}";
		break;
	case CTimerd::TIMER_STANDBY:
		result = "{=L:timerlist.type.standby=}";
		break;
	case CTimerd::TIMER_RECORD:
		result = "{=L:timerlist.type.record=}";
		break;
	case CTimerd::TIMER_REMIND:
		result = "{=L:timerlist.type.remind=}";
		break;
	case CTimerd::TIMER_EXEC_PLUGIN:
		result = "{=L:timerlist.type.execplugin=}";
		break;
	case CTimerd::TIMER_SLEEPTIMER:
		result = "{=L:timerlist.type.sleeptimer=}";
		break;
	default:
		result = "{=L:timerlist.type.unknown=}";
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
		result = "{=L:timerlist.repeat.once=}";
		break;
	case CTimerd::TIMERREPEAT_DAILY:
		result = "{=L:timerlist.repeat.daily=}";
		break;
	case CTimerd::TIMERREPEAT_WEEKLY:
		result = "{=L:timerlist.repeat.weekly=}";
		break;
	case CTimerd::TIMERREPEAT_BIWEEKLY:
		result = "{=L:timerlist.repeat.biweekly=}";
		break;
	case CTimerd::TIMERREPEAT_FOURWEEKLY:
		result = "{=L:timerlist.repeat.fourweekly=}";
		break;
	case CTimerd::TIMERREPEAT_MONTHLY:
		result = "{=L:timerlist.repeat.monthly=}";
		break;
	case CTimerd::TIMERREPEAT_BYEVENTDESCRIPTION:
		result = "{=L:timerlist.repeat.byeventdescription=}";
		break;
	case CTimerd::TIMERREPEAT_WEEKDAYS:
		result = "{=L:timerlist.repeat.weekdays=}";
		break;
	default:
		if (rep > CTimerd::TIMERREPEAT_WEEKDAYS)
		{
			if (rep & 0x0200)
				result += "{=L:date.mo=} ";
			if (rep & 0x0400)
				result += "{=L:date.tu=} ";
			if (rep & 0x0800)
				result += "{=L:date.we=} ";
			if (rep & 0x1000)
				result += "{=L:date.th=} ";
			if (rep & 0x2000)
				result += "{=L:date.fr=} ";
			if (rep & 0x4000)
				result += "{=L:date.sa=} ";
			if (rep & 0x8000)
				result += "{=L:date.su=} ";
		}
		else
			result = "{=L:timerlist.type.unknown=}";
	}
	return result;
}

//-------------------------------------------------------------------------
std::string CNeutrinoAPI::getVideoAspectRatioAsString(void)
{
	int aspectRatio = videoDecoder->getAspectRatio();
	if (aspectRatio >= 0 && aspectRatio <= 4)
		return videoformat_names[aspectRatio];
	else
		return "{=L:unknown=}";
}
//-------------------------------------------------------------------------
int CNeutrinoAPI::setVideoAspectRatioAsString(std::string newRatioString)
{
	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
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
std::string CNeutrinoAPI::getVideoResolutionAsString(void)
{
	int xres, yres, framerate;
	videoDecoder->getPictureInfo(xres, yres, framerate);
	std::stringstream out;
	out << xres << "x" << yres;
	return out.str();
}

//-------------------------------------------------------------------------
std::string CNeutrinoAPI::getVideoFramerateAsString(void)
{
	int xres, yres, framerate;
	std::string sframerate = "{=L:unknown=}";
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
std::string CNeutrinoAPI::getAudioInfoAsString(void)
{
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
std::string CNeutrinoAPI::getCryptInfoAsString(void)
{
	std::stringstream out;
	std::string casys[11]=	{"Irdeto:","Betacrypt:","Seca:","Viaccess:","Nagra:","Conax: ","Cryptoworks:","Videoguard:","EBU:","XCrypt:","PowerVU:"};
	int caids[] =		{ 0x600, 0x1700, 0x0100, 0x0500, 0x1800, 0xB00, 0xD00, 0x900, 0x2600, 0x4a00, 0x0E00 };

	OpenThreads::ScopedPointerLock<OpenThreads::Mutex> lock(pmutex);
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
std::string CNeutrinoAPI::getLogoFile(t_channel_id channelId)
{
	std::string channelName = GetServiceName(channelId);
	std::string logoString;
	if (g_PicViewer->GetLogoName(channelId, channelName, logoString, NULL, NULL))
		return logoString;
	return "";
}

std::string CNeutrinoAPI::GetRemoteBoxIP(std::string _rbname)
{
	std::string c_url = "";
	for (std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin(); it != g_settings.timer_remotebox_ip.end(); ++it)
	{
		if (it->rbname == _rbname)
		{
			if (!it->user.empty() && !it->pass.empty())
				c_url += it->user + ":" + it->pass +"@";
			c_url += it->rbaddress;
			c_url += ":" + to_string(it->port);
			break;
		}
	}
	return c_url;
}

void CNeutrinoAPI::SendAllTimers(std::string url, bool force)
{
	CTimerd::TimerList timerlist;
	timerlist.clear();
	Timerd->getTimerList(timerlist);
	sort(timerlist.begin(), timerlist.end());

	int pre,post;
	Timerd->getRecordingSafety(pre,post);
	CHTTPTool httpTool;
	std::string r_url;

	for(CTimerd::TimerList::iterator timer = timerlist.begin(); timer != timerlist.end(); ++timer)
	{
		if (timer->eventType == CTimerd::TIMER_RECORD) {
			r_url = "http://";
			r_url += url;
			r_url += "/control/timer?action=new";
			r_url += "&alarm=" + to_string((int)timer->alarmTime + pre);
			r_url += "&stop=" + to_string((int)timer->stopTime - post);
			r_url += "&announce=" + to_string((int)timer->announceTime + pre);
			r_url += "&channel_id=" + string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer->channel_id);
			r_url += "&aj=on";
			r_url += "&rs=on";

			r_url = httpTool.downloadString(r_url, -1, 300);

			if ((r_url=="ok") || force)
				Timerd->removeTimerEvent(timer->eventID);
		}
	}
}
