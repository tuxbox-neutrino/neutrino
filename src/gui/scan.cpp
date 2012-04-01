/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2011 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
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
#include "gui/scan_setup.h"

#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/record.h>
#include <driver/volume.h>

#include <gui/color.h>

#include <gui/widget/menue.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/progressbar.h>

#include <system/settings.h>
#include <system/safe_system.h>

#include <global.h>
#include <neutrino.h>

#include <gui/customcolor.h>

#include <zapit/satconfig.h>
#include <zapit/frontend_c.h>
#include <zapit/zapit.h>
#include <video.h>
extern cVideo * videoDecoder;

#define NEUTRINO_SCAN_START_SCRIPT	CONFIGDIR "/scan.start"
#define NEUTRINO_SCAN_STOP_SCRIPT	CONFIGDIR "/scan.stop"
#define NEUTRINO_SCAN_SETTINGS_FILE	CONFIGDIR "/scan.conf"

#define BAR_BORDER 2
#define BAR_WIDTH 150
#define BAR_HEIGHT 16//(13 + BAR_BORDER*2)

CScanTs::CScanTs()
{
	frameBuffer = CFrameBuffer::getInstance();
	radar = 0;
	total = done = 0;
	freqready = 0;

	sigscale = new CProgressBar(true, BAR_WIDTH, BAR_HEIGHT);
	snrscale = new CProgressBar(true, BAR_WIDTH, BAR_HEIGHT);
}

extern int scan_fta_flag;//in zapit descriptors definiert
//extern int start_fast_scan(int scan_mode, int opid);
#include <zapit/getservices.h>

void CScanTs::prev_next_TP( bool up)
{
	t_satellite_position position = 0;

	for (sat_iterator_t sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
		if (!strcmp(sit->second.name.c_str(), scansettings.satNameNoDiseqc)) {
			position = sit->first;
			break;
		}
	}

	extern std::map<transponder_id_t, transponder> select_transponders;
	transponder_list_t::iterator tI;
	bool next_tp = false;

	if(up){
		for (tI = select_transponders.begin(); tI != select_transponders.end(); tI++) {
			t_satellite_position satpos = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xFFF;
			if (GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xF000)
				satpos = -satpos;
			if (satpos != position)
				continue;
			if(tI->second.feparams.frequency > TP.feparams.frequency){
				next_tp = true;
				break;
			}
		}
	}else{
		for ( tI=select_transponders.end() ; tI != select_transponders.begin(); tI-- ){
			t_satellite_position satpos = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xFFF;
			if (GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xF000)
				satpos = -satpos;
			if (satpos != position)
				continue;
			if(tI->second.feparams.frequency < TP.feparams.frequency){
				next_tp = true;
				break;
			}
		}
	}

	if(next_tp){
		TP.feparams.frequency = tI->second.feparams.frequency;
		if(g_info.delivery_system == DVB_S) {
			TP.feparams.u.qpsk.symbol_rate = tI->second.feparams.u.qpsk.symbol_rate;
			TP.feparams.u.qpsk.fec_inner =   tI->second.feparams.u.qpsk.fec_inner;
			TP.polarization = tI->second.polarization;
		} else {
			TP.feparams.u.qam.symbol_rate	= tI->second.feparams.u.qam.symbol_rate;
			TP.feparams.u.qam.fec_inner	= tI->second.feparams.u.qam.fec_inner;
			TP.feparams.u.qam.modulation	= tI->second.feparams.u.qam.modulation;
		}
		testFunc();
	}
}

void CScanTs::testFunc()
{
	int w = x + width - xpos2;
	char buffer[128];
	char * f, *s, *m;
	if(CFrontend::getInstance()->getInfo()->type == FE_QPSK) {
		CFrontend::getInstance()->getDelSys(TP.feparams.u.qpsk.fec_inner, dvbs_get_modulation((fe_code_rate_t)TP.feparams.u.qpsk.fec_inner), f, s, m);
		snprintf(buffer,sizeof(buffer), "%u %c %d %s %s %s", TP.feparams.frequency/1000, TP.polarization == 0 ? 'H' : 'V', TP.feparams.u.qpsk.symbol_rate/1000, f, s, m);
	} else if(CFrontend::getInstance()->getInfo()->type == FE_QAM) {
		CFrontend::getInstance()->getDelSys(scansettings.TP_fec, scansettings.TP_mod, f, s, m);
		snprintf(buffer,sizeof(buffer), "%u %d %s %s %s", atoi(scansettings.TP_freq)/1000, atoi(scansettings.TP_rate)/1000, f, s, m);
	}
	paintLine(xpos2, ypos_cur_satellite, w - 95, scansettings.satNameNoDiseqc);
	paintLine(xpos2, ypos_frequency, w, buffer);
	success = g_Zapit->tune_TP(TP);

}
int CScanTs::exec(CMenuTarget* /*parent*/, const std::string & actionKey)
{
	diseqc_t            diseqcType = NO_DISEQC;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	//bool manual = (scansettings.scan_mode == 2);
	int scan_mode = scansettings.scan_mode;
	scan_fta_flag = scansettings.scan_fta_flag;

	sat_iterator_t sit;
	bool scan_all = actionKey == "all";
	bool test = actionKey == "test";
	bool manual = (actionKey == "manual") || test;
	bool fast = (actionKey == "fast");

	CZapitClient::ScanSatelliteList satList;
	CZapitClient::commandSetScanSatelliteList sat;
	int _scan_pids = CZapit::getInstance()->scanPids();

	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	fw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth();
	width       = w_max(fw * 42, 0);
	height      = h_max(hheight + (10 * mheight), 0); //9 lines
	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;
	xpos_radar = x + 36 * fw;
	ypos_radar = y + hheight + (mheight >> 1);
	xpos1 = x + 10;

	if(scan_all)
		scan_mode |= 0xFF00;

	sigscale->reset();
	snrscale->reset();
	lastsig = lastsnr = -1;

	if (!frameBuffer->getActive())
		return menu_return::RETURN_EXIT_ALL;

	CRecordManager::getInstance()->StopAutoRecord();
	g_Zapit->stopPlayBack();
	frameBuffer->paintBackground();
	videoDecoder->ShowPicture(DATADIR "/neutrino/icons/scan.jpg");
	g_Sectionsd->setPauseScanning(true);

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);

//printf("[neutrino] scan_mode %d TP_freq %s TP_rate %s TP_fec %d TP_pol %d\n", scansettings.scan_mode, scansettings.TP_freq, scansettings.TP_rate, scansettings.TP_fec, scansettings.TP_pol);

	if(manual) {
		CZapit::getInstance()->scanPids(true);
		TP.scan_mode = scansettings.scan_mode;
		TP.feparams.frequency = atoi(scansettings.TP_freq);
		if(g_info.delivery_system == DVB_S) {
			TP.feparams.u.qpsk.symbol_rate = atoi(scansettings.TP_rate);
			TP.feparams.u.qpsk.fec_inner = (fe_code_rate_t) scansettings.TP_fec;
			TP.polarization = scansettings.TP_pol;
		} else {
			TP.feparams.u.qam.symbol_rate	= atoi(scansettings.TP_rate);
			TP.feparams.u.qam.fec_inner	= (fe_code_rate_t)scansettings.TP_fec;
			TP.feparams.u.qam.modulation	= (fe_modulation_t) scansettings.TP_mod;
		}
		//printf("[neutrino] freq %d rate %d fec %d pol %d\n", TP.feparams.frequency, TP.feparams.u.qpsk.symbol_rate, TP.feparams.u.qpsk.fec_inner, TP.polarization);
	}
	satList.clear();
	if(fast) {
	}
	else if(manual || !scan_all) {
		for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			if(!strcmp(sit->second.name.c_str(),scansettings.satNameNoDiseqc)) {
				sat.position = sit->first;
				strncpy(sat.satName, scansettings.satNameNoDiseqc, 50);
				satList.push_back(sat);
				break;
			}
		}
	} else {
		for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			if(sit->second.use_in_scan) {
				sat.position = sit->first;
				strncpy(sat.satName, sit->second.name.c_str(), 50);
				satList.push_back(sat);
			}
		}
	}
	success = false;

	if(!manual) {
		g_RCInput->close_click();
		if (safe_system(NEUTRINO_SCAN_START_SCRIPT) != 0)
                	perror(NEUTRINO_SCAN_START_SCRIPT " failed");
	}

	/* send diseqc type to zapit */
	diseqcType = (diseqc_t) scansettings.diseqcMode;
	g_Zapit->setDiseqcType(diseqcType);

	/* send diseqc repeat to zapit */
	g_Zapit->setDiseqcRepeat( scansettings.diseqcRepeat);
	g_Zapit->setScanBouquetMode( (CZapitClient::bouquetMode)scansettings.bouquetMode);

	/* send satellite list to zapit */
	if(satList.size())
		g_Zapit->setScanSatelliteList( satList);

        /* send scantype to zapit */
        g_Zapit->setScanType((CZapitClient::scanType) scansettings.scanType );

	tuned = CFrontend::getInstance()->getStatus();
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
		success = g_Zapit->startScan(scan_mode);

	/* poll for messages */
	istheend = !success;
	while (!istheend) {
		paintRadar();

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS( 250 );

		do {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
			if (test && (msg == CRCInput::RC_down)) {
				prev_next_TP(false);
				continue;
			}
			else if (test && (msg == CRCInput::RC_up)) {
				prev_next_TP(true);
				continue;
			}
			else if (test && (msg <= CRCInput::RC_MaxRC)) {
				istheend = true;
				msg = CRCInput::RC_timeout;
			}
			
			else if(msg == CRCInput::RC_home) {
				if(manual && scansettings.scan_mode)
					continue;
				if (ShowLocalizedMessage(LOCALE_SCANTS_ABORT_HEADER, LOCALE_SCANTS_ABORT_BODY, CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo) == CMessageBox::mbrYes) {
					g_Zapit->stopScan();
				}
			}
			else
				msg = handleMsg(msg, data);
		}
		while (!(msg == CRCInput::RC_timeout));
		showSNR(); // FIXME commented until scan slowdown will be solved
		frameBuffer->blit();
	}
	/* to join scan thread */
	g_Zapit->stopScan();

	if(!manual) {
		if (safe_system(NEUTRINO_SCAN_STOP_SCRIPT) != 0)
                	perror(NEUTRINO_SCAN_STOP_SCRIPT " failed");
		g_RCInput->open_click();
	}
	if(!test) {
		//ShowLocalizedMessage(LOCALE_MESSAGEBOX_INFO, success ? LOCALE_SCANTS_FINISHED : LOCALE_SCANTS_FAILED, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);

		const char * text = g_Locale->getText(success ? LOCALE_SCANTS_FINISHED : LOCALE_SCANTS_FAILED);
		//paintLine(xpos2, ypos_frequency, xpos_frequency, text);
		frameBuffer->paintBoxRel(x, y, width, hheight, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(xpos1, y + hheight, width, text, COL_MENUHEAD, 0, true); // UTF-8
		frameBuffer->blit();
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

	CZapit::getInstance()->scanPids(_scan_pids);
	videoDecoder->StopPicture();
	frameBuffer->Clear();
	g_Sectionsd->setPauseScanning(false);
	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);

	return menu_return::RETURN_REPAINT;
}

int CScanTs::handleMsg(neutrino_msg_t msg, neutrino_msg_data_t data)
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
			sprintf(buffer, "%d", data);
			paintLine(xpos2, ypos_transponder, w - (8*fw), buffer);
			total = data;
			snprintf(str, sizeof(buffer), "scan: %d/%d", done, total);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
			break;

		case NeutrinoMessages::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS:
			if (total == 0) data = 0;
			done = data;
			sprintf(buffer, "%d/%d", done, total);
			paintLine(xpos2, ypos_transponder, w - (8*fw), buffer);
			snprintf(str, sizeof(buffer), "scan %d/%d", done, total);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
			break;

		case NeutrinoMessages::EVT_SCAN_REPORT_FREQUENCY:
			freqready = 1;
			sprintf(buffer, "%u", data);
			xpos_frequency = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(buffer, true);
			paintLine(xpos2, ypos_frequency, xpos_frequency, buffer);
			paintRadar();
			break;

		case NeutrinoMessages::EVT_SCAN_REPORT_FREQUENCYP:
			{
				int pol = data & 0xFF;
				int fec = (data >> 8) & 0xFF;
				int rate = data >> 16;
				char * f, *s, *m;
				CFrontend::getInstance()->getDelSys(fec, (fe_modulation_t)0, f, s, m); // FIXME
				snprintf(buffer,sizeof(buffer), " %c %d %s %s %s", pol == 0 ? 'H' : 'V', rate, f, s, m);
				//(pol == 0) ? sprintf(buffer, "-H") : sprintf(buffer, "-V");
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
			sprintf(buffer, " = %d", data);
			paintLine(xpos1 + 3 * (6*fw), ypos_service_numbers + mheight, width - 3 * (6*fw) - 10, buffer);
			break;

		case NeutrinoMessages::EVT_SCAN_FOUND_TV_CHAN:
			sprintf(buffer, "%d", data);
			paintLine(xpos1, ypos_service_numbers + mheight, (6*fw), buffer);
			break;

		case NeutrinoMessages::EVT_SCAN_FOUND_RADIO_CHAN:
			sprintf(buffer, "%d", data);
			paintLine(xpos1 + (6*fw), ypos_service_numbers + mheight, (6*fw), buffer);
			break;

		case NeutrinoMessages::EVT_SCAN_FOUND_DATA_CHAN:
			sprintf(buffer, "%d", data);
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
			CVolume::getInstance()->setVolume(msg, true, true);
			break;
		default:
			if ((msg >= CRCInput::RC_WithData) && (msg < CRCInput::RC_WithData + 0x10000000))
				delete (unsigned char*) data;
			break;
	}
	return msg;
}

void CScanTs::paintRadar(void)
{
	char filename[30];

	if(tuned != CFrontend::getInstance()->getStatus()) {
		tuned = CFrontend::getInstance()->getStatus();
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
	int ypos;

	ypos = y;

	//frameBuffer->paintBoxRel(x, ypos, width, hheight, COL_MENUHEAD_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos, width, hheight, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(xpos1, ypos + hheight, width, fortest ? g_Locale->getText(LOCALE_SCANTS_TEST) : g_Locale->getText(LOCALE_SCANTS_HEAD), COL_MENUHEAD, 0, true); // UTF-8
	//frameBuffer->paintBoxRel(x, ypos + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	frameBuffer->loadPal(tuned ? "radar.pal" : "radar_red.pal", 18, 38);

	ypos = y + hheight + (mheight >> 1);

	ypos_cur_satellite = ypos;

	if (g_info.delivery_system == DVB_S)
	{	//sat
		paintLineLocale(xpos1, &ypos, width - xpos1, LOCALE_SCANTS_ACTSATELLITE);
		xpos2 = xpos1 + 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCANTS_ACTSATELLITE), true); // UTF-8
	}
	if (g_info.delivery_system == DVB_C)
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
	int barwidth = 150;
	uint16_t ssig, ssnr;
	int sig, snr;
	int posx, posy;
	int sw;

	ssig = CFrontend::getInstance()->getSignalStrength();
	ssnr = CFrontend::getInstance()->getSignalNoiseRatio();
	snr = (ssnr & 0xFFFF) * 100 / 65535;
	sig = (ssig & 0xFFFF) * 100 / 65535;

	posy = y + height - mheight - 5;

	if (lastsig != sig) {
		lastsig = sig;
		posx = x + 20;
		sprintf(percent, "%d%%", sig);
		sw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("100%");
		sigscale->paintProgressBar2(posx - 1, posy+2, sig);

		posx = posx + barwidth + 3;
		frameBuffer->paintBoxRel(posx, posy -1, sw, mheight-8, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx+2, posy + mheight-(mheight-BAR_HEIGHT)/4, sw, percent, COL_MENUCONTENTDARK);

		frameBuffer->paintBoxRel(posx+(4*fw), posy - 2, 4*fw, mheight, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx+2+(4*fw), posy + mheight-(mheight-BAR_HEIGHT)/4, 4*fw, "SIG", COL_MENUCONTENT);

	}
	if (lastsnr != snr) {
		lastsnr = snr;
		posx = x + 20 + (20 * fw);
		sprintf(percent, "%d%%", snr);
		sw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("100%");
		snrscale->paintProgressBar2(posx - 1, posy+2, snr); 

		posx = posx + barwidth + 3;
		frameBuffer->paintBoxRel(posx, posy - 1, sw, mheight-8, COL_MENUCONTENT_PLUS_0, 0, true);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx + 2, posy + mheight-(mheight-BAR_HEIGHT)/4, sw, percent, COL_MENUCONTENTDARK, 0, true);

		frameBuffer->paintBoxRel(posx+(4*fw), posy - 2, 4*fw, mheight, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx+2+(4*fw), posy + mheight-(mheight-BAR_HEIGHT)/4, 4*fw, "SNR", COL_MENUCONTENT);
		
	}
}
