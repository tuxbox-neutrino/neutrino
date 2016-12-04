/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2011-2012 Stefan Seyfried

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
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gui/scan.h>
#include <gui/scan_setup.h>

#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/record.h>
#include <driver/volume.h>
#include <driver/display.h>

#include <gui/color.h>

#include <gui/widget/menue.h>
#include <gui/widget/msgbox.h>
#include <gui/components/cc.h>
#include <gui/movieplayer.h>

#include <system/settings.h>
#include <system/helpers.h>

#include <global.h>
#include <neutrino.h>

#include <zapit/femanager.h>
#include <zapit/scan.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>
#include <video.h>

extern cVideo * videoDecoder;

#define NEUTRINO_SCAN_START_SCRIPT	CONFIGDIR "/scan.start"
#define NEUTRINO_SCAN_STOP_SCRIPT	CONFIGDIR "/scan.stop"

#define BAR_BORDER 2
#define BAR_WIDTH 150
#define BAR_HEIGHT 16//(13 + BAR_BORDER*2)

CScanTs::CScanTs(delivery_system_t DelSys)
{
	frameBuffer = CFrameBuffer::getInstance();
	radar = 0;
	total = done = 0;
	delsys = DelSys;
	signalbox = NULL;
	memset(&TP, 0, sizeof(TP)); // valgrind
}

CScanTs::~CScanTs()
{
}

void CScanTs::prev_next_TP( bool up)
{
	t_satellite_position position = 0;

	if (CFrontend::isSat(delsys))
		position = CServiceManager::getInstance()->GetSatellitePosition(scansettings.satName);
	else if (CFrontend::isCable(delsys))
		position = CServiceManager::getInstance()->GetSatellitePosition(scansettings.cableName);
	else if (CFrontend::isTerr(delsys))
		position = CServiceManager::getInstance()->GetSatellitePosition(scansettings.terrestrialName);

	transponder_list_t &select_transponders = CServiceManager::getInstance()->GetSatelliteTransponders(position);
	transponder_list_t::iterator tI;
	bool next_tp = false;

	/* FIXME transponders with duplicate frequency skipped */
	if(up) {
		for (tI = select_transponders.begin(); tI != select_transponders.end(); ++tI) {
			if(tI->second.feparams.frequency > TP.feparams.frequency){
				next_tp = true;
				break;
			}
		}
	} else {
		for (tI = select_transponders.end(); tI != select_transponders.begin();) {
			--tI;
			if(tI->second.feparams.frequency < TP.feparams.frequency) {
				next_tp = true;
				break;
			}
		}
	}

	if(next_tp) {
		TP.feparams = tI->second.feparams;
		testFunc();
	}
}

void CScanTs::testFunc()
{
	int w = x + width - xpos2;
	char buffer[128];
	const char *f, *s, *m, *f2;

	if (CFrontend::isSat(delsys)) {
		CFrontend::getDelSys(TP.feparams.delsys, TP.feparams.fec_inner, TP.feparams.modulation, f, s, m);
		snprintf(buffer,sizeof(buffer), "%u %c %d %s %s %s", TP.feparams.frequency/1000, transponder::pol(TP.feparams.polarization), TP.feparams.symbol_rate/1000, f, s, m);
	} else if (CFrontend::isCable(delsys)) {
		CFrontend::getDelSys(TP.feparams.delsys, TP.feparams.fec_inner, TP.feparams.modulation, f, s, m);
		snprintf(buffer,sizeof(buffer), "%u %d %s %s %s", TP.feparams.frequency/1000, TP.feparams.symbol_rate/1000, f, s, m);
	} else if (CFrontend::isTerr(delsys)) {
		CFrontend::getDelSys(TP.feparams.delsys, TP.feparams.code_rate_HP, TP.feparams.modulation, f, s, m);
		CFrontend::getDelSys(TP.feparams.delsys, TP.feparams.code_rate_LP, TP.feparams.modulation, f2, s, m);
		snprintf(buffer,sizeof(buffer), "%u %d %s %s %s %s", TP.feparams.frequency/1000, TP.feparams.bandwidth, f, f2, s, m);
	}

	printf("CScanTs::testFunc: %s\n", buffer);
	paintLine(xpos2, ypos_cur_satellite, w - 95, pname.c_str());
	paintLine(xpos2, ypos_frequency, w, buffer);
	paintRadar();
	success = g_Zapit->tune_TP(TP);
}

int CScanTs::exec(CMenuTarget* /*parent*/, const std::string & actionKey)
{
	printf("CScanTs::exec %s\n", actionKey.c_str());
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int scan_flags = 0;
	if(scansettings.scan_fta_flag)
		scan_flags |= CServiceScan::SCAN_FTA;
	if(scansettings.scan_bat)
		scan_flags |= CServiceScan::SCAN_BAT;
	if(scansettings.scan_reset_numbers)
		scan_flags |= CServiceScan::SCAN_RESET_NUMBERS;
	if(scansettings.scan_logical_numbers)
		scan_flags |= CServiceScan::SCAN_LOGICAL_NUMBERS;
	if(scansettings.scan_logical_hd)
		scan_flags |= CServiceScan::SCAN_LOGICAL_HD;

	/* channel types to scan, TV/RADIO/ALL */
	scan_flags |= scansettings.scanType;

	sat_iterator_t sit;
	bool scan_all = actionKey == "all";
	bool test = actionKey == "test";
	bool manual = (actionKey == "manual") || test;
	bool fast = (actionKey == "fast");

	if (CFrontend::isSat(delsys))
		pname = scansettings.satName;
	else if (CFrontend::isCable(delsys))
		pname = scansettings.cableName;
	else if (CFrontend::isTerr(delsys))
		pname = scansettings.terrestrialName;
	else
		printf("CScanTs::exec:%d unknown delivery_system %d\n", __LINE__, delsys);

	int scan_pids = CZapit::getInstance()->scanPids();

	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	fw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth();
	width       = w_max(fw * 42, 0);
	int tmp = (BAR_WIDTH + 4 + 7 * fw) * 2 + fw + 40; /* that's from the crazy calculation in showSNR() */
	if (width < tmp)
		width = w_max(tmp, 0);
	height      = h_max(hheight + (12 * mheight), 0); //9 lines
	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;
	xpos_radar = x + width - 20 - 64; /* TODO: don't assume radar is 64x64... */
	ypos_radar = y + hheight + (mheight >> 1);
	xpos1 = x + 10;

	if (!frameBuffer->getActive())
		return menu_return::RETURN_EXIT_ALL;

	CRecordManager::getInstance()->StopAutoRecord();
	CNeutrinoApp::getInstance()->stopPlayBack();
#ifdef ENABLE_PIP
	CZapit::getInstance()->StopPip();
#endif

	frameBuffer->paintBackground();
	frameBuffer->showFrame("scan.jpg");
	g_Sectionsd->setPauseScanning(true);

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);

//printf("[neutrino] scan_mode %d TP_freq %s TP_rate %s TP_fec %d TP_pol %d\n", scansettings.scan_mode, scansettings.TP_freq, scansettings.TP_rate, scansettings.TP_fec, scansettings.TP_pol);

	if(manual) {
		CZapit::getInstance()->scanPids(scan_pids);
		if(scansettings.scan_nit_manual)
			scan_flags |= CServiceScan::SCAN_NIT;
		TP.scan_mode = scan_flags;
		if (CFrontend::isSat(delsys)) {
			TP.feparams.frequency = atoi(scansettings.sat_TP_freq.c_str());
			TP.feparams.symbol_rate = atoi(scansettings.sat_TP_rate.c_str());
			TP.feparams.fec_inner = (fe_code_rate_t) scansettings.sat_TP_fec;
			TP.feparams.polarization = scansettings.sat_TP_pol;
			TP.feparams.delsys = (delivery_system_t)scansettings.sat_TP_delsys;
			TP.feparams.modulation = (fe_modulation_t) scansettings.sat_TP_mod;
			TP.feparams.pilot = (zapit_pilot_t) scansettings.sat_TP_pilot;
		} else if (CFrontend::isTerr(delsys)) {
			/* DVB-T. TODO: proper menu and parameter setup, not all "AUTO" */
			TP.feparams.frequency = atoi(scansettings.terrestrial_TP_freq.c_str());
//			if (TP.feparams.frequency < 300000)
//				TP.feparams.bandwidth	= BANDWIDTH_7_MHZ;
//			else
//				TP.feparams.bandwidth	= BANDWIDTH_8_MHZ;
			TP.feparams.bandwidth		= (fe_bandwidth_t)scansettings.terrestrial_TP_bw;
			TP.feparams.code_rate_HP	= (fe_code_rate_t)scansettings.terrestrial_TP_coderate_HP;
			TP.feparams.code_rate_LP	= (fe_code_rate_t)scansettings.terrestrial_TP_coderate_LP;
			TP.feparams.modulation		= (fe_modulation_t)scansettings.terrestrial_TP_constel;
			TP.feparams.transmission_mode	= (fe_transmit_mode_t)scansettings.terrestrial_TP_transmit_mode;
			TP.feparams.guard_interval	= (fe_guard_interval_t)scansettings.terrestrial_TP_guard;
			TP.feparams.hierarchy		= (fe_hierarchy_t)scansettings.terrestrial_TP_hierarchy;
			TP.feparams.delsys		= (delivery_system_t)scansettings.terrestrial_TP_delsys;
		} else if (CFrontend::isCable(delsys)) {
			TP.feparams.frequency	= atoi(scansettings.cable_TP_freq.c_str());
			TP.feparams.symbol_rate	= atoi(scansettings.cable_TP_rate.c_str());
			TP.feparams.fec_inner	= (fe_code_rate_t)scansettings.cable_TP_fec;
			TP.feparams.modulation	= (fe_modulation_t)scansettings.cable_TP_mod;
			TP.feparams.delsys	= (delivery_system_t)scansettings.cable_TP_delsys;
		}
		//printf("[neutrino] freq %d rate %d fec %d pol %d\n", TP.feparams.frequency, TP.feparams.symbol_rate, TP.feparams.fec_inner, TP.feparams.polarization);
	} else {
		if(scansettings.scan_nit)
			scan_flags |= CServiceScan::SCAN_NIT;
	}

	if (CFrontend::isCable(delsys))
		CServiceScan::getInstance()->SetCableNID(scansettings.cable_nid);

	CZapitClient::commandSetScanSatelliteList sat;
	memset(&sat, 0, sizeof(sat)); // valgrind
	CZapitClient::ScanSatelliteList satList;
	satList.clear();
	if(fast) {
	}
	else if(manual || !scan_all) {
		sat.position = CServiceManager::getInstance()->GetSatellitePosition(pname);
		strncpy(sat.satName, pname.c_str(), 49);
		satList.push_back(sat);
	} else {
		satellite_map_t & satmap = CServiceManager::getInstance()->SatelliteList();
		for(sit = satmap.begin(); sit != satmap.end(); ++sit) {
			if(sit->second.use_in_scan) {
				sat.position = sit->first;
				strncpy(sat.satName, sit->second.name.c_str(), 49);
				satList.push_back(sat);
			}
		}
	}
	success = false;

	if(!manual) {
		g_RCInput->close_click();
                if (my_system(NEUTRINO_SCAN_START_SCRIPT) != 0)
                	perror(NEUTRINO_SCAN_START_SCRIPT " failed");
	}

	g_Zapit->setScanBouquetMode( (CZapitClient::bouquetMode)scansettings.bouquetMode);

	/* send satellite list to zapit */
	if(!satList.empty())
		g_Zapit->setScanSatelliteList( satList);

	tuned = -1;
	paint(test);

	/* go */
	if(test) {
		testFunc();
	} else if(manual)
		success = g_Zapit->scan_TP(TP);
	else if(fast) {
		CServiceScan::getInstance()->QuietFastScan(false);
		success = CZapit::getInstance()->StartFastScan(scansettings.fast_type, scansettings.fast_op);
	}
	else
		success = g_Zapit->startScan(scan_flags);

	/* poll for messages */
	istheend = !success;
	while (!istheend) {
		paintRadar();

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS( 250 );

		do {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
			if (test && (msg == CRCInput::RC_down || msg == CRCInput::RC_left)) {
				prev_next_TP(false);
				continue;
			}
			else if (test && (msg == CRCInput::RC_up || msg == CRCInput::RC_right)) {
				prev_next_TP(true);
				continue;
			}
			else if (test && (msg <= CRCInput::RC_MaxRC)) {
				istheend = true;
				msg = CRCInput::RC_timeout;
			}
			
			else if(msg == CRCInput::RC_home) {
				if(manual && !scansettings.scan_nit_manual)
					continue;
				if (ShowMsg(LOCALE_SCANTS_ABORT_HEADER, LOCALE_SCANTS_ABORT_BODY, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes) {
					g_Zapit->stopScan();
				}
			}
			else
				msg = handleMsg(msg, data);
		}
		while (!(msg == CRCInput::RC_timeout));
		showSNR();
	}
	/* to join scan thread */
	g_Zapit->stopScan();

	if(!manual) {
                if (my_system(NEUTRINO_SCAN_STOP_SCRIPT) != 0)
                	perror(NEUTRINO_SCAN_STOP_SCRIPT " failed");
		g_RCInput->open_click();
	}
	if(!test) {
		CComponentsHeaderLocalized header(x, y, width, hheight, success ? LOCALE_SCANTS_FINISHED : LOCALE_SCANTS_FAILED);
		header.paint(CC_SAVE_SCREEN_NO);
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(0xFFFF);
		do {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
			if ( msg <= CRCInput::RC_MaxRC )
				msg = CRCInput::RC_timeout;
			else
				CNeutrinoApp::getInstance()->handleMsg( msg, data );
		} while (!(msg == CRCInput::RC_timeout));
	}

	delete signalbox;
	signalbox = NULL;
	hide();

	CZapit::getInstance()->scanPids(scan_pids);
	frameBuffer->stopFrame();
	frameBuffer->Clear();
	g_Sectionsd->setPauseScanning(false);
	if (CNeutrinoApp::getInstance()->channelList)
		CNeutrinoApp::getInstance()->channelList->zapTo_ChannelID(CZapit::getInstance()->GetCurrentChannelID(), true); /* force re-zap */
	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);

	return menu_return::RETURN_REPAINT;
}

/* this is not type "int", because it does not return a code indicating success but
 * instead returns altered "msg". This is different ot all other "handleMsg" functions
 * and should probably be fixed somewhen... */
neutrino_msg_t CScanTs::handleMsg(neutrino_msg_t msg, neutrino_msg_data_t data)
{
	int w = x + width - xpos2;
//printf("CScanTs::handleMsg: x %d xpos2 %d width %d w %d\n", x, xpos2, width, w);
	char buffer[128];
	char str[256];
	switch (msg) {
		case NeutrinoMessages::EVT_SCAN_SATELLITE:
			paintLine(xpos2, ypos_cur_satellite, w - (8*fw), (char *)data);
			break;

		case NeutrinoMessages::EVT_SCAN_NUM_TRANSPONDERS:
			sprintf(buffer, "%ld", data);
			paintLine(xpos2, ypos_transponder, w - (8*fw), buffer);
			total = data;
			snprintf(str, sizeof(buffer), "scan: %d/%d", done, total);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
			break;

		case NeutrinoMessages::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS:
			//if (total == 0) data = 0; // why ??
			done = data;
			sprintf(buffer, "%d/%d", done, total);
			paintLine(xpos2, ypos_transponder, w - (8*fw), buffer);
			snprintf(str, sizeof(buffer), "scan %d/%d", done, total);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
			break;

		case NeutrinoMessages::EVT_SCAN_REPORT_FREQUENCYP:
			{
				FrontendParameters *feparams = (FrontendParameters*) data;
				const char *f, *s, *m;

				CFrontend::getDelSys(feparams->delsys, feparams->fec_inner, feparams->modulation,  f, s, m);
				uint32_t freq = feparams->frequency/1000;
				if (CFrontend::isSat(feparams->delsys))
					snprintf(buffer,sizeof(buffer), "%u %c %d %s %s %s", freq, transponder::pol(feparams->polarization), feparams->symbol_rate/1000, f, s, m);
				else
					snprintf(buffer,sizeof(buffer), "%u %d %s %s", freq, feparams->symbol_rate/1000, s, m);
				paintLine(xpos2, ypos_frequency, w - (7*fw), buffer);
			}
			break;

		case NeutrinoMessages::EVT_SCAN_PROVIDER:
			paintLine(xpos2, ypos_provider, w, (char*)data); // UTF-8
			break;

		case NeutrinoMessages::EVT_SCAN_SERVICENAME:
			paintLine(xpos2, ypos_channel, w, (char *)data); // UTF-8
			break;

		case NeutrinoMessages::EVT_SCAN_NUM_CHANNELS:
			sprintf(buffer, " = %ld", data);
			paintLine(xpos1 + 3 * (6*fw), ypos_service_numbers + mheight, width - 3 * (6*fw) - 10, buffer);
			break;

		case NeutrinoMessages::EVT_SCAN_FOUND_TV_CHAN:
			sprintf(buffer, "%ld", data);
			paintLine(xpos1, ypos_service_numbers + mheight, (6*fw), buffer);
			break;

		case NeutrinoMessages::EVT_SCAN_FOUND_RADIO_CHAN:
			sprintf(buffer, "%ld", data);
			paintLine(xpos1 + (6*fw), ypos_service_numbers + mheight, (6*fw), buffer);
			break;

		case NeutrinoMessages::EVT_SCAN_FOUND_DATA_CHAN:
			sprintf(buffer, "%ld", data);
			paintLine(xpos1 + 2 * (6*fw), ypos_service_numbers + mheight, (6*fw), buffer);
			break;

		case NeutrinoMessages::EVT_SCAN_COMPLETE:
		case NeutrinoMessages::EVT_SCAN_FAILED:
			success = (msg == NeutrinoMessages::EVT_SCAN_COMPLETE);
			istheend = true;
			msg = CRCInput::RC_timeout;
			break;
		case CRCInput::RC_plus:
		case CRCInput::RC_minus:
		case CRCInput::RC_left:
		case CRCInput::RC_right:
			CVolume::getInstance()->setVolume(msg);
			break;
		default:
			break;
	}
	if ((msg >= CRCInput::RC_WithData) && (msg < CRCInput::RC_WithData + 0x10000000))
		delete[] (unsigned char*) data;
	return msg;
}

void CScanTs::paintRadar(void)
{
	char filename[30];

	CFrontend * frontend = CServiceScan::getInstance()->GetFrontend();
	bool status = frontend->getStatus();
	if(tuned != status) {
		tuned = status;
		frameBuffer->loadPal(tuned ? "radar.pal" : "radar_red.pal", 18, 38);
	}

	snprintf(filename,sizeof(filename), "radar%d.raw", radar);
	radar = (radar + 1) % 10;
	frameBuffer->paintIcon8(filename, xpos_radar, ypos_radar, 18);
}

void CScanTs::hide()
{
	//frameBuffer->loadPal("radiomode.pal", 18, COL_MAXFREE);
	//frameBuffer->paintBackgroundBoxRel(0, 0, 720, 576);
	frameBuffer->paintBackground();
}

void CScanTs::paintLineLocale(int px, int * py, int pwidth, const neutrino_locale_t l)
{
	frameBuffer->paintBoxRel(px, *py, pwidth, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(px+2, *py + mheight, pwidth, g_Locale->getText(l), COL_MENUCONTENTINACTIVE_TEXT);
	*py += mheight;
}

void CScanTs::paintLine(int px, int py, int w, const char * const txt)
{
//printf("CScanTs::paintLine x %d y %d w %d width %d xpos2 %d: %s\n", px, py, w, width, xpos2, txt);
	frameBuffer->paintBoxRel(px, py, w, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(px+2, py + mheight, w, txt, COL_MENUCONTENT_TEXT);
}

void CScanTs::paint(bool fortest)
{
	CComponentsHeaderLocalized header(x, y, width, hheight, fortest ? LOCALE_SCANTS_TEST : LOCALE_SCANTS_HEAD);
	header.setCaptionAlignment(CTextBox::CENTER);
	header.paint(CC_SAVE_SCREEN_NO);

	frameBuffer->paintBoxRel(x, y + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	frameBuffer->loadPal(tuned ? "radar.pal" : "radar_red.pal", 18, 38);

	int ypos = y + hheight + (mheight >> 1);

	ypos_cur_satellite = ypos;

	if(CFrontend::isSat(delsys))
	{	//sat
		paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_ACTSATELLITE);
		xpos2 = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCANTS_ACTSATELLITE))+2;
	}
	else if(CFrontend::isCable(delsys))
	{	//cable
		paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_ACTCABLE);
		xpos2 = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCANTS_ACTCABLE))+2;
	}
	else if(CFrontend::isTerr(delsys))
	{	//terrestrial
		paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_ACTTERRESTRIAL);
		xpos2 = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCANTS_ACTTERRESTRIAL))+2;
	}

	ypos_transponder = ypos;
	paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_TRANSPONDERS);
	xpos2 = greater_xpos(xpos2, LOCALE_SCANTS_TRANSPONDERS);

	ypos_frequency = ypos;
	paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_FREQDATA);
	xpos2 = greater_xpos(xpos2, LOCALE_SCANTS_FREQDATA);

	ypos += mheight >> 1; // 1/2 blank line

	ypos_provider = ypos;
	paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_PROVIDER);
	xpos2 = greater_xpos(xpos2, LOCALE_SCANTS_PROVIDER);

	ypos_channel = ypos;
	paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_CHANNEL);
	xpos2 = greater_xpos(xpos2, LOCALE_SCANTS_CHANNEL);

	ypos += mheight >> 1; // 1/2 blank line

	ypos_service_numbers = ypos; paintLineLocale(xpos1         	,&ypos, (6*fw)			, LOCALE_SCANTS_NUMBEROFTVSERVICES   );
	ypos = ypos_service_numbers; paintLineLocale(xpos1 +     (6*fw), &ypos, (6*fw)			, LOCALE_SCANTS_NUMBEROFRADIOSERVICES);
	ypos = ypos_service_numbers; paintLineLocale(xpos1 + 2 * (6*fw), &ypos, (6*fw)			, LOCALE_SCANTS_NUMBEROFDATASERVICES );
	ypos = ypos_service_numbers; paintLineLocale(xpos1 + 3 * (6*fw), &ypos, width - 3 * (6*fw) - 10	, LOCALE_SCANTS_NUMBEROFTOTALSERVICES);
}

int CScanTs::greater_xpos(int xpos, const neutrino_locale_t txt)
{
	int txt_xpos = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(txt))+2;
	if (txt_xpos > xpos)
		return txt_xpos;
	else
		return xpos;
}

void CScanTs::showSNR ()
{
	if (signalbox == NULL){
		CFrontend * frontend = CServiceScan::getInstance()->GetFrontend();
		//signalbox = new CSignalBox(xpos1, y + height - mheight - 5, width - 2*(xpos1-x), g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), frontend, false);
		signalbox = new CSignalBox(xpos1, y + height - (mheight*2*3)/2 - 5, width - 2*(xpos1-x), (g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()*2*3)/2, frontend, true);
		signalbox->setColorBody(COL_MENUCONTENT_PLUS_0);
		signalbox->setTextColor(COL_MENUCONTENT_TEXT);
		signalbox->doPaintBg(true);
	}

	signalbox->paint(false);
}
