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
#include <gui/widget/messagebox.h>
#include <gui/components/cc_frm.h>
#include <gui/components/cc_item_progressbar.h>

#include <system/settings.h>
#include <system/helpers.h>

#include <global.h>
#include <neutrino.h>

#include <gui/customcolor.h>

#include <zapit/femanager.h>
#include <zapit/scan.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>
#include <video.h>
extern cVideo * videoDecoder;

#define NEUTRINO_SCAN_START_SCRIPT	CONFIGDIR "/scan.start"
#define NEUTRINO_SCAN_STOP_SCRIPT	CONFIGDIR "/scan.stop"
#define NEUTRINO_SCAN_SETTINGS_FILE	CONFIGDIR "/scan.conf"

#define BAR_BORDER 2
#define BAR_WIDTH 150
#define BAR_HEIGHT 16//(13 + BAR_BORDER*2)

CScanTs::CScanTs(int dtype)
{
	frameBuffer = CFrameBuffer::getInstance();
	radar = 0;
	total = done = 0;
	freqready = 0;

	deltype = dtype;
	sigscale = new CProgressBar();
	sigscale->setBlink();
	
	snrscale = new CProgressBar();
	snrscale->setBlink();
	memset(&TP, 0, sizeof(TP)); // valgrind
}

CScanTs::~CScanTs()
{
	delete sigscale;
	delete snrscale;
}

void CScanTs::prev_next_TP( bool up)
{
	t_satellite_position position = 0;

	if (deltype == FE_QPSK)
		position = CServiceManager::getInstance()->GetSatellitePosition(scansettings.satName);
	else
		position = CServiceManager::getInstance()->GetSatellitePosition(scansettings.cableName);

	transponder_list_t &select_transponders = CServiceManager::getInstance()->GetSatelliteTransponders(position);
	transponder_list_t::iterator tI;
	bool next_tp = false;

	/* FIXME transponders with duplicate frequency skipped */
	if(up) {
		for (tI = select_transponders.begin(); tI != select_transponders.end(); ++tI) {
			if(tI->second.feparams.dvb_feparams.frequency > TP.feparams.dvb_feparams.frequency){
				next_tp = true;
				break;
			}
		}
	} else {
		for (tI = select_transponders.end(); tI != select_transponders.begin();) {
			--tI;
			if(tI->second.feparams.dvb_feparams.frequency < TP.feparams.dvb_feparams.frequency) {
				next_tp = true;
				break;
			}
		}
	}

	if(next_tp) {
		TP.feparams.dvb_feparams.frequency = tI->second.feparams.dvb_feparams.frequency;
		if (deltype == FE_QPSK) {
			TP.feparams.dvb_feparams.u.qpsk.symbol_rate = tI->second.feparams.dvb_feparams.u.qpsk.symbol_rate;
			TP.feparams.dvb_feparams.u.qpsk.fec_inner =   tI->second.feparams.dvb_feparams.u.qpsk.fec_inner;
			TP.polarization = tI->second.polarization;
		} else if (deltype == FE_OFDM) {
			/* DVB-T. TODO: proper menu and parameter setup, not all "AUTO" */
			if (TP.feparams.dvb_feparams.frequency < 300000)
				TP.feparams.dvb_feparams.u.ofdm.bandwidth	= BANDWIDTH_7_MHZ;
			else
				TP.feparams.dvb_feparams.u.ofdm.bandwidth	= BANDWIDTH_8_MHZ;
			TP.feparams.dvb_feparams.u.ofdm.code_rate_HP	= FEC_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.code_rate_LP	= FEC_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.constellation	= QAM_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.transmission_mode = TRANSMISSION_MODE_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.guard_interval	= GUARD_INTERVAL_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.hierarchy_information = HIERARCHY_AUTO;
		} else {
			TP.feparams.dvb_feparams.u.qam.symbol_rate	= tI->second.feparams.dvb_feparams.u.qam.symbol_rate;
			TP.feparams.dvb_feparams.u.qam.fec_inner	= tI->second.feparams.dvb_feparams.u.qam.fec_inner;
			TP.feparams.dvb_feparams.u.qam.modulation	= tI->second.feparams.dvb_feparams.u.qam.modulation;
		}
		testFunc();
	}
}

void CScanTs::testFunc()
{
	int w = x + width - xpos2;
	char buffer[128];
	char * f, *s, *m;

	if(deltype == FE_QPSK) {
		CFrontend::getDelSys(deltype, TP.feparams.dvb_feparams.u.qpsk.fec_inner, dvbs_get_modulation((fe_code_rate_t)TP.feparams.dvb_feparams.u.qpsk.fec_inner), f, s, m);
		snprintf(buffer,sizeof(buffer), "%u %c %d %s %s %s", TP.feparams.dvb_feparams.frequency/1000, transponder::pol(TP.polarization), TP.feparams.dvb_feparams.u.qpsk.symbol_rate/1000, f, s, m);
	} else if (deltype == FE_QAM) {
		CFrontend::getDelSys(deltype, TP.feparams.dvb_feparams.u.qam.fec_inner, TP.feparams.dvb_feparams.u.qam.modulation, f, s, m);
		snprintf(buffer,sizeof(buffer), "%u %d %s %s %s", TP.feparams.dvb_feparams.frequency/1000, TP.feparams.dvb_feparams.u.qam.symbol_rate/1000, f, s, m);
	} else if (deltype == FE_OFDM) {
		sprintf(buffer, "%u", TP.feparams.dvb_feparams.frequency); /* no way int can overflow the buffer */
	}
printf("CScanTs::testFunc: %s\n", buffer);
	paintLine(xpos2, ypos_cur_satellite, w - 95, pname);
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

	switch (deltype) {
		case FE_QPSK:	pname = scansettings.satName;	break;
		case FE_QAM:	pname = scansettings.cableName;	break;
		case FE_OFDM:	pname = scansettings.terrName;	break;
		default:	printf("CScanTs::exec:%d unknown deltype %d\n", __LINE__, deltype);
	}

	int scan_pids = CZapit::getInstance()->scanPids();

	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	fw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth();
	width       = w_max(fw * 42, 0);
	int tmp = (BAR_WIDTH + 4 + 7 * fw) * 2 + fw + 40; /* that's from the crazy calculation in showSNR() */
	if (width < tmp)
		width = w_max(tmp, 0);
	height      = h_max(hheight + (10 * mheight), 0); //9 lines
	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;
	xpos_radar = x + width - 20 - 64; /* TODO: don't assume radar is 64x64... */
	ypos_radar = y + hheight + (mheight >> 1);
	xpos1 = x + 10;

	sigscale->reset();
	snrscale->reset();
	lastsig = lastsnr = -1;

	if (!frameBuffer->getActive())
		return menu_return::RETURN_EXIT_ALL;

	CRecordManager::getInstance()->StopAutoRecord();
	g_Zapit->stopPlayBack();
#ifdef ENABLE_PIP
	CZapit::getInstance()->StopPip();
#endif

	frameBuffer->paintBackground();
	videoDecoder->ShowPicture(DATADIR "/neutrino/icons/scan.jpg");
	g_Sectionsd->setPauseScanning(true);

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);

//printf("[neutrino] scan_mode %d TP_freq %s TP_rate %s TP_fec %d TP_pol %d\n", scansettings.scan_mode, scansettings.TP_freq, scansettings.TP_rate, scansettings.TP_fec, scansettings.TP_pol);

	if(manual) {
		CZapit::getInstance()->scanPids(true);
		if(scansettings.scan_nit_manual)
			scan_flags |= CServiceScan::SCAN_NIT;
		TP.scan_mode = scan_flags;
		if (deltype == FE_QPSK) {
			TP.feparams.dvb_feparams.frequency = atoi(scansettings.sat_TP_freq);
			TP.feparams.dvb_feparams.u.qpsk.symbol_rate = atoi(scansettings.sat_TP_rate);
			TP.feparams.dvb_feparams.u.qpsk.fec_inner = (fe_code_rate_t) scansettings.sat_TP_fec;
			TP.polarization = scansettings.sat_TP_pol;
		} else if (deltype == FE_OFDM) {
			/* DVB-T. TODO: proper menu and parameter setup, not all "AUTO" */
			TP.feparams.dvb_feparams.frequency = atoi(scansettings.terr_TP_freq);
			if (TP.feparams.dvb_feparams.frequency < 300000)
				TP.feparams.dvb_feparams.u.ofdm.bandwidth	= BANDWIDTH_7_MHZ;
			else
				TP.feparams.dvb_feparams.u.ofdm.bandwidth	= BANDWIDTH_8_MHZ;
			TP.feparams.dvb_feparams.u.ofdm.code_rate_HP	= FEC_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.code_rate_LP	= FEC_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.constellation	= QAM_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.transmission_mode = TRANSMISSION_MODE_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.guard_interval	= GUARD_INTERVAL_AUTO;
			TP.feparams.dvb_feparams.u.ofdm.hierarchy_information = HIERARCHY_AUTO;
		} else {
			TP.feparams.dvb_feparams.frequency = atoi(scansettings.cable_TP_freq);
			TP.feparams.dvb_feparams.u.qam.symbol_rate	= atoi(scansettings.cable_TP_rate);
			TP.feparams.dvb_feparams.u.qam.fec_inner	= (fe_code_rate_t)scansettings.cable_TP_fec;
			TP.feparams.dvb_feparams.u.qam.modulation	= (fe_modulation_t) scansettings.cable_TP_mod;
		}
		//printf("[neutrino] freq %d rate %d fec %d pol %d\n", TP.feparams.dvb_feparams.frequency, TP.feparams.dvb_feparams.u.qpsk.symbol_rate, TP.feparams.dvb_feparams.u.qpsk.fec_inner, TP.polarization);
	} else {
		if(scansettings.scan_nit)
			scan_flags |= CServiceScan::SCAN_NIT;
	}
	if(deltype == FE_QAM)
		CServiceScan::getInstance()->SetCableNID(scansettings.cable_nid);

	CZapitClient::commandSetScanSatelliteList sat;
	memset(&sat, 0, sizeof(sat)); // valgrind
	CZapitClient::ScanSatelliteList satList;
	satList.clear();
	if(fast) {
	}
	else if(manual || !scan_all) {
		sat.position = CServiceManager::getInstance()->GetSatellitePosition(pname);
		strncpy(sat.satName, pname, 49);
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
	frameBuffer->blit();

	/* go */
	if(test) {
		testFunc();
	} else if(manual)
		success = g_Zapit->scan_TP(TP);
	else if(fast) {
//		success = CZapit::getInstance()->StartFastScan(scansettings.fast_type, scansettings.fast_op);
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
				if (ShowLocalizedMessage(LOCALE_SCANTS_ABORT_HEADER, LOCALE_SCANTS_ABORT_BODY, CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo) == CMessageBox::mbrYes) {
					g_Zapit->stopScan();
				}
			}
			else
				msg = handleMsg(msg, data);
		}
		while (!(msg == CRCInput::RC_timeout));
		showSNR();
		frameBuffer->blit();
	}
	/* to join scan thread */
	g_Zapit->stopScan();

	if(!manual) {
                if (my_system(NEUTRINO_SCAN_STOP_SCRIPT) != 0)
                	perror(NEUTRINO_SCAN_STOP_SCRIPT " failed");
		g_RCInput->open_click();
	}
	if(!test) {
		CComponentsHeader header(x, y, width, hheight, success ? LOCALE_SCANTS_FINISHED : LOCALE_SCANTS_FAILED, NULL /*no header icon*/);
		header.paint(CC_SAVE_SCREEN_NO);
		// frameBuffer->blit(); // ??
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(0xFFFF);
		do {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
			if ( msg <= CRCInput::RC_MaxRC )
				msg = CRCInput::RC_timeout;
			else
				CNeutrinoApp::getInstance()->handleMsg( msg, data );
		} while (!(msg == CRCInput::RC_timeout));
	}

	hide();

	CZapit::getInstance()->scanPids(scan_pids);
	videoDecoder->StopPicture();
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

		case NeutrinoMessages::EVT_SCAN_REPORT_FREQUENCY:
			freqready = 1;
			sprintf(buffer, "%lu", data);
			xpos_frequency = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(buffer, true);
			paintLine(xpos2, ypos_frequency, xpos_frequency, buffer);
			//paintRadar();
			break;

		case NeutrinoMessages::EVT_SCAN_REPORT_FREQUENCYP:
			{
				int pol = data & 0xFF;
				int fec = (data >> 8) & 0xFF;
				int rate = data >> 16;
				char * f, *s, *m;
				CFrontend * frontend = CServiceScan::getInstance()->GetFrontend();
				frontend->getDelSys(fec, (fe_modulation_t)0, f, s, m); // FIXME
				snprintf(buffer,sizeof(buffer), " %c %d %s %s %s", transponder::pol(pol), rate, f, s, m);
				paintLine(xpos2 + xpos_frequency, ypos_frequency, w - xpos_frequency - (7*fw), buffer);
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
	/* almost all messages paint something, so blit here */
	frameBuffer->blit();
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
	freqready = 0;
}

void CScanTs::paintLineLocale(int px, int * py, int pwidth, const neutrino_locale_t l)
{
	frameBuffer->paintBoxRel(px, *py, pwidth, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(px, *py + mheight, pwidth, g_Locale->getText(l), COL_MENUCONTENTINACTIVE, 0, true); // UTF-8
	*py += mheight;
}

void CScanTs::paintLine(int px, int py, int w, const char * const txt)
{
//printf("CScanTs::paintLine x %d y %d w %d width %d xpos2 %d: %s\n", px, py, w, width, xpos2, txt);
	frameBuffer->paintBoxRel(px, py, w, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(px, py + mheight, w, txt, COL_MENUCONTENT, 0, true); // UTF-8
}

void CScanTs::paint(bool fortest)
{
	CComponentsHeader header(x, y, width, hheight, fortest ? LOCALE_SCANTS_TEST : LOCALE_SCANTS_HEAD, NULL /*no header icon*/);
	header.paint(CC_SAVE_SCREEN_NO);

	frameBuffer->paintBoxRel(x, y + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	frameBuffer->loadPal(tuned ? "radar.pal" : "radar_red.pal", 18, 38);

	int ypos = y + hheight + (mheight >> 1);

	ypos_cur_satellite = ypos;

	if(deltype == FE_QPSK)
	{	//sat
		paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_ACTSATELLITE);
		xpos2 = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCANTS_ACTSATELLITE), true); // UTF-8
	}
	if(deltype == FE_QAM)
	{	//cable
		paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_ACTCABLE);
		xpos2 = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCANTS_ACTCABLE), true); // UTF-8
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
	int txt_xpos = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(txt), true); // UTF-8
	if (txt_xpos > xpos)
		return txt_xpos;
	else
		return xpos;
}

void CScanTs::showSNR ()
{
	char percent[10];
	int barwidth = BAR_WIDTH;
	uint16_t ssig, ssnr;
	int sig, snr;
	int posx, posy;
	int sw;

	CFrontend * frontend = CServiceScan::getInstance()->GetFrontend();
	ssig = frontend->getSignalStrength();
	ssnr = frontend->getSignalNoiseRatio();
	snr = (ssnr & 0xFFFF) * 100 / 65535;
	sig = (ssig & 0xFFFF) * 100 / 65535;

	posy = y + height - mheight - 5;
	//TODO: move sig/snr display into its own class, similar or same code also to find in motorcontrol
	if (lastsig != sig) { 
		lastsig = sig;
		posx = x + 20;
		sprintf(percent, "%d%%", sig);
		sw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("100%");
		sigscale->setProgress(posx - 1, posy+2, BAR_WIDTH, BAR_HEIGHT, sig, 100);
		sigscale->paint();

		posx = posx + barwidth + 3;
		frameBuffer->paintBoxRel(posx, posy -1, sw, mheight-8, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx+2, posy + mheight-(mheight-BAR_HEIGHT)/4, sw, percent, COL_MENUCONTENTDARK);

		frameBuffer->paintBoxRel(posx+(4*fw), posy - 2, 4*fw, mheight, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx+2+(4*fw), posy + mheight-(mheight-BAR_HEIGHT)/4, 4*fw, "SIG", COL_MENUCONTENT);

	}
	if (lastsnr != snr) {
		lastsnr = snr;
		posx = x + 20 + barwidth + 3 + 4 * fw + 4 * fw;
		sprintf(percent, "%d%%", snr);
		sw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("100%");
		snrscale->setProgress(posx - 1, posy+2, BAR_WIDTH, BAR_HEIGHT, snr, 100);
		snrscale->paint();

		posx = posx + barwidth + 3;
		frameBuffer->paintBoxRel(posx, posy - 1, sw, mheight-8, COL_MENUCONTENT_PLUS_0, 0, true);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx + 2, posy + mheight-(mheight-BAR_HEIGHT)/4, sw, percent, COL_MENUCONTENTDARK, 0, true);

		frameBuffer->paintBoxRel(posx+(4*fw), posy - 2, 4*fw, mheight, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx+2+(4*fw), posy + mheight-(mheight-BAR_HEIGHT)/4, 4*fw, "SNR", COL_MENUCONTENT);
		
	}
}
