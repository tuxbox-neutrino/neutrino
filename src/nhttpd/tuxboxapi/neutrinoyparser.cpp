//=============================================================================
// NHTTPD
// Neutrino yParser Extenstion
//=============================================================================
// c++
#include <string>
#include <iostream>
#include <fstream>
#include <limits>
#include <algorithm>
// system
#include <sys/types.h> //ntohs
#include <netinet/in.h> //ntohs
#include <inttypes.h> //ntohs

#include <global.h>
#include <neutrino.h>
#include <system/settings.h>
// yhttpd
#include <yhttpd.h>
#include <ytypes_globals.h>
#include <mod_yparser.h>
// tuxbox
#include <zapit/client/zapittools.h> //timer list
// nhttpd
#include "neutrinoyparser.h"
#include "neutrinoapi.h"


#include <zapit/channel.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>
#include <eitd/sectionsd.h>
#include <cs_api.h>
#include <system/configure_network.h>

extern CBouquetManager *g_bouquetManager;

//=============================================================================
// Constructor & Destructor & Initialization
//=============================================================================
CNeutrinoYParser::CNeutrinoYParser(CNeutrinoAPI	*_NeutrinoAPI)
{
	NeutrinoAPI = _NeutrinoAPI;
}
//-------------------------------------------------------------------------
CNeutrinoYParser::~CNeutrinoYParser(void)
{
}
//=============================================================================
// Hooks!
//=============================================================================
//-----------------------------------------------------------------------------
// HOOK: response_hook Handler
// This is the main dispatcher for this module
//-----------------------------------------------------------------------------
THandleStatus CNeutrinoYParser::Hook_SendResponse(CyhookHandler *hh)
{
	hh->status = HANDLED_NONE;

	log_level_printf(4,"Neutrinoparser Hook Start url:%s\n",hh->UrlData["url"].c_str());
	init(hh);

	CNeutrinoYParser *yP = new CNeutrinoYParser(NeutrinoAPI);		// create a Session
	if (hh->UrlData["fileext"] == "yhtm" || hh->UrlData["fileext"] == "yjs" || hh->UrlData["fileext"] == "ysh") // yParser for y*-Files
		yP->ParseAndSendFile(hh);
	else if(hh->UrlData["path"] == "/y/")	// /y/<cgi> commands
	{
		yP->Execute(hh);
		if(hh->status == HANDLED_NOT_IMPLEMENTED)
			hh->status = HANDLED_NONE;	// y-calls can be implemented anywhere
	}

	delete yP;

	log_level_printf(4,"Neutrinoparser Hook Ende status:%d\n",(int)hh->status);
//	log_level_printf(5,"Neutrinoparser Hook Result:%s\n",hh->yresult.c_str());

	return hh->status;
}
//-----------------------------------------------------------------------------
// HOOK: webserver_readconfig_hook Handler
// This hook ist called from ReadCong (webserver)
//-----------------------------------------------------------------------------
THandleStatus CNeutrinoYParser::Hook_ReadConfig(CConfigFile *Config, CStringList &ConfigList)
{
//	ConfigList["ExtrasDocumentRoot"]= Config->getString("ExtrasDocRoot", EXTRASDOCUMENTROOT);
//	ConfigList["ExtrasDocumentURL"]	= Config->getString("ExtrasDocURL", EXTRASDOCUMENTURL);
	ConfigList["TUXBOX_LOGOS_URL"]= Config->getString("Tuxbox.LogosURL", TUXBOX_LOGOS_URL);

	if (Config->getInt32("configfile.version") < 3)
	{
		Config->setString("Tuxbox.LogosURL", Config->getString("ExtrasDocURL", EXTRASDOCUMENTURL) +"/logos");
		Config->setInt32("configfile.version", CONF_VERSION);
		Config->saveConfig(HTTPD_CONFIGFILE);
	}
	//std::string logo = 
	ConfigList["TUXBOX_LOGOS_URL"];
	return HANDLED_CONTINUE;
}

//=============================================================================
// y-func : Dispatching
//=============================================================================
const CNeutrinoYParser::TyFuncCall CNeutrinoYParser::yFuncCallList[]=
{
	{"mount-get-list", 				&CNeutrinoYParser::func_mount_get_list},
	{"mount-set-values", 			&CNeutrinoYParser::func_mount_set_values},
	{"get_bouquets_as_dropdown",	&CNeutrinoYParser::func_get_bouquets_as_dropdown},
	{"get_bouquets_as_templatelist",&CNeutrinoYParser::func_get_bouquets_as_templatelist},
	{"get_actual_bouquet_number",	&CNeutrinoYParser::func_get_actual_bouquet_number},
	{"get_channels_as_dropdown",	&CNeutrinoYParser::func_get_channels_as_dropdown},
	{"get_bouquets_with_epg",		&CNeutrinoYParser::func_get_bouquets_with_epg},
	{"get_actual_channel_id",		&CNeutrinoYParser::func_get_actual_channel_id},
	{"get_logo_name",		&CNeutrinoYParser::func_get_logo_name},
	{"get_mode",					&CNeutrinoYParser::func_get_mode},
	{"get_video_pids",				&CNeutrinoYParser::func_get_video_pids},
	{"get_audio_pid",				&CNeutrinoYParser::func_get_radio_pid},
	{"get_audio_pids_as_dropdown",	&CNeutrinoYParser::func_get_audio_pids_as_dropdown},
	{"umount_get_list",				&CNeutrinoYParser::func_unmount_get_list},
	{"get_partition_list",			&CNeutrinoYParser::func_get_partition_list},
	{"get_boxtype",					&CNeutrinoYParser::func_get_boxtype},
	{"get_boxmodel",				&CNeutrinoYParser::func_get_boxmodel},
	{"get_current_stream_info",		&CNeutrinoYParser::func_get_current_stream_info},
	{"get_timer_list",				&CNeutrinoYParser::func_get_timer_list},
	{"set_timer_form",				&CNeutrinoYParser::func_set_timer_form},
	{"bouquet_editor_main",			&CNeutrinoYParser::func_bouquet_editor_main},
	{"set_bouquet_edit_form",		&CNeutrinoYParser::func_set_bouquet_edit_form},
};
//-------------------------------------------------------------------------
// y-func : dispatching and executing
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::YWeb_cgi_func(CyhookHandler *hh, std::string ycmd)
{
	std::string func="", para="", yresult="ycgi func not found";
	bool found = false;
	ySplitString(ycmd," ",func, para);

	log_level_printf(4,"NeutrinoYParser: func:(%s)\n", func.c_str());
	for(unsigned int i = 0;i < (sizeof(yFuncCallList)/sizeof(yFuncCallList[0])); i++)
		if (func == yFuncCallList[i].func_name)
		{
			yresult = (this->*yFuncCallList[i].pfunc)(hh, para);
			found = true;
			break;
		}
	log_level_printf(8,"NeutrinoYParser: func:(%s) para:(%s) Result:(%s)\n", func.c_str(), para.c_str(), yresult.c_str() );
	if(!found)
		yresult = CyParser::YWeb_cgi_func(hh, ycmd);
	return yresult;
}
//=============================================================================
// y-func : Functions for Neutrino
//=============================================================================
//-------------------------------------------------------------------------
// y-func : mount_get_list
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_mount_get_list(CyhookHandler *, std::string)
{
	CConfigFile *Config = new CConfigFile(',');
	std::string ysel, ytype, yip, ylocal_dir, ydir, ynr, yresult;
	int yitype;

	Config->loadConfig(NEUTRINO_CONFIGFILE);
	for(unsigned int i=0; i <= 7; i++)
	{
		ynr=itoa(i);
		ysel = ((i==0) ? "checked=\"checked\"" : "");
		yitype = Config->getInt32("network_nfs_type_"+ynr,0);
		ytype = ( (yitype==0) ? "NFS" :((yitype==1) ? "CIFS" : "FTPFS") );
		yip = Config->getString("network_nfs_ip_"+ynr,"");
		ydir = Config->getString("network_nfs_dir_"+ynr,"");
		ylocal_dir = Config->getString("network_nfs_local_dir_"+ynr,"");
		if(!ydir.empty())
			ydir="("+ydir+")";

		yresult += string_printf("<input type='radio' name='R1' value='%d' %s />%d %s - %s %s %s<br/>",
			i,ysel.c_str(),i,ytype.c_str(),yip.c_str(),ylocal_dir.c_str(), ydir.c_str());
	}
	delete Config;
	return yresult;
}

//-------------------------------------------------------------------------
// y-func : mount_set_values
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_mount_set_values(CyhookHandler *hh, std::string)
{
	CConfigFile *Config = new CConfigFile(',');
	std::string ynr, yresult;

	Config->loadConfig(NEUTRINO_CONFIGFILE);
	ynr = hh->ParamList["nr"];
	Config->setString("network_nfs_type_"+ynr,hh->ParamList["type"]);
	Config->setString("network_nfs_ip_"+ynr,hh->ParamList["ip"]);
	Config->setString("network_nfs_dir_"+ynr,hh->ParamList["dir"]);
	Config->setString("network_nfs_local_dir_"+ynr,hh->ParamList["localdir"]);
	Config->setString("network_nfs_mac_"+ynr,hh->ParamList["mac"]);
	Config->setString("network_nfs_mount_options1_"+ynr,hh->ParamList["opt1"]);
	Config->setString("network_nfs_mount_options2_"+ynr,hh->ParamList["opt2"]);
	Config->setString("network_nfs_automount_"+ynr,hh->ParamList["automount"]);
	Config->setString("network_nfs_username_"+ynr,hh->ParamList["username"]);
	Config->setString("network_nfs_password_"+ynr,hh->ParamList["password"]);
	Config->saveConfig(NEUTRINO_CONFIGFILE);

	delete Config;
	return yresult;
}
//-------------------------------------------------------------------------
// y-func : get_bouquets_as_dropdown [<bouquet>] <doshowhidden>
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_bouquets_as_dropdown(CyhookHandler *, std::string para)
{
	std::string yresult, sel, nr_str, do_show_hidden;
	int nr=1;

	ySplitString(para," ",nr_str, do_show_hidden);
	if(!nr_str.empty())
		nr = atoi(nr_str.c_str());

	int mode = NeutrinoAPI->Zapit->getMode();
	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++) {
		//ZapitChannelList &channels = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->Bouquets[i]->radioChannels : g_bouquetManager->Bouquets[i]->tvChannels;
		ZapitChannelList channels;
		if (mode == CZapitClient::MODE_RADIO)
			g_bouquetManager->Bouquets[i]->getRadioChannels(channels);
		else
			g_bouquetManager->Bouquets[i]->getTvChannels(channels);
		sel=(nr==(i+1)) ? "selected=\"selected\"" : "";
		if(!channels.empty() && (!g_bouquetManager->Bouquets[i]->bHidden || do_show_hidden == "true"))
			yresult += string_printf("<option value=%u %s>%s</option>\n", i + 1, sel.c_str(),
				std::string(g_bouquetManager->Bouquets[i]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) :g_bouquetManager->Bouquets[i]->Name.c_str()).c_str());
			//yresult += string_printf("<option value=%u %s>%s</option>\n", i + 1, sel.c_str(), (encodeString(std::string(g_bouquetManager->Bouquets[i]->Name.c_str()))).c_str());
	}
	return yresult;
}

//-------------------------------------------------------------------------
// y-func : get_bouquets_as_templatelist <tempalte> [~<do_show_hidden>]
// TODO: select actual Bouquet
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_bouquets_as_templatelist(CyhookHandler *, std::string para)
{
	std::string yresult, ytemplate, do_show_hidden;

	ySplitString(para,"~",ytemplate, do_show_hidden);
	//ytemplate += "\n"; //FIXME add newline to printf
	int mode = NeutrinoAPI->Zapit->getMode();
	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++) {
		//ZapitChannelList &channels = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->Bouquets[i]->radioChannels : g_bouquetManager->Bouquets[i]->tvChannels;
		ZapitChannelList channels;
		if (mode == CZapitClient::MODE_RADIO)
			g_bouquetManager->Bouquets[i]->getRadioChannels(channels);
		else
			g_bouquetManager->Bouquets[i]->getTvChannels(channels);
		if(!channels.empty() && (!g_bouquetManager->Bouquets[i]->bHidden || do_show_hidden == "true")) {
			yresult += string_printf(ytemplate.c_str(), i + 1, g_bouquetManager->Bouquets[i]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : g_bouquetManager->Bouquets[i]->Name.c_str());
			yresult += "\r\n";
		}
	}
	return yresult;
}
//-------------------------------------------------------------------------
// y-func : get_actual_bouquet_number
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_actual_bouquet_number(CyhookHandler *, std::string)
{
	int actual=0;
	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++) {
		if(g_bouquetManager->existsChannelInBouquet(i, CZapit::getInstance()->GetCurrentChannelID())) {
			actual=i+1;
			break;
		}
	}
	return std::string(itoa(actual));
}

//-------------------------------------------------------------------------
// y-func : get_channel_dropdown [<bouquet_nr> [<channel_id>]]
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_channels_as_dropdown(CyhookHandler *, std::string para)
{
	std::string abouquet, achannel_id, yresult, sel;

	int bnumber = 1;
	int mode = NeutrinoAPI->Zapit->getMode();

	ySplitString(para," ",abouquet, achannel_id);
	if(!abouquet.empty())
		bnumber = atoi(abouquet.c_str());
	if(bnumber > 0) {
		bnumber--;
		ZapitChannelList channels;
		//channels = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->Bouquets[bnumber]->radioChannels : g_bouquetManager->Bouquets[bnumber]->tvChannels;
		if (mode == CZapitClient::MODE_RADIO)
			g_bouquetManager->Bouquets[bnumber]->getRadioChannels(channels);
		else
			g_bouquetManager->Bouquets[bnumber]->getTvChannels(channels);
		for(int j = 0; j < (int) channels.size(); j++) {
			CEPGData epg;
			CZapitChannel * channel = channels[j];
			char buf[100],id[20];
			snprintf(id,sizeof(id),PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS,channel->getChannelID());
			std::string _sid = std::string(id);
			sel = (_sid == achannel_id) ? "selected=\"selected\"" : "";
			CEitManager::getInstance()->getActualEPGServiceKey(channel->getChannelID(), &epg);
			snprintf(buf,sizeof(buf),"<option value=" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " %s>%.20s - %.30s</option>\n", channel->getChannelID(), sel.c_str(), channel->getName().c_str(),epg.title.c_str());
			yresult += buf;
		}
	}
	return yresult;
}
//-------------------------------------------------------------------------
// TODO: clean up code
// use templates?
//-------------------------------------------------------------------------
std::string CNeutrinoYParser::func_get_bouquets_with_epg(CyhookHandler *hh, std::string para)
{
	int BouquetNr = 0;
	std::string abnumber, tmp,yresult;
	ZapitChannelList channels;
	//int num;
	int mode = NeutrinoAPI->Zapit->getMode();

	ySplitString(para," ",abnumber, tmp);
	if(!abnumber.empty())
		BouquetNr = atoi(abnumber.c_str());
	if (BouquetNr > 0) {
		BouquetNr--;
#if 0
		channels = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->Bouquets[BouquetNr]->radioChannels : g_bouquetManager->Bouquets[BouquetNr]->tvChannels;
		num = 1 + (mode == CZapitClient::MODE_RADIO ? g_bouquetManager->radioChannelsBegin().getNrofFirstChannelofBouquet(BouquetNr) : g_bouquetManager->tvChannelsBegin().getNrofFirstChannelofBouquet(BouquetNr)) ;
#endif
		if (mode == CZapitClient::MODE_RADIO)
			g_bouquetManager->Bouquets[BouquetNr]->getRadioChannels(channels);
		else
			g_bouquetManager->Bouquets[BouquetNr]->getTvChannels(channels);
	} else {
		CBouquetManager::ChannelIterator cit = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin();
		for (; !(cit.EndOfChannels()); cit++)
			channels.push_back(*cit);
		//num = 1;
	}
	NeutrinoAPI->GetChannelEvents();

	int i = 1;
	char classname;
	t_channel_id current_channel = CZapit::getInstance()->GetCurrentChannelID();
	int prozent = 100;
	CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
	std::string timestr;
	bool have_logos = false;

	if (!hh->WebserverConfigList["Tuxbox.LogosURL"].empty() && hh->WebserverConfigList["Tuxbox.DisplayLogos"] == "true" )
		have_logos = true;

	for(int j = 0; j < (int) channels.size(); j++)
	{
		CZapitChannel * channel = channels[j];
		CChannelEvent event;
		NeutrinoAPI->GetChannelEvent(channel->getChannelID(), event);

		classname = (i++ & 1) ? 'a' : 'b';
		if (channel->getChannelID() == current_channel)
			classname = 'c';

		std::string bouquetstr = (BouquetNr >= 0) ? ("&amp;bouquet=" + itoa(BouquetNr)) : "";
		yresult += "<tr>";

		if (have_logos) {
			std::string channel_logo = func_get_logo_name(hh, string_printf(PRINTF_CHANNEL_ID_TYPE, channel->getChannelID()));
			std::string zaplink;
			if (channel_logo.empty())
				zaplink = channel->getName().c_str();
			else
				zaplink = string_printf(
						"<img class=\"channel_logo\" src=\"%s\" title=\"%s\" alt=\"%s\" />"
						, channel_logo.c_str()
						, channel->getName().c_str()
						, channel->getName().c_str()
					);

			yresult += string_printf(
					"<td class=\"%c logo_cell %s\" width=\"44\" rowspan=\"2\">"
					"<a href=\"javascript:do_zap('" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS"')\">"
					"%s"
					"</a>"
					"</td>"
					, classname
					, (channel_logo.empty() ? "no_logo" : "")
					, channel->getChannelID()
					, zaplink.c_str()
					, channel->getName().c_str()
					, channel->getName().c_str()
				);
		}

		/* timer slider */
		if(event.eventID && event.duration > 0)
		{
			prozent = 100 * (time(NULL) - event.startTime) / event.duration;
			yresult += string_printf("<td class=\"%c title_cell\"><table class=\"title_table\" border=\"0\" cellspacing=\"0\" cellpadding=\"3\"><tr><td class=\"cslider_cell\">\n"
					"\t<table border=\"0\" rules=\"none\" class=\"cslider cslider_table\">"
					"<tr>"
					"<td class=\"cslider cslider_used\" width=\"%d\"></td>"
					"<td class=\"cslider cslider_free\" width=\"%d\"></td>"
					"</tr>"
					"</table>\n</td>\n"
					, classname
					, (prozent / 10) * 3
					, (10 - (prozent / 10))*3
				);
		}
		else
		{
			yresult += string_printf("<td class=\"%c title_cell\"><table class=\"title_table\" border=\"0\" cellspacing=\"0\" cellpadding=\"3\"><tr><td class=\"cslider_cell\">\n"
					"\t<table border=\"0\" rules=\"none\" class=\"cslider cslider_table\">"
					"<tr>"
					"<td class=\"cslider cslider_noepg\"></td>"
					"</tr>"
					"</table>\n</td>\n"
					, classname
				);
		}

		/* channel name */
		yresult += "<td>\n";

		if (channel->getChannelID() == current_channel)
			yresult += "<a name=\"akt\"></a>\n";

		yresult += string_printf("<a class=\"clist\" href=\"javascript:do_zap('" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS "')\">"
				"%d. %s%s"
				"</a>\n"
				, channel->getChannelID()
				, channel->number
				, channel->getName().c_str()
				, (channel->getServiceType() == ST_NVOD_REFERENCE_SERVICE) ? " (NVOD)" : ""
			);

		yresult += "</td>\n";

		/* buttons */
		yresult += "<td align=\"right\" >\n";

		if (channel->getChannelID() == current_channel)
		{
			yresult += "<a href=\"javascript:do_streaminfo()\">";
			yresult += "<img src=\"/images/streaminfo.png\" alt=\"Streaminfo\" title=\"Streaminfo\" />";
			yresult += "</a>\n";
		}

		if (!channel->getUrl().empty())
		{
			yresult += "<img src=\"/images/webtv.png\" alt=\"WebTV\" title=\"WebTV\" />\n";
		}

		if (channel->scrambled)
		{
			yresult += "<img src=\"/images/key.png\" alt=\"Scrambled\" title=\"Scrambled\" />\n";
		}
		if (event.eventID)
		{
			yresult += string_printf("<a href=\"javascript:do_epg('" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS "','" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS "')\">"
					"<img src=\"/images/elist.png\" alt=\"Program preview\" title=\"EPG\" />"
					"</a>\n"
					, channel->getChannelID()
					, channel->getChannelID() & 0xFFFFFFFFFFFFULL
				);
		}

		yresult += string_printf("<a href=\"javascript:do_stream('" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS "','%s')\">"
				"<img src=\"/images/stream.png\" alt=\"Stream\" title=\"Stream\" />"
				"</a>\n"
				, channel->getChannelID()
				, channel->getName().c_str()
			);

		yresult += "</td></tr></table>\n</td>\n</tr>\n";

		if (channel->getServiceType() == ST_NVOD_REFERENCE_SERVICE)
		{
			CSectionsdClient::NVODTimesList nvod_list;
			if (CEitManager::getInstance()->getNVODTimesServiceKey(channel->getChannelID(), nvod_list))
			{
				CZapitClient::subServiceList subServiceList;

				for (CSectionsdClient::NVODTimesList::iterator ni = nvod_list.begin(); ni != nvod_list.end(); ++ni)
				{
					CZapitClient::commandAddSubServices cmd;
					CEPGData epg;

					// Byte Sequence by ntohs
					cmd.original_network_id = ntohs(ni->original_network_id);
					cmd.service_id = ntohs(ni->service_id);
					cmd.transport_stream_id = ntohs(ni->transport_stream_id);

					t_channel_id channel_id = CREATE_CHANNEL_ID(cmd.service_id, cmd.original_network_id, cmd.transport_stream_id);

					timestr = timeString(ni->zeit.startzeit); // FIXME: time is wrong (at least on little endian)!
					CEitManager::getInstance()->getActualEPGServiceKey(channel_id, &epg); // FIXME: der scheissendreck geht nit!!!
					yresult += string_printf("<tr>\n<td align=\"left\" style=\"width: 31px\" class=\"%cepg\">&nbsp;</td>", classname);
					yresult += string_printf("<td class=\"%cepg\">%s&nbsp;", classname, timestr.c_str());
					yresult += string_printf("%s<a href=\"javascript:do_zap('"
							PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
							")'\">%04x:%04x:%04x %s</a>", // FIXME: get name
							(channel_id == current_channel) ? "<a name=\"akt\"></a>" : " ",
							channel_id,
							bouquetstr.c_str(),
							cmd.transport_stream_id,
							cmd.original_network_id,
							cmd.service_id,
							epg.title.c_str());
					yresult += string_printf("</td>\n</tr>");

					subServiceList.push_back(cmd);
				}

				if (!(subServiceList.empty()))
					NeutrinoAPI->Zapit->setSubServices(subServiceList);
			}
		}

		else if (event.eventID)
		{
			bool has_current_next = true;
			CEitManager::getInstance()->getCurrentNextServiceKey(channel->getEpgID(), currentNextInfo);
			timestr = timeString(event.startTime);

			CShortEPGData epg;
			std::string EPGInfoC = "";
			if (CEitManager::getInstance()->getEPGidShort(currentNextInfo.current_uniqueKey, &epg))
			{
				EPGInfoC += epg.info1;
				EPGInfoC += epg.info2;
			}

			yresult += string_printf("<tr><td class=\"%cepg\">",classname);
			yresult += string_printf("%s&nbsp;<span class='pointer' title='%s'>%s</span>&nbsp;"
					"<span style=\"font-size: 8pt; white-space: nowrap\">(%ld {=L:from=} %d {=L:unit.short.minute=}, %d%%)</span>"
					, timestr.c_str()
					, EPGInfoC.c_str()
					, event.description.c_str()
					, (time(NULL) - event.startTime)/60
					, event.duration / 60,prozent);

			if ((has_current_next) && (currentNextInfo.flags & CSectionsdClient::epgflags::has_next)) {
				std::string EPGInfoN = "";
				if (CEitManager::getInstance()->getEPGidShort(currentNextInfo.next_uniqueKey, &epg))
				{
					EPGInfoN += epg.info1;
					EPGInfoN += epg.info2;
				}
				timestr = timeString(currentNextInfo.next_zeit.startzeit);
				yresult += string_printf("<br />%s&nbsp;<span class='pointer' title='%s'>%s</span>"
						, timestr.c_str()
						, EPGInfoN.c_str()
						, currentNextInfo.next_name.c_str());
			}

			yresult += string_printf("</td></tr>\n");
		}
		else
		yresult += string_printf("<tr><td class=\"%cepg\">&nbsp;<br />&nbsp;</td></tr>\n",classname);
	}
	return yresult;
}

//-------------------------------------------------------------------------
// y-func : get_actual_channel_id
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_actual_channel_id(CyhookHandler *, std::string)
{
	return string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, CZapit::getInstance()->GetCurrentChannelID());
}

//-------------------------------------------------------------------------
// func: Get logo name for Webif
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_logo_name(CyhookHandler *hh, std::string channelId)
{
	std::string LogosURL = hh->WebserverConfigList["Tuxbox.LogosURL"];
	if (hh->WebserverConfigList["Tuxbox.DisplayLogos"] == "true" && !LogosURL.empty())
	{
		std::string fileType[] = { ".png", ".jpg" , ".gif" };

		std::string channelIdShort = channelId.substr(channelId.length() - 12);
		channelIdShort = channelIdShort.erase(0, min(channelIdShort.find_first_not_of('0'), channelIdShort.size()-1));

		std::string channelName = "";
		t_channel_id chId = 0;
		if (sscanf(channelId.c_str(), "%" PRIx64, &chId) == 1)
			channelName = NeutrinoAPI->GetServiceName(chId);

		for (size_t i = 0; i < (sizeof(fileType) / sizeof(fileType[0])); i++)
		{
			// first check Tuxbox.LogosURL from nhttpd.conf
			if (access((LogosURL + "/" + channelName + fileType[i]).c_str(), R_OK) == 0)
				return LogosURL + "/" + channelName + fileType[i];
			else if (access((LogosURL + "/" + channelIdShort + fileType[i]).c_str(), R_OK) == 0)
				return LogosURL + "/" + channelIdShort + fileType[i];
			else // fallback to default logos
				return NeutrinoAPI->getLogoFile(chId);
		}
	}
	return "";
}

//-------------------------------------------------------------------------
// y-func : get_mode (returns tv|radio|unknown)
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_mode(CyhookHandler *, std::string)
{
	std::string yresult;
	int mode = NeutrinoAPI->Zapit->getMode();

	if ( mode == CZapitClient::MODE_TV)
		yresult = "tv";
	else if ( mode == CZapitClient::MODE_RADIO)
		yresult = "radio";
	else
		yresult = "unknown";
	return yresult;
}

//-------------------------------------------------------------------------
// y-func : get_video_pids (para=audio channel, returns: 0x0000,0x0000,0x0000)
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_video_pids(CyhookHandler *, std::string para)
{
	std::string yresult;
	CZapitClient::responseGetPIDs pids;
	int apid=0,apid_no=0,apid_idx=0;
	pids.PIDs.vpid=0;

	if(!para.empty())
		apid_no = atoi(para.c_str());
	NeutrinoAPI->Zapit->getPIDS(pids);

	if( apid_no < (int)pids.APIDs.size())
		apid_idx=apid_no;
	if(!pids.APIDs.empty())
		apid = pids.APIDs[apid_idx].pid;
	yresult = string_printf("0x%04x,0x%04x,0x%04x", pids.PIDs.pmtpid, pids.PIDs.vpid, apid);
	if (pids.PIDs.pcrpid != pids.PIDs.vpid)
		yresult += string_printf(",0x%04x", pids.PIDs.pcrpid);
	return yresult;
}
//-------------------------------------------------------------------------
// y-func : get_radio_pids (returns: 0x0000)
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_radio_pid(CyhookHandler *, std::string)
{
	CZapitClient::responseGetPIDs pids;
	int apid=0;

	NeutrinoAPI->Zapit->getPIDS(pids);
	if(!pids.APIDs.empty())
		apid = pids.APIDs[0].pid;

	return string_printf("0x%04x",apid);
}

//-------------------------------------------------------------------------
// y-func : get_audio_pids_as_dropdown (from controlapi)
// prara: [apid] option value = apid-Value. Default apid-Index
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_audio_pids_as_dropdown(CyhookHandler *, std::string para)
{
	std::string yresult;
	static bool init_iso=true;
	bool idx_as_id=true;
	unsigned int selected_apid = 0;
	t_channel_id current_channel_id = 0;
	CZapitChannel * channel = NULL;
	if(para == "apid")
		idx_as_id=false;
	else if(!para.empty() && ("channel="== para.substr(0,8))){
		if (sscanf(para.c_str(), "channel=%" SCNx64 ":audio=%i:", &current_channel_id,&selected_apid) == 2) {
			if(current_channel_id != 0 && CZapit::getInstance()->GetCurrentChannelID() != current_channel_id){
				channel = CServiceManager::getInstance()->FindChannel(current_channel_id);
			}
		}
	}
	if(init_iso)
	{
		if(_initialize_iso639_map())
			init_iso=false;
	}
	if (channel){//check audio pid if current_channel != vlc live channel
		//wait for channel lock
		for (int i = 0; i < 30 && channel->getAudioChannelCount()==0;i++){
			usleep(100000);
		}
		for (unsigned int i = 0; i <  channel->getAudioChannelCount(); i++) {
			CZapitAudioChannel::ZapitAudioChannelType atype = channel->getAudioChannel(i)->audioChannelType;
			std::string a_desc;
			if(!(init_iso)){
				a_desc = _getISO639Description( channel->getAudioChannel(i)->description.c_str() );
			}else{
				a_desc = channel->getAudioChannel(i)->description.c_str();
			}
			if (channel->getAudioChannel(i)->audioChannelType == CZapitAudioChannel::EAC3) {
				yresult += string_printf("<option value=%05u %s>%s %s</option>\r\n",i
				, (i==selected_apid) ? "selected=\"selected\"" : ""
				, a_desc.c_str()
				,"(EAC3)");
			} else {
				yresult += string_printf("<option value=%05u %s>%s %s</option>\r\n",i
				, (i==selected_apid) ? "selected=\"selected\"" : ""
				,a_desc.c_str()
				,atype?"(AC3)":" ");
			}
		}
	}
	if( yresult.empty()){
		bool eit_not_ok=true;
		if(current_channel_id==0)
			current_channel_id = CZapit::getInstance()->GetCurrentChannelID();

		CZapitClient::responseGetPIDs pids;
		CSectionsdClient::ComponentTagList tags;
		pids.PIDs.vpid=0;
		NeutrinoAPI->Zapit->getPIDS(pids);

		CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
		CEitManager::getInstance()->getCurrentNextServiceKey(current_channel_id, currentNextInfo);
		if (CEitManager::getInstance()->getComponentTagsUniqueKey(currentNextInfo.current_uniqueKey,tags))
		{
			unsigned int tag = 0;
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
							yresult += string_printf("<option value=%05u %s>%s</option>\r\n",idx_as_id ? j : pids.APIDs[j].pid,(tag==selected_apid) ? "selected=\"selected\"" : "",tags[i].component.c_str());
							tag++;
						}
						else
						{
							if(!(init_iso))
							{
								std::string tmp_desc = _getISO639Description( pids.APIDs[j].desc);
								strncpy(pids.APIDs[j].desc, tmp_desc.c_str(), DESC_MAX_LEN -1);
							}
							yresult += string_printf("<option value=%05u %s>%s %s</option>\r\n",idx_as_id ? j : pids.APIDs[j].pid,(j==selected_apid) ? "selected=\"selected\"" : "",std::string(pids.APIDs[j].desc).c_str(),pids.APIDs[j].is_ac3 ? " (AC3)": pids.APIDs[j].is_aac ? "(AAC)" : pids.APIDs[j].is_eac3 ? "(EAC3)" : " ");
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
					std::string tmp_desc = _getISO639Description( pids.APIDs[i].desc);
					strncpy(pids.APIDs[i].desc, tmp_desc.c_str(), DESC_MAX_LEN -1);
				}
				yresult += string_printf("<option value=%05u %s>%s %s</option>\r\n",
							 idx_as_id ? i : it->pid, (i==selected_apid) ? "selected=\"selected\"" : "",pids.APIDs[i].desc,
							 pids.APIDs[i].is_ac3 ? " (AC3)": pids.APIDs[i].is_aac ? "(AAC)" : pids.APIDs[i].is_eac3 ? "(EAC3)" : " ");
				i++;
			}
		}
		if(pids.APIDs.empty())
			yresult = "00000"; // shouldnt happen, but print at least one apid
	}
	return yresult;
}

//-------------------------------------------------------------------------
// y-func : build umount list
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_unmount_get_list(CyhookHandler *, std::string)
{
	std::string ysel, ymount, ylocal_dir, yfstype, yresult, mounts;

	std::ifstream in;
	in.open("/proc/mounts", std::ifstream::in);
	int j=0;
	while(in.good() && (j<8))
	{
		yfstype="";
		in >> ymount >> ylocal_dir >> yfstype;
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		yfstype = trim(yfstype);
		if( (yfstype == "nfs") << (yfstype == "ftp") || (yfstype == "lufsd") )
		{
			mounts=ylocal_dir +" on "+ ymount + " ("+yfstype+")";
			ysel = ((j==0) ? "checked=\"checked\"" : "");
			yresult += string_printf("<input type='radio' name='R1' value='%s' %s />%d %.120s<br/>",
				ylocal_dir.c_str(),ysel.c_str(),j,mounts.c_str());
			j++;
		}
	}
	return yresult;
}

//-------------------------------------------------------------------------
// y-func : build partition list
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_partition_list(CyhookHandler *, std::string)
{
	std::string ysel, ymtd, yname, dummy, yresult;
	char ytmp[200];

	std::ifstream in;
	in.open("/proc/mtd", std::ifstream::in);
	int j=0;
	while(in.good() && (j<8))
	{
		ymtd="";
		in >> ymtd >> dummy >> dummy; //format:  mtd# start end "name  "
		in.getline(ytmp, 200); // Rest of line is the mtd description
		yname = ytmp;
		if((j>0) && (!ymtd.empty()))// iggnore first line
		{
			ysel = ((j==1) ? "checked=\"checked\"" : "");
			yresult += string_printf("<input type='radio' name='R1' value='%d' %s title='%s' />%d %s<br/>",
				j-1,ysel.c_str(),yname.c_str(),j-1,yname.c_str());
		}
		j++;
	}
	return yresult;
}
//-------------------------------------------------------------------------
// y-func : get boxtypetext
//-------------------------------------------------------------------------
std::string CNeutrinoYParser::func_get_boxtype(CyhookHandler *, std::string)
{
	std::string boxvendor(g_info.hw_caps->boxvendor);
	/* stay compatible with present code in Y_Blocks.txt */
	if (boxvendor.compare("Coolstream") == 0)
		boxvendor = "CST";

	std::string boxname(g_info.hw_caps->boxname);
	/* workaround for Neo2 */
	if ((boxname.compare("Neo") == 0) && (CFEManager::getInstance()->getFrontendCount() > 1))
		boxname += " Twin";

	return boxvendor + " " + boxname;
}
//-------------------------------------------------------------------------
// y-func : get boxmodel
//-------------------------------------------------------------------------
std::string CNeutrinoYParser::func_get_boxmodel(CyhookHandler *, std::string)
{
	return g_info.hw_caps->boxarch;
}
//-------------------------------------------------------------------------
// y-func : get stream info
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_current_stream_info(CyhookHandler *hh, std::string)
{
	CZapitClient::CCurrentServiceInfo serviceinfo;

	serviceinfo = NeutrinoAPI->Zapit->getCurrentServiceInfo();
	hh->ParamList["onid"] = itoh(serviceinfo.onid);
	hh->ParamList["sid"] = itoh(serviceinfo.sid);
	hh->ParamList["tsid"] = itoh(serviceinfo.tsid);
	hh->ParamList["vpid"] = itoh(serviceinfo.vpid);
	hh->ParamList["apid"] = itoh(serviceinfo.apid);
	hh->ParamList["vtxtpid"] = (serviceinfo.vtxtpid != 0)?itoh(serviceinfo.vtxtpid):"not available";
	hh->ParamList["pmtpid"] = (serviceinfo.pmtpid != 0)?itoh(serviceinfo.pmtpid):"not available";
	hh->ParamList["pcrpid"] = (serviceinfo.pcrpid != 0)?itoh(serviceinfo.pcrpid):"not available";
	hh->ParamList["tsfrequency"] = string_printf("%d.%d MHz", serviceinfo.tsfrequency/1000, serviceinfo.tsfrequency%1000);
	hh->ParamList["polarisation"] = serviceinfo.polarisation==1?"h":"v";
	hh->ParamList["ServiceName"] = NeutrinoAPI->GetServiceName(CZapit::getInstance()->GetCurrentChannelID());
	hh->ParamList["VideoFormat"] = NeutrinoAPI->getVideoResolutionAsString();
//	hh->ParamList["BitRate"] = NeutrinoAPI->getVideoFramerateAsString();
	hh->ParamList["AspectRatio"] = NeutrinoAPI->getVideoAspectRatioAsString();
	hh->ParamList["FPS"] = NeutrinoAPI->getVideoFramerateAsString();
	hh->ParamList["AudioType"] = NeutrinoAPI->getAudioInfoAsString();
	hh->ParamList["Crypt"] = NeutrinoAPI->getCryptInfoAsString();
	return "";
}
//-------------------------------------------------------------------------
// Template 1:classname, 2:zAlarmTime, 3: zStopTime, 4:zRep, 5:zRepCouunt
//		6:zType, 7:sAddData, 8:timer->eventID, 9:timer->eventID
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_get_timer_list(CyhookHandler *, std::string para)
{
	std::string yresult;
	CTimerd::TimerList timerlist;				// List of bouquets

	timerlist.clear();
	NeutrinoAPI->Timerd->getTimerList(timerlist);
	sort(timerlist.begin(), timerlist.end());

	CZapitClient::BouquetChannelList channellist_tv;
	CZapitClient::BouquetChannelList channellist_radio;
	channellist_tv.clear();
	channellist_radio.clear();

	int i = 1;
	char classname= 'a';
	CTimerd::TimerList::iterator timer = timerlist.begin();
	for(; timer != timerlist.end();++timer)
	{
		classname = (i++&1)?'a':'b';

		// build alarm/stoptime
		char zAlarmTime[25] = {0};
		struct tm *alarmTime = localtime(&(timer->alarmTime));
		strftime(zAlarmTime,20,"%d.%m. %H:%M",alarmTime);

		char zAnnounceTime[25] = {0};
		struct tm *announceTime = localtime(&(timer->announceTime));
		strftime(zAnnounceTime,20,"%d.%m. %H:%M",announceTime);

		char zStopTime[25] = {0};
		if(timer->stopTime > 0)
		{
			struct tm *stopTime = localtime(&(timer->stopTime));
			strftime(zStopTime,20,"%d.%m. %H:%M",stopTime);
		}
		// repeat
		std::string zRep = NeutrinoAPI->timerEventRepeat2Str(timer->eventRepeat);
		std::string zRepCount;
		if (timer->eventRepeat == CTimerd::TIMERREPEAT_ONCE)
			zRepCount = "-";
		else
			zRepCount = (timer->repeatCount == 0) ? "&#x221E;" : string_printf("%dx",timer->repeatCount);
		// timer type
		std::string zType = NeutrinoAPI->timerEventType2Str(timer->eventType);
		// Add Data
		std::string sAddData="";
		switch(timer->eventType)
		{
			//case CTimerd::TIMER_NEXTPROGRAM :
			case CTimerd::TIMER_ZAPTO :
			case CTimerd::TIMER_RECORD :
			{
				sAddData = NeutrinoAPI->GetServiceName(timer->channel_id);
				if (sAddData.empty())
					sAddData = CServiceManager::getInstance()->IsChannelTVChannel(timer->channel_id) ? "Unknown TV-Channel" : "Unknown Radio-Channel";

				if( timer->apids != TIMERD_APIDS_CONF)
				{
					std::string separator = "";
					sAddData += '(';
					if( timer->apids & TIMERD_APIDS_STD )
					{
						sAddData += "STD";
						separator = "/";
					}
					if( timer->apids & TIMERD_APIDS_ALT )
					{
						sAddData += separator;
						sAddData += "ALT";
						separator = "/";
					}
					if( timer->apids & TIMERD_APIDS_AC3 )
					{
						sAddData += separator;
						sAddData += "AC3";
						separator = "/";
					}
					sAddData += ')';
				}
				if(timer->epgID!=0)
				{
					CEPGData epgdata;
					if (CEitManager::getInstance()->getEPGid(timer->epgID, timer->epg_starttime, &epgdata))
						sAddData+="<br/>" + epgdata.title;
					else
						sAddData+=std::string("<br/>")+timer->epgTitle;
				}
				else
					sAddData+=std::string("<br/>")+timer->epgTitle;
			}
			break;
			case CTimerd::TIMER_STANDBY :
			{
				sAddData = "Standby: ";
				if(timer->standby_on)
					sAddData+= "An";
				else
					sAddData+="Aus";
			}
			break;
			case CTimerd::TIMER_REMIND :
				sAddData = std::string(timer->message).substr(0,20);
				break;
			case CTimerd::TIMER_EXEC_PLUGIN :
				sAddData = std::string(timer->pluginName);
				break;

			default:{}
		}
		yresult += string_printf(para.c_str(), classname, zAlarmTime, zStopTime, zRep.c_str(), zRepCount.c_str(),
					zType.c_str(), sAddData.c_str(),timer->eventID,timer->eventID);
	}
	//classname = (i++&1)?'a':'b';

	return yresult;
}
/*------------------------------------------------------------------------------
	CTimerd::TIMER_RECORD = 5
	CTimerd::TIMER_STANDBY = 4
	CTimerd::TIMER_NEXTPROGRAM = 2 !!! no longer used !!!
	CTimerd::TIMER_ZAPTO = 3
	CTimerd::TIMER_REMIND = 6
	CTimerd::TIMER_EXEC_PLUGIN = 8
	CTimerd::TIMERREPEAT_WEEKDAYS = 256
	CTimerd::TIMERREPEAT_ONCE = 0
 */
// para ::= <type=new|modify> [timerid]
std::string  CNeutrinoYParser::func_set_timer_form(CyhookHandler *hh, std::string para)
{
	unsigned timerId=0;
	std::string cmd, stimerid;
	CTimerd::responseGetTimer timer;             // Timer
	time_t now_t = time(NULL);
	ySplitString(para, " ", cmd, stimerid);
	if(cmd != "new")
	{
		// init timerid
		if(!stimerid.empty())
			timerId = (unsigned)atoi(stimerid.c_str());

		NeutrinoAPI->Timerd->getTimer(timer, timerId);
		std::string zType = NeutrinoAPI->timerEventType2Str(timer.eventType);

		hh->ParamList["timerId"] = itoa(timerId);
		hh->ParamList["zType"] = zType;
	}
	// Alarm/StopTime
	struct tm *alarmTime = (cmd == "new") ? localtime(&now_t) : localtime(&(timer.alarmTime));

	hh->ParamList["alarm_mday"] = string_printf("%02d", alarmTime->tm_mday);
	hh->ParamList["alarm_mon"]  = string_printf("%02d", alarmTime->tm_mon +1);
	hh->ParamList["alarm_year"] = string_printf("%04d", alarmTime->tm_year + 1900);
	hh->ParamList["alarm_hour"] = string_printf("%02d", alarmTime->tm_hour);
	hh->ParamList["alarm_min"]  = string_printf("%02d", alarmTime->tm_min);

	struct tm *stopTime = (cmd == "new") ? alarmTime : ( (timer.stopTime > 0) ? localtime(&(timer.stopTime)) : NULL );
	if(stopTime != NULL)
	{
		hh->ParamList["stop_mday"] = string_printf("%02d", stopTime->tm_mday);
		hh->ParamList["stop_mon"]  = string_printf("%02d", stopTime->tm_mon +1);
		hh->ParamList["stop_year"] = string_printf("%04d", stopTime->tm_year + 1900);
		hh->ParamList["stop_hour"] = string_printf("%02d", stopTime->tm_hour);
		hh->ParamList["stop_min"]  = string_printf("%02d", stopTime->tm_min);

		// APid settings for Record
		if(timer.apids == TIMERD_APIDS_CONF)
			hh->ParamList["TIMERD_APIDS_CONF"] = "y";
		if(timer.apids & TIMERD_APIDS_STD)
			hh->ParamList["TIMERD_APIDS_STD"]  = "y";
		if(timer.apids & TIMERD_APIDS_ALT)
			hh->ParamList["TIMERD_APIDS_ALT"]  = "y";
		if(timer.apids & TIMERD_APIDS_AC3)
			hh->ParamList["TIMERD_APIDS_AC3"]  = "y";
	}
	else
		hh->ParamList["stop_mday"] = "0";
	// event type
	std::string sel;
	for(int i=1; i<=8;i++)
	{
		if (i != (int)CTimerd::__TIMER_NEXTPROGRAM)
		{
			std::string zType = NeutrinoAPI->timerEventType2Str((CTimerd::CTimerEventTypes) i);
			if(cmd != "new")
				sel = (i==(int)timer.eventType) ? "selected=\"selected\"" : "";
			else
				sel = (i==(int)CTimerd::TIMER_RECORD) ? "selected=\"selected\"" : "";
			hh->ParamList["timertype"] +=
				string_printf("<option value=\"%d\" %s>%s\n",i,sel.c_str(),zType.c_str());
		}
	}
	// Repeat types
	std::string zRep;
	sel = "";
	for(int i=0; i<=6;i++)
	{
		if(i!=(int)CTimerd::TIMERREPEAT_BYEVENTDESCRIPTION)
		{

			zRep = NeutrinoAPI->timerEventRepeat2Str((CTimerd::CTimerEventRepeat) i);
			if(cmd != "new")
				sel = (((int)timer.eventRepeat) == i) ? "selected=\"selected\"" : "";
			hh->ParamList["repeat"] +=
				string_printf("<option value=\"%d\" %s>%s</option>\n",i,sel.c_str(),zRep.c_str());
		}
	}
	// Repeat Weekdays
	zRep = NeutrinoAPI->timerEventRepeat2Str(CTimerd::TIMERREPEAT_WEEKDAYS);
	if(timer.eventRepeat >= CTimerd::TIMERREPEAT_WEEKDAYS && cmd != "new")
		sel = "selected=\"selected\"";
	else
		sel = "";
	hh->ParamList["repeat"] +=
		string_printf("<option value=\"%d\" %s>%s</option>\n",(int)CTimerd::TIMERREPEAT_WEEKDAYS, sel.c_str(), zRep.c_str());

	// Weekdays
	std::string weekdays;
	NeutrinoAPI->Timerd->setWeekdaysToStr(timer.eventRepeat, weekdays);
	hh->ParamList["weekdays"]=	 weekdays;

	// timer repeats
	if (timer.eventRepeat == CTimerd::TIMERREPEAT_ONCE)
		hh->ParamList["TIMERREPEAT_ONCE"]  = "y";
	hh->ParamList["timer_repeatCount"]  = itoa(timer.repeatCount);

	// program row
	t_channel_id current_channel = (cmd == "new") ? CZapit::getInstance()->GetCurrentChannelID() : timer.channel_id;
	CBouquetManager::ChannelIterator cit = g_bouquetManager->tvChannelsBegin();
	for (; !(cit.EndOfChannels()); cit++) {
		if (((*cit)->flags & CZapitChannel::REMOVED) || (*cit)->flags & CZapitChannel::NOT_FOUND)
			continue;
		sel = ((*cit)->getChannelID() == current_channel) ? "selected=\"selected\"" : "";
		hh->ParamList["program_row"] +=
			string_printf("<option value=\""
				PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				"\" %s>%s</option>\n",
				(*cit)->getChannelID(), sel.c_str(), (*cit)->getName().c_str());
	}
	cit = g_bouquetManager->radioChannelsBegin();
	for (; !(cit.EndOfChannels()); cit++) {
		if (((*cit)->flags & CZapitChannel::REMOVED) || (*cit)->flags & CZapitChannel::NOT_FOUND)
			continue;
		sel = ((*cit)->getChannelID() == current_channel) ? "selected=\"selected\"" : "";
		hh->ParamList["program_row"] +=
			string_printf("<option value=\""
				PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
				"\" %s>%s</option>\n",
				(*cit)->getChannelID(), sel.c_str(), (*cit)->getName().c_str());
	}
	// recordingDir
	hh->ParamList["RECORD_DIR_MAXLEN"] = itoa(RECORD_DIR_MAXLEN-1);
	if(cmd != "new") {
		if(timer.eventType == CTimerd::TIMER_RECORD)
			hh->ParamList["timer_recordingDir"] = timer.recordingDir;
	}
	else
	{
		// get Default Recordingdir
		CConfigFile *Config = new CConfigFile(',');
		Config->loadConfig(NEUTRINO_CONFIGFILE);
		hh->ParamList["timer_recordingDir"] = Config->getString("network_nfs_recordingdir", "/mnt/filme");
		delete Config;
	}
	hh->ParamList["standby"] = (cmd == "new")? "0" : ((timer.standby_on)?"1":"0");
	hh->ParamList["message"] = (cmd == "new")? "" : timer.message;
	hh->ParamList["pluginname"] = (cmd == "new")? "" : timer.pluginName;

	return "";
}

//-------------------------------------------------------------------------
// func: Bouquet Editor List
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_bouquet_editor_main(CyhookHandler *hh, std::string para)
{
	std::string yresult;
	int selected = -1;
	if (hh->ParamList["saved"] == "1")
		hh->ParamList["have_saved"]="true";

	if (!hh->ParamList["selected"].empty())
		selected = atoi(hh->ParamList["selected"].c_str());

	int bouquetSize = (int) g_bouquetManager->Bouquets.size();
	int mode = NeutrinoAPI->Zapit->getMode();
	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++) {
		ZapitChannelList * channels = mode == CZapitClient::MODE_RADIO ? &g_bouquetManager->Bouquets[i]->radioChannels : &g_bouquetManager->Bouquets[i]->tvChannels;
		if(!channels->empty() || g_bouquetManager->Bouquets[i]->bUser){
			CZapitBouquet * bouquet = g_bouquetManager->Bouquets[i];

			char classname = ((i & 1) == 0) ? 'a' : 'b';
			classname = (selected == (int) i + 1)?'c':classname;

			std::string akt = (selected == (int) (i + 1)) ? "<a name=\"akt\"></a>" : "";
			// lock/unlock
			std::string lock_action = (bouquet->bLocked) ? "unlock" : "lock";
			std::string lock_img 	= (bouquet->bLocked) ? "lock" : "unlock";
			std::string lock_alt 	= (bouquet->bLocked) ? "unlock" : "lock";
			// hide/show
			std::string hidden_action= (bouquet->bHidden) ? "show" : "hide";
			std::string hidden_img 	= (bouquet->bHidden) ? "hidden" : "visible";
			std::string hidden_alt 	= (bouquet->bHidden) ? "hide" : "show";
			// move down
			std::string down_show 	= (i + 1 < bouquetSize) ? "visible" : "hidden";
			//move up
			std::string up_show 	= (i > 0) ? "visible" : "hidden";
			// build from template
			yresult += string_printf(para.c_str(), classname, akt.c_str(),
				i + 1, lock_action.c_str(), lock_img.c_str(), lock_alt.c_str(), //lock
				i + 1, hidden_action.c_str(), hidden_img.c_str(), hidden_alt.c_str(), //hhidden
				i + 1, bouquet->Name.c_str(), bouquet->Name.c_str(), //link
				i + 1, bouquet->Name.c_str(), //rename
				i + 1, bouquet->Name.c_str(), //delete
				down_show.c_str(), i + 1, //down arrow
				up_show.c_str(), i + 1); //up arrow
		}
	}
	return yresult;
}

//-------------------------------------------------------------------------
// func: Bouquet Edit
//-------------------------------------------------------------------------
std::string  CNeutrinoYParser::func_set_bouquet_edit_form(CyhookHandler *hh, std::string)
{
	if (!(hh->ParamList["selected"].empty()))
	{
		int selected = atoi(hh->ParamList["selected"].c_str()) - 1;
		int mode = NeutrinoAPI->Zapit->getMode();
		ZapitChannelList* channels = mode == CZapitClient::MODE_TV ? &(g_bouquetManager->Bouquets[selected]->tvChannels) : &(g_bouquetManager->Bouquets[selected]->radioChannels);
		for(int j = 0; j < (int) channels->size(); j++) {
			hh->ParamList["bouquet_channels"] +=
				string_printf("<option value=\""
					PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					"\">%s</option>\n",
					(*channels)[j]->getChannelID(),
					(*channels)[j]->getName().c_str());
		}
		ZapitChannelList Channels;
		Channels.clear();
		if (mode == CZapitClient::MODE_RADIO)
			CServiceManager::getInstance()->GetAllRadioChannels(Channels);
		else
			CServiceManager::getInstance()->GetAllTvChannels(Channels);

		sort(Channels.begin(), Channels.end(), CmpChannelByChName());

		for (int i = 0; i < (int) Channels.size(); i++) {
			if (!g_bouquetManager->existsChannelInBouquet(selected, Channels[i]->getChannelID())){
				hh->ParamList["all_channels"] +=
					string_printf("<option value=\""
						PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
						"\">%s</option>\n",
						Channels[i]->getChannelID(),
						Channels[i]->getName().c_str());
			}
		}
		return "";
	}
	else
		return "No Bouquet selected";
}
