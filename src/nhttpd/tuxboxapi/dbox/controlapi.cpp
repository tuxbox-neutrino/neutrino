//=============================================================================
// NHTTPD
// Neutrino ControlAPI
//=============================================================================
#include <config.h>
// C
#include <stdio.h>
#include <cctype>
// C++
#include <string>
#include <fstream>
#include <map>
// system
#include <unistd.h>
#include <dirent.h>
// tuxbox
#include <neutrinoMessages.h>
#include <zapit/client/zapittools.h>
#include <zapit/channel.h>
#include <zapit/bouquets.h>
#include <configfile.h>
// yhttpd
#include "yhttpd.h"
#include "ytypes_globals.h"
#include "ylogging.h"
#include "helper.h"
// nhttpd
#include "neutrinoapi.h"
#include "controlapi.h"
#include "lcdapi.h"

#define EVENTDEV "/dev/input/event0"
//-----------------------------------------------------------------------------
enum {	// not defined in input.h but used like that, at least in 2.4.22
	KEY_RELEASED = 0,
	KEY_PRESSED,
	KEY_AUTOREPEAT
};

//=============================================================================
// Initialization of static variables
//=============================================================================
std::string CControlAPI::PLUGIN_DIRS[PLUGIN_DIR_COUNT];

//=============================================================================
// constructor und destructor
//=============================================================================
CControlAPI::CControlAPI(CNeutrinoAPI *_NeutrinoAPI)
{
	NeutrinoAPI = _NeutrinoAPI;
}
//-----------------------------------------------------------------------------
void CControlAPI::init(CyhookHandler *hh)
{
	if(PLUGIN_DIRS[0] == "")
	{	// given in nhttpd.conf
		PLUGIN_DIRS[0]=hh->WebserverConfigList["WebsiteMain.override_directory"];
		PLUGIN_DIRS[0].append("/scripts");
		PLUGIN_DIRS[1]=hh->WebserverConfigList["WebsiteMain.directory"];
		PLUGIN_DIRS[1].append("/scripts");
		PLUGIN_DIRS[2]="/var/tuxbox/plugins";
		PLUGIN_DIRS[3]=PLUGINDIR;
		PLUGIN_DIRS[4]="/mnt/plugins";
	}
}

//=============================================================================
// Hooks!
//=============================================================================
//-----------------------------------------------------------------------------
THandleStatus CControlAPI::Hook_PrepareResponse(CyhookHandler *hh)
{
	init(hh);

	if(hh->UrlData["path"] == "/control/"
		|| hh->UrlData["path"] == "/cgi-bin/"
		|| hh->UrlData["path"] == "/fb/"
		)
		return HANDLED_READY;
	else
		return HANDLED_NONE;
}
//-----------------------------------------------------------------------------
// HOOK: response_hook Handler
// This is the main dispatcher for this module
//-----------------------------------------------------------------------------
THandleStatus CControlAPI::Hook_SendResponse(CyhookHandler *hh)
{
	hh->status = HANDLED_NONE;

//	log_level_printfX(4,"CControlAPI hook start url:%s\n",hh->UrlData["url"].c_str());
	init(hh);

	if(hh->UrlData["path"] == "/control/"
		|| hh->UrlData["path"] == "/cgi-bin/")
		Execute(hh);
	if(hh->UrlData["path"] == "/fb/")		// fb-compatibility for timer-calls
		compatibility_Timer(hh);
//	log_level_printfX(4,"CControlAPI hook ende status:%d\n",(int)hh->status);
//	log_level_printfX(5,"CControlAPI hook result:%s\n",hh->yresult.c_str());

	return hh->status;
}

//=============================================================================
//-------------------------------------------------------------------------
// timer compatibility
// do add/modify/remove and Return (redirect) Timerlist
//-------------------------------------------------------------------------
void CControlAPI::compatibility_Timer(CyhookHandler *hh)
{
	log_level_printf(4,"CControlAPI Compatibility Timer Start url:%s\n",hh->UrlData["url"].c_str());
	if(NeutrinoAPI->Timerd->isTimerdAvailable() && hh->ParamList.size() > 0)
	{
		if(hh->ParamList["action"] == "remove")
		{
			unsigned removeId = atoi(hh->ParamList["id"].c_str());
			NeutrinoAPI->Timerd->removeTimerEvent(removeId);
		}
		else if(hh->ParamList["action"] == "modify")
			doModifyTimer(hh);
		else if(hh->ParamList["action"] == "new")
			doNewTimer(hh);
	}
	hh->SendRedirect("/Y_Timer_List.yhtm");
}

//=============================================================================
// Main Dispatcher / Call definitions
//=============================================================================
const CControlAPI::TyCgiCall CControlAPI::yCgiCallList[]=
{
	// channel & bouquet & epg & zapping handling
	{"getservicesxml", 	&CControlAPI::GetServicesxmlCGI,""},
	{"getbouquetsxml", 	&CControlAPI::GetBouquetsxmlCGI,""},
	{"channellist", 	&CControlAPI::ChannellistCGI,	"text/plain"},
	{"getbouquet", 		&CControlAPI::GetBouquetCGI,	"+xml"},
	{"getbouquets", 	&CControlAPI::GetBouquetsCGI,	"text/plain"},
	{"getmode", 		&CControlAPI::GetModeCGI,		"text/plain"},
	{"setmode", 		&CControlAPI::SetModeCGI,		"text/plain"},
	{"epg", 			&CControlAPI::EpgCGI,			""},
	{"zapto", 			&CControlAPI::ZaptoCGI,			"text/plain"},
	{"getonidsid", 		&CControlAPI::GetChannel_IDCGI,	"text/plain"},
	// boxcontrol - system
	{"standby", 		&CControlAPI::StandbyCGI,		"text/plain"},
	{"shutdown", 		&CControlAPI::ShutdownCGI,		"text/plain"},
	{"reboot", 			&CControlAPI::RebootCGI,		"text/plain"},
	{"esound", 			&CControlAPI::EsoundCGI,		"text/plain"},
	{"getdate", 		&CControlAPI::GetDateCGI,		"text/plain"},
	{"gettime", 		&CControlAPI::GetTimeCGI,		"text/plain"},
	{"settings", 		&CControlAPI::SettingsCGI,		"text/plain"},
	{"info", 			&CControlAPI::InfoCGI,			"text/plain"},
	{"version", 		&CControlAPI::VersionCGI,		""},
	// boxcontrol - devices
	{"volume", 			&CControlAPI::VolumeCGI,		"text/plain"},
	{"lcd", 			&CControlAPI::LCDAction,		"text/plain"},
	{"system", 			&CControlAPI::SystemCGI,		"text/plain"},
	{"message", 		&CControlAPI::MessageCGI,		"text/plain"},
	{"rc", 				&CControlAPI::RCCGI,			"text/plain"},
	{"rcem", 			&CControlAPI::RCEmCGI,			"text/plain"},
	// Start skripts, plugins
	{"startplugin", 	&CControlAPI::StartPluginCGI,	"text/plain"},
	{"exec", 			&CControlAPI::ExecCGI,			"+xml"},
	{"yweb", 			&CControlAPI::YWebCGI,			"text/plain"},
	// video handling
	{"aspectratio", 	&CControlAPI::AspectRatioCGI,	"text/plain"},
	{"videoformat", 	&CControlAPI::VideoFormatCGI,	"text/plain"},
	{"videooutput", 	&CControlAPI::VideoOutputCGI,	"text/plain"},
	{"vcroutput", 		&CControlAPI::VCROutputCGI,		"text/plain"},
	{"scartmode", 		&CControlAPI::ScartModeCGI,		"text/plain"},
	// timer
	{"timer", 			&CControlAPI::TimerCGI,			"text/plain"},
	// bouquet editing
	{"setbouquet", 		&CControlAPI::setBouquetCGI,	"text/plain"},
	{"savebouquet",		&CControlAPI::saveBouquetCGI,	"text/plain"},
	{"movebouquet",		&CControlAPI::moveBouquetCGI,	"text/plain"},
	{"deletebouquet",	&CControlAPI::deleteBouquetCGI,	"text/plain"},
	{"addbouquet",		&CControlAPI::addBouquetCGI,	"text/plain"},
	{"renamebouquet",	&CControlAPI::renameBouquetCGI,	"text/plain"},
	{"changebouquet",	&CControlAPI::changeBouquetCGI,	"text/plain"},
	{"updatebouquet",	&CControlAPI::updateBouquetCGI,	"text/plain"},
	// utils
	{"build_live_url",	&CControlAPI::build_live_url,	""},


};
//-----------------------------------------------------------------------------
// Main Dispatcher
//-----------------------------------------------------------------------------
void CControlAPI::Execute(CyhookHandler *hh)
{
	int index = -1;
	std::string yresult;
	std::string filename = hh->UrlData["filename"];

	log_level_printf(4,"ControlAPI.Execute filename:(%s)\n",filename.c_str());
	// tolower(filename)
	for(unsigned int i = 0; i < filename.length(); i++)
		filename[i] = tolower(filename[i]);

	// debugging informations
	if(CLogging::getInstance()->getDebug())
	{
		dprintf("Execute CGI : %s\n",filename.c_str());
		for(CStringList::iterator it = hh->ParamList.begin() ;
			 it != hh->ParamList.end() ; it++)
				dprintf("  Parameter %s : %s\n",it->first.c_str(), it->second.c_str());
	}

	// get function index
	for(unsigned int i = 0;i < (sizeof(yCgiCallList)/sizeof(yCgiCallList[0])); i++)
		if (filename == yCgiCallList[i].func_name)
		{
			index = i;
			break;
		}
	if(index == -1) // function not found
	{
		hh->SetError(HTTP_NOT_IMPLEMENTED, HANDLED_NOT_IMPLEMENTED);
		return;
	}

	// send header
	else if(std::string(yCgiCallList[index].mime_type) == "")	// decide in function
		;
	else if(std::string(yCgiCallList[index].mime_type) == "+xml")		// Parameter xml?
		if (hh->ParamList["xml"] != "")
			hh->SetHeader(HTTP_OK, "text/xml; charset=UTF-8");
		else
			hh->SetHeader(HTTP_OK, "text/html; charset=UTF-8");
	else
		hh->SetHeader(HTTP_OK, yCgiCallList[index].mime_type);
	// response
	hh->status = HANDLED_READY;
	if (hh->Method == M_HEAD)	// HEAD or function call
		return;
	else
	{
		(this->*yCgiCallList[index].pfunc)(hh);
		return;
	}
}

//=============================================================================
// CGI Functions
// CyhookHandler contains input/output abstractions
//=============================================================================
void CControlAPI::TimerCGI(CyhookHandler *hh)
{
	if (NeutrinoAPI->Timerd->isTimerdAvailable())
	{
		if (!hh->ParamList.empty() && hh->ParamList["format"].empty())
		{
			if (hh->ParamList["action"] == "new")
				doNewTimer(hh);
			else if (hh->ParamList["action"] == "modify")
				doModifyTimer(hh);
			else if (hh->ParamList["action"] == "remove")
			{
				unsigned removeId = atoi(hh->ParamList["id"].c_str());
				NeutrinoAPI->Timerd->removeTimerEvent(removeId);
				hh->SendOk();
			}
			else if(hh->ParamList["get"] != "")
			{
				int pre=0,post=0;
				NeutrinoAPI->Timerd->getRecordingSafety(pre,post);
				if(hh->ParamList["get"] == "pre")
					hh->printf("%d\n", pre);
				else if(hh->ParamList["get"] == "post")
					hh->printf("%d\n", post);
				else
					hh->SendError();
			}

		}
		else
			SendTimers(hh);
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::SetModeCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		if (hh->ParamList["1"] == "status")	// display recoding status
		{
			if (NeutrinoAPI->Zapit->isRecordModeActive())
				hh->WriteLn("on");
			else
				hh->WriteLn("off");
			return;
		}

		if (hh->ParamList["1"] == "radio")	// switch to radio mode
		{
			int mode = NeutrinoMessages::mode_radio;
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::CHANGEMODE, CEventServer::INITID_HTTPD, (void *)&mode,sizeof(int));
			sleep(1);
			NeutrinoAPI->UpdateBouquets();
		}
		else if (hh->ParamList["1"] == "tv")	// switch to tv mode
		{
			int mode = NeutrinoMessages::mode_tv;
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::CHANGEMODE, CEventServer::INITID_HTTPD, (void *)&mode,sizeof(int));
			sleep(1);
			NeutrinoAPI->UpdateBouquets();
		}
		else if (hh->ParamList["record"] == "start")	// start record mode
		{
			if(hh->ParamList["stopplayback"] == "true")
				NeutrinoAPI->Zapit->stopPlayBack();
			NeutrinoAPI->Sectionsd->setPauseScanning(true);
			NeutrinoAPI->Zapit->setRecordMode(true);
		}
		else if (hh->ParamList["record"] == "stop")	// stop record mode
		{
			NeutrinoAPI->Zapit->setRecordMode(false);
			NeutrinoAPI->Sectionsd->setPauseScanning(false);
			if (!NeutrinoAPI->Zapit->isPlayBackActive())
				NeutrinoAPI->Zapit->startPlayBack();
		}
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::GetModeCGI(CyhookHandler *hh)
{
	int mode = NeutrinoAPI->Zapit->getMode();
	if ( mode == CZapitClient::MODE_TV)
		hh->WriteLn("tv");
	else if ( mode == CZapitClient::MODE_RADIO)
		return hh->WriteLn("radio");
	else
		return hh->WriteLn("unknown");
}

//-----------------------------------------------------------------------------
void CControlAPI::ExecCGI(CyhookHandler *hh)
{
	bool res = false;
	std::string script, result;
	// override standard header
	if (hh->ParamList.size() > 1 && hh->ParamList["xml"].empty())
		hh->SetHeader(HTTP_OK, "text/html; charset=UTF-8");
	else if (hh->ParamList.size() > 1 && !hh->ParamList["xml"].empty())
		hh->SetHeader(HTTP_OK, "text/xml; charset=UTF-8");
	else
		hh->SetHeader(HTTP_OK, "text/plain; charset=UTF-8");
	if (hh->ParamList.size() > 0)
	{
		script = hh->ParamList["1"];
		unsigned int len = hh->ParamList.size();
		for(unsigned int y=2;y<=len;y++)
			if(!hh->ParamList[itoa(y)].empty())
			{
				script += " ";
				script += hh->ParamList[itoa(y)];
			}
		result = YexecuteScript(hh, script);
	}
	else
		printf("[CControlAPI] no script given\n");

	res = (result != "error");
	if (res)
		hh->Write(result);
	else
		hh->SetError(HTTP_NOT_FOUND);
}

//-----------------------------------------------------------------------------
void CControlAPI::SystemCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{

		if (hh->ParamList["1"] == "getAViAExtIec") {
			hh->printf("%d\n", NeutrinoAPI->Zapit->IecState());
		}
		else if (hh->ParamList["setAViAExtIec"] == "on")
		{
			NeutrinoAPI->Zapit->IecOn();
			hh->SendOk();
		}
		else if (hh->ParamList["setAViAExtIec"] == "off")
		{
			NeutrinoAPI->Zapit->IecOff();
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "getAViAExtPlayBack")
			hh->printf("%d\n", NeutrinoAPI->Zapit->PlaybackState());
		else if (hh->ParamList["setAViAExtPlayBack"] == "pes")
		{
			NeutrinoAPI->Zapit->PlaybackPES();
			hh->SendOk();
		}
		else if (hh->ParamList["setAViAExtPlayBack"] == "spts")
		{
			NeutrinoAPI->Zapit->PlaybackSPTS();
			hh->SendOk();
		}
		else
			hh->SendError();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::StandbyCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		if (hh->ParamList["1"] == "on")	// standby mode on
		{
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::STANDBY_ON, CEventServer::INITID_HTTPD);
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "off")// standby mode off
		{
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::STANDBY_OFF, CEventServer::INITID_HTTPD);
			hh->SendOk();
		}
		else
			hh->SendError();
	}
	else
		if(NeutrinoAPI->Controld->getVideoPowerDown())
			hh->WriteLn("on");
		else
			hh->WriteLn("off");
}

//-----------------------------------------------------------------------------
void CControlAPI::RCCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		if (hh->ParamList["1"] == "lock")	// lock remote control
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::LOCK_RC, CEventServer::INITID_HTTPD);
		else if (hh->ParamList["1"] == "unlock")// unlock remote control
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::UNLOCK_RC, CEventServer::INITID_HTTPD);
		else
			hh->SendError();
	}
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::EsoundCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		if (hh->ParamList["1"] == "on") // esound on
		{
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::ESOUND_ON, CEventServer::INITID_HTTPD);
		}
		else if (hh->ParamList["1"] == "off") // esound off
		{
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::ESOUND_OFF, CEventServer::INITID_HTTPD);
		}
		else
			hh->SendError();
	}
	hh->SendOk();
}

//-----------------------------------------------------------------------------
// Get actual Date
// security: strftime has buffer-overflow limit. ok!
//-----------------------------------------------------------------------------
void CControlAPI::GetDateCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		//paramlos
		char *timestr = new char[50];
		struct timeval tm;
		gettimeofday(&tm, NULL);
		strftime(timestr, 20, "%d.%m.%Y\n", localtime(&tm.tv_sec) );
		hh->Write(timestr);
		delete[] timestr;
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
// Get actual Time
// security: strftime has buffer-overflow limit. ok!
//-----------------------------------------------------------------------------
void CControlAPI::GetTimeCGI(CyhookHandler *hh)
{
	time_t now = time(NULL);

	if (hh->ParamList.empty())
	{
		//paramlos
		char *timestr = new char[50];
		struct tm *tm = localtime(&now);
		strftime(timestr, 20, "%H:%M:%S\n", tm );
		hh->Write(timestr);
		delete[] timestr;
	}
	else if (hh->ParamList["1"].compare("rawtime") == 0)
		hh->printf("%ld\n",now);
	else
		hh->SendError();
}
//-----------------------------------------------------------------------------
// send settings
void CControlAPI::SettingsCGI(CyhookHandler *hh)
{
	SendSettings(hh);
}
//-----------------------------------------------------------------------------
// send services.xml
void CControlAPI::GetServicesxmlCGI(CyhookHandler *hh)
{
	hh->SendFile("/var/tuxbox/config/zapit/services.xml");
}

//-----------------------------------------------------------------------------
// send bouquets.xml
void CControlAPI::GetBouquetsxmlCGI(CyhookHandler *hh)
{
	hh->SendFile("/var/tuxbox/config/zapit/bouquets.xml");
}

//-----------------------------------------------------------------------------
// get actual channel_id
void CControlAPI::GetChannel_IDCGI(CyhookHandler *hh)
{
	CZapitClient::CCurrentServiceInfo current_pids = NeutrinoAPI->Zapit->getCurrentServiceInfo();
	hh->printf("%x%04x%04x\n",current_pids.tsid, current_pids.onid, current_pids.sid);
}

//-----------------------------------------------------------------------------
void CControlAPI::MessageCGI(CyhookHandler *hh)
{
	std::string message;
	int event = 0;

	if (!(hh->ParamList["popup"].empty()))
	{
		message = hh->ParamList["popup"];
		event = NeutrinoMessages::EVT_POPUP;
	}
	else if (!(hh->ParamList["nmsg"].empty()))
	{
		message = hh->ParamList["nmsg"];
		event = NeutrinoMessages::EVT_EXTMSG;
	}
	else
	{
		hh->SendError();
		return;
	}

	if (event != 0)
	{
		message=decodeString(message);
		NeutrinoAPI->EventServer->sendEvent(event, CEventServer::INITID_HTTPD, (void *) message.c_str(), message.length() + 1);
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::InfoCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
		hh->Write("Neutrino\n");
	else
	{
		if (hh->ParamList["1"] == "streaminfo")		// print streaminfo
			SendStreamInfo(hh);
		else if (hh->ParamList["1"] == "settings")	// print settings
			SendSettings(hh);
		else if (hh->ParamList["1"] == "version")	// send version file
			hh->SendFile("/.version");
		else if (hh->ParamList["1"] == "httpdversion")	// print httpd version typ (only ffor comptibility)
			hh->Write("3");
		else if (hh->ParamList["1"] == "nhttpd_version")// print nhttpd version
			hh->printf("%s\n", HTTPD_VERSION);
		else
			hh->SendError();
	}
}
//-----------------------------------------------------------------------------
void CControlAPI::ShutdownCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::SHUTDOWN, CEventServer::INITID_HTTPD);
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::RebootCGI(CyhookHandler *hh)
{
	FILE *f = fopen("/tmp/.reboot", "w");
	fclose(f);
	return ShutdownCGI(hh);
}

//-----------------------------------------------------------------------------
int CControlAPI::rc_send(int ev, unsigned int code, unsigned int value)
{
	struct input_event iev;
	iev.type=EV_KEY;
	iev.code=code;
	iev.value=value;
	return write(ev,&iev,sizeof(iev));
}

//-----------------------------------------------------------------------------
// security: use const char-Pointers
struct key {
	const char *name;
	const int code;
};

#ifndef KEY_TOPLEFT
#define KEY_TOPLEFT	0x1a2
#endif

#ifndef KEY_TOPRIGHT
#define KEY_TOPRIGHT	0x1a3
#endif

#ifndef KEY_BOTTOMLEFT
#define KEY_BOTTOMLEFT	0x1a4
#endif

#ifndef KEY_BOTTOMRIGHT
#define KEY_BOTTOMRIGHT	0x1a5
#endif

static const struct key keynames[] = {
  {"KEY_0",		KEY_0},
  {"KEY_1",		KEY_1},
  {"KEY_2",		KEY_2},
  {"KEY_3",		KEY_3},
  {"KEY_4",		KEY_4},
  {"KEY_5",		KEY_5},
  {"KEY_6",		KEY_6},
  {"KEY_7",		KEY_7},
  {"KEY_8",		KEY_8},
  {"KEY_9",		KEY_9},
  {"KEY_BACKSPACE",	KEY_BACKSPACE},
  {"KEY_HOME",		KEY_HOME},
  {"KEY_UP",		KEY_UP},
  {"KEY_PAGEUP",	KEY_PAGEUP},
  {"KEY_LEFT",		KEY_LEFT},
  {"KEY_RIGHT",		KEY_RIGHT},
  {"KEY_DOWN",		KEY_DOWN},
  {"KEY_PAGEDOWN",	KEY_PAGEDOWN},
  {"KEY_MUTE",		KEY_MUTE},
  {"KEY_VOLUMEDOWN",	KEY_VOLUMEDOWN},
  {"KEY_VOLUMEUP",	KEY_VOLUMEUP},
  {"KEY_POWER",		KEY_POWER},
  {"KEY_HELP",		KEY_HELP},
  {"KEY_SETUP",		KEY_SETUP},
  {"KEY_OK",		KEY_OK},
  {"KEY_RED",		KEY_RED},
  {"KEY_GREEN",		KEY_GREEN},
  {"KEY_YELLOW",	KEY_YELLOW},
  {"KEY_BLUE",		KEY_BLUE},
  {"KEY_TOPLEFT",	KEY_TOPLEFT},
  {"KEY_TOPRIGHT",	KEY_TOPRIGHT},
  {"KEY_BOTTOMLEFT",	KEY_BOTTOMLEFT},
  {"KEY_BOTTOMRIGHT",	KEY_BOTTOMRIGHT},

  //////////////// Keys on the IR Keyboard
  {"KEY_ESC",		KEY_ESC},

  {"KEY_MINUS",		KEY_MINUS},
  {"KEY_EQUAL",		KEY_EQUAL},
  {"KEY_TAB",		KEY_TAB},
  {"KEY_Q",		KEY_Q},
  {"KEY_W",		KEY_W},
  {"KEY_E",		KEY_E},
  {"KEY_R",		KEY_R},
  {"KEY_T",		KEY_T},
  {"KEY_Y",		KEY_Y},
  {"KEY_U",		KEY_U},
  {"KEY_I",		KEY_I},
  {"KEY_O",		KEY_O},
  {"KEY_P",		KEY_P},
  {"KEY_LEFTBRACE",	KEY_LEFTBRACE},
  {"KEY_RIGHTBRACE",	KEY_RIGHTBRACE},
  {"KEY_ENTER",		KEY_ENTER},
  {"KEY_LEFTCTRL",	KEY_LEFTCTRL},
  {"KEY_A",		KEY_A},
  {"KEY_S",		KEY_S},
  {"KEY_D",		KEY_D},
  {"KEY_F",		KEY_F},
  {"KEY_G",		KEY_G},
  {"KEY_H",		KEY_H},
  {"KEY_J",		KEY_J},
  {"KEY_K",		KEY_K},
  {"KEY_L",		KEY_L},
  {"KEY_SEMICOLON",	KEY_SEMICOLON},
  {"KEY_APOSTROPHE",	KEY_APOSTROPHE},
  {"KEY_GRAVE",		KEY_GRAVE},
  {"KEY_LEFTSHIFT",	KEY_LEFTSHIFT},
  {"KEY_BACKSLASH",	KEY_BACKSLASH},
  {"KEY_Z",		KEY_Z},
  {"KEY_X",		KEY_X},
  {"KEY_C",		KEY_C},
  {"KEY_V",		KEY_V},
  {"KEY_B",		KEY_B},
  {"KEY_N",		KEY_N},
  {"KEY_M",		KEY_M},
  {"KEY_COMMA",		KEY_COMMA},
  {"KEY_DOT",		KEY_DOT},
  {"KEY_SLASH",		KEY_SLASH},
  {"KEY_RIGHTSHIFT",	KEY_RIGHTSHIFT},
  {"KEY_KPASTERISK",	KEY_KPASTERISK},
  {"KEY_LEFTALT",	KEY_LEFTALT},
  {"KEY_SPACE",		KEY_SPACE},
  {"KEY_CAPSLOCK",	KEY_CAPSLOCK},
  {"KEY_F1",		KEY_F1},
  {"KEY_F2",		KEY_F2},
  {"KEY_F3",		KEY_F3},
  {"KEY_F4",		KEY_F4},
  {"KEY_F5",		KEY_F5},
  {"KEY_F6",		KEY_F6},
  {"KEY_F7",		KEY_F7},
  {"KEY_F8",		KEY_F8},
  {"KEY_F9",		KEY_F9},
  {"KEY_F10",		KEY_F10},
  {"KEY_NUMLOCK",	KEY_NUMLOCK},
  {"KEY_SCROLLLOCK",	KEY_SCROLLLOCK},
  {"KEY_KP7",		KEY_KP7},
  {"KEY_KP8",		KEY_KP8},
  {"KEY_KP9",		KEY_KP9},
  {"KEY_KPMINUS",	KEY_KPMINUS},
  {"KEY_KP4",		KEY_KP4},
  {"KEY_KP5",		KEY_KP5},
  {"KEY_KP6",		KEY_KP6},
  {"KEY_KPPLUS",	KEY_KPPLUS},
  {"KEY_KP1",		KEY_KP1},
  {"KEY_KP2",		KEY_KP2},
  {"KEY_KP3",		KEY_KP3},
  {"KEY_KP0",		KEY_KP0},
  {"KEY_KPDOT",		KEY_KPDOT},
  {"KEY_102ND",		KEY_102ND},
  {"KEY_KPENTER",	KEY_KPENTER},
  {"KEY_KPSLASH",	KEY_KPSLASH},
  {"KEY_SYSRQ",		KEY_SYSRQ},
  {"KEY_RIGHTALT",	KEY_RIGHTALT},
  {"KEY_END",		KEY_END},
  {"KEY_INSERT",	KEY_INSERT},
  {"KEY_DELETE",	KEY_DELETE},

  {"KEY_PAUSE",		KEY_PAUSE},

  {"KEY_LEFTMETA",	KEY_LEFTMETA},
  {"KEY_RIGHTMETA",	KEY_RIGHTMETA},

  {"BTN_LEFT",		BTN_LEFT},
  {"BTN_RIGHT",		BTN_RIGHT}
};

// The code here is based on rcsim. Thx Carjay!
void CControlAPI::RCEmCGI(CyhookHandler *hh)
{
  if (hh->ParamList.empty()) {
    hh->SendError();
    return;
  }
  std::string keyname = hh->ParamList["1"];
  int sendcode = -1;
  for (unsigned int i = 0; sendcode == -1 && i < sizeof(keynames)/sizeof(key); i++) {
    if (!strcmp(keyname.c_str(), keynames[i].name))
      sendcode = keynames[i].code;
  }

  if (sendcode == -1) {
    printf("[nhttpd] Key %s not found\n", keyname.c_str());
    hh->SendError();
    return;
  }
  unsigned int repeat = 1;
  unsigned int delay = 250;
  if (hh->ParamList["delay"] != "")
    delay = atoi(hh->ParamList["delay"].c_str());
  if (hh->ParamList["duration"] != "")
    repeat = atoi(hh->ParamList["duration"].c_str())*1000/delay;
  if (hh->ParamList["repeat"] != "")
    repeat = atoi(hh->ParamList["repeat"].c_str());

  int evd = open(EVENTDEV, O_RDWR);
  if (evd < 0) {
    hh->SendError();
    perror("opening event0 failed");
    return;
  }
  if (rc_send(evd, sendcode, KEY_PRESSED) < 0){
    perror("writing 'KEY_PRESSED' event failed");
    hh->SendError();
    close(evd);
    return;
  }
  for (unsigned int i = 0; i < repeat - 1; i++) {
    usleep(delay*1000);
    if (rc_send(evd, sendcode, KEY_AUTOREPEAT) < 0){
      perror("writing 'KEY_AUTOREPEAT' event failed");
      close(evd);
     hh->SendError();
      return;
    }
  }
  if (rc_send(evd, sendcode, KEY_RELEASED)<0){
    perror("writing 'KEY_RELEASED' event failed");
    close(evd);
       hh->SendError();
    return;
  }
  close(evd);
  hh->SendOk();
}
//-----------------------------------------------------------------------------
void CControlAPI::AspectRatioCGI(CyhookHandler *hh)
{
	hh->printf("%s", NeutrinoAPI->Controld->getAspectRatio() == '\0' ? "4:3" : "16:9");
}
//-----------------------------------------------------------------------------
void CControlAPI::VideoFormatCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty() || hh->ParamList["1"] == "status") {
	  	hh->WriteLn(NeutrinoAPI->videoformat_names[(unsigned int)NeutrinoAPI->Controld->getVideoFormat()]);
		return;
	}

	int new_video_format = -1;
	if (hh->ParamList["1"] == "automatic")
		new_video_format = CControldClient::VIDEOFORMAT_AUTO;
	else if (hh->ParamList["1"] == "16:9")
		new_video_format = CControldClient::VIDEOFORMAT_16_9;
	else if (hh->ParamList["1"] == "4:3"
		 || hh->ParamList["1"] == "4:3-LB")
		new_video_format = CControldClient::VIDEOFORMAT_4_3;
	else if (hh->ParamList["1"] == "4:3-PS")
		new_video_format = CControldClient::VIDEOFORMAT_4_3_PS;

	if (new_video_format != -1) {
		NeutrinoAPI->Controld->setVideoFormat(new_video_format);
		hh->SendOk();
	} else {
		hh->SendError();
	}
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::VideoOutputCGI(CyhookHandler *hh)
{
	unsigned int videooutput=0;
	if (hh->ParamList.empty() || hh->ParamList["1"] == "status") {
		hh->WriteLn(NeutrinoAPI->videooutput_names[(unsigned int) NeutrinoAPI->Controld->getVideoOutput()]);
		return ;
	} else if (hh->ParamList["1"] == "cvbs")
		videooutput = CControldClient::VIDEOOUTPUT_COMPOSITE;
	else if (hh->ParamList["1"] == "rgb")
		videooutput = CControldClient::VIDEOOUTPUT_RGB;
	else if (hh->ParamList["1"] == "s-video")
		videooutput = CControldClient::VIDEOOUTPUT_SVIDEO;
	else if (hh->ParamList["1"] == "yuv-vbs")
		videooutput = CControldClient::VIDEOOUTPUT_YUV_VBS;
	else if (hh->ParamList["1"] == "yuv-cvbs")
		videooutput = CControldClient::VIDEOOUTPUT_YUV_CVBS;
	else {
		hh->SendError();
		return;
	}

	NeutrinoAPI->Controld->setVideoOutput(videooutput);
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::VCROutputCGI(CyhookHandler *hh)
{
	unsigned int vcroutput;
	if (hh->ParamList.empty() || hh->ParamList["1"] == "status") {
		hh->WriteLn(NeutrinoAPI->videooutput_names[(unsigned char)NeutrinoAPI->Controld->getVCROutput()]);

		return;
	} else if (hh->ParamList["1"] == "cvbs")
		vcroutput = CControldClient::VIDEOOUTPUT_COMPOSITE;
	else if (hh->ParamList["1"] == "s-video")
		vcroutput = CControldClient::VIDEOOUTPUT_SVIDEO;
	else {
		hh->SendError();
		return;
	}

	// S-Video on VCR only possible when S-Video or CVBS on TV; enforce
	if (vcroutput == CControldClient::VIDEOOUTPUT_SVIDEO
	    && (NeutrinoAPI->Controld->getVideoOutput() != CControldClient::VIDEOOUTPUT_COMPOSITE)
	    && (NeutrinoAPI->Controld->getVideoOutput() != CControldClient::VIDEOOUTPUT_SVIDEO)) {
		hh->SendError();
		return;
	}
	NeutrinoAPI->Controld->setVCROutput(vcroutput);
	hh->SendOk();

	return;
}

//-----------------------------------------------------------------------------
void CControlAPI::ScartModeCGI(CyhookHandler *hh)
{
	bool new_status;
	if (hh->ParamList.empty() || hh->ParamList["1"] == "status") {
		hh->printf(NeutrinoAPI->Controld->getScartMode() ? "on" : "off");
		return;
	} else if (hh->ParamList["1"] == "on")
		new_status = true;
	else if (hh->ParamList["1"] == "off")
		new_status = false;
	else {
		hh->SendError();
		return;
	}
	NeutrinoAPI->Controld->setScartMode(new_status);
	hh->SendOk();
}

//-------------------------------------------------------------------------
void CControlAPI::VolumeCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty()) {//without param: show actual volumen
		hh->printf("%d", NeutrinoAPI->Controld->getVolume());
	}
	else if (hh->ParamList["1"].compare("mute") == 0)
	{
		NeutrinoAPI->Controld->setMute(true);
		hh->SendOk();
	}
	else if (hh->ParamList["1"].compare("unmute") == 0)
	{
		NeutrinoAPI->Controld->setMute(false);
		hh->SendOk();
	}
	else if (hh->ParamList["1"].compare("status") == 0)
	{
		hh->Write((char *) (NeutrinoAPI->Controld->getMute() ? "1" : "0"));	//  mute
	}
	else if(hh->ParamList["1"]!="")
	{	//set volume
		char vol = atol( hh->ParamList["1"].c_str() );
		NeutrinoAPI->Controld->setVolume(vol);
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::ChannellistCGI(CyhookHandler *hh)
{
	SendChannelList(hh);
}

//-----------------------------------------------------------------------------
void CControlAPI::GetBouquetCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		int mode = CZapitClient::MODE_CURRENT;

		if (!(hh->ParamList["mode"].empty()))
		{
			if (hh->ParamList["mode"].compare("TV") == 0)
				mode = CZapitClient::MODE_TV;
			if (hh->ParamList["mode"].compare("RADIO") == 0)
				mode = CZapitClient::MODE_RADIO;
		}

		// Get Bouquet Number. First matching current channel
		if (hh->ParamList["1"] == "actual")
		{
			int actual=0;
			CZapitClient::BouquetChannelList *bouquet;
			CZapitClient::BouquetList blist;
			//easier?
			for (unsigned int i = 0; i < NeutrinoAPI->BouquetList.size() && actual == 0;i++)
			{
				//hh->printf("%u %s\n", (NeutrinoAPI->BouquetList[i].bouquet_nr) + 1, NeutrinoAPI->BouquetList[i].name);
				bouquet = NeutrinoAPI->GetBouquet((NeutrinoAPI->BouquetList[i].bouquet_nr) + 1, mode);
				CZapitClient::BouquetChannelList::iterator channel = bouquet->begin();
				for (unsigned int j = 0; channel != bouquet->end() && actual == 0; channel++,j++)
				{
					if(channel->channel_id == NeutrinoAPI->Zapit->getCurrentServiceID())
						actual=i+1;
				}
			}
			hh->printf("%d",actual);
		}
		else if (!(hh->ParamList["xml"].empty()))
		{
			hh->WriteLn("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
			hh->WriteLn("<bouquetlist>");
			hh->printf("<bouquet>\n\t<bnumber>%s</bnumber>\n</bouquet>\n",hh->ParamList["bouquet"].c_str());
			CZapitClient::BouquetChannelList *bouquet;
			bouquet = NeutrinoAPI->GetBouquet(atoi(hh->ParamList["bouquet"].c_str()), mode);
			CZapitClient::BouquetChannelList::iterator channel = bouquet->begin();

			for (unsigned int i = 0; channel != bouquet->end(); channel++,i++)
				hh->printf("<channel>\n\t<number>%u</number>\n\t<id>"
					PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					"</id>\n\t<name><![CDATA[%s]]></name>\n</channel>\n",
					channel->nr,
					channel->channel_id,
					channel->name);
			hh->WriteLn("</bouquetlist>");
		}
		else
		{
			CZapitClient::BouquetChannelList *bouquet;
			bouquet = NeutrinoAPI->GetBouquet(atoi(hh->ParamList["bouquet"].c_str()), mode);
			CZapitClient::BouquetChannelList::iterator channel = bouquet->begin();

			for (unsigned int i = 0; channel != bouquet->end(); channel++,i++)
				hh->printf("%u "
					PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					" %s\n",
					channel->nr,
					channel->channel_id,
					channel->name);
		}
	}
	else
		hh->WriteLn("error");
}

//-----------------------------------------------------------------------------
void CControlAPI::GetBouquetsCGI(CyhookHandler *hh)
{
	for (unsigned int i = 0; i < NeutrinoAPI->BouquetList.size();i++)
		hh->printf("%u %s\n", (NeutrinoAPI->BouquetList[i].bouquet_nr) + 1, NeutrinoAPI->BouquetList[i].name);
}

//-----------------------------------------------------------------------------
void CControlAPI::EpgCGI(CyhookHandler *hh)
{
	CChannelEvent *event;
	NeutrinoAPI->GetChannelEvents();

	if (hh->ParamList.empty())
	{
		hh->SetHeader(HTTP_OK, "text/plain");
		CZapitClient::BouquetChannelList *channellist = NeutrinoAPI->GetChannelList(CZapitClient::MODE_CURRENT);
		CZapitClient::BouquetChannelList::iterator channel = channellist->begin();
		for(; channel != channellist->end();channel++)
		{
			event = NeutrinoAPI->ChannelListEvents[channel->channel_id];
			if (event)
				hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
						" %llu %s\n",
						channel->channel_id,
						event->eventID,
						event->description.c_str());
		}
	}
	else if (hh->ParamList["xml"].empty())
	{
		hh->SetHeader(HTTP_OK, "text/plain");
		if (hh->ParamList["1"] == "ext")
		{
			CZapitClient::BouquetChannelList *channellist = NeutrinoAPI->GetChannelList(CZapitClient::MODE_CURRENT);
			CZapitClient::BouquetChannelList::iterator channel = channellist->begin();
			for(; channel != channellist->end();channel++)
			{
				event = NeutrinoAPI->ChannelListEvents[channel->channel_id];
				if(event)
					hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
							" %ld %u %llu %s\n",
							channel->channel_id,
							event->startTime,
							event->duration,
							event->eventID,
							event->description.c_str());
			}
		}
		else if (hh->ParamList["eventid"] != "")
		{
			//special epg query
			unsigned long long epgid;
			sscanf( hh->ParamList["eventid"].c_str(), "%llu", &epgid);
			CShortEPGData epg;
			if (NeutrinoAPI->Sectionsd->getEPGidShort(epgid,&epg))
			{
				hh->WriteLn(epg.title);
				hh->WriteLn(epg.info1);
				hh->WriteLn(epg.info2);
			}
		}
		else if (hh->ParamList["eventid2fsk"] != "")
		{
			if (hh->ParamList["starttime"] != "")
			{
				unsigned long long epgid;
				time_t starttime;
				sscanf( hh->ParamList["fskid"].c_str(), "%llu", &epgid);
				sscanf( hh->ParamList["starttime"].c_str(), "%lu", &starttime);
				CEPGData longepg;
				if(NeutrinoAPI->Sectionsd->getEPGid(epgid, starttime, &longepg))
				{
					hh->printf("%u\n", longepg.fsk);
					return;
				}
			}
			hh->SendError();
		}
		else if (!(hh->ParamList["id"].empty()))
		{
			t_channel_id channel_id;
			sscanf(hh->ParamList["id"].c_str(),
			       SCANF_CHANNEL_ID_TYPE,
			       &channel_id);
			NeutrinoAPI->eList = NeutrinoAPI->Sectionsd->getEventsServiceKey(channel_id);
			CChannelEventList::iterator eventIterator;
			for (eventIterator = NeutrinoAPI->eList.begin(); eventIterator != NeutrinoAPI->eList.end(); eventIterator++)
			{
				CShortEPGData epg;
				if (NeutrinoAPI->Sectionsd->getEPGidShort(eventIterator->eventID,&epg))
				{
					hh->printf("%llu %ld %d\n", eventIterator->eventID, eventIterator->startTime, eventIterator->duration);
					hh->printf("%s\n",epg.title.c_str());
					hh->printf("%s\n",epg.info1.c_str());
					hh->printf("%s\n\n",epg.info2.c_str());
				}
			}
		}
		else
		{
			//eventlist for a chan
			t_channel_id channel_id;
			sscanf(hh->ParamList["1"].c_str(),
			       SCANF_CHANNEL_ID_TYPE,
			       &channel_id);
			SendEventList(hh, channel_id);
		}
	}
	//	xml=true&channelid=<channel_id>|channelname=<channel name>[&details=true][&max=<max items>][&stoptime=<long:stop time>]
	//	details=true : Show EPG Info1 and info2
	//	stoptime : show only items until stoptime reached
	else if (!(hh->ParamList["xml"].empty()))
	{
		hh->SetHeader(HTTP_OK, "text/xml");

		t_channel_id channel_id = (t_channel_id)-1;

		if (!(hh->ParamList["channelid"].empty()))
		{
			sscanf(hh->ParamList["channelid"].c_str(),
			SCANF_CHANNEL_ID_TYPE,
			&channel_id);
		}
		else if (!(hh->ParamList["channelname"].empty()))
		{
			channel_id = NeutrinoAPI->ChannelNameToChannelId( hh->ParamList["channelname"].c_str() );
		}
		if(channel_id != (t_channel_id)-1)
		{
			hh->WriteLn("<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>");
			hh->WriteLn("<epglist>");
			NeutrinoAPI->eList = NeutrinoAPI->Sectionsd->getEventsServiceKey(channel_id);
			hh->printf("<channel_id>"
					PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					"</channel_id>\r\n", channel_id);
			hh->printf("<channel_name><![CDATA[%s]]></channel_name>\r\n", NeutrinoAPI->GetServiceName(channel_id).c_str());

			// max = maximal output items
			int max = -1;
			if (!(hh->ParamList["max"].empty()))
				max = atoi( hh->ParamList["max"].c_str() );

			// stoptime = maximal output items until starttime >= stoptime
			long stoptime = -1;
			if (!(hh->ParamList["stoptime"].empty()))
				stoptime = atol( hh->ParamList["stoptime"].c_str() );
			int i=0;
			CChannelEventList::iterator eventIterator;
			for (eventIterator = NeutrinoAPI->eList.begin(); eventIterator != NeutrinoAPI->eList.end(); eventIterator++, i++)
			{
				if( (max != -1 && i >= max) || ( stoptime != -1 && eventIterator->startTime >= stoptime))
					break;
				hh->WriteLn("<prog>");
				hh->printf("\t<eventid>%llu</eventid>\r\n", eventIterator->eventID);
				hh->printf("\t<eventid_hex>%llx</eventid_hex>\r\n", eventIterator->eventID);
				hh->printf("\t<start_sec>%ld</start_sec>\r\n", eventIterator->startTime);
				char zbuffer[25] = {0};
				struct tm *mtime = localtime(&eventIterator->startTime);
				strftime(zbuffer,20,"%H:%M",mtime);
				hh->printf("\t<start_t>%s</start_t>\r\n", zbuffer);
				memset(zbuffer, 0, 25);
				strftime(zbuffer,20,"%d.%m.%Y",mtime);
				hh->printf("\t<date>%s</date>\r\n", zbuffer);
				hh->printf("\t<stop_sec>%ld</stop_sec>\r\n", eventIterator->startTime+eventIterator->duration);
				long _stoptime = eventIterator->startTime+eventIterator->duration;
				mtime = localtime(&_stoptime);
				strftime(zbuffer,20,"%H:%M",mtime);
				hh->printf("\t<stop_t>%s</stop_t>\r\n", zbuffer);
				hh->printf("\t<duration_min>%d</duration_min>\r\n", (int)(eventIterator->duration/60));
				hh->printf("\t<description><![CDATA[%s]]></description>\r\n", eventIterator->description.c_str());

				if (!(hh->ParamList["details"].empty()))
				{
					CShortEPGData epg;
					if (NeutrinoAPI->Sectionsd->getEPGidShort(eventIterator->eventID,&epg))
					{
						hh->printf("\t<info1><![CDATA[%s]]></info1>\r\n",epg.info1.c_str());
						hh->printf("\t<info2><![CDATA[%s]]></info2>\r\n",epg.info2.c_str());
					}
				}
				hh->WriteLn("</prog>");
			}
			hh->WriteLn("</epglist>");
		}
	}
}

//-----------------------------------------------------------------------------
void CControlAPI::VersionCGI(CyhookHandler *hh)
{
	hh->SendFile("/.version");
}

//-----------------------------------------------------------------------------
void CControlAPI::ZaptoCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				"\n",
				NeutrinoAPI->Zapit->getCurrentServiceID());
		return;
	}
	else
	{
		if (hh->ParamList["1"] == "getpids")		// getpids !
			SendcurrentVAPid(hh);
		else if (hh->ParamList["1"] == "getallpids")		// getpids !
			SendAllCurrentVAPid(hh);
		else if (hh->ParamList["1"] == "stopplayback")
		{
			NeutrinoAPI->Zapit->stopPlayBack();
			NeutrinoAPI->Sectionsd->setPauseScanning(true);
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "startplayback")
		{
			NeutrinoAPI->Zapit->startPlayBack();
			NeutrinoAPI->Sectionsd->setPauseScanning(false);
			dprintf("start playback requested..\n");
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "statusplayback")
			hh->Write((char *) (NeutrinoAPI->Zapit->isPlayBackActive() ? "1" : "0"));
		else if (hh->ParamList["1"] == "stopsectionsd")
		{
			NeutrinoAPI->Sectionsd->setPauseScanning(true);
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "startsectionsd")
		{
			NeutrinoAPI->Sectionsd->setPauseScanning(false);
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "statussectionsd")
			hh->Write((char *) (NeutrinoAPI->Sectionsd->getIsScanningActive() ? "scanning active" : "scanning paused\n"));
		else if (hh->ParamList["1"] == "restartsectionsd")
		{
			NeutrinoAPI->Sectionsd->Restart();
			hh->printf("--&gt; restarting sectionsd...<br>");
			NeutrinoAPI->Sectionsd->RegisterNeutrino();
			hh->printf("--&gt; registering neutrino...<br>");
			NeutrinoAPI->Sectionsd->setPauseScanning(false);
			hh->printf("--&gt; set scanning active...<br>");
			NeutrinoAPI->Sectionsd->setServiceChanged(NeutrinoAPI->Zapit->getCurrentServiceID(), false);
			hh->printf("--&gt; send sectionsd ServiceChanged...<br>");
			hh->printf("<hr style=\"color:black; background-color:black; height:2px; width:250px; margin-left:0; text-align:left;\">");
			hh->Write((char *) (NeutrinoAPI->Sectionsd->getIsScanningActive() ? "sectionsd restarted, scanning active!" : "sectionsd restarted, scanning paused"));
		}
		else if (hh->ParamList["1"] == "freememsectionsd")
		{
			NeutrinoAPI->Sectionsd->freeMemory();
			hh->printf("freeing memory... done!\n");
		}
		else if (hh->ParamList["1"] == "getallsubchannels")
		{
			t_channel_id current_channel = NeutrinoAPI->Zapit->getCurrentServiceID();
			CSectionsdClient::LinkageDescriptorList desc;
			CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
			NeutrinoAPI->Sectionsd->getCurrentNextServiceKey(current_channel, currentNextInfo);
			if (NeutrinoAPI->Sectionsd->getLinkageDescriptorsUniqueKey(currentNextInfo.current_uniqueKey,desc))
			{
				for(unsigned int i=0;i< desc.size();i++)
				{
					t_channel_id sub_channel_id =
						CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
						desc[i].serviceId, desc[i].originalNetworkId, desc[i].transportStreamId);
					hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
							" %s\n",
							sub_channel_id,
							(desc[i].name).c_str());
				}
			}
		}
		else if (hh->ParamList["name"] != "")
		{
			t_channel_id channel_id;
			channel_id = NeutrinoAPI->ChannelNameToChannelId(hh->ParamList["name"]);
			if(channel_id != (t_channel_id)-1)
			{
				NeutrinoAPI->ZapToChannelId(channel_id);
				hh->SendOk();
			}
			else
				hh->SendError();
		}
		else
		{
			NeutrinoAPI->ZapTo(hh->ParamList["1"].c_str());

			hh->SendOk();
		}
		return;
	}
	hh->SendError();
	return;
}

//-----------------------------------------------------------------------------
void CControlAPI::StartPluginCGI(CyhookHandler *hh)
{
	std::string pluginname;
	if (!(hh->ParamList.empty()))
	{
		if (hh->ParamList["name"] != "")
		{
			pluginname = hh->ParamList["name"];
			pluginname=decodeString(pluginname);
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::EVT_START_PLUGIN,
				CEventServer::INITID_HTTPD,
				(void *) pluginname.c_str(),
				pluginname.length() + 1);

			hh->SendOk();
		}
		else
			hh->SendError();

	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::LCDAction(CyhookHandler *hh)
{
	int tval;
	int error=0;
	int xpos=10, ypos=10, size=12, color=1, font=0;
	int x1,y1,x2,y2,coll,colf;

	if (hh->ParamList.empty())
	{
		hh->SendError();
		return;
	}

	if (hh->ParamList["lock"] != "") {
		if(sscanf( hh->ParamList["lock"].c_str(), "%d", &tval))
			NeutrinoAPI->LcdAPI->LockDisplay(tval);
		else
			error=1;
	}
	if (hh->ParamList["clear"] != "") {
		if(sscanf( hh->ParamList["clear"].c_str(), "%d", &tval)){
			if(tval)
				NeutrinoAPI->LcdAPI->Clear();
		}
		else
			error=1;
	}
	if (hh->ParamList["png"] != "") {
		if(! NeutrinoAPI->LcdAPI->ShowPng((char*)hh->ParamList["png"].c_str()))
			error=1;
	}
	if (hh->ParamList["raw"] != "") {
		char *sptr=strdup((char*)hh->ParamList["raw"].c_str()),*pptr;
		int loop=4;
		pptr=sptr;
		error=1;
		if(sscanf(pptr, "%d,%d,%d,%d",&x1,&y1,&x2,&y2)==4) {
			while(loop-- && ((pptr=strchr(pptr,','))!=NULL))
				++pptr;
			if(pptr) {
				NeutrinoAPI->LcdAPI->ShowRaw(x1,y1,x2,y2,pptr);
				error=0;
			}
		}
		if(sptr)
			free(sptr);
	}
	if (hh->ParamList["line"] != "") {
		if(sscanf( hh->ParamList["line"].c_str(), "%d,%d,%d,%d,%d",&x1,&y1,&x2,&y2,&coll)==5)
			NeutrinoAPI->LcdAPI->DrawLine(x1,y1,x2,y2,coll);
		else
			error=1;
	}
	if (hh->ParamList["rect"] != "") {
		if(sscanf( hh->ParamList["rect"].c_str(), "%d,%d,%d,%d,%d,%d",&x1,&y1,&x2,&y2,&coll,&colf)==6)
			NeutrinoAPI->LcdAPI->DrawRect(x1,y1,x2,y2,coll,colf);
		else
			error=1;
	}
	if (hh->ParamList["xpos"] != "") {
		if(sscanf( hh->ParamList["xpos"].c_str(), "%d", &tval))
			xpos=tval;
		else
			error=1;
	}
	if (hh->ParamList["ypos"] != "") {
		if(sscanf( hh->ParamList["ypos"].c_str(), "%d", &tval))
			ypos=tval;
		else
			error=1;
	}
	if (hh->ParamList["size"] != "") {
		if(sscanf( hh->ParamList["size"].c_str(), "%d", &tval))
			size=tval;
		else
			error=1;
	}
	if (hh->ParamList["color"] != "") {
		if(sscanf( hh->ParamList["color"].c_str(), "%d", &tval))
			color=tval;
		else
			error=1;
	}
	if (hh->ParamList["font"] != "") {
		if(sscanf( hh->ParamList["font"].c_str(), "%d", &tval) && tval>=0 && tval<3)
			font=tval;
		else
			error=1;
	}
	if (hh->ParamList["text"] != "")
		NeutrinoAPI->LcdAPI->DrawText(xpos, ypos, size, color, font, (char*)hh->ParamList["text"].c_str());
	if (hh->ParamList["update"] != "") {
		if(sscanf( hh->ParamList["update"].c_str(), "%d", &tval)) {
			if(tval)
				NeutrinoAPI->LcdAPI->Update();
		}
		else
			error=1;
	}

	if(error) 	hh->SendError();
	else 		hh->SendOk();
}

//-------------------------------------------------------------------------
// Send functions (for ExecuteCGI)
//-------------------------------------------------------------------------
void CControlAPI::SendEventList(CyhookHandler *hh, t_channel_id channel_id)
{
	int pos;
	NeutrinoAPI->eList = NeutrinoAPI->Sectionsd->getEventsServiceKey(channel_id);
	CChannelEventList::iterator eventIterator;

	for (eventIterator = NeutrinoAPI->eList.begin(); eventIterator != NeutrinoAPI->eList.end(); eventIterator++, pos++)
		hh->printf("%llu %ld %d %s\n", eventIterator->eventID, eventIterator->startTime, eventIterator->duration, eventIterator->description.c_str());
}

//-----------------------------------------------------------------------------
void CControlAPI::SendChannelList(CyhookHandler *hh)
{
	CZapitClient::BouquetChannelList *channellist = NeutrinoAPI->GetChannelList(CZapitClient::MODE_CURRENT);
	CZapitClient::BouquetChannelList::iterator channel = channellist->begin();

	for(; channel != channellist->end();channel++)
		hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				" %s\n",
				channel->channel_id,
				channel->name);
}

//-----------------------------------------------------------------------------
void CControlAPI::SendStreamInfo(CyhookHandler *hh)
{

	int bitInfo[10];
	NeutrinoAPI->GetStreamInfo(bitInfo);
	hh->printf("%d\n%d\n", bitInfo[0], bitInfo[1] );	//Resolution x y
	hh->printf("%d\n", bitInfo[4]*50);			//Bitrate bit/sec

	switch (bitInfo[2]) //format
	{
		case 2: hh->Write("4:3\n"); break;
		case 3: hh->Write("16:9\n"); break;
		case 4: hh->Write("2.21:1\n"); break;
		default: hh->Write("unknown\n"); break;
	}
	switch (bitInfo[3]) //fps
	{
		case 3: hh->Write("25\n"); break;
		case 6: hh->Write("50\n"); break;
		default: hh->Write("unknown\n");
	}
	hh->WriteLn(NeutrinoAPI->audiotype_names[bitInfo[6]]);
}

//-----------------------------------------------------------------------------
void CControlAPI::SendcurrentVAPid(CyhookHandler *hh)
{
	CZapitClient::responseGetPIDs pids;
	pids.PIDs.vpid=0;
	NeutrinoAPI->Zapit->getPIDS(pids);

	hh->printf("%u\n", pids.PIDs.vpid);
	if(!pids.APIDs.empty())
		hh->printf("%u\n", pids.APIDs[0].pid);
	else
		hh->printf("0\n");
}

//-----------------------------------------------------------------------------
void CControlAPI::SendAllCurrentVAPid(CyhookHandler *hh)
{
	static bool init_iso=true;
	if(init_iso)
	{
		if(_initialize_iso639_map())
			init_iso=false;
	}
	bool eit_not_ok=true;
	CZapitClient::responseGetPIDs pids;

	CSectionsdClient::ComponentTagList tags;
	pids.PIDs.vpid=0;
	NeutrinoAPI->Zapit->getPIDS(pids);

	hh->printf("%05u\n", pids.PIDs.vpid);

	t_channel_id current_channel = NeutrinoAPI->Zapit->getCurrentServiceID();
	CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
	NeutrinoAPI->Sectionsd->getCurrentNextServiceKey(current_channel, currentNextInfo);
	if (NeutrinoAPI->Sectionsd->getComponentTagsUniqueKey(currentNextInfo.current_uniqueKey,tags))
	{
		for (unsigned int i=0; i< tags.size(); i++)
		{
			for (unsigned short j=0; j< pids.APIDs.size(); j++)
			{
				if ( pids.APIDs[j].component_tag == tags[i].componentTag )
				{
 					if(!tags[i].component.empty())
					{
						if(!(isalnum(tags[i].component[0])))
							tags[i].component=tags[i].component.substr(1,tags[i].component.length()-1);
						hh->printf("%05u %s\n",pids.APIDs[j].pid,tags[i].component.c_str());
					}
					else
					{
						if(!(init_iso))
						{
							strcpy( pids.APIDs[j].desc, _getISO639Description( pids.APIDs[j].desc ) );
						}
			 			hh->printf("%05u %s %s\n",pids.APIDs[j].pid,pids.APIDs[j].desc,pids.APIDs[j].is_ac3 ? " (AC3)": " ");
					}
					eit_not_ok=false;
					break;
				}
			}
		}
	}
	if(eit_not_ok)
	{
		unsigned short i = 0;
		for (CZapitClient::APIDList::iterator it = pids.APIDs.begin(); it!=pids.APIDs.end(); it++)
		{
			if(!(init_iso))
			{
				strcpy( pids.APIDs[i].desc, _getISO639Description( pids.APIDs[i].desc ) );
			}
 			hh->printf("%05u %s %s\n",it->pid,pids.APIDs[i].desc,pids.APIDs[i].is_ac3 ? " (AC3)": " ");
			i++;
		}
	}

	if(pids.APIDs.empty())
		hh->printf("0\n"); // shouldnt happen, but print at least one apid
	if(pids.PIDs.vtxtpid)
		hh->printf("%05u vtxt\n",pids.PIDs.vtxtpid);
	if (pids.PIDs.pmtpid)
		hh->printf("%05u pmt\n",pids.PIDs.pmtpid);

}
//-----------------------------------------------------------------------------
void CControlAPI::SendSettings(CyhookHandler *hh)
{
	hh->WriteLn(
		"Boxtype " +
		NeutrinoAPI->Dbox_Hersteller[NeutrinoAPI->Controld->getBoxType()] +
		"\n"
		"videooutput " +
		NeutrinoAPI->videooutput_names[(unsigned char)NeutrinoAPI->Controld->getVideoOutput()] +
		"\n"
		"videoformat " +
		NeutrinoAPI->videoformat_names[(unsigned char)NeutrinoAPI->Controld->getVideoFormat()]
	);
}
//-----------------------------------------------------------------------------
void CControlAPI::SendTimers(CyhookHandler *hh)
{
	CTimerd::TimerList timerlist;			// List of bouquets
	bool send_id = false;

	if (hh->ParamList["format"] == "id")
		send_id = true;

	timerlist.clear();
	NeutrinoAPI->Timerd->getTimerList(timerlist);

	CTimerd::TimerList::iterator timer = timerlist.begin();

	for(; timer != timerlist.end();timer++)
	{
		// Add Data
		char zAddData[22+1] = { 0 };
		if (send_id)
		{
			zAddData[0] = '0';
			zAddData[1] = 0;
		}

		switch(timer->eventType) {
		case CTimerd::TIMER_NEXTPROGRAM:
		case CTimerd::TIMER_ZAPTO:
		case CTimerd::TIMER_RECORD:
			if (!send_id)
			{
				strncpy(zAddData, NeutrinoAPI->Zapit->getChannelName(timer->channel_id).c_str(), 22);
				if (zAddData[0] == 0)
					strcpy(zAddData, NeutrinoAPI->Zapit->isChannelTVChannel(timer->channel_id) ? "Unbekannter TV-Kanal" : "Unbekannter Radiokanal");
			}
			else
				sprintf(zAddData, PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer->channel_id);

			zAddData[22]=0;

			break;

		case CTimerd::TIMER_STANDBY:
			if (!send_id)
				sprintf(zAddData,"Standby: %s",(timer->standby_on ? "ON" : "OFF"));
			break;

		case CTimerd::TIMER_REMIND :
			if (!send_id)
				strncpy(zAddData, timer->message, 22);
			zAddData[22]=0;
			break;

		default:
			break;
		}

		hh->printf("%d %d %d %d %d %d %d %s\n",
				timer->eventID,
				(int)timer->eventType,
				(int)timer->eventRepeat,
				(int)timer->repeatCount,
				(int)timer->announceTime,
				(int)timer->alarmTime,
				(int)timer->stopTime,
				zAddData);
	}
}

//-----------------------------------------------------------------------------
// yweb : Extentions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Dispatcher
//-----------------------------------------------------------------------------
void CControlAPI::YWebCGI(CyhookHandler *hh)
{
	bool status=true;
	int para;
	if (hh->ParamList["video_stream_pids"] != "")
	{
		para=0;
		sscanf( hh->ParamList["video_stream_pids"].c_str(), "%d", &para);
		YWeb_SendVideoStreamingPids(hh, para);
	}
	else if (hh->ParamList["1"] == "radio_stream_pid")
		YWeb_SendRadioStreamingPid(hh);

	if(!status)
		hh->SendError();
}

//-----------------------------------------------------------------------------
// Get Streaming Pids 0x$pmt,0x$vpid,0x$apid with apid_no is the Number of Audio-Pid
//-----------------------------------------------------------------------------
void CControlAPI::YWeb_SendVideoStreamingPids(CyhookHandler *hh, int apid_no)
{
	CZapitClient::responseGetPIDs pids;
	int apid=0,apid_idx=0;
	pids.PIDs.vpid=0;
	NeutrinoAPI->Zapit->getPIDS(pids);

	if( apid_no < (int)pids.APIDs.size())
		apid_idx=apid_no;
	if(!pids.APIDs.empty())
		apid = pids.APIDs[apid_idx].pid;
	if(hh->ParamList["no_commas"] != "")
		hh->printf("0x%04x 0x%04x 0x%04x",pids.PIDs.pmtpid,pids.PIDs.vpid,apid);
	else
		hh->printf("0x%04x,0x%04x,0x%04x",pids.PIDs.pmtpid,pids.PIDs.vpid,apid);
}

//-----------------------------------------------------------------------------
// Get Streaming Pids 0x$pmt,0x$vpid,0x$apid with apid_no is the Number of Audio-Pid
//-----------------------------------------------------------------------------
void CControlAPI::YWeb_SendRadioStreamingPid(CyhookHandler *hh)
{
	CZapitClient::responseGetPIDs pids;
	int apid=0;
	NeutrinoAPI->Zapit->getPIDS(pids);

	if(!pids.APIDs.empty())
		apid = pids.APIDs[0].pid;
	hh->printf("0x%04x",apid);
}

//-----------------------------------------------------------------------------
std::string CControlAPI::YexecuteScript(CyhookHandler *, std::string cmd)
{
	std::string script, para, result;
	bool found = false;

	// split script and parameters
	int pos;
	if ((pos = cmd.find_first_of(" ")) > 0)
	{
		script = cmd.substr(0, pos);
		para = cmd.substr(pos+1,cmd.length() - (pos+1)); // snip
	}
	else
		script=cmd;
	// get file
	std::string fullfilename;
	script += ".sh"; //add script extention
	char cwd[255];
	getcwd(cwd, 254);

	for (unsigned int i=0;i<PLUGIN_DIR_COUNT && !found;i++)
	{
		fullfilename = PLUGIN_DIRS[i]+"/"+script;
		FILE *test =fopen(fullfilename.c_str(),"r"); // use fopen: popen does not work
		if( test != NULL )
		{
			fclose(test);
			chdir(PLUGIN_DIRS[i].c_str());
			FILE *f = popen( (fullfilename+" "+para).c_str(),"r"); //execute
			if (f != NULL)
			{
				found = true;

				char output[1024];
				while (fgets(output,1024,f)) // get script output
					result += output;
				pclose(f);
			}
		}
	}
	chdir(cwd);

	if (!found)
	{
		printf("[CControlAPI] script %s not found in\n",script.c_str());
		for (unsigned int i=0;i<PLUGIN_DIR_COUNT;i++) {
			printf("%s\n",PLUGIN_DIRS[i].c_str());
		}
		result="error";
	}
	return result;
}
//-------------------------------------------------------------------------
void CControlAPI::doModifyTimer(CyhookHandler *hh)
{
	hh->ParamList["update"]="1";
	doNewTimer(hh);
}
//-------------------------------------------------------------------------
void CControlAPI::doNewTimer(CyhookHandler *hh)
{
	time_t	announceTimeT = 0,
		stopTimeT = 0,
		alarmTimeT = 0,
		tnull = 0;
	unsigned int repCount = 0;
	int alHour=0;

	// if alarm given then in parameters im time_t format
	if(hh->ParamList["alarm"] != "")
	{
		alarmTimeT = atoi(hh->ParamList["alarm"].c_str());
		if(hh->ParamList["stop"] != "")
			stopTimeT = atoi(hh->ParamList["stop"].c_str());
		if(hh->ParamList["announce"] != "")
			announceTimeT = atoi(hh->ParamList["announce"].c_str());
		else
			announceTimeT = alarmTimeT;
	}
	else if(hh->ParamList["alDate"] != "") //given formatted
	{
		// Alarm Date - Format exact! DD.MM.YYYY
		tnull = time(NULL);
		struct tm *alarmTime=localtime(&tnull);
		alarmTime->tm_sec = 0;
		if(sscanf(hh->ParamList["alDate"].c_str(),"%2d.%2d.%4d",&(alarmTime->tm_mday), &(alarmTime->tm_mon), &(alarmTime->tm_year)) == 3)
		{
			alarmTime->tm_mon -= 1;
			alarmTime->tm_year -= 1900;
		}

		// Alarm Time - Format exact! HH:MM
		if(hh->ParamList["alTime"] != "")
			sscanf(hh->ParamList["alTime"].c_str(),"%2d.%2d",&(alarmTime->tm_hour), &(alarmTime->tm_min));
		alHour = alarmTime->tm_hour;
		correctTime(alarmTime);
		alarmTimeT = mktime(alarmTime);
		announceTimeT = alarmTimeT;
		struct tm *stopTime = localtime(&alarmTimeT);
		stopTime->tm_sec = 0;
		// Stop Time - Format exact! HH:MM
		if(hh->ParamList["stTime"] != "")
			sscanf(hh->ParamList["stTime"].c_str(),"%2d.%2d",&(stopTime->tm_hour), &(stopTime->tm_min));

		// Stop Date - Format exact! DD.MM.YYYY
		if(hh->ParamList["stDate"] != "")
			if(sscanf(hh->ParamList["stDate"].c_str(),"%2d.%2d.%4d",&(stopTime->tm_mday), &(stopTime->tm_mon), &(stopTime->tm_year)) == 3)
			{
				stopTime->tm_mon -= 1;
				stopTime->tm_year -= 1900;
			}
		correctTime(stopTime);
		stopTimeT = mktime(stopTime);
		if(hh->ParamList["stDate"] == "" && alHour > stopTime->tm_hour)
			stopTimeT += 24* 60 * 60; // add 1 Day
	}
	else	// alarm/stop time given in pieces
	{
		// alarm time
		time_t now = time(NULL);
		struct tm *alarmTime=localtime(&now);
		if(hh->ParamList["ad"] != "")
			alarmTime->tm_mday = atoi(hh->ParamList["ad"].c_str());
		if(hh->ParamList["amo"] != "")
			alarmTime->tm_mon = atoi(hh->ParamList["amo"].c_str())-1;
		if(hh->ParamList["ay"] != "")
			alarmTime->tm_year = atoi(hh->ParamList["ay"].c_str())-1900;
		if(hh->ParamList["ah"] != "")
			alarmTime->tm_hour = atoi(hh->ParamList["ah"].c_str());
		if(hh->ParamList["ami"] != "")
			alarmTime->tm_min = atoi(hh->ParamList["ami"].c_str());
		alarmTime->tm_sec = 0;
		correctTime(alarmTime);
		alarmTimeT = mktime(alarmTime);
		announceTimeT = alarmTimeT;

		// stop time
		struct tm *stopTime = alarmTime;
		if(hh->ParamList["sd"] != "")
			stopTime->tm_mday = atoi(hh->ParamList["sd"].c_str());
		if(hh->ParamList["smo"] != "")
			stopTime->tm_mon = atoi(hh->ParamList["smo"].c_str())-1;
		if(hh->ParamList["sy"] != "")
			stopTime->tm_year = atoi(hh->ParamList["sy"].c_str())-1900;
		if(hh->ParamList["sh"] != "")
			stopTime->tm_hour = atoi(hh->ParamList["sh"].c_str());
		if(hh->ParamList["smi"] != "")
			stopTime->tm_min = atoi(hh->ParamList["smi"].c_str());
		stopTime->tm_sec = 0;
		correctTime(stopTime);
		stopTimeT = mktime(stopTime);
	}

	if(announceTimeT != 0)
		announceTimeT -= 60;

	CTimerd::CTimerEventTypes type;
	if(hh->ParamList["type"] != "")
		type  = (CTimerd::CTimerEventTypes) atoi(hh->ParamList["type"].c_str());
	else // default is: record
		type = CTimerd::TIMER_RECORD;

	// repeat
	if(hh->ParamList["repcount"] != "")
	{
		repCount = atoi(hh->ParamList["repcount"].c_str());
	}
	CTimerd::CTimerEventRepeat rep;
	if(hh->ParamList["rep"] != "")
		rep = (CTimerd::CTimerEventRepeat) atoi(hh->ParamList["rep"].c_str());
	else // default: no repeat
		rep = (CTimerd::CTimerEventRepeat)0;

	if(((int)rep) >= ((int)CTimerd::TIMERREPEAT_WEEKDAYS) && hh->ParamList["wd"] != "")
		NeutrinoAPI->Timerd->getWeekdaysFromStr((int*)&rep, hh->ParamList["wd"].c_str());
	// apids
	bool changeApids=false;
	unsigned char apids=0;
	if(hh->ParamList["apcf"] == "on")
	{
		changeApids=true;
		apids=0;
	}
	else
	{
		if(hh->ParamList["apst"] == "on")
		{
			changeApids=true;
			apids |= TIMERD_APIDS_STD;
		}
		if(hh->ParamList["apal"] == "on")
		{
			changeApids=true;
			apids |= TIMERD_APIDS_ALT;
		}
		if(hh->ParamList["apac"] == "on")
		{
			changeApids=true;
			apids |= TIMERD_APIDS_AC3;
		}
	}

	CTimerd::RecordingInfo recinfo;
	CTimerd::EventInfo eventinfo;
	eventinfo.epgID = 0;
	eventinfo.epg_starttime = 0;
	eventinfo.apids = TIMERD_APIDS_CONF;
	eventinfo.recordingSafety = (hh->ParamList["rs"] == "1");

	// channel by Id or name
	if(hh->ParamList["channel_id"] != "")
		sscanf(hh->ParamList["channel_id"].c_str(),
		SCANF_CHANNEL_ID_TYPE,
		&eventinfo.channel_id);
	else
		eventinfo.channel_id = NeutrinoAPI->ChannelNameToChannelId(hh->ParamList["channel_name"]);

	std::string _rec_dir = hh->ParamList["rec_dir"];
	void *data=NULL;
	if(type == CTimerd::TIMER_RECORD)
		announceTimeT-=120;
	if(type == CTimerd::TIMER_STANDBY)
	{
		bool standby_on = (hh->ParamList["sbon"]=="1");
		data=&standby_on;
	}
	else if(type==CTimerd::TIMER_NEXTPROGRAM || type==CTimerd::TIMER_ZAPTO)
		data= &eventinfo;
	else if (type==CTimerd::TIMER_RECORD)
	{
		if(_rec_dir == "")
		{
			// get Default Recordingdir
			CConfigFile *Config = new CConfigFile(',');
			Config->loadConfig(NEUTRINO_CONFIGFILE);
			_rec_dir = Config->getString("recording_dir_0", "/mnt/filme");
			delete Config;
		}
		if(changeApids)
			eventinfo.apids = apids;
		recinfo = eventinfo;
		strncpy(recinfo.recordingDir, _rec_dir.c_str(), RECORD_DIR_MAXLEN-1);
		data = &recinfo;
	}
	else if(type==CTimerd::TIMER_REMIND)
	{
		char msg[REMINDER_MESSAGE_MAXLEN];
		memset(msg, 0, sizeof(msg));
		strncpy(msg, hh->ParamList["msg"].c_str(),REMINDER_MESSAGE_MAXLEN-1);
		data=msg;
	}
	else if(type==CTimerd::TIMER_EXEC_PLUGIN)
	{
		char msg[EXEC_PLUGIN_NAME_MAXLEN];
		memset(msg, 0, sizeof(msg));
		strncpy(msg, hh->ParamList["PluginName"].c_str(),EXEC_PLUGIN_NAME_MAXLEN-1);
		data=msg;
	}
	// update or add timer
	if(hh->ParamList["update"]=="1")
	{
		if(hh->ParamList["id"] != "")
		{
			unsigned modyId = atoi(hh->ParamList["id"].c_str());
			if(type == CTimerd::TIMER_RECORD)
				NeutrinoAPI->Timerd->modifyRecordTimerEvent(modyId, announceTimeT, alarmTimeT, stopTimeT, rep,repCount,_rec_dir.c_str());
			else
				NeutrinoAPI->Timerd->modifyTimerEvent(modyId, announceTimeT, alarmTimeT, stopTimeT, rep,repCount);
//					NeutrinoAPI->Timerd->removeTimerEvent(modyId);
			if(changeApids)
				NeutrinoAPI->Timerd->modifyTimerAPid(modyId,apids);
		}
		else
		{
			CTimerd::TimerList timerlist;
			timerlist.clear();
			NeutrinoAPI->Timerd->getTimerList(timerlist);
			CTimerd::TimerList::iterator timer = timerlist.begin();

			// Look for Recording Safety Timers too
			time_t real_alarmTimeT = alarmTimeT;
			if(eventinfo.recordingSafety)
			{
				int pre,post;
				NeutrinoAPI->Timerd->getRecordingSafety(pre,post);
				real_alarmTimeT -= pre;
			}

			for(; timer != timerlist.end();timer++)
				if(timer->alarmTime == real_alarmTimeT)
				{
					NeutrinoAPI->Timerd->removeTimerEvent(timer->eventID);
					break;
				}
			NeutrinoAPI->Timerd->addTimerEvent(type,data,announceTimeT,alarmTimeT,stopTimeT,rep,repCount);
		}
	}
	else
		NeutrinoAPI->Timerd->addTimerEvent(type,data,announceTimeT,alarmTimeT,stopTimeT,rep,repCount);

	hh->SendOk();
}
//-------------------------------------------------------------------------
void CControlAPI::setBouquetCGI(CyhookHandler *hh)
{
	if (hh->ParamList["selected"] != "") {
		int selected = atoi(hh->ParamList["selected"].c_str());
		if(hh->ParamList["action"].compare("hide") == 0)
			NeutrinoAPI->Zapit->setBouquetHidden(selected - 1,true);
		else if(hh->ParamList["action"].compare("show") == 0)
			NeutrinoAPI->Zapit->setBouquetHidden(selected - 1,false);
		else if(hh->ParamList["action"].compare("lock") == 0)
			NeutrinoAPI->Zapit->setBouquetLock(selected - 1,true);
		else if(hh->ParamList["action"].compare("unlock") == 0)
			NeutrinoAPI->Zapit->setBouquetLock(selected - 1,false);
		hh->SendOk();
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::saveBouquetCGI(CyhookHandler *hh)
{
	NeutrinoAPI->Zapit->saveBouquets();
	NeutrinoAPI->UpdateBouquets();
	hh->SendOk();
}
//-------------------------------------------------------------------------
void CControlAPI::moveBouquetCGI(CyhookHandler *hh)
{
	if (hh->ParamList["selected"] != "" && (
		hh->ParamList["action"] == "up" ||
		hh->ParamList["action"] == "down"))
	{
		int selected = atoi(hh->ParamList["selected"].c_str());
		if (hh->ParamList["action"] == "up") {
			NeutrinoAPI->Zapit->moveBouquet(selected - 1, (selected - 1) - 1);
			selected--;
		} else {
			NeutrinoAPI->Zapit->moveBouquet(selected - 1, (selected + 1) - 1);
			selected++;
		}
		hh->SendOk();
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::deleteBouquetCGI(CyhookHandler *hh)
{
	int selected = -1;

	if (hh->ParamList["selected"] != "") {
		selected = atoi(hh->ParamList["selected"].c_str());
		NeutrinoAPI->Zapit->deleteBouquet(selected - 1);
		hh->SendOk();
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::addBouquetCGI(CyhookHandler *hh)
{
	if (!hh->ParamList["name"].empty())
	{
		std::string tmp = hh->ParamList["name"];
		if (NeutrinoAPI->Zapit->existsBouquet(tmp.c_str()) == -1)
		{
			NeutrinoAPI->Zapit->addBouquet(tmp.c_str());
			hh->SendOk();
		}
		else
			hh->SendError();
	}
}
//-------------------------------------------------------------------------
void CControlAPI::renameBouquetCGI(CyhookHandler *hh)
{
	if (hh->ParamList["selected"] != "")
	{
		if (hh->ParamList["nameto"] != "")
		{
			if (NeutrinoAPI->Zapit->existsBouquet((hh->ParamList["nameto"]).c_str()) == -1)
			{
				NeutrinoAPI->Zapit->renameBouquet(atoi(hh->ParamList["selected"].c_str()) - 1, ZapitTools::Latin1_to_UTF8(hh->ParamList["nameto"].c_str()).c_str());
				hh->SendOk();
				return;
			}
		}
	}
	hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::changeBouquetCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList["selected"].empty()))
	{
		int selected = atoi(hh->ParamList["selected"].c_str());
		CZapitClient::BouquetChannelList BChannelList;
		NeutrinoAPI->Zapit->getBouquetChannels(selected - 1, BChannelList, CZapitClient::MODE_CURRENT);
		CZapitClient::BouquetChannelList::iterator channels = BChannelList.begin();
		for(; channels != BChannelList.end();channels++)
		{
			NeutrinoAPI->Zapit->removeChannelFromBouquet(selected - 1, channels->channel_id);
		}

		t_channel_id channel_id;
		int delta;
		const char * bchannels = hh->ParamList["bchannels"].c_str();
		while (sscanf(bchannels,
			      SCANF_CHANNEL_ID_TYPE
			      "%n",
			      &channel_id,
			      &delta) > 0)
		{
			NeutrinoAPI->Zapit->addChannelToBouquet(selected - 1, channel_id);
			bchannels += (delta + 1); // skip the separating ',', too
		}

		NeutrinoAPI->Zapit->renumChannellist();
		NeutrinoAPI->UpdateBouquets();
		if(hh->ParamList["redirect"] != "")
			hh->SendRewrite(hh->ParamList["redirect"]);
		else
			hh->SendOk();
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::updateBouquetCGI(CyhookHandler *hh)
{
	NeutrinoAPI->UpdateBouquets();
	hh->SendOk();
}
//-------------------------------------------------------------------------
// audio_no : (optional) audio channel
// host : (optional) ip of dbox
void CControlAPI::build_live_url(CyhookHandler *hh)
{
	std::string xpids,port,yresult;
	int mode = NeutrinoAPI->Zapit->getMode();

	if ( mode == CZapitClient::MODE_TV)
	{
		CZapitClient::responseGetPIDs pids;
		int apid=0,apid_no=0,apid_idx=0;
		pids.PIDs.vpid=0;

		if(hh->ParamList["audio_no"] !="")
			apid_no = atoi(hh->ParamList["audio_no"].c_str());
		NeutrinoAPI->Zapit->getPIDS(pids);

		if( apid_no < (int)pids.APIDs.size())
			apid_idx=apid_no;
		if(!pids.APIDs.empty())
			apid = pids.APIDs[apid_idx].pid;
		xpids = string_printf("0x%04x,0x%04x,0x%04x",pids.PIDs.pmtpid,pids.PIDs.vpid,apid);
	}
	else if ( mode == CZapitClient::MODE_RADIO)
	{
		CZapitClient::responseGetPIDs pids;
		int apid=0;

		NeutrinoAPI->Zapit->getPIDS(pids);
		if(!pids.APIDs.empty())
			apid = pids.APIDs[0].pid;

		xpids = string_printf("0x%04x",apid);
	}
	else
		hh->SendError();
	// build url
	std::string url = "";
	if(hh->ParamList["host"] !="")
		url = "http://"+hh->ParamList["host"];
	else
		url = "http://"+hh->HeaderList["Host"];
	url += (mode == CZapitClient::MODE_TV) ? ":31339/0," : ":31338/";
	url += xpids;
	// response url
	if(hh->ParamList["vlc_link"] !="")
	{
		write_to_file("/tmp/vlc.m3u", url);
		hh->SendRedirect("/tmp/vlc.m3u");
	}
	else
	{
		hh->SetHeader(HTTP_OK, "text/html");
		hh->Write(url);
	}
}
