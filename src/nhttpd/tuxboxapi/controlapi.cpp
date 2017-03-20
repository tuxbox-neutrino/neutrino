//=============================================================================
// NHTTPD
// Neutrino ControlAPI
//=============================================================================
#include <config.h>
// C
#include <stdio.h>
#include <stdint.h>
#include <cctype>

// C++
#include <string>
#include <fstream>
#include <map>
// system
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h> // for statfs
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <unistd.h>
#include <dirent.h>
// tuxbox
#include <global.h>
#include <neutrinoMessages.h>
#include <zapit/client/zapittools.h>
#include <zapit/zapit.h>
#include <eitd/sectionsd.h>
#include <configfile.h>
#include <system/configure_network.h>
#include <cs_api.h>
#include <global.h>
#include <gui/plugins.h>//for relodplugins
#include <neutrino.h>
#include <driver/screenshot.h>
#include <gui/rc_lock.h>
#include <rcsim.h>
#include <driver/pictureviewer/pictureviewer.h>
extern CPictureViewer *g_PicViewer;

// yhttpd
#include <yhttpd.h>
#include <ytypes_globals.h>
#include <ylogging.h>
#include <helper.h>
// nhttpd
#include "neutrinoapi.h"
#include "controlapi.h"
#include <video.h>
#include <zapit/femanager.h>

extern cVideo * videoDecoder;

extern CPlugins *g_Plugins;//for relodplugins
extern CBouquetManager *g_bouquetManager;
#define EVENTDEV "/dev/input/input0"

//-----------------------------------------------------------------------------
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
	if(PLUGIN_DIRS[0].empty())
	{	// given in nhttpd.conf
		PLUGIN_DIRS[0]=PLUGIN_DIRS[1]=hh->WebserverConfigList["WebsiteMain.override_directory"];
		PLUGIN_DIRS[1].append("/scripts");
		PLUGIN_DIRS[2]=PLUGIN_DIRS[3]=hh->WebserverConfigList["WebsiteMain.directory"];
		PLUGIN_DIRS[3].append("/scripts");
		PLUGIN_DIRS[4]=GAMESDIR;
		PLUGIN_DIRS[5]=g_settings.plugin_hdd_dir;
		PLUGIN_DIRS[6]=PLUGINDIR_MNT;
		PLUGIN_DIRS[7]=PLUGINDIR_VAR;
		PLUGIN_DIRS[8]=PLUGINDIR;
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
	if(NeutrinoAPI->Timerd->isTimerdAvailable() && !hh->ParamList.empty() )
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
	{"getservicesxml",	&CControlAPI::GetServicesxmlCGI,	""},
	{"getbouquetsxml",	&CControlAPI::GetBouquetsxmlCGI,	""},
	{"getubouquetsxml",	&CControlAPI::GetUBouquetsxmlCGI,	""},
	{"channellist",		&CControlAPI::ChannellistCGI,		"text/plain"},
	{"logolist",		&CControlAPI::LogolistCGI,		"text/plain"},
	{"getbouquet",		&CControlAPI::GetBouquetCGI,		"+xml"},
	{"getchannel",		&CControlAPI::GetChannelCGI,		""},
	{"getbouquets",		&CControlAPI::GetBouquetsCGI,		"+xml"},
	{"getmode",		&CControlAPI::GetModeCGI,		"text/plain"},
	{"setmode",		&CControlAPI::SetModeCGI,		"text/plain"},
	{"epgsearchxml",	&CControlAPI::EpgSearchXMLCGI,		""},
	{"epgsearch",		&CControlAPI::EpgSearchCGI,		""},
	{"epg",			&CControlAPI::EpgCGI,			""},
	{"zapto",		&CControlAPI::ZaptoCGI,			"text/plain"},
	{"signal",		&CControlAPI::SignalInfoCGI,		"text/plain"},
	{"getonidsid",		&CControlAPI::GetChannelIDCGI,		"text/plain"},
	{"getchannelid",	&CControlAPI::GetChannelIDCGI,		""},
	{"getepgid",		&CControlAPI::GetEpgIDCGI,		""},
	{"currenttpchannels",	&CControlAPI::GetTPChannel_IDCGI,	"text/plain"},
	// boxcontrol - system
	{"standby",		&CControlAPI::StandbyCGI,		"text/plain"},
	{"shutdown",		&CControlAPI::ShutdownCGI,		"text/plain"},
	{"reboot",		&CControlAPI::RebootCGI,		"text/plain"},
	{"getdate",		&CControlAPI::GetDateCGI,		"text/plain"},
	{"gettime",		&CControlAPI::GetTimeCGI,		"text/plain"},
	{"info",		&CControlAPI::InfoCGI,			"text/plain"},
	{"version",		&CControlAPI::VersionCGI,		""},
	{"reloadsetup",		&CControlAPI::ReloadNeutrinoSetupCGI,	""},
	{"reloadplugins",	&CControlAPI::ReloadPluginsCGI,		""},
	{"reloadchannels",	&CControlAPI::ReloadChannelsCGI,	""},
#ifdef SCREENSHOT
	{"screenshot",		&CControlAPI::ScreenshotCGI,		""},
#endif
	// boxcontrol - devices
	{"volume",		&CControlAPI::VolumeCGI,		"text/plain"},
	{"lcd",			&CControlAPI::LCDAction,		"text/plain"},
	{"system",		&CControlAPI::SystemCGI,		"text/plain"},
	{"message",		&CControlAPI::MessageCGI,		"text/plain"},
	{"rc",			&CControlAPI::RCCGI,			"text/plain"},
	{"rcem",		&CControlAPI::RCEmCGI,			"text/plain"},
	// Start skripts, plugins
	{"startplugin",		&CControlAPI::StartPluginCGI,		"text/plain"},
	{"exec",		&CControlAPI::ExecCGI,			"text/plain"},
	{"yweb",		&CControlAPI::YWebCGI,			"text/plain"},
	// video & Audio handling
	{"aspectratio",		&CControlAPI::AspectRatioCGI,		"text/plain"},
	{"videoformat",		&CControlAPI::VideoFormatCGI,		"text/plain"},
	{"videooutput",		&CControlAPI::VideoOutputCGI,		"text/plain"},
	{"vcroutput",		&CControlAPI::VCROutputCGI,		"text/plain"},
	{"scartmode",		&CControlAPI::ScartModeCGI,		"text/plain"},
	{"audio",		&CControlAPI::AudioCGI,			"text/plain"},
	{"crypt",		&CControlAPI::CryptCGI,			"text/plain"},
	// timer
	{"timer",		&CControlAPI::TimerCGI,			"text/plain"},
	// bouquet editing
	{"setbouquet",		&CControlAPI::setBouquetCGI,		"text/plain"},
	{"savebouquet",		&CControlAPI::saveBouquetCGI,		"text/plain"},
	{"movebouquet",		&CControlAPI::moveBouquetCGI,		"text/plain"},
	{"deletebouquet",	&CControlAPI::deleteBouquetCGI,		"text/plain"},
	{"addbouquet",		&CControlAPI::addBouquetCGI,		"text/plain"},
	{"renamebouquet",	&CControlAPI::renameBouquetCGI,		"text/plain"},
	{"changebouquet",	&CControlAPI::changeBouquetCGI,		"text/plain"},
	{"updatebouquet",	&CControlAPI::updateBouquetCGI,		"text/plain"},
	// xmltv
	{"xmltv.data",		&CControlAPI::xmltvepgCGI,		"+xml"},
	{"xmltv.m3u",		&CControlAPI::xmltvm3uCGI,		""},
	// utils
	{"build_live_url",	&CControlAPI::build_live_url,		""},
	{"get_logo",		&CControlAPI::logoCGI,			"text/plain"},
	// settings
	{"config",		&CControlAPI::ConfigCGI,		"text/plain"},
	// filehandling
	{"file",		&CControlAPI::FileCGI,			"+xml"},
	{"statfs",		&CControlAPI::StatfsCGI,		"+xml"},
	{"getdir",		&CControlAPI::getDirCGI,		"+xml"},
	{"getmovies",		&CControlAPI::getMoviesCGI,		"+xml"}
};
//-----------------------------------------------------------------------------
// Main Dispatcher
//-----------------------------------------------------------------------------
void CControlAPI::Execute(CyhookHandler *hh)
{
	int index = -1;
	std::string filename = hh->UrlData["filename"];

	log_level_printf(4,"ControlAPI.Execute filename:(%s)\n",filename.c_str());
	// tolower(filename)
	for(unsigned int i = 0; i < filename.length(); i++)
		filename[i] = tolower(filename[i]);

	func_req = filename;

	// debugging informations
	if(CLogging::getInstance()->getDebug())
	{
		dprintf("Execute CGI : %s\n", func_req.c_str());
		for(CStringList::iterator it = hh->ParamList.begin(); it != hh->ParamList.end(); ++it)
			dprintf("  Parameter %s : %s\n", it->first.c_str(), it->second.c_str());
	}

	// get function index
	for(unsigned int i = 0; i < (sizeof(yCgiCallList)/sizeof(yCgiCallList[0])); i++)
		if (func_req == yCgiCallList[i].func_name)
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
	else if(std::string(yCgiCallList[index].mime_type).empty())	// decide in function
		;
	else if(std::string(yCgiCallList[index].mime_type) == "+xml")		// Parameter xml?
		if (hh->getOutType() == xml)
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
	hh->outStart();

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
			else if(!hh->ParamList["get"].empty())
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
		else {
			if (hh->getOutType() == plain)
				SendTimersPlain(hh);
			else
				SendTimers(hh);
		}
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
			if(CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby){
				neutrino_msg_data_t mode = NeutrinoMessages::mode_radio;
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::CHANGEMODE, CEventServer::INITID_HTTPD, (void *)&mode,sizeof(neutrino_msg_data_t));
				sleep(1);
				NeutrinoAPI->UpdateBouquets();
			}else{
				extern CRemoteControl * g_RemoteControl;
				g_RemoteControl->radioMode();
			}
		}
		else if (hh->ParamList["1"] == "tv")	// switch to tv mode
		{
			if(CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby){
				neutrino_msg_data_t mode = NeutrinoMessages::mode_tv;
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::CHANGEMODE, CEventServer::INITID_HTTPD, (void *)&mode,sizeof(neutrino_msg_data_t));
				sleep(1);
				NeutrinoAPI->UpdateBouquets();
			}else{
				extern CRemoteControl * g_RemoteControl;
				g_RemoteControl->tvMode();
			}
		}
		else if (hh->ParamList["record"] == "start")	// start record mode
		{
#if 0
			if(hh->ParamList["stopplayback"] == "true")
				NeutrinoAPI->Zapit->stopPlayBack();
			NeutrinoAPI->Sectionsd->setPauseScanning(true);
			NeutrinoAPI->Zapit->setRecordMode(true);
#endif
			CTimerd::RecordingInfo recinfo;
			recinfo.eventID = 0;
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::RECORD_START, CEventServer::INITID_HTTPD, (void *)&recinfo, sizeof(CTimerd::RecordingInfo));
		}
		else if (hh->ParamList["record"] == "stop")	// stop record mode
		{
#if 0
			NeutrinoAPI->Zapit->setRecordMode(false);
			NeutrinoAPI->Sectionsd->setPauseScanning(false);
			if (!NeutrinoAPI->Zapit->isPlayBackActive())
				NeutrinoAPI->Zapit->startPlayBack();
#endif
			CTimerd::RecordingInfo recinfo;
			recinfo.eventID = 0; // FIXME must present
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::RECORD_STOP, CEventServer::INITID_HTTPD, (void *)&recinfo, sizeof(CTimerd::RecordingInfo));
		}
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::GetModeCGI(CyhookHandler *hh)
{
	hh->outStart();
	std::string result = "";
	std::string key = "mode";

	if (hh->ParamList_exist("channelsmode") && hh->ParamList["channelsmode"] != "false")
	{
		key = "channelsmode";
		int mode = NeutrinoAPI->Zapit->getMode();
		if (mode == CZapitClient::MODE_TV)
			result = "tv";
		else if (mode == CZapitClient::MODE_RADIO)
			result = "radio";
		else
			result = "unknown";
	}
	else
	{
		int mode = CNeutrinoApp::getInstance()->getMode();
		if (mode == NeutrinoMessages::mode_tv)
			result = "tv";
		else if (mode == NeutrinoMessages::mode_radio)
			result = "radio";
		else if (mode == NeutrinoMessages::mode_scart)
			result = "scart";
		else if (mode == NeutrinoMessages::mode_standby)
			result = "standby";
		else if (mode == NeutrinoMessages::mode_audio)
			result = "audio";
		else if (mode == NeutrinoMessages::mode_pic)
			result = "pic";
		else if (mode == NeutrinoMessages::mode_ts)
			result = "ts";
		else if (mode == NeutrinoMessages::mode_webtv)
			result = "webtv";
		else if (mode == NeutrinoMessages::mode_upnp)
			result = "upnp";
		else
			result = "unknown";
	}

	if (!result.empty())
	{
		if (hh->getOutType() != plain)
		{
			result = hh->outPair(key, result, false);
			result = hh->outObject("getmode", result);
		}
		hh->SendResult(result);
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::ExecCGI(CyhookHandler *hh)
{
	std::string script, result;
	if (!hh->ParamList.empty() )
	{
		script = hh->ParamList["1"];
		unsigned int len = hh->ParamList.size();
		for(unsigned int y=2; y<=len; y++)
			if(!hh->ParamList[itoa(y)].empty())
			{
				script += " ";
				script += hh->ParamList[itoa(y)];
			}
		result = yExecuteScript(script);
	}
	else
	{
		log_level_printf(0, "[%s] no script given\n", __func__);
		result = "error";
	}

	if (result == "error")
		hh->SetError(HTTP_NOT_FOUND);
	else
		hh->WriteLn(result);
}

//-----------------------------------------------------------------------------
void CControlAPI::SystemCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		//FIXME: No system information until now
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::StandbyCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		bool CEC_HDMI_off = false;
		if (!(hh->ParamList["cec"].empty())){
			if(hh->ParamList["cec"]=="off"){
				CEC_HDMI_off = true;
			}
		}

		if (hh->ParamList["1"] == "on")	// standby mode on
		{
			//dont use CEC with standbyoff (TV off) --- use: control/standby?off&cec=off
			if(g_settings.hdmi_cec_standby && CEC_HDMI_off){
				videoDecoder->SetCECAutoStandby(0);
			}

			if(CNeutrinoApp::getInstance()->getMode() != 4)
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::STANDBY_ON, CEventServer::INITID_HTTPD);
			hh->SendOk();

			if(g_settings.hdmi_cec_standby && CEC_HDMI_off){//dont use CEC with standbyoff (TV off)
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::EVT_HDMI_CEC_STANDBY, CEventServer::INITID_HTTPD);
			}
		}
		else if (hh->ParamList["1"] == "off")// standby mode off
		{
			//dont use CEC with with view on (TV on) --- use: control/standby?off&cec=off
			if(g_settings.hdmi_cec_view_on && CEC_HDMI_off){
				videoDecoder->SetCECAutoView(0);
			}

			NeutrinoAPI->Zapit->setStandby(false);
			if(CNeutrinoApp::getInstance()->getMode() == 4)
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::STANDBY_OFF, CEventServer::INITID_HTTPD);
			hh->SendOk();

			if(g_settings.hdmi_cec_view_on && CEC_HDMI_off){//dont use CEC with view on (TV on)
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::EVT_HDMI_CEC_VIEW_ON, CEventServer::INITID_HTTPD);
			}
		}
		else
			hh->SendError();

	}
	else
		if(CNeutrinoApp::getInstance()->getMode() == 4)//mode_standby = 4
			hh->WriteLn("on");
		else
			hh->WriteLn("off");
}

//-----------------------------------------------------------------------------
void CControlAPI::RCCGI(CyhookHandler *hh)
{
	bool locked = CRCLock::getInstance()->isLocked();

	if (hh->ParamList.empty() || hh->ParamList["1"] == "status")
	{
		if (locked)
			hh->WriteLn("off");
		else
			hh->WriteLn("on");
	}
	else
	{
		if (hh->ParamList["1"] == "lock")
		{
			if (!locked)
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::LOCK_RC, CEventServer::INITID_HTTPD);
		}
		else if (hh->ParamList["1"] == "unlock")
		{
			if (locked)
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::UNLOCK_RC, CEventServer::INITID_HTTPD);
		}
		else
		{
			hh->SendError();
			return;
		}
		hh->SendOk();
	}
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
		struct timeb tm;
		ftime(&tm);
		strftime(timestr, 20, "%d.%m.%Y\n", localtime(&tm.time) );
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
// send services.xml
void CControlAPI::GetServicesxmlCGI(CyhookHandler *hh)
{
	hh->SendFile(CONFIGDIR "/zapit/services.xml");
}

//-----------------------------------------------------------------------------
// send bouquets.xml
void CControlAPI::GetBouquetsxmlCGI(CyhookHandler *hh)
{
	hh->SendFile(CONFIGDIR "/zapit/bouquets.xml");
}
//-----------------------------------------------------------------------------

// send ubouquets.xml
void CControlAPI::GetUBouquetsxmlCGI(CyhookHandler *hh)
{
	hh->SendFile(CONFIGDIR "/zapit/ubouquets.xml");
}

//-------------------------------------------------------------------------
/** Display channel id's
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getonidsid
 * /control/getchannelid[?format=plain|json|xml]
 * @endcode
 *
 * @par output
 * @code
 * /control/getonidsid
 * @endcode
 *
 * @code
 * 3f300012b66
 * @endcode
 *
 * @par output (xml)
 * @code
 * /control/getchannelid?format=xml
 * @endcode
 *
 * @code
 * <id>
 *     <id>361d03f300012b66</id>
 *     <short_id>3f300012b66</short_id>
 * </id>
 * @endcode
 */
//-----------------------------------------------------------------------------
// get actual channel_id
void CControlAPI::GetChannelIDCGI(CyhookHandler *hh)
{
	t_channel_id channel_id = CZapit::getInstance()->GetCurrentChannelID();
	if (func_req == "getonidsid") //what a terrible name!
	{
		hh->WriteLn(string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id & 0xFFFFFFFFFFFFULL));
		return;
	}

	hh->outStart();
	std::string result = "";
	result = hh->outPair("id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id), true);
	result += hh->outPair("short_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id & 0xFFFFFFFFFFFFULL), false);
	result = hh->outObject("id", result);
	hh->SendResult(result);
}

//-------------------------------------------------------------------------
/** Display epg id's
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getepgid[?format=plain|json|xml]
 * @endcode
 *
 * @par output (xml)
 * @code
 * /control/getepgid?format=xml
 * @endcode
 *
 * @code
 * <epg_id>
 *     <epg_id>361d03f300012b66</epg_id>
 *     <short_epg_id>3f300012b66</short_epg_id>
 * </id>
 * @endcode
 */
//-----------------------------------------------------------------------------
// get actual epg_id
void CControlAPI::GetEpgIDCGI(CyhookHandler *hh)
{
	t_channel_id channel_id = CZapit::getInstance()->GetCurrentChannelID();
	t_channel_id epg_id = channel_id;
	CZapitChannel * ch = CServiceManager::getInstance()->FindChannel(channel_id);
	if (ch)
		epg_id = ch->getEpgID();

	hh->outStart();
	std::string result = "";
	result = hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
	result += hh->outPair("short_epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id & 0xFFFFFFFFFFFFULL), false);
	result = hh->outObject("epg_id", result);
	hh->SendResult(result);
}

//-----------------------------------------------------------------------------
void CControlAPI::GetTPChannel_IDCGI(CyhookHandler *hh)
{
	SendChannelList(hh, true);
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

	if (!(hh->ParamList["timeout"].empty()))
	{
		message += "&timeout=";
		message += hh->ParamList["timeout"];
	}

	if (event != 0)
	{
		//message=decodeString(message);
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
		hh->Write(PACKAGE_NAME " " PACKAGE_VERSION "\n");
	else
	{
		if (hh->ParamList["1"] == "streaminfo")		// print streaminfo
			SendStreamInfo(hh);
		else if (hh->ParamList["1"] == "version")	// send version file
			hh->SendFile(TARGET_PREFIX "/.version");
		else if (hh->ParamList["1"] == "httpdversion")	// print httpd version typ (just for compatibility)
			hh->Write("3");
		else if (hh->ParamList["1"] == "nhttpd_version")// print nhttpd version
			hh->printf("%s\n", HTTPD_VERSION);
		else if (hh->ParamList["1"] == "hwinfo")	// print hwinfo
			HWInfoCGI(hh);
		else
			hh->SendError();
	}
}

void CControlAPI::HWInfoCGI(CyhookHandler *hh)
{
	static CNetAdapter netadapter; 
	std::string eth_id = netadapter.getMacAddr();
	std::transform(eth_id.begin(), eth_id.end(), eth_id.begin(), ::tolower);

	std::string boxvendor(g_info.hw_caps->boxvendor);
	/*
	   I don't know the current legal situation.
	   So better let's change the vendor's name to CST.
	*/
	if (boxvendor.compare("Coolstream") == 0)
		boxvendor = "CST";

	hh->printf("%s %s (%s)\nMAC:%s\n", boxvendor.c_str(), g_info.hw_caps->boxname, g_info.hw_caps->boxarch, eth_id.c_str());
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
	if (hh->ParamList.empty())
	{
		NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::REBOOT, CEventServer::INITID_HTTPD);
		hh->SendOk();
	}
	else
		hh->SendError();
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
// The code here is based on rcsim. Thx Carjay!
void CControlAPI::RCEmCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty()) {
		hh->SendError();
		return;
	}
	std::string _keyname = hh->ParamList["1"];
	int sendcode = -1;
	for (unsigned int i = 0; sendcode == -1 && i < sizeof(keyname)
			/ sizeof(key); i++) {
		if (!strcmp(_keyname.c_str(), keyname[i].name))
			sendcode = keyname[i].code;
	}

	if (sendcode == -1) {
		printf("[nhttpd] Key %s not found\n", _keyname.c_str());
		hh->SendError();
		return;
	}
#if 0
	unsigned int repeat = 1;
	unsigned int delay = 250;
	if (!hh->ParamList["delay"].empty())
		delay = atoi(hh->ParamList["delay"].c_str());
	if (!hh->ParamList["duration"].empty())
		repeat = atoi(hh->ParamList["duration"].c_str()) * 1000 / delay;
	if (!hh->ParamList["repeat"].empty())
		repeat = atoi(hh->ParamList["repeat"].c_str());
#endif
#if 0
	int evd = open(EVENTDEV, O_RDWR);
	if (evd < 0) {
		perror("opening " EVENTDEV " failed");
		hh->SendError();
		return;
	}
	if (rc_send(evd, sendcode, KEY_PRESSED) < 0) {
		perror("writing 'KEY_PRESSED' event failed");
		hh->SendError();
		close(evd);
		return;
	}
	if (rc_send(evd, sendcode, KEY_RELEASED) < 0) {
		perror("writing 'KEY_RELEASED' event failed");
		hh->SendError();
		close(evd);
		return;
	}
	close(evd);
#endif
	/* 0 == KEY_PRESSED in rcinput.cpp */
	g_RCInput->postMsg((neutrino_msg_t) sendcode, 0);
	hh->SendOk();
}
//-----------------------------------------------------------------------------
void CControlAPI::AspectRatioCGI(CyhookHandler *hh)
{
	hh->printf("%s",(NeutrinoAPI->getVideoAspectRatioAsString()).c_str());
}
//-----------------------------------------------------------------------------
void CControlAPI::VideoFormatCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty() || hh->ParamList["1"] == "status") {
		hh->printf("%s",(NeutrinoAPI->getVideoAspectRatioAsString()).c_str());
		return;
	}
	if (NeutrinoAPI->setVideoAspectRatioAsString(hh->ParamList["1"]) != -1)
		hh->SendOk();
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::VideoOutputCGI(CyhookHandler *hh)
{
// FIXME: not implemented
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::VCROutputCGI(CyhookHandler *hh)
{
// FIXME: not implemented
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::ScartModeCGI(CyhookHandler *hh)
{
// FIXME: not implemented
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::AudioCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty() || hh->ParamList["1"] == "info") {
		hh->printf("%s",(NeutrinoAPI->getAudioInfoAsString()).c_str());
		return;
	}
	//TODO: more
}

//-------------------------------------------------------------------------
void CControlAPI::VolumeCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty()) {//without param: show actual volumen
		unsigned int volume;
		NeutrinoAPI->Zapit->getVolume(&volume, &volume);
		hh->printf("%d", volume);
	}
	else if (hh->ParamList["1"].compare("mute") == 0)
	{
		char mute = 1;
		NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::EVT_SET_MUTE, CEventServer::INITID_HTTPD, (void *)&mute, sizeof(char));
		hh->SendOk();
	}
	else if (hh->ParamList["1"].compare("unmute") == 0)
	{
		char mute = 0;
		NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::EVT_SET_MUTE, CEventServer::INITID_HTTPD, (void *)&mute, sizeof(char));
		hh->SendOk();
	}
	else if (hh->ParamList["1"].compare("status") == 0) { // Mute status
		(NeutrinoAPI->Zapit->getMuteStatus()) ? hh->Write("1") :  hh->Write("0");
	}
	else if(!hh->ParamList["1"].empty()) { //set volume
		char vol = atol( hh->ParamList["1"].c_str() );
		NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::EVT_SET_VOLUME, CEventServer::INITID_HTTPD, (void *)&vol, sizeof(char));
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::CryptCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty() || hh->ParamList["1"] == "info") {
		hh->printf("%s",(NeutrinoAPI->getCryptInfoAsString()).c_str());
		return;
	}
	//TODO: more
}

//-----------------------------------------------------------------------------
void CControlAPI::ChannellistCGI(CyhookHandler *hh)
{
	SendChannelList(hh);
}

void CControlAPI::LogolistCGI(CyhookHandler *hh)
{
	hh->outStart();

	std::string result = "";
	bool isFirstLine = true;

	bool files = false;
	unsigned int s = hh->ParamList.size();
	for (unsigned int i = 1; i <= s; i++)
	{
		files = (hh->ParamList[itoa(i)] == "files" && hh->ParamList["files"] != "false");
		if (files)
			break;
	}

	int mode = NeutrinoAPI->Zapit->getMode();
	CBouquetManager::ChannelIterator cit = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin();
	for (; !(cit.EndOfChannels()); cit++)
	{
		std::string item = "";
		std::string id = "";
		std::string logo = "";

		std::vector<t_channel_id> v;
		CZapitChannel * channel = *cit;
		size_t pos = std::find(v.begin(), v.end(), channel->getChannelID()) - v.begin();
		if (pos < v.size())
			continue;
		v.push_back(channel->getChannelID());

		std::string logo_used = "";
		std::string logo_real = "";
		if (files)
		{
			char _real[PATH_MAX + 1] = {0};
			if (g_PicViewer->GetLogoName(channel->getChannelID(), NeutrinoAPI->GetServiceName(channel->getChannelID()), logo_used, NULL, NULL))
			{
				realpath(logo_used.c_str(), _real);
				logo_real = string(_real);
				if (strcmp(logo_used.c_str(), logo_real.c_str()) == 0)
					logo_real.clear();
			}
		}

		if (hh->outType == plain)
		{
			std::string outLine = string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS";%s;" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS"", channel->getChannelID(), channel->getName().c_str(), (channel->getChannelID() & 0xFFFFFFFFFFFFULL));
			if (files)
			{
				if (!logo_used.empty())
					outLine += string_printf(";%s", logo_used.c_str());
				if (!logo_real.empty())
					outLine += string_printf(";%s", logo_real.c_str());
			}
			item = hh->outSingle(outLine);
		}
		else
		{
			item = hh->outPair("name", hh->outValue(channel->getName()), true);

			id = hh->outPair("id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID()), true);
			id += hh->outPair("short_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID() & 0xFFFFFFFFFFFFULL), false);
			item += hh->outObject("id", id, files);

			if (files)
			{
				logo = hh->outPair("used", logo_used, true);
				logo += hh->outPair("real", logo_real, false);
				item += hh->outObject("logo", logo);
			}
		}
		if (isFirstLine)
			isFirstLine = false;
		else
			result += hh->outNext();
		result += hh->outArrayItem("channel", item, false);
	}
	result = hh->outArray("logolist", result);

	hh->SendResult(result);
}
//-----------------------------------------------------------------------------
// get actual and next event data for given channel
//-----------------------------------------------------------------------------
std::string CControlAPI::_GetBouquetActualEPGItem(CyhookHandler *hh, CZapitChannel * channel)
{
	std::string result, firstEPG, secondEPG = "";
	t_channel_id current_channel = CZapit::getInstance()->GetCurrentChannelID();
	std::string timestr;
	CShortEPGData epg;

	CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
	CChannelEvent event;
	NeutrinoAPI->GetChannelEvent(channel->getChannelID(), event);

	bool return_epginfo = (hh->ParamList["epginfo"] != "false");

	result += hh->outPair("isActiveChannel", (channel->getChannelID() == current_channel) ? "true" : "false", false);

	if (event.eventID) {
		int percentage = 100;
		if (event.duration > 0)
			percentage = 100 * (time(NULL) - event.startTime) / event.duration;
		CEitManager::getInstance()->getCurrentNextServiceKey(channel->getChannelID(), currentNextInfo);
		timestr = timeString(event.startTime);

		firstEPG += hh->outPair("eventid", string_printf("%llu", currentNextInfo.current_uniqueKey), true);
		firstEPG += hh->outPair("description", hh->outValue(event.description), true);
		if (return_epginfo && CEitManager::getInstance()->getEPGidShort(currentNextInfo.current_uniqueKey, &epg))
		{
			firstEPG += hh->outPair("info1", hh->outValue(epg.info1), true);
			firstEPG += hh->outPair("info2", hh->outValue(epg.info2), true);
		}
		firstEPG += hh->outPair("startTime", timestr, true);
		firstEPG += hh->outPair("timeTotal", string_printf("%d", event.duration / 60), true);
		firstEPG += hh->outPair("timeElapsed", string_printf("%d", (time(NULL) - event.startTime) / 60), true);
		firstEPG += hh->outPair("percentage", string_printf("%d", percentage), false);

		if (currentNextInfo.flags & CSectionsdClient::epgflags::has_next) {
			timestr = timeString(currentNextInfo.next_zeit.startzeit);
			secondEPG += hh->outPair("eventid", string_printf("%llu", currentNextInfo.next_uniqueKey), true);
			secondEPG += hh->outPair("description", hh->outValue(currentNextInfo.next_name), true);
			if (return_epginfo && CEitManager::getInstance()->getEPGidShort(currentNextInfo.next_uniqueKey, &epg))
			{
				secondEPG += hh->outPair("info1", hh->outValue(epg.info1), true);
				secondEPG += hh->outPair("info2", hh->outValue(epg.info2), true);
			}
			secondEPG += hh->outPair("startTime", timestr, true);
			secondEPG += hh->outPair("timeTotal", string_printf("%d", currentNextInfo.next_zeit.dauer / 60), false);
		}
	}

	if(!firstEPG.empty()) {
		result += hh->outNext();
		result += hh->outObject("firstEPG", firstEPG);
	}
	if(!secondEPG.empty()) {
		result += hh->outNext();
		result += hh->outObject("secondEPG", secondEPG);
	}
	return result;
}

//-----------------------------------------------------------------------------
// produce data (collection) for given channel
std::string CControlAPI::_GetBouquetWriteItem(CyhookHandler *hh, CZapitChannel * channel, int bouquetNr, int channelNr)
{
	std::string result = "";
	bool isEPGdetails = !(hh->ParamList["epg"].empty());
	if (hh->outType == json || hh->outType == xml) {
		if (channelNr > -1)
			result += hh->outPair("number", string_printf("%u", channelNr), true);
		result += hh->outPair("id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID()), true);
		result += hh->outPair("short_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID() & 0xFFFFFFFFFFFFULL), true);
		result += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getEpgID()), true);
		result += hh->outPair("short_epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getEpgID() & 0xFFFFFFFFFFFFULL), true);
		result += hh->outPair("name", hh->outValue(channel->getName()), true);
		result += hh->outPair("logo", hh->outValue(NeutrinoAPI->getLogoFile(channel->getChannelID())), false);
		if (bouquetNr > -1)
		{
			result += hh->outNext();
			result += hh->outPair("bouquetnr", string_printf("%d", bouquetNr), false);
		}
		if (isEPGdetails)
		{
			result += hh->outNext();
			result += _GetBouquetActualEPGItem(hh, channel);
		}
		result = hh->outArrayItem("channel", result, false);
	}
	else {
		CChannelEvent event;
		NeutrinoAPI->GetChannelEvent(channel->getChannelID(), event);

		if (event.eventID && isEPGdetails) {
			result += string_printf("%u "
					PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					" %s (%s)\n",
					channelNr,
					channel->getChannelID(),
					channel->getName().c_str(), event.description.c_str());
		} else {
			result += string_printf("%u "
					PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					" %s\n",
					channelNr,
					channel->getChannelID(),
					channel->getName().c_str());
		}
	}
	return result;
}
//-------------------------------------------------------------------------
/** List all channels for given bouquet (or all) or show actual bouquet number
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * Get bouquet list (all) oder filtered to a given bouquet number
 * Option epg=true for actual and next epg data for each channel
 * @code
 * /control/getbouquet?[bouquet=<bouquet number>][&mode=TV|RADIO][&epg=true[&epginfo=false]]
 * @endcode
 * Get the actual used bouquet number
 * @code
 * /control/getbouquet?actual
 * @endcode
 *
 * @par output (json)
 * @code
 * /control/getbouquet?bouquet=2&format=json
 * @endcode
 * @code
 * {"success": "true",
 * "data": {
 *     "channels": [
 *         {
 *             "number": "1",
 *             "id": "12ea043100016e3d",
 *             "short_id": "43100016e3d",
 *             "epg_id": "12ea043100016e3d",
 *             "short_epg_id": "43100016e3d",
 *             "name": "ARD-TEST-1",
 *             "logo": "",
 *             "bouquetnr": "1"
 *         },
 * [...snip...]
 *         {
 *             "number": "1376",
 *             "id": "ffffffffa8aae742",
 *             "short_id": "ffffa8aae742",
 *             "epg_id": "6e5d040f00012887",
 *             "short_epg_id": "40f00012887",
 *             "name": "Tagesschau24 (WebTV)",
 *             "logo": "/share/tuxbox/neutrino/icons/logo/ffffa8aae742.png",
 *             "bouquetnr": "1"
 *         }
 *     ]
 * }
 * }
 * @endcode
 *
 * @par output (plain) output only (channel number, channel id, channel name)
 * @code
 * /control/getbouquet?bouquet=2
 * @endcode
 * @code
 * 1 12ea043100016e3d ARD-TEST-1
 * [...snip...]
 * 1376 ffffffffa8aae742 Tagesschau24 (WebTV)
 * @endcode
 *
 * @par output (xml)
 * @code
 * /control/getbouquet?bouquet=2&format=xml
 * @endcode
 * @code
 * <channels>
 * <channel>
 * 	<number>1</number>
 * 	<id>12ea043100016e3d</id>
 * 	<short_id>43100016e3d</short_id>
 * 	<epg_id>12ea043100016e3d</epg_id>
 * 	<short_epg_id>43100016e3d</short_epg_id>
 * 	<name><![CDATA[ARD-TEST-1]]></name>
 * 	<logo><![CDATA[]]></logo>
 * 	<bouquet>1</bouquet>
 * </channel>
 * [...snip...]
 * <channel>
 * 	<number>1376</number>
 * 	<id>ffffffffa8aae742</id>
 * 	<short_id>ffffa8aae742</short_id>
 * 	<epg_id>6e5d040f00012887</epg_id>
 * 	<short_epg_id>40f00012887</short_epg_id>
 * 	<name><![CDATA[Tagesschau24 (WebTV)]]></name>
 * 	<logo><![CDATA[/share/tuxbox/neutrino/icons/logo/ffffa8aae742.png]]></logo>
 * 	<bouquet>1</bouquet>
 * </channel>
 * </channels>
 * @endcode
 */
//-------------------------------------------------------------------------
void CControlAPI::GetBouquetCGI(CyhookHandler *hh)
{
	TOutType outType = hh->outStart(true /*old mode*/);

	std::string result = "";
	if (!(hh->ParamList.empty())) {
		int mode = NeutrinoAPI->Zapit->getMode();

		if (hh->ParamList["mode"].compare("TV") == 0)
			mode = CZapitClient::MODE_TV;
		else if (hh->ParamList["mode"].compare("RADIO") == 0)
			mode = CZapitClient::MODE_RADIO;
		else if (hh->ParamList["mode"].compare("all") == 0)
			mode = CZapitClient::MODE_ALL;

		// Get Bouquet Number. First matching current channel
		if (hh->ParamList["1"] == "actual") {
			int actual = 0;
			for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++) {
				if (g_bouquetManager->existsChannelInBouquet(i, CZapit::getInstance()->GetCurrentChannelID())) {
					actual = i + 1;
					break;
				}
			}
			hh->printf("%d", actual);
		}
		else {
			int BouquetNr = -1; // -1 = all bouquets
			int startBouquet = 0;
			int bsize = (int) g_bouquetManager->Bouquets.size();
			if (!hh->ParamList["bouquet"].empty()) {
				// list for given bouquet
				BouquetNr = atoi(hh->ParamList["bouquet"].c_str());
				if (BouquetNr > 0)
					BouquetNr--;
				if((BouquetNr > 0) && (BouquetNr >= bsize))
					BouquetNr = bsize-1;

				startBouquet = BouquetNr;
				bsize = BouquetNr+1;
			}
			if (!(hh->ParamList["epg"].empty()))
				NeutrinoAPI->GetChannelEvents();
			const char *json_delimiter = "";
			if (mode == CZapitClient::MODE_RADIO || mode == CZapitClient::MODE_ALL)
				for (int i = startBouquet; i < bsize; i++) {
					ZapitChannelList channels = g_bouquetManager->Bouquets[i]->radioChannels;
					int num = 1 + g_bouquetManager->radioChannelsBegin().getNrofFirstChannelofBouquet(i);
					int size = (int) channels.size();
					for (int j = 0; j < size; j++) {
						CZapitChannel * channel = channels[j];
						result += json_delimiter;
						json_delimiter = (outType == json) ? ",\n" : "";
						result += _GetBouquetWriteItem(hh, channel, i, num + j);
					}
				}
			if (mode == CZapitClient::MODE_TV || mode == CZapitClient::MODE_ALL)
				for (int i = startBouquet; i < bsize; i++) {
					ZapitChannelList channels = g_bouquetManager->Bouquets[i]->tvChannels;
					int num = 1 + g_bouquetManager->tvChannelsBegin().getNrofFirstChannelofBouquet(i);
					int size = (int) channels.size();
					for (int j = 0; j < size; j++) {
						CZapitChannel * channel = channels[j];
						result += json_delimiter;
						json_delimiter = (outType == json) ? ",\n" : "";
						result += _GetBouquetWriteItem(hh, channel, i, num + j);
					}
				}
			result = hh->outArray("channels", result);

			hh->SendResult(result);
		}
	}
	else
		hh->SendError("no parameter");
}

//-------------------------------------------------------------------------
/** Show some infos about current or given (by full ID!) channel
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getchannel[?format=plain|xml|json][&id=<channel_id>][&epg=true[&epginfo=false]]
 * @endcode
 *
 * @par output (json)
 * @code
 * /control/getchannel?format=json
 * @endcode
 *
 * @code
 * {
 * "success": "true",
 * "data": {
 *     "channel": [
 *         {
 *             "id": "361d03f300012b66",
 *             "short_id": "3f300012b66",
 *             "epg_id": "361d03f300012b66",
 *             "short_epg_id": "3f300012b66",
 *             "name": "ZDF HD",
 *             "logo": "/share/tuxbox/neutrino/icons/logo/3f300012b66.png"
 *         }
 *     ]
 * }
 * }
 * @endcode
 */
//-------------------------------------------------------------------------
// get actual channel info
void CControlAPI::GetChannelCGI(CyhookHandler *hh)
{
	hh->outStart();

	std::string result = "";

	t_channel_id channel_id = 0;
	if (hh->ParamList["id"].empty())
		channel_id = CZapit::getInstance()->GetCurrentChannelID();
	else
		sscanf(hh->ParamList["id"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);

	if (channel_id != 0)
	{
		NeutrinoAPI->GetChannelEvents();
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
		if (channel)
		{
			result = _GetBouquetWriteItem(hh, channel, -1, -1);
			result = hh->outArray("channel", result);
			hh->SendResult(result);
		}
		else
			hh->SendError(hh->ParamList["id"] + " seems wrong");
	}
	else
		hh->SendError();
}

//-------------------------------------------------------------------------
/** Return all bouquets
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getbouquets?[showhidden=true|false][&encode=true|false][&format=|xml|json]
 *
 * @endcode
 * @par
 * @e showhidden = true (default) | false -> show hidden bouquets
 * @n @e encode = true | false (default) use URLencode
 * @n @e Result: bouquet number, bouquet name *
 *
 * @par output (json)
 * @code
 * /control/getbouquets&format=json
 * @endcode
 * @code
 * {"success": "true", "data":{"bouquets": [{"number": "2",
 * "name": "ARD"
 * },
 * {"number": "12",
 * "name": "Digital Free"
 * },
 * [...snip...]
 * {"number": "23",
 * "name": "Other"
 * }
 * ]
 * }}
 * @endcode
 *
 * @par output (plain)
 * @code
 * /control/getbouquets
 * @endcode
 * @code
 * 2 ARD
 * 12 Digital Free
 * [...snip...]
 * 22 ZDFvision
 * 23 Other
 * @endcode
 *
 * @par output (xml)
 * @code
 * /control/getbouquets&format=xml
 * @endcode
 * @code
 * <bouquets>
 * <bouquet>
 * <number>2</number>
 * <name>ARD</name>
 * </bouquet>
 * [...snip...]
 * <number>23</number>
 * <name>Other</name>
 * </bouquet>
 * </bouquets>
 * @endcode
 */
//-------------------------------------------------------------------------
void CControlAPI::GetBouquetsCGI(CyhookHandler *hh)
{
	bool show_hidden = true;
	bool encode = false;
	std::string result = "";

	TOutType outType = hh->outStart();

	if (hh->ParamList["showhidden"] == "false")
		show_hidden = false;

	if (hh->ParamList["encode"] == "true")
		encode = true;

	bool fav = false;
	if (hh->ParamList["fav"] == "true")
		fav = true;

	int mode = NeutrinoAPI->Zapit->getMode();
	if (hh->ParamList["mode"].compare("all") == 0)
		mode = CZapitClient::MODE_ALL;
	else if (hh->ParamList["mode"].compare("TV") == 0)
		mode = CZapitClient::MODE_TV;
	else if (hh->ParamList["mode"].compare("RADIO") == 0)
		mode = CZapitClient::MODE_RADIO;
 
	std::string bouquet;
	for (int i = 0, size = (int) g_bouquetManager->Bouquets.size(); i < size; i++) {
		std::string item = "";
		unsigned int channel_count = 0;
		switch (mode) {
			case CZapitClient::MODE_RADIO:
				channel_count = g_bouquetManager->Bouquets[i]->radioChannels.size();
				break;
			case CZapitClient::MODE_TV:
				channel_count = g_bouquetManager->Bouquets[i]->tvChannels.size();
				break;
			case CZapitClient::MODE_ALL:
				channel_count = g_bouquetManager->Bouquets[i]->radioChannels.size() + g_bouquetManager->Bouquets[i]->tvChannels.size();
		}
		if (channel_count && (!g_bouquetManager->Bouquets[i]->bHidden || show_hidden) && (!fav || g_bouquetManager->Bouquets[i]->bUser)) {
			bouquet = std::string(g_bouquetManager->Bouquets[i]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : g_bouquetManager->Bouquets[i]->Name.c_str());
			if (encode)
				bouquet = encodeString(bouquet); // encode (URLencode) the bouquetname
			if (outType == plain)
				item = string_printf("%u", i + 1) + " " + bouquet + "\n";
			else
			{
				item = hh->outPair("number", string_printf("%u", i + 1), true);
				item += hh->outPair("name", bouquet, false);
			}
			result += hh->outArrayItem("bouquet", item, (i < size-1));
		}
	}
	result = hh->outArray("bouquets", result);

	hh->SendResult(result);
}
//-----------------------------------------------------------------------------
//	details EPG Information for channelid
//-----------------------------------------------------------------------------
std::string CControlAPI::channelEPGformated(CyhookHandler *hh, int bouquetnr, t_channel_id channel_id, int max, long stoptime)
{
	std::string result = "";
	std::string channelData = "";

	t_channel_id epg_id = channel_id;
	CZapitChannel * ch = CServiceManager::getInstance()->FindChannel(channel_id);
	if (ch)
		epg_id = ch->getEpgID();

	channelData += hh->outPair("channel_name", hh->outValue(NeutrinoAPI->GetServiceName(channel_id)), true);
	channelData += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id), true);
	channelData += hh->outPair("channel_short_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id & 0xFFFFFFFFFFFFULL), true);
	channelData += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
	channelData += hh->outPair("short_epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id & 0xFFFFFFFFFFFFULL), (bouquetnr > -1));
	if (bouquetnr > -1)
		channelData += hh->outPair("bouquetnr", string_printf("%d", bouquetnr), false);
	if (hh->outType == json)
		channelData = hh->outObject("channelData", channelData);
	int i = 0;
	CChannelEventList::iterator eventIterator;
	bool isFirstLine = true;

	CChannelEventList eList;
	CEitManager::getInstance()->getEventsServiceKey(epg_id, eList);

	for (eventIterator = eList.begin(); eventIterator != eList.end(); ++eventIterator, i++) {
		if ((max != -1 && i >= max) || (stoptime != -1 && eventIterator->startTime >= stoptime))
			break;
		std::string prog = "";
		if (hh->outType == plain)
			prog += hh->outSingle("");

		prog += hh->outPair("bouquetnr", string_printf("%d", bouquetnr), true);
		prog += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id), true);
		prog += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
		prog += hh->outPair("eventid", string_printf("%llu", eventIterator->eventID), true);
		prog += hh->outPair("eventid_hex", string_printf("%llx", eventIterator->eventID), true);
		prog += hh->outPair("start_sec", string_printf("%ld", eventIterator->startTime), true);
		char zbuffer[25] = { 0 };
		struct tm *mtime = localtime(&eventIterator->startTime);
		strftime(zbuffer, 20, "%H:%M", mtime);
		prog += hh->outPair("start_t", std::string(zbuffer), true);
		memset(zbuffer, 0, sizeof(zbuffer));
		strftime(zbuffer, 20, "%d.%m.%Y", mtime);
		prog += hh->outPair("date", std::string(zbuffer), true);
		prog += hh->outPair("stop_sec", string_printf("%ld", eventIterator->startTime + eventIterator->duration), true);
		long _stoptime = eventIterator->startTime + eventIterator->duration;
		mtime = localtime(&_stoptime);
		strftime(zbuffer, 20, "%H:%M", mtime);
		prog += hh->outPair("stop_t", std::string(zbuffer), true);
		prog += hh->outPair("duration_min", string_printf("%d", (int) (eventIterator->duration / 60)), true);

		if (!(hh->ParamList["details"].empty())) {
			CShortEPGData epg;
			if (CEitManager::getInstance()->getEPGidShort(eventIterator->eventID, &epg)) {
				prog += hh->outPair("info1", hh->outValue(epg.info1), true);
				prog += hh->outPair("info2", hh->outValue(epg.info2), true);
			}
		}
		prog += hh->outPair("description", hh->outValue(eventIterator->description), false);

		if(isFirstLine)
			isFirstLine = false;
		else
			result += hh->outNext();
		result += hh->outArrayItem("prog", prog, false);
	}
	if(hh->outType == json)
		result = hh->outArray("progData", result);
	result = channelData + hh->outNext() + result;
	return result;
}

//-----------------------------------------------------------------------------
// Detailed EPG list in XML or JSON
//-----------------------------------------------------------------------------
void CControlAPI::epgDetailList(CyhookHandler *hh)
{
	// ------ get parameters -------
	// max = maximal output items
	int max = -1;
	if (!(hh->ParamList["max"].empty()))
		max = atoi(hh->ParamList["max"].c_str());

	// stoptime = maximal output items until starttime >= stoptime
	long stoptime = -1;
	if (!(hh->ParamList["stoptime"].empty()))
		stoptime = atol(hh->ParamList["stoptime"].c_str());

	// determine channelid
	t_channel_id channel_id = (t_channel_id) -1;
	if (!(hh->ParamList["channelid"].empty())) {
		sscanf(hh->ParamList["channelid"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);
	}
	else if (!(hh->ParamList["channelname"].empty())) {
		channel_id = NeutrinoAPI->ChannelNameToChannelId(hh->ParamList["channelname"].c_str());
	}

	// or determine bouquetnr -> iterate the bouquet
	int bouquetnr = -1;
	bool all_bouquets = false;

	if (hh->ParamList["bouquetnr"] == "all")
		all_bouquets = true;
	else if (!(hh->ParamList["bouquetnr"].empty())) {
		bouquetnr = atoi(hh->ParamList["bouquetnr"].c_str());
		bouquetnr--;
	}

	// fallback
	if ((channel_id == (t_channel_id) -1) && (bouquetnr == -1))
	{
		channel_id = CZapit::getInstance()->GetCurrentChannelID();
		//all_bouquets = true; //better, but broken
	}

	// ------ generate output ------
	hh->outStart(true /*old mode*/);
	std::string result = "";

	if (bouquetnr >= 0 || all_bouquets) {
		int bouquet_size = (int) g_bouquetManager->Bouquets.size();
		int start_bouquet = 0;

		if(bouquetnr >= 0 && !all_bouquets) {
			start_bouquet = bouquetnr;
			bouquet_size = bouquetnr+1;
		}
		// list all bouquets			if(encode)
		ZapitChannelList channels;
		int mode = NeutrinoAPI->Zapit->getMode();

		for (int i = start_bouquet; i < bouquet_size; i++) {
			channels = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->Bouquets[i]->radioChannels : g_bouquetManager->Bouquets[i]->tvChannels;
			std::string bouquet = std::string(g_bouquetManager->Bouquets[i]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : g_bouquetManager->Bouquets[i]->Name.c_str());
			bouquet = encodeString(bouquet); // encode (URLencode) the bouquetname
			std::string res_channels = "";

			for (int j = 0, csize = (int) channels.size(); j < csize; j++) {
				CZapitChannel * channel = channels[j];
				res_channels += hh->outArrayItem("channel", channelEPGformated(hh, j + 1, channel->getChannelID(), max, stoptime), j<csize-1);
			}
			if(all_bouquets) {
				res_channels = hh->outPair("number", string_printf("%d", i + 1), true) +
						hh->outPair("name", hh->outValue(bouquet), true) +
						res_channels;
				result += hh->outArrayItem("bouquet", res_channels, i < bouquet_size-1);
			}
			else
				result += res_channels;
		}
	}
	else
		// list one channel, no bouquetnr given
		result = channelEPGformated(hh, 0, channel_id, max, stoptime);

	result = hh->outObject("epglist", result);

	hh->SendResult(result);
}
//-------------------------------------------------------------------------------------------------
inline static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}
extern const char * GetGenre(const unsigned char contentClassification); // UTF-8

void CControlAPI::EpgSearchXMLCGI(CyhookHandler *hh) 
{
	SendFoundEvents(hh, true);
}

//-------------------------------------------------------------------------
/** Return EPG search data
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/epgsearch?<keywords>
 * or
 * /control/epgsearch?search=<keywords>[&epginfo=true|false|search][&format=plain|xml|json]
 * @endcode
 */

//-------------------------------------------------------------------------
void CControlAPI::EpgSearchCGI(CyhookHandler *hh)
{
	SendFoundEvents(hh);
}

void CControlAPI::SendFoundEvents(CyhookHandler *hh, bool xml_format)
{
	if (hh->ParamList.empty())
	{
		hh->SendError();
		return;
	}

	std::string result ="";
	std::string item = "";
	t_channel_id channel_id;
	CChannelEventList evtlist;

	bool search_epginfo = (hh->ParamList["epginfo"] != "false");
	bool return_epginfo = (hh->ParamList["epginfo"] == "true" || hh->ParamList["epginfo"].empty());

	std::string search_keyword = (hh->ParamList["search"].empty()) ? hh->ParamList["1"] : hh->ParamList["search"];
	const int search_epg_item = search_epginfo ? 5 /*SEARCH_EPG_ALL*/ : 1 /*SEARCH_EPG_TITLE*/;

	if (xml_format) // to stay backward compatible :/
		hh->ParamList["format"] = "xml";
	hh->outStart(true /*old mode*/);

	/* TODO: maybe add following options as in tuxbox neutrino
		hh->ParamList["epgitem"]
		hh->ParamList["mode"]
		hh->ParamList["channelid"]
		hh->ParamList["channelname"]
		hh->ParamList["bouquet"]
	*/

	std::vector<t_channel_id> v;
	int channel_nr = CNeutrinoApp::getInstance()->channelList->getSize(); //unique channelList TV or Radio
	for (int channel = 0; channel < channel_nr; channel++)
	{
	    channel_id =  CNeutrinoApp::getInstance()->channelList->getChannelFromIndex(channel)->getChannelID();
	    v.push_back(channel_id);
	}
	std::map<t_channel_id, t_channel_id> ch_id_map;
	std::vector<t_channel_id>::iterator it;
	for (it = v.begin(); it != v.end(); ++it)
	{
		ch_id_map[*it & 0xFFFFFFFFFFFFULL] = *it;
	}

	CEitManager::getInstance()->getEventsServiceKey(0, evtlist, search_epg_item, search_keyword, true);

	if (!evtlist.empty())
	{
		std::map<t_channel_id, t_channel_id>::iterator map_it;
		CChannelEventList::iterator e;
		for (e = evtlist.begin(); e != evtlist.end(); ++e)
		{
			map_it = ch_id_map.find(e->channelID);
			if (map_it != ch_id_map.end())
			{
				e->channelID = map_it->second;//map channelID48 to channelID
			}
			else
			{
				evtlist.erase(e--);// remove event for not found channels in channelList
			}
		}
	}
	if (!evtlist.empty())
	{
		sort(evtlist.begin(), evtlist.end(), sortByDateTime);
	}

	time_t azeit=time(NULL);
	CShortEPGData epg;
	CEPGData longepg;
	char tmpstr[256] ={0};
	std::string genre;
	CChannelEventList::iterator eventIterator;
	unsigned int u_azeit = ( azeit > -1)? azeit:0;
	for (eventIterator = evtlist.begin(); eventIterator != evtlist.end(); ++eventIterator)
	{
		bool got_next = (eventIterator != (evtlist.end() - 1));
		if (CEitManager::getInstance()->getEPGidShort(eventIterator->eventID, &epg))
		{
			if( (eventIterator->startTime+eventIterator->duration) < u_azeit)
				continue;

			t_channel_id chan_id = eventIterator->channelID;
			t_channel_id epg_id = chan_id;
			CZapitChannel * ch = CServiceManager::getInstance()->FindChannel(chan_id);
			if (ch)
				epg_id = ch->getEpgID();

			struct tm *tmStartZeit = localtime(&eventIterator->startTime);
			item.clear();
			if (hh->outType == json || hh->outType == xml)
			{
				item += hh->outPair("channelname", NeutrinoAPI->GetServiceName(chan_id), true);
				item += hh->outPair("epgtitle", hh->outValue(epg.title), true);
				if (return_epginfo) {
					item += hh->outPair("info1", hh->outValue(epg.info1), true);
					item += hh->outPair("info2", hh->outValue(epg.info2), true);
				}
				if (CEitManager::getInstance()->getEPGid(eventIterator->eventID, eventIterator->startTime, &longepg))
					{
					item += hh->outPair("fsk", string_printf("%u", longepg.fsk), true);
					genre = "";
#ifdef FULL_CONTENT_CLASSIFICATION
					if (!longepg.contentClassification.empty())
						genre = GetGenre(longepg.contentClassification[0]);
#else
					if (longepg.contentClassification)
						genre = GetGenre(longepg.contentClassification);
#endif
					item += hh->outPair("genre", ZapitTools::UTF8_to_UTF8XML(genre.c_str()), true);
				}
				strftime(tmpstr, sizeof(tmpstr), "%Y-%m-%d", tmStartZeit );
				item += hh->outPair("date", tmpstr, true);
				strftime(tmpstr, sizeof(tmpstr), "%H:%M", tmStartZeit );
				item += hh->outPair("time", tmpstr, true);
				item += hh->outPair("duration", string_printf("%d", eventIterator->duration / 60), true);
				item += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, chan_id), true);
				item += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
				item += hh->outPair("eventid", string_printf("%llu", eventIterator->eventID), false);

				result += hh->outArrayItem("item", item, got_next);
			}
			else // outType == plain
			{
				std::string datetimer_str ;
				strftime(tmpstr, sizeof(tmpstr), "%Y-%m-%d %H:%M", tmStartZeit );
				datetimer_str = tmpstr;
				datetimer_str += " ";
				datetimer_str += g_Locale->getText(CLocaleManager::getMonth(tmStartZeit));
				datetimer_str += " ";
				datetimer_str += g_Locale->getText(CLocaleManager::getWeekday(tmStartZeit));
				snprintf(tmpstr, sizeof(tmpstr)," [%d min]",eventIterator->duration / 60);
				datetimer_str += tmpstr;

				result += hh->outSingle(datetimer_str);
				result += hh->outSingle(NeutrinoAPI->GetServiceName(chan_id));
				result += hh->outSingle(epg.title);
				if (return_epginfo) {
					if(!epg.info1.empty())
						result += hh->outSingle(epg.info1);
					if(!epg.info2.empty())
						result += hh->outSingle(epg.info2);
				}
				if (CEitManager::getInstance()->getEPGid(eventIterator->eventID, eventIterator->startTime, &longepg)) {
					result += hh->outSingle(string_printf("fsk:%u", longepg.fsk));
					genre = "";
#ifdef FULL_CONTENT_CLASSIFICATION
					if (!longepg.contentClassification.empty())
						genre = GetGenre(longepg.contentClassification[0]);
#else
					if (longepg.contentClassification)
						genre = GetGenre(longepg.contentClassification);
#endif
					if(!genre.empty())
						result += hh->outSingle(ZapitTools::UTF8_to_UTF8XML(genre.c_str()));
				}
				result += hh->outSingle("----------------------------------------------------------");
			}
		}
	}
	result = hh->outArray("epgsearch", result);

	hh->SendResult(result);
}

//-------------------------------------------------------------------------
/** Return EPG data
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/epg
 * /control/epg?<channelid64> 64Bit, hex
 * /control/epg?id=<channelid>
 * /control/epg?search=<keywords>
 * /control/epg?eventid=<eventid>
 * /control/epg?ext
 * /control/epg?xml=true&channelid=<channelid>|channelname=<channel name>[&details=true][&max=<max items>][&stoptime=<long:stop time>]
 *  	details=true : Show EPG Info1 and info2
 *		stoptime : show only items until stoptime reached
 * @endcode
 */

//-------------------------------------------------------------------------
void CControlAPI::EpgCGI(CyhookHandler *hh)
{
	bool param_empty = hh->ParamList.empty();
	hh->SetHeader(HTTP_OK, "text/plain; charset=UTF-8"); // default
	// Detailed EPG list in XML or JSON
	if (hh->getOutType() == xml || hh->getOutType() == json || !hh->ParamList["detaillist"].empty()) {
		epgDetailList(hh);
	}
	// Standard list normal or extended
	else if (param_empty || hh->ParamList["1"] == "ext") {
		hh->SetHeader(HTTP_OK, "text/plain; charset=UTF-8");
		bool isExt = (hh->ParamList["1"] == "ext");
		CChannelEvent event;
		NeutrinoAPI->GetChannelEvents();

		int mode = NeutrinoAPI->Zapit->getMode();
		CBouquetManager::ChannelIterator cit = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin();
		for (; !(cit.EndOfChannels()); cit++) {
			CZapitChannel * channel = *cit;
			NeutrinoAPI->GetChannelEvent(channel->getChannelID(), event);
			if (event.eventID) {
				if (!isExt) {
					hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					" %llu %s\n", channel->getChannelID(), event.eventID, event.description.c_str());
				}
				else { // ext output
					hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					" %ld %u %llu %s\n", channel->getChannelID(), event.startTime, event.duration, event.eventID, event.description.c_str());
				}
			}
		}
	}
	else if (!hh->ParamList["search"].empty())
	{
		SendFoundEvents(hh, (hh->getOutType() == xml));
	}
	// query details for given eventid
	else if (!hh->ParamList["eventid"].empty()) {
		//special epg query
		uint64_t epgid = 0;
		sscanf(hh->ParamList["eventid"].c_str(), "%" SCNu64 "", &epgid);
		CShortEPGData epg;
		if (CEitManager::getInstance()->getEPGidShort(epgid, &epg)) {
			hh->WriteLn(epg.title);
			hh->WriteLn(epg.info1);
			hh->WriteLn(epg.info2);
		}
	}
	else if (!hh->ParamList["eventid2fsk"].empty()) {
		if (!hh->ParamList["starttime"].empty()) {
			uint64_t epgid = 0;
			time_t starttime = 0;
			sscanf(hh->ParamList["fskid"].c_str(), "%" SCNu64 "", &epgid);
			sscanf(hh->ParamList["starttime"].c_str(), "%lu", &starttime);
			CEPGData longepg;
			if (CEitManager::getInstance()->getEPGid(epgid, starttime, &longepg)) {
				hh->printf("%u\n", longepg.fsk);
				return;
			}
		}
		hh->SendError();
	}
	// list EPG for channel id
	else if (!(hh->ParamList["id"].empty())) {
		t_channel_id channel_id = 0;
		sscanf(hh->ParamList["id"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);
		CChannelEventList eList;
		CEitManager::getInstance()->getEventsServiceKey(channel_id, eList);
		CChannelEventList::iterator eventIterator;
		for (eventIterator = eList.begin(); eventIterator != eList.end(); ++eventIterator) {
			CShortEPGData epg;
			if (CEitManager::getInstance()->getEPGidShort(eventIterator->eventID, &epg)) {
				hh->printf("%llu %ld %d\n", eventIterator->eventID, eventIterator->startTime, eventIterator->duration);
				hh->printf("%s\n", epg.title.c_str());
				hh->printf("%s\n", epg.info1.c_str());
				hh->printf("%s\n\n", epg.info2.c_str());
			}
		}
	}
	// list EPG for channelID 64Bit
	else {
		//eventlist for a chan
		t_channel_id channel_id = 0;
		sscanf(hh->ParamList["1"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);
		SendEventList(hh, channel_id);
	}
}

//-----------------------------------------------------------------------------
void CControlAPI::VersionCGI(CyhookHandler *hh)
{
	hh->SendFile(TARGET_PREFIX "/.version");
}
//-----------------------------------------------------------------------------
void CControlAPI::ReloadNeutrinoSetupCGI(CyhookHandler *hh)
{
	NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::RELOAD_SETUP, CEventServer::INITID_HTTPD);
	hh->SendOk();
}

void CControlAPI::ReloadPluginsCGI(CyhookHandler *hh)
{
	g_Plugins->loadPlugins();
	hh->SendOk();
}

void CControlAPI::ReloadChannelsCGI(CyhookHandler *hh)
{
	CServiceManager::getInstance()->SaveServices(true, true);
	NeutrinoAPI->Zapit->reinitChannels();
	CNeutrinoApp::getInstance()->SDTreloadChannels = false;
	hh->SendOk();
}

#ifdef SCREENSHOT
void CControlAPI::ScreenshotCGI(CyhookHandler *hh)
{
	bool enableOSD = true;
	bool enableVideo = true;
	std::string filename = "screenshot";

	if(hh->ParamList["osd"] == "0")
		enableOSD = false;
	if(hh->ParamList["video"] == "0")
		enableVideo = false;
	if(!hh->ParamList["name"].empty())
		filename = hh->ParamList["name"];

	CScreenShot * sc = new CScreenShot("/tmp/" + filename + ".png", (CScreenShot::screenshot_format_t)0 /*PNG*/);
	sc->EnableOSD(enableOSD);
	sc->EnableVideo(enableVideo);
#if 0
	sc->Start();
	hh->SendOk(); // FIXME what if sc->Start() failed?
#else
	if (sc->StartSync())
		hh->SendOk();
	else
		hh->SendError();
#endif
}
#endif

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
			if(NeutrinoAPI->Zapit->isPlayBackActive()){
				NeutrinoAPI->Zapit->stopPlayBack();
				NeutrinoAPI->Sectionsd->setPauseScanning(true);
			}
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "startplayback")
		{
			if(!NeutrinoAPI->Zapit->isPlayBackActive()){
				NeutrinoAPI->Zapit->startPlayBack();
				NeutrinoAPI->Sectionsd->setPauseScanning(false);
				dprintf("start playback requested..\n");
			}
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
			hh->Write((char *) (NeutrinoAPI->Sectionsd->getIsScanningActive() ? "1" : "0"));
		else if (hh->ParamList["1"] == "getallsubchannels")
		{
			t_channel_id current_channel = NeutrinoAPI->Zapit->getCurrentServiceID();
			CSectionsdClient::LinkageDescriptorList desc;
			CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
			CEitManager::getInstance()->getCurrentNextServiceKey(current_channel, currentNextInfo);

			if (currentNextInfo.flags & CSectionsdClient::epgflags::current_has_linkagedescriptors &&
			    CEitManager::getInstance()->getLinkageDescriptorsUniqueKey(currentNextInfo.current_uniqueKey, desc))
			{
				CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(current_channel);
				t_satellite_position satellitePosition = channel->getSatellitePosition();
				for(unsigned int i=0; i< desc.size(); i++)
				{
					t_channel_id sub_channel_id =
						      ((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000+ abs(satellitePosition))) << 48) |
						      (uint64_t) CREATE_CHANNEL_ID(desc[i].serviceId, desc[i].originalNetworkId, desc[i].transportStreamId);
					hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
						   " %s\n",
						   sub_channel_id,
						   (desc[i].name).c_str());
				}
			}
		}
		else if (!hh->ParamList["subchannel"].empty())
		{
			extern CRemoteControl * g_RemoteControl;
			if (!g_RemoteControl->subChannels.empty())
			{
				NeutrinoAPI->ZapToSubService(hh->ParamList["subchannel"].c_str());
				hh->SendOk();
			}
			else
				hh->SendError();
		}
		else if (!hh->ParamList["name"].empty())
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
		if (!hh->ParamList["name"].empty())
		{
			pluginname = hh->ParamList["name"];
			//pluginname=decodeString(pluginname);
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
	hh->SendOk();
}

//-------------------------------------------------------------------------
// Send functions (for ExecuteCGI)
//-------------------------------------------------------------------------
void CControlAPI::SendEventList(CyhookHandler *hh, t_channel_id channel_id)
{
	int pos = 0;
	CChannelEventList eList;
	CEitManager::getInstance()->getEventsServiceKey(channel_id, eList);
	CChannelEventList::iterator eventIterator;

	for (eventIterator = eList.begin(); eventIterator != eList.end(); ++eventIterator, pos++)
		hh->printf("%llu %ld %d %s\n", eventIterator->eventID, eventIterator->startTime, eventIterator->duration, eventIterator->description.c_str());
}

//-----------------------------------------------------------------------------
void CControlAPI::SendChannelList(CyhookHandler *hh, bool currentTP)
{
	t_channel_id current_channel = 0;
	std::vector<t_channel_id> v;

	if(currentTP){
		current_channel = CZapit::getInstance()->GetCurrentChannelID();
		current_channel=(current_channel>>16);
	}

	int mode = NeutrinoAPI->Zapit->getMode();
	hh->SetHeader(HTTP_OK, "text/plain; charset=UTF-8");
	CBouquetManager::ChannelIterator cit = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin();
	for (; !(cit.EndOfChannels()); cit++) {
		CZapitChannel * channel = *cit;
		if(!currentTP || (channel->getChannelID() >>16) == current_channel){

			size_t pos = std::find(v.begin(), v.end(), channel->getChannelID()) - v.begin();
			if( pos < v.size() )
				continue;
			v.push_back(channel->getChannelID());

			hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " %s\n", channel->getChannelID(), channel->getName().c_str());
		}
	}
}

//-----------------------------------------------------------------------------
void CControlAPI::SignalInfoCGI(CyhookHandler *hh)
{
	CFrontend *frontend = CFEManager::getInstance()->getLiveFE();
	if(frontend){
		bool parame_empty = false;

		if (hh->ParamList["1"].empty())
			parame_empty = true;

		if ( parame_empty || (hh->ParamList["1"] == "sig") ){
			unsigned int sig = frontend->getSignalStrength() & 0xFFFF;
			sig = (sig & 0xFFFF) * 100 / 65535;
			if (parame_empty)
				hh->printf("SIG: ");
			hh->printf("%3u\n", sig);
		}
		if ( parame_empty || (hh->ParamList["1"] == "snr") ){
			unsigned int snr = frontend->getSignalNoiseRatio() & 0xFFFF;
			snr = (snr & 0xFFFF) * 100 / 65535;
			if (parame_empty)
				hh->printf("SNR: ");
			hh->printf("%3u\n", snr);
		}
		if ( parame_empty || (hh->ParamList["1"] == "ber") ){
			unsigned int ber = frontend->getBitErrorRate();
			if (parame_empty)
				hh->printf("BER: ");
			hh->printf("%3u\n", ber);
		}
	}else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::SendStreamInfo(CyhookHandler *hh)
{
	hh->WriteLn(NeutrinoAPI->getVideoResolutionAsString());
	hh->WriteLn(NeutrinoAPI->getVideoAspectRatioAsString());
	hh->WriteLn(NeutrinoAPI->getVideoFramerateAsString());
	hh->WriteLn(NeutrinoAPI->getAudioInfoAsString());
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
	CEitManager::getInstance()->getCurrentNextServiceKey(current_channel, currentNextInfo);
	if (CEitManager::getInstance()->getComponentTagsUniqueKey(currentNextInfo.current_uniqueKey,tags))
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
							std::string tmp_desc = _getISO639Description( pids.APIDs[j].desc);
							strncpy(pids.APIDs[j].desc, tmp_desc.c_str(), DESC_MAX_LEN -1);
						}
						hh->printf("%05u %s %s\n",pids.APIDs[j].pid,pids.APIDs[j].desc,pids.APIDs[j].is_ac3 ? " (AC3)": pids.APIDs[j].desc,pids.APIDs[j].is_aac ? "(AAC)" : pids.APIDs[j].desc,pids.APIDs[j].is_eac3 ? "(EAC3)" : " ");
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
		for (CZapitClient::APIDList::iterator it = pids.APIDs.begin(); it!=pids.APIDs.end(); ++it)
		{
			if(!(init_iso))
			{
				strncpy( pids.APIDs[i].desc, _getISO639Description( pids.APIDs[i].desc ),DESC_MAX_LEN );
			}
			hh->printf("%05u %s %s\n",it->pid,pids.APIDs[i].desc,pids.APIDs[i].is_ac3 ? " (AC3)": pids.APIDs[i].desc,pids.APIDs[i].is_aac ? "(AAC)" : pids.APIDs[i].desc,pids.APIDs[i].is_eac3 ? "(EAC3)" : " ");
			i++;
		}
	}

	if(pids.APIDs.empty())
		hh->printf("0\n"); // shouldnt happen, but print at least one apid
	if(pids.PIDs.vtxtpid)
		hh->printf("%05u vtxt\n",pids.PIDs.vtxtpid);
	if (pids.PIDs.pmtpid)
		hh->printf("%05u pmt\n",pids.PIDs.pmtpid);
	if (pids.PIDs.pcrpid)
		hh->printf("%05u pcr\n",pids.PIDs.pcrpid);
}
//-----------------------------------------------------------------------------
void CControlAPI::SendTimersPlain(CyhookHandler *hh)
{
	CTimerd::TimerList timerlist;			// List of bouquets
	bool send_id = false;

	if (hh->ParamList["format"] == "id")
		send_id = true;

	timerlist.clear();
	NeutrinoAPI->Timerd->getTimerList(timerlist);

	CTimerd::TimerList::iterator timer = timerlist.begin();

	for(; timer != timerlist.end(); ++timer)
	{
		// Add Data
		char zAddData[22+1] = { 0 };
		if (send_id)
		{
			zAddData[0] = '0';
			zAddData[1] = 0;
		}

		switch(timer->eventType) {
		//case CTimerd::TIMER_NEXTPROGRAM:
		case CTimerd::TIMER_ZAPTO:
		case CTimerd::TIMER_RECORD:
			if (!send_id)
			{
				strncpy(zAddData, NeutrinoAPI->GetServiceName(timer->channel_id).c_str(), 22);
				if (zAddData[0] == 0)
					strcpy(zAddData, CServiceManager::getInstance()->IsChannelTVChannel(timer->channel_id) ?
							"Unknown TV-Channel" : "Unknown Radio-Channel");
			}
			else
				snprintf(zAddData,sizeof(zAddData), PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer->channel_id);

			zAddData[22]=0;

			break;

		case CTimerd::TIMER_STANDBY:
			if (!send_id)
				snprintf(zAddData,sizeof(zAddData),"Standby: %s",(timer->standby_on ? "ON" : "OFF"));
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
std::string CControlAPI::_SendTime(CyhookHandler *hh, struct tm *Time, int digits) {
	char zTime[25] = {0};
	char zDate[25] = {0};
	strftime(zTime,20,"%H:%M",Time);
	strftime(zDate,20,"%d.%m.%Y",Time);
	std::string result = "";

	result += hh->outPair("text", string_printf("%s %s", zDate, zTime), true);
	result += hh->outPair("date", string_printf("%s", zDate), true);
	result += hh->outPair("time", string_printf("%s", zTime), true);
	result += hh->outPair("digits", string_printf("%d", digits), true);
	result += hh->outPair("day", string_printf("%d", Time->tm_mday), true);
	result += hh->outPair("month", string_printf("%d", Time->tm_mon+1), true);
	result += hh->outPair("year", string_printf("%d", Time->tm_year+1900), true);
	result += hh->outPair("hour", string_printf("%d", Time->tm_hour), true);
	result += hh->outPair("min", string_printf("%d", Time->tm_min), false);

	return result;
}
//-----------------------------------------------------------------------------
// build json/xml for all timer data (needed for yWeb 3)
//-----------------------------------------------------------------------------
void CControlAPI::SendTimers(CyhookHandler *hh)
{
	// Init local timer iterator
	CTimerd::TimerList timerlist;			// List of timers
	timerlist.clear();
	NeutrinoAPI->Timerd->getTimerList(timerlist);
	sort(timerlist.begin(), timerlist.end());		// sort timer
	CTimerd::TimerList::iterator timer = timerlist.begin();
	std::string result = "";
	std::string config = "";
	std::string timer_list = "";

	// Look for Recording Safety Timers too
	int pre=0, post=0;
	NeutrinoAPI->Timerd->getRecordingSafety(pre,post);
	config += hh->outPair("pre_delay", string_printf("%d", pre), true);
	config += hh->outPair("post_delay", string_printf("%d", post), false);

	result += hh->outObject("config", config, true);

	for(int i = 0; timer != timerlist.end(); ++timer)
	{
		if (i > 0)
			timer_list += hh->outNext();

		std::string timer_item = "";

		timer_item += hh->outPair("type", NeutrinoAPI->timerEventType2Str(timer->eventType), true);
		timer_item += hh->outPair("id", string_printf("%d", timer->eventID), true);
		timer_item += hh->outPair("state", string_printf("%d", (int)timer->eventState), true);
		timer_item += hh->outPair("type_number", string_printf("%d", (int)timer->eventType), true);

		// alarmtime
		std::string alarm = "";

		struct tm *alarmTime = localtime(&(timer->alarmTime));
		alarm += hh->outArrayItem("normal", _SendTime(hh, alarmTime, (int)timer->alarmTime), true);

		time_t real_alarmTimeT = timer->alarmTime - pre;
		struct tm *safetyAlarmTime = localtime(&real_alarmTimeT);
		alarm += hh->outArrayItem("safety", _SendTime(hh, safetyAlarmTime, (int)real_alarmTimeT), false);

		timer_item += hh->outArray("alarm", alarm, true);

		// announcetime
		std::string announce = "";

		struct tm *announceTime = localtime(&(timer->announceTime));
		announce += hh->outArrayItem("normal", _SendTime(hh, announceTime, (int)timer->announceTime), true);

		time_t real_announceTimeT = timer->announceTime - pre;
		struct tm *safetyAnnounceTime = localtime(&real_announceTimeT);
		announce += hh->outArrayItem("safety", _SendTime(hh, safetyAnnounceTime, (int)real_announceTimeT), false);

		timer_item += hh->outArray("announce", announce, true);

		// stoptime
		if(timer->stopTime > 0) {
			std::string stop = "";

			struct tm *stopTime = localtime(&(timer->stopTime));
			stop += hh->outArrayItem("normal", _SendTime(hh, stopTime, (int)timer->stopTime), true);

			time_t real_stopTimeT = timer->stopTime - post;
			struct tm *safetyStopTime = localtime(&real_stopTimeT);
			stop += hh->outArrayItem("safety", _SendTime(hh, safetyStopTime, (int)real_stopTimeT), false);

			timer_item += hh->outArray("stop", stop, true);
		}

		// repeat
		std::string repeat = "";

		std::string zRep = NeutrinoAPI->timerEventRepeat2Str(timer->eventRepeat);
		std::string zRepCount;
		if (timer->eventRepeat == CTimerd::TIMERREPEAT_ONCE)
			zRepCount = "-";
		else
			zRepCount = (timer->repeatCount == 0) ? "&#x221E;" : string_printf("%dx",timer->repeatCount);
		std::string weekdays;
		NeutrinoAPI->Timerd->setWeekdaysToStr(timer->eventRepeat, weekdays);

		repeat += hh->outPair("count", zRepCount, true);
		repeat += hh->outPair("number", string_printf("%d", (int)timer->eventRepeat), true);
		repeat += hh->outPair("text", zRep, true);
		repeat += hh->outPair("weekdays", weekdays, false);

		timer_item += hh->outObject("repeat", repeat, false);

		// channel infos
		std::string channel_name = NeutrinoAPI->GetServiceName(timer->channel_id);
		if (channel_name.empty())
			channel_name = CServiceManager::getInstance()->IsChannelTVChannel(timer->channel_id) ? "Unknown TV-Channel" : "Unknown Radio-Channel";

		// epg title
		std::string title = timer->epgTitle;
		if(timer->epgID!=0) {
			CEPGData epgdata;
			if (CEitManager::getInstance()->getEPGid(timer->epgID, timer->epg_starttime, &epgdata))
				title = epgdata.title;
		}

		// timer specific data
		switch(timer->eventType)
		{
#if 0
		case CTimerd::TIMER_NEXTPROGRAM : {
			timer_item += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer->channel_id), true);
			timer_item += hh->outPair("channel_name", channel_name, true);
			timer_item += hh->outPair("title", title, false);
		}
		break;
#endif

		case CTimerd::TIMER_ZAPTO : {
			timer_item += hh->outNext();
			timer_item += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer->channel_id), true);
			timer_item += hh->outPair("channel_name", channel_name, true);
			timer_item += hh->outPair("title", title, false);
		}
		break;

		case CTimerd::TIMER_RECORD : {
			timer_item += hh->outNext();
			timer_item += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer->channel_id), true);
			timer_item += hh->outPair("channel_name", channel_name, true);
			timer_item += hh->outPair("title", title, true);

			// audio
			std::string audio = "";
			std::string apids_conf = "true";
			std::string apids_std = "false";
			std::string apids_alt = "false";
			std::string apids_ac3 = "false";

			if (timer->apids != TIMERD_APIDS_CONF)
			{
				apids_conf = "false";
				if (timer->apids & TIMERD_APIDS_STD)
					apids_std = "true";
				if (timer->apids & TIMERD_APIDS_ALT)
					apids_alt = "true";
				if (timer->apids & TIMERD_APIDS_AC3)
					apids_ac3 = "true";
			}

			audio += hh->outPair("apids_conf", apids_conf, true);
			audio += hh->outPair("apids_std", apids_std, true);
			audio += hh->outPair("apids_alt", apids_alt, true);
			audio += hh->outPair("apids_ac3", apids_ac3, false);

			timer_item += hh->outObject("audio", audio, true);

			timer_item += hh->outPair("recording_dir", timer->recordingDir, true);
			timer_item += hh->outPair("epg_id", string_printf("%d", (int)timer->epgID), false);
		}
		break;

		case CTimerd::TIMER_STANDBY : {
			timer_item += hh->outNext();
			timer_item += hh->outPair("status", (timer->standby_on) ? "on" : "off", false);
		}
		break;

		case CTimerd::TIMER_REMIND : {
			std::string _message;
			_message = std::string(timer->message).substr(0,20);
			timer_item += hh->outNext();
			timer_item += hh->outPair("message", _message, false);
		}
		break;

		case CTimerd::TIMER_EXEC_PLUGIN : {
			timer_item += hh->outNext();
			timer_item += hh->outPair("plugin", timer->pluginName, false);
		}
		break;

		case CTimerd::TIMER_SLEEPTIMER : {
		}
		break;

		case CTimerd::TIMER_IMMEDIATE_RECORD : {
		}
		break;

		default:
		{}
		}
		timer_list += hh->outArrayItem("timer", timer_item, false);
		i++;
	}
	result += hh->outArray("timer_list", timer_list);
	if (hh->getOutType() == json)
		result = hh->outArrayItem("timer", result, false);
	result = hh->outArray("timer", result);

	hh->SendResult(result);
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
	if (!hh->ParamList["video_stream_pids"].empty())
	{
		int para=0;
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
	if(!hh->ParamList["no_commas"].empty())
	{
		hh->printf("0x%04x 0x%04x 0x%04x",pids.PIDs.pmtpid,pids.PIDs.vpid,apid);
		if (pids.PIDs.pcrpid != pids.PIDs.vpid)
			hh->printf(" 0x%04x", pids.PIDs.pcrpid);
	}
	else
	{
		hh->printf("0x%04x,0x%04x,0x%04x",pids.PIDs.pmtpid,pids.PIDs.vpid,apid);
		if (pids.PIDs.pcrpid != pids.PIDs.vpid)
			hh->printf(",0x%04x", pids.PIDs.pcrpid);
	}
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
	if(!hh->ParamList["alarm"].empty())
	{
		alarmTimeT = atoi(hh->ParamList["alarm"].c_str());
		if(!hh->ParamList["stop"].empty())
			stopTimeT = atoi(hh->ParamList["stop"].c_str());
		if(!hh->ParamList["announce"].empty())
			announceTimeT = atoi(hh->ParamList["announce"].c_str());
		else
			announceTimeT = alarmTimeT;
	}
	else if(!hh->ParamList["alDate"].empty()) //given formatted
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
		if(!hh->ParamList["alTime"].empty())
			sscanf(hh->ParamList["alTime"].c_str(),"%2d.%2d",&(alarmTime->tm_hour), &(alarmTime->tm_min));
		alHour = alarmTime->tm_hour;
		correctTime(alarmTime);
		alarmTimeT = mktime(alarmTime);
		announceTimeT = alarmTimeT;
		struct tm *stopTime = localtime(&alarmTimeT);
		stopTime->tm_sec = 0;
		// Stop Time - Format exact! HH:MM
		if(!hh->ParamList["stTime"].empty())
			sscanf(hh->ParamList["stTime"].c_str(),"%2d.%2d",&(stopTime->tm_hour), &(stopTime->tm_min));

		// Stop Date - Format exact! DD.MM.YYYY
		if(!hh->ParamList["stDate"].empty())
			if(sscanf(hh->ParamList["stDate"].c_str(),"%2d.%2d.%4d",&(stopTime->tm_mday), &(stopTime->tm_mon), &(stopTime->tm_year)) == 3)
			{
				stopTime->tm_mon -= 1;
				stopTime->tm_year -= 1900;
			}
		correctTime(stopTime);
		stopTimeT = mktime(stopTime);
		if(hh->ParamList["stDate"].empty() && alHour > stopTime->tm_hour)
			stopTimeT += 24* 60 * 60; // add 1 Day
	}
	else	// alarm/stop time given in pieces
	{
		// alarm time
		time_t now = time(NULL);
		struct tm *alarmTime=localtime(&now);
		if(!hh->ParamList["ad"].empty())
			alarmTime->tm_mday = atoi(hh->ParamList["ad"].c_str());
		if(!hh->ParamList["amo"].empty())
			alarmTime->tm_mon = atoi(hh->ParamList["amo"].c_str())-1;
		if(!hh->ParamList["ay"].empty())
			alarmTime->tm_year = atoi(hh->ParamList["ay"].c_str())-1900;
		if(!hh->ParamList["ah"].empty())
			alarmTime->tm_hour = atoi(hh->ParamList["ah"].c_str());
		if(!hh->ParamList["ami"].empty())
			alarmTime->tm_min = atoi(hh->ParamList["ami"].c_str());
		alarmTime->tm_sec = 0;
		correctTime(alarmTime);
		alarmTimeT = mktime(alarmTime);
		announceTimeT = alarmTimeT;

		// stop time
		struct tm *stopTime = alarmTime;
		if(!hh->ParamList["sd"].empty())
			stopTime->tm_mday = atoi(hh->ParamList["sd"].c_str());
		if(!hh->ParamList["smo"].empty())
			stopTime->tm_mon = atoi(hh->ParamList["smo"].c_str())-1;
		if(!hh->ParamList["sy"].empty())
			stopTime->tm_year = atoi(hh->ParamList["sy"].c_str())-1900;
		if(!hh->ParamList["sh"].empty())
			stopTime->tm_hour = atoi(hh->ParamList["sh"].c_str());
		if(!hh->ParamList["smi"].empty())
			stopTime->tm_min = atoi(hh->ParamList["smi"].c_str());
		stopTime->tm_sec = 0;
		correctTime(stopTime);
		stopTimeT = mktime(stopTime);
	}

	if(announceTimeT != 0)
		announceTimeT -= 60;

	CTimerd::CTimerEventTypes type;
	if(!hh->ParamList["type"].empty())
		type  = (CTimerd::CTimerEventTypes) atoi(hh->ParamList["type"].c_str());
	else // default is: record
		type = CTimerd::TIMER_RECORD;

	// repeat
	if(!hh->ParamList["repcount"].empty())
	{
		repCount = atoi(hh->ParamList["repcount"].c_str());
	}
	CTimerd::CTimerEventRepeat rep;
	if(!hh->ParamList["rep"].empty())
		rep = (CTimerd::CTimerEventRepeat) atoi(hh->ParamList["rep"].c_str());
	else // default: no repeat
		rep = (CTimerd::CTimerEventRepeat)0;
	if(((int)rep) >= ((int)CTimerd::TIMERREPEAT_WEEKDAYS) && !hh->ParamList["wd"].empty())
		NeutrinoAPI->Timerd->getWeekdaysFromStr(&rep, hh->ParamList["wd"]);

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
	eventinfo.recordingSafety = (hh->ParamList["rs"] == "1") || (hh->ParamList["rs"] == "on");
	eventinfo.autoAdjustToEPG = (hh->ParamList["aj"] == "1") || (hh->ParamList["aj"] == "on");

	// channel by Id or name
	if(!hh->ParamList["channel_id"].empty())
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
	/* else if(type==CTimerd::TIMER_NEXTPROGRAM || type==CTimerd::TIMER_ZAPTO) */
	else if (type == CTimerd::TIMER_ZAPTO)
		data= &eventinfo;
	else if (type==CTimerd::TIMER_RECORD)
	{
		if(_rec_dir.empty())
		{
			// get Default Recordingdir
			CConfigFile *Config = new CConfigFile(',');
			Config->loadConfig(NEUTRINO_CONFIGFILE);
			_rec_dir = Config->getString("network_nfs_recordingdir", "/mnt/filme");
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
		if(!hh->ParamList["id"].empty())
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

			for(; timer != timerlist.end();++timer)
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
	if (!hh->ParamList["selected"].empty()) {
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
	if (!hh->ParamList["selected"].empty() && (
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
	if (!hh->ParamList["selected"].empty()) {
		int selected = atoi(hh->ParamList["selected"].c_str());
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
	if (!hh->ParamList["selected"].empty())
	{
		if (!hh->ParamList["nameto"].empty())
		{
			if (NeutrinoAPI->Zapit->existsBouquet((hh->ParamList["nameto"]).c_str()) == -1)
			{
				NeutrinoAPI->Zapit->renameBouquet(atoi(hh->ParamList["selected"].c_str()) - 1, hh->ParamList["nameto"].c_str());
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
		NeutrinoAPI->Zapit->getBouquetChannels(selected - 1, BChannelList, CZapitClient::MODE_CURRENT, true);
		CZapitClient::BouquetChannelList::iterator channels = BChannelList.begin();
		for(; channels != BChannelList.end(); ++channels)
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
		if(!hh->ParamList["redirect"].empty())
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
//-----------------------------------------------------------------------------
//	details EPG Information in xmltv format from all user bouquets
//-----------------------------------------------------------------------------
void CControlAPI::xmltvepgCGI(CyhookHandler *hh)
{
	int mode = NeutrinoAPI->Zapit->getMode();
	hh->ParamList["format"] = "xml";
	hh->outStart();

	t_channel_id channel_id;
	std::string result = "";
	std::string channelTag = "", channelData = "";
	std::string programmeTag = "", programmeData = "";

	ZapitChannelList chanlist;
	CChannelEventList eList;
	CChannelEventList::iterator eventIterator;

	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++)
	{
		if (mode == CZapitClient::MODE_RADIO)
			g_bouquetManager->Bouquets[i]->getRadioChannels(chanlist);
		else
			g_bouquetManager->Bouquets[i]->getTvChannels(chanlist);
		if(!chanlist.empty() && !g_bouquetManager->Bouquets[i]->bHidden && g_bouquetManager->Bouquets[i]->bUser)
		{
			for(int j = 0; j < (int) chanlist.size(); j++)
			{
				CZapitChannel * channel = chanlist[j];
				channel_id = channel->getChannelID() & 0xFFFFFFFFFFFFULL;
				channelTag = "channel id=\""+string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id)+"\"";
				channelData = hh->outPair("display-name", hh->outValue(channel->getName()), true);
				result += hh->outObject(channelTag, channelData);

				eList.clear();

				CEitManager::getInstance()->getEventsServiceKey(channel_id, eList);

				if (eList.size() == 0)
					continue;

				if (eList.size() > 50)
					eList.erase(eList.begin()+50,eList.end());

				for (eventIterator = eList.begin(); eventIterator != eList.end(); ++eventIterator)
				{
					if (eventIterator->get_channel_id() == channel_id)
					{
						programmeTag  = "programme ";
						programmeTag += "channel=\""+string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id)+"\" ";
						char zbuffer[25] = { 0 };
						struct tm *mtime = localtime(&eventIterator->startTime);
						strftime(zbuffer, 21, "%Y%m%d%H%M%S +0200", mtime);
						programmeTag += "start=\""+std::string(zbuffer)+"\" ";
						long _stoptime = eventIterator->startTime + eventIterator->duration;
						mtime = localtime(&_stoptime);
						strftime(zbuffer, 21, "%Y%m%d%H%M%S +0200", mtime);
						programmeTag += "stop=\""+std::string(zbuffer)+"\" ";

						programmeData  = hh->outPair("title lang=\"de\"", hh->outValue(eventIterator->description), false);
						programmeData += hh->outPair("desc lang=\"de\"", hh->outValue(eventIterator->text), true);

						result += hh->outArrayItem(programmeTag, programmeData, false);
					}
				}
			}
		}
	}


	result = hh->outObject("tv generator-info-name=\"Neutrino XMLTV Generator v1.0\"", result);

	result = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\n" + result;

	hh->SendResult(result);
}

void CControlAPI::xmltvm3uCGI(CyhookHandler *hh)
{
    hh->outStart();
    std::string result = "";

    int mode = NeutrinoAPI->Zapit->getMode();
    // build url
    std::string url = "";
    if(!hh->ParamList["host"].empty())
        url = "http://"+hh->ParamList["host"];
    else
        url = "http://"+hh->HeaderList["Host"];
    /* strip off optional custom port */
    if (url.rfind(":") != 4)
        url = url.substr(0, url.rfind(":"));

    url += ":31339/id=";

    result += "#EXTM3U\n";

    for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++)
    {
        ZapitChannelList chanlist;
        if (mode == CZapitClient::MODE_RADIO)
            g_bouquetManager->Bouquets[i]->getRadioChannels(chanlist);
        else
            g_bouquetManager->Bouquets[i]->getTvChannels(chanlist);
        if(!chanlist.empty() && !g_bouquetManager->Bouquets[i]->bHidden && g_bouquetManager->Bouquets[i]->bUser)
        {
            for(int j = 0; j < (int) chanlist.size(); j++)
            {
                CZapitChannel * channel = chanlist[j];
				std::string bouq_name = g_bouquetManager->Bouquets[i]->Name;
				std::string chan_id_short = string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID() & 0xFFFFFFFFFFFFULL);
                result += "#EXTINF:-1 tvg-id=\""+chan_id_short+"\" tvg-logo=\""+NeutrinoAPI->getLogoFile(channel->getChannelID())+"\" group-title=\""+bouq_name+"\", [COLOR gold]"+channel->getName()+"[/COLOR]\n";
                result += url+string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID())+"\n";
            }
        }
    }

    hh->SendResult(result);
}
//-------------------------------------------------------------------------
// audio_no : (optional) audio channel
// host : (optional) ip of dbox
void CControlAPI::build_live_url(CyhookHandler *hh)
{
	std::string xpids;
	int mode = NeutrinoAPI->Zapit->getMode();

	if ( mode == CZapitClient::MODE_TV)
	{
		CZapitClient::responseGetPIDs pids;
		int apid=0,apid_no=0,apid_idx=0;
		pids.PIDs.vpid=0;

		if(!hh->ParamList["audio_no"].empty())
			apid_no = atoi(hh->ParamList["audio_no"].c_str());
		NeutrinoAPI->Zapit->getPIDS(pids);

		if( apid_no < (int)pids.APIDs.size())
			apid_idx=apid_no;
		if(!pids.APIDs.empty())
			apid = pids.APIDs[apid_idx].pid;
		xpids = string_printf("0x%04x,0x%04x,0x%04x",pids.PIDs.pmtpid,pids.PIDs.vpid,apid);
		if (pids.PIDs.pcrpid != pids.PIDs.vpid)
			xpids += string_printf(",0x%04x", pids.PIDs.pcrpid);
	}
	else if ( mode == CZapitClient::MODE_RADIO)
	{
		CZapitClient::responseGetPIDs pids;
		int apid=0;

		NeutrinoAPI->Zapit->getPIDS(pids);
		if(!pids.APIDs.empty())
			apid = pids.APIDs[0].pid;

		//xpids = string_printf("0x%04x",apid);
		xpids = string_printf("0x%04x,0x%04x",pids.PIDs.pmtpid,apid);
	}
	else
		hh->SendError();
	// build url
	std::string url = "";
	if(!hh->ParamList["host"].empty())
		url = "http://"+hh->ParamList["host"];
	else
		url = "http://"+hh->HeaderList["Host"];
	/* strip off optional custom port */
	if (url.rfind(":") != 4)
		url = url.substr(0, url.rfind(":"));

	//url += (mode == CZapitClient::MODE_TV) ? ":31339/0," : ":31338/";
	url += ":31339/0,";
	url += xpids;

	// response url
	if(!hh->ParamList["vlc_link"].empty())
	{
		write_to_file("/tmp/vlc.m3u", url);
		hh->SendRedirect("/tmp/vlc.m3u");
	}
	else
	{
		hh->SetHeader(HTTP_OK, "text/html; charset=UTF-8");
		hh->Write(url);
	}
}
//-------------------------------------------------------------------------
void CControlAPI::logoCGI(CyhookHandler *hh)
{
	t_channel_id channel_id;
	sscanf(hh->ParamList["1"].c_str(),
	       SCANF_CHANNEL_ID_TYPE,
	       &channel_id);
	hh->Write(NeutrinoAPI->getLogoFile(channel_id));
}
//-------------------------------------------------------------------------
/** Get Config File or save values to given config file
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/config?config=neutrino|nhttpd|yweb[&action=submit&key1=value1&key2=value2&...][&format=|xml|json]
 * @endcode
 *
 * @par example:
 * @code
 * /control/config?config=neutrino
 * /control/config?config=neutrino&format=json
 * /control/config?config=neutrino&action=submit&epg_dir=/media/sda1/epg
 * @endcode
 *
 * @par output (json)
 * /control/config?config=neutrino&format=json
 * @code
 * {"success": "true", "data":{"analog_mode1": "16",
 * "analog_mode2": "1",
 * [...snip...]
 * "zap_cycle": "0",
 * "zapto_pre_time": "0",
 * }}
 * @endcode
 *
 * @par output (plain)
 * /control/config?config=neutrino
 * @code
 * analog_mode1=16
 * analog_mode2=1
 * [...snip...]
 * zap_cycle=0
 * zapto_pre_time=0
 * @endcode
 *
 * @par output (xml)
 * /control/config?config=neutrino&format=xml
 * @code
 * <config>
 * <analog_mode1>16</analog_mode1>
 * <analog_mode2>1</analog_mode2>
 * [...snip...]
 * <zap_cycle>0</zap_cycle>
 * <zapto_pre_time>0</zapto_pre_time>
 * </config>
 * @endcode
 */
//-------------------------------------------------------------------------
void CControlAPI::ConfigCGI(CyhookHandler *hh)
{
	bool load = true;
	CConfigFile *Config = new CConfigFile(',');
	ConfigDataMap conf;
	std::string config_filename = "";
	std::string error = "";
	std::string result = "";
	std::string configFileName = hh->ParamList["config"];

	hh->outStart();

	if (hh->ParamList["action"] == "submit")
		load = false;

	// Para "config" describes the config type
	if (configFileName == "neutrino")
		config_filename = NEUTRINO_CONFIGFILE;
	else if (configFileName == "moviebrowser")
		config_filename = MOVIEBROWSER_CONFIGFILE;
	else if (configFileName == "nhttpd")
		config_filename = HTTPD_CONFIGFILE;
	else if (configFileName == "yweb")
		config_filename = YWEB_CONFIGFILE;

	if (!config_filename.empty()) {
		Config->loadConfig(config_filename);

		if (load) {	// get and output list
			conf = Config->getConfigDataMap();
			ConfigDataMap::iterator it, end, start;
			for (start = conf.begin(), it=start, end = conf.end(); it != end; ++it) {
				std::string key = it->first;
				replace(key, ".", "_dot_");
				replace(key, "-", "_bind_");
				if (!(hh->ParamList["config"] == "nhttpd" && it->first == "mod_auth.password")) {
					if(it != start)
						result += hh->outNext();
					result += hh->outPair(key, it->second, false);
				}
			}
		}
		else { // set values and save list
			for (CStringList::iterator it = hh->ParamList.begin(); it != hh->ParamList.end(); ++it) {
				std::string key = it->first;
				replace(key, "_dot_", ".");
				replace(key, "_bind_", "-");
				if (key != "_dc" && key != "action" && key != "format" && key != "config") {
					Config->setString(key, it->second);
				}
			}
			if (!config_filename.empty())
				Config->saveConfig(config_filename);
		}
	}
	else {
		if(!configFileName.empty())
			error = string_printf("no config defined for %s", (hh->ParamList["config"]).c_str());
	}

	hh->WriteLn(hh->outObject("config", result));

	if (error.empty())
		hh->SendResult(result);
	else
		hh->SendError(error);

	delete Config;
}

//-----------------------------------------------------------------------------
/** Get a list of files with attributtes for a given path
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/file?action=list&path={path}[&format=|xml|json][&sort=false]
 * @endcode
 *
 * @par example:
 * @code
 * /control/file?action=list&path=/
 * /control/file?action=list&path=/&format=json
 * /control/file?action=list&path=/&format=json&sort=false
 * @endcode
 *
 * @par output
 * @code
 * {"success": "true", "data":{"filelist": [{"name": "timeshift",
 * "type_str": "dir",
 * "type": "4",
 * "fullname": "/timeshift",
 * "mode": "41ffld",
 * "nlink": "2",
 * "user": "root",
 * "group": "root",
 * "size": "1024",
 * "time": "Sun Sep  4 07:38:10 2011",
 * "time_t": "1315114690"
 * },
 * @endcode
 * ... snip ...
 * @code
 * {"name": "root",
 * "type_str": "dir",
 * "type": "4",
 * "fullname": "/root",
 * "mode": "41edld",
 * "nlink": "2",
 * "user": "1000",
 * "group": "1000",
 * "size": "1024",
 * "time": "Wed Aug 31 12:09:29 2011",
 * "time_t": "1314785369"
 * },
 * ]
 * }}
 * @endcode
 *
 *  @par Not implemented now:
 *  action =new_folder|delete|read_file|write_file|set_properties
 */
//-----------------------------------------------------------------------------
void CControlAPI::FileCGI(CyhookHandler *hh)
{
	std::string result = "";

	if (hh->ParamList["action"] == "list") { // directory list: action=list&path=<path>
		DIR *dirp;

		hh->outStart();

		std::string path = hh->ParamList["path"];
		if ((dirp = opendir(path.c_str()))) {
			struct dirent *entry;
			std::vector<FileCGI_List> filelist;
			while ((entry = readdir(dirp))) {
				if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
					continue;
				std::string ftype;
				if (entry->d_type == DT_DIR)
					ftype = "dir";
				else if (entry->d_type == DT_LNK)
					ftype = "lnk";
				else if (entry->d_type == 8)
					ftype = "file";
				if (path[path.length() - 1] != '/')
					path += "/";
				std::string fullname = path + entry->d_name;

				FileCGI_List listitem;
				listitem.name = std::string(entry->d_name);
				listitem.type_str = ftype;
				listitem.type = entry->d_type;
				listitem.fullname = fullname;

				filelist.push_back(listitem);
			}
			closedir(dirp);

			if (hh->ParamList["sort"] != "false")
				sort(filelist.begin(), filelist.end(), fsort);

			for(std::vector<FileCGI_List>::iterator f = filelist.begin(); f != filelist.end(); ++f)
			{
				bool got_next = (f != filelist.end()-1);

				std::string item = "";
				item += hh->outPair("name",	hh->outValue(f->name.c_str()), true);
				item += hh->outPair("type_str",	hh->outValue(f->type_str.c_str()), true);
				item += hh->outPair("type",	string_printf("%d", (int) f->type), true);
				item += hh->outPair("fullname",	hh->outValue(f->fullname.c_str()), true);

				struct stat statbuf;
				if (stat(f->fullname.c_str(), &statbuf) != -1) {
					item += hh->outPair("mode", string_printf("%xld", (long) statbuf.st_mode), true);

					/* Print out type, permissions, and number of links. */
					//TODO:	hh->printf("\t\t<permission>%10.10s</permission>\n", sperm (statbuf.st_mode));
					item += hh->outPair("nlink", string_printf("%d", statbuf.st_nlink), true);

					/* Print out owner's name if it is found using getpwuid(). */
					struct passwd *pwd;
					if ((pwd = getpwuid(statbuf.st_uid)) != NULL)
						item += hh->outPair("user", pwd->pw_name, true);
					else
						item += hh->outPair("user", string_printf("%d", statbuf.st_uid), true);

					/* Print out group name if it is found using getgrgid(). */
					struct group *grp;
					if ((grp = getgrgid(statbuf.st_gid)) != NULL)
						item += hh->outPair("group", grp->gr_name, true);
					else
						item += hh->outPair("group", string_printf("%d", statbuf.st_gid), true);

					/* Print size of file. */
					item += hh->outPair("size", string_printf("%jd", (intmax_t) statbuf.st_size), true);

					struct tm *tm = localtime(&statbuf.st_mtime);
					char datestring[256] = {0};
					/* Get localized date string. */
					strftime(datestring, sizeof(datestring), nl_langinfo(D_T_FMT), tm);
					item += hh->outPair("time", hh->outValue(datestring), true);
					item += hh->outPair("time_t", string_printf("%ld", (long) statbuf.st_mtime), false);
				}
				result += hh->outArrayItem("item", item, got_next);
			}
		}
		result = hh->outArray("filelist", result);

		hh->SendResult(result);
	}
	// create new folder
	else if (hh->ParamList["action"] == "new_folder") {
		hh->SetHeader(HTTP_OK, "text/plain; charset=UTF-8");
		//TODO
	}
	else if (hh->ParamList["action"] == "delete") {
		hh->SetHeader(HTTP_OK, "text/plain; charset=UTF-8");
		//TODO
	}
}

//-----------------------------------------------------------------------------
/** Get a list of statfs output for a given path
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/statfs[?path={path}][&format=plain|xml|json]
 * @endcode
 *
 * @par example:
 * @code
 * /control/statfs
 * /control/statfs?path=/media/sda1/movies&format=json
 * @endcode
 *
 * @par output
 * @code
 * {"success": "true", "data":
 * {
 * 	"statfs": {
 * 		"path": "/media/sda1/movies",
 * 		"f_type": "0x4d44",
 * 		"f_bsize": "4096",
 * 		"f_blocks": "488444",
 * 		"f_bfree": "365874",
 * 		"f_bavail": "365874",
 * 		"f_files": "0",
 * 		"f_ffree": "0",
 * 		"f_fsid": "0x801, 0",
 * 		"f_namelen": "1530",
 * 		"f_frsize": "24"
 * 	}
 * }}
 * @endcode
 */
//-----------------------------------------------------------------------------
void CControlAPI::StatfsCGI(CyhookHandler *hh)
{
	std::string result = "";

	if (hh->ParamList["path"].empty())
		hh->ParamList["path"] = "/";

	hh->outStart();

	std::string path = hh->ParamList["path"];
	struct statfs s;
	if (::statfs(path.c_str(), &s) == 0)
	{
		std::string item = "";
		item += hh->outPair("path", path.c_str(), true);
		item += hh->outPair("f_type", string_printf("%#lx", (unsigned long) s.f_type), true);
		item += hh->outPair("f_bsize", string_printf("%lu", (unsigned long) s.f_bsize), true);
		item += hh->outPair("f_blocks", string_printf("%lu", (unsigned long) s.f_blocks), true);
		item += hh->outPair("f_bfree", string_printf("%lu", (unsigned long) s.f_bfree), true);
		item += hh->outPair("f_bavail", string_printf("%lu", (unsigned long) s.f_bavail), true);
		item += hh->outPair("f_files", string_printf("%lu", (unsigned long) s.f_files), true);
		item += hh->outPair("f_ffree", string_printf("%lu", (unsigned long) s.f_ffree), true);
		item += hh->outPair("f_fsid", string_printf("%#x, %#x", (unsigned) s.f_fsid.__val[0], (unsigned) s.f_fsid.__val[1]), true);
		item += hh->outPair("f_namelen", string_printf("%lu", (unsigned long) s.f_namelen), true);
		item += hh->outPair("f_frsize", string_printf("%lu", (unsigned long) s.f_frsize), false);

		result = hh->outObject("statfs", item);

		hh->SendResult(result);
	}
	else
		hh->SendError("statfs failed");
}

//-----------------------------------------------------------------------------
/** Get neutrino directories
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getdir?dir=allmoviedirs&[&subdirs=true][&format=|xml|json]
 * @endcode
 *
{"success": "true", "data":{"dirs": [{"dir": "/mnt/series/",
"used": "1"
}
,{"dir": "/mnt/movies/",
"used": "1"
}
,{"dir": "/mnt/movies/subdir"
}
{"dir": "/media/sda1/movie"
}
,]
}}
 * @endcode
 *
 */
//-----------------------------------------------------------------------------
void CControlAPI::getDirCGI(CyhookHandler *hh)
{
	std::string result = "";
	std::string item = "";
	bool isFirstLine = true;

	hh->outStart(true /*old mode*/);

	//Shows all 7 directories stored in the moviebrowser.conf
	if (hh->ParamList["dir"] == "moviedir" || hh->ParamList["dir"] == "allmoviedirs" ) {
		CConfigFile *Config = new CConfigFile(',');
		Config->loadConfig(MOVIEBROWSER_CONFIGFILE);
		char index[21];
		std::string mb_dir;

		for(int i=0;i<8;i++) {
			snprintf(index, sizeof(index), "%d", i);
			mb_dir = "mb_dir_";
			mb_dir = mb_dir + index;
			mb_dir = Config->getString(mb_dir, "");

			if(!mb_dir.empty()) {
				item += hh->outPair("dir", hh->outValue(mb_dir), false);
				if(isFirstLine) {
					isFirstLine = false;
				}
				else {
					result += hh->outNext();
				}
				result += hh->outArrayItem("item", item, false);
				item = "";
				if (hh->ParamList["subdirs"] == "true") {
					result = getSubdirectories(hh, mb_dir, result);
				}
			}
		}
	}

	//Shows the neutrino recording dir
	if (hh->ParamList["dir"] == "recordingdir" || hh->ParamList["dir"] == "allmoviedirs" ) {
		item += hh->outPair("dir", hh->outValue(g_settings.network_nfs_recordingdir), false);
		if(isFirstLine) {
			isFirstLine = false;
		}
		else {
			result += hh->outNext();
		}
		result += hh->outArrayItem("item", item, false);
		if (hh->ParamList["subdirs"] == "true") {
			result = getSubdirectories(hh, g_settings.network_nfs_recordingdir, result);
		}
	}


	result = hh->outArray("dirs", result);

	hh->SendResult(result);
}

//Helpfunction to get subdirs of a dir
std::string CControlAPI::getSubdirectories(CyhookHandler *hh, std::string path, std::string result)
{
	std::string item = "";
	DIR *dirp;
	struct dirent *entry;

	if ((dirp = opendir(path.c_str()))) {
		while ((entry = readdir(dirp))) {
			if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
				if (path[path.length() - 1] != '/') {
					path += "/";
				}
				std::string fullname = path + entry->d_name;
				item += hh->outPair("dir", hh->outValue(fullname), false);
				result += hh->outNext();
				result += hh->outArrayItem("item", item, false);
				item = "";
				result = getSubdirectories(hh, fullname, result);
			}
		}
		closedir(dirp);
	}
	return result;
}


//-----------------------------------------------------------------------------
/** Get neutrino movies
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getmovies?ir=allmoviedirs&[&subdirs=true][&format=|xml|json]
 * @endcode
 *
{"success": "true", "data":{"movies": [{"title": "Sample.mkv",
"path": "/media/sda1/movies/Sample.mkv",
"size": "1136242099"
}
,{"title": "Aufnahme1.ts",
"path": "/media/sda1/recording/Aufnahme1.ts",
"size": "941"
}
,{"title": "Aufnahme2.ts",
"path": "/media/sda1/recording/Aufnahme2.ts",
"size": "941"
}
]
}}
 * @endcode
 *
 */
//-----------------------------------------------------------------------------
void CControlAPI::getMoviesCGI(CyhookHandler *hh) {
	std::string result = "";
	bool subdirs = true;

	if(hh->ParamList["subdirs"] == "false") {
		subdirs = false;
	}

	hh->outStart();

	//Shows all movies with path in moviebrowser.conf
	if (hh->ParamList["dir"] == "moviedir" || hh->ParamList["dir"] == "allmoviedirs" ) {
		CConfigFile *Config = new CConfigFile(',');
		Config->loadConfig(MOVIEBROWSER_CONFIGFILE);
		char index[21];
		std::string mb_dir;

		for(int i=0;i<8;i++) {
			snprintf(index, sizeof(index), "%d", i);
			mb_dir = "mb_dir_";
			mb_dir = mb_dir + index;
			mb_dir = Config->getString(mb_dir, "");

			if(!mb_dir.empty()) {
				result = readMovies(hh, mb_dir, result, subdirs);
			}
		}
	}

	//Shows all movies in the recordingdir
	if (hh->ParamList["dir"] == "recordingdir" || hh->ParamList["dir"] == "allmoviedirs" ) {
		result = readMovies(hh, g_settings.network_nfs_recordingdir, result, subdirs);
	}

	//Shows movie from a given path
	if (hh->ParamList["dir"][0] == '/') {
		result = readMovies(hh, hh->ParamList["dir"], result, subdirs);
	}

	result = hh->outArray("movies", result);

	hh->SendResult(result);
}

//Helpfunction to get movies of a dir
std::string CControlAPI::readMovies(CyhookHandler *hh, std::string path, std::string result, bool subdirs) {
	std::string item = "";
	std::string fullname;
	DIR *dirp;
	struct dirent *entry;

	if ((dirp = opendir(path.c_str()))) {
		if (path[path.length() - 1] != '/') {
			path += "/";
		}
		while ((entry = readdir(dirp))) {
			if(entry->d_type == 8) {
				fullname = path + entry->d_name;
				item += hh->outPair("title", hh->outValue(entry->d_name),true);
				item += hh->outPair("path", hh->outValue(fullname), true);
				struct stat statbuf;
				if (stat(fullname.c_str(), &statbuf) != -1) {
					/* Print size of file. */
					item += hh->outPair("size", string_printf("%jd", (intmax_t) statbuf.st_size), false);
				}
				if(!result.empty()) {
					result += hh->outNext();
				}
				result += hh->outArrayItem("item", item, false);
				item = "";
			}
		}
		closedir(dirp);
		if ((dirp = opendir(path.c_str()))) {
			if(subdirs)
			{
				while ((entry = readdir(dirp))) {
					if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
						fullname = path + entry->d_name;
						result = readMovies(hh, fullname, result, subdirs);
					}
				}
			}
			closedir(dirp);
		}
	}
	return result;
}
