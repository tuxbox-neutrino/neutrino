/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gui/scale.h>
#include <gui/scan.h>

#include <driver/rcinput.h>
#include <driver/screen_max.h>

#include <gui/color.h>

#include <gui/widget/menue.h>
#include <gui/widget/messagebox.h>

#include <system/settings.h>

#include <global.h>
#include <neutrino.h>

#include <gui/customcolor.h>

#include <zapit/satconfig.h>
#include <zapit/frontend_c.h>
#include <video_cs.h>
extern cVideo * videoDecoder;
extern CFrontend * frontend;

#define NEUTRINO_SCAN_START_SCRIPT	CONFIGDIR "/scan.start"
#define NEUTRINO_SCAN_STOP_SCRIPT	CONFIGDIR "/scan.stop"
#define NEUTRINO_SCAN_SETTINGS_FILE	CONFIGDIR "/scan.conf"
TP_params TP;

#define RED_BAR 40
#define YELLOW_BAR 70
#define GREEN_BAR 100
#define BAR_BORDER 2
#define BAR_WIDTH 150
#define BAR_HEIGHT 16//(13 + BAR_BORDER*2)

#define ROUND_RADIUS 9

CScanTs::CScanTs()
{
	frameBuffer = CFrameBuffer::getInstance();
	radar = 0;
	total = done = 0;
	freqready = 0;

	sigscale = new CScale(BAR_WIDTH, BAR_HEIGHT, RED_BAR, GREEN_BAR, YELLOW_BAR);
	snrscale = new CScale(BAR_WIDTH, BAR_HEIGHT, RED_BAR, GREEN_BAR, YELLOW_BAR);

}

extern int scan_pids;
#define get_set CNeutrinoApp::getInstance()->getScanSettings()
int CScanTs::exec(CMenuTarget* parent, const std::string & actionKey)
{
	diseqc_t            diseqcType = NO_DISEQC;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	//bool manual = (get_set.scan_mode == 2);
	int scan_mode = get_set.scan_mode;
	sat_iterator_t sit;
	bool scan_all = actionKey == "all";
	bool test = actionKey == "test";
	bool manual = (actionKey == "manual") || test;
	CZapitClient::ScanSatelliteList satList;
	CZapitClient::commandSetScanSatelliteList sat;
	int _scan_pids = scan_pids;

	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	width       = w_max(550, 0);
	height      = h_max(hheight + (10 * mheight), 0); //9 lines
	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;
	xpos_radar = x + 470;
	ypos_radar = y + hheight + (mheight >> 1);
	xpos1 = x + 10;

	if(scan_all)
		scan_mode |= 0xFF00;

	sigscale->reset();
	snrscale->reset();

	if (!frameBuffer->getActive())
		return menu_return::RETURN_EXIT_ALL;

        g_Zapit->stopPlayBack();
	frameBuffer->paintBackground();
	videoDecoder->ShowPicture(DATADIR "/neutrino/icons/scan.jpg");
	g_Sectionsd->setPauseScanning(true);

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8);

//printf("[neutrino] scan_mode %d TP_freq %s TP_rate %s TP_fec %d TP_pol %d\n", get_set.scan_mode, get_set.TP_freq, get_set.TP_rate, get_set.TP_fec, get_set.TP_pol);

	if(manual) {
		scan_pids = true;
		TP.scan_mode = get_set.scan_mode;
		TP.feparams.frequency = atoi(get_set.TP_freq);
		if(g_info.delivery_system == DVB_S) {
			TP.feparams.u.qpsk.symbol_rate = atoi(get_set.TP_rate);
			TP.feparams.u.qpsk.fec_inner = (fe_code_rate_t) get_set.TP_fec;
			TP.polarization = get_set.TP_pol;
		} else {
			TP.feparams.u.qam.symbol_rate	= atoi(get_set.TP_rate);
			TP.feparams.u.qam.fec_inner	= (fe_code_rate_t)get_set.TP_fec;
			TP.feparams.u.qam.modulation	= (fe_modulation_t) get_set.TP_mod;
		}
		//printf("[neutrino] freq %d rate %d fec %d pol %d\n", TP.feparams.frequency, TP.feparams.u.qpsk.symbol_rate, TP.feparams.u.qpsk.fec_inner, TP.polarization);
	} 
	satList.clear();
	if(manual || !scan_all) {
		for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			if(!strcmp(sit->second.name.c_str(),get_set.satNameNoDiseqc)) {
				sat.position = sit->first;
				strncpy(sat.satName, get_set.satNameNoDiseqc, 50);
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
                if (system(NEUTRINO_SCAN_START_SCRIPT) != 0)
                	perror(NEUTRINO_SCAN_START_SCRIPT " failed");
	}

	/* send diseqc type to zapit */
	diseqcType = (diseqc_t) CNeutrinoApp::getInstance()->getScanSettings().diseqcMode;
	g_Zapit->setDiseqcType(diseqcType);
	
	/* send diseqc repeat to zapit */
	g_Zapit->setDiseqcRepeat( CNeutrinoApp::getInstance()->getScanSettings().diseqcRepeat);
	g_Zapit->setScanBouquetMode( (CZapitClient::bouquetMode)CNeutrinoApp::getInstance()->getScanSettings().bouquetMode);

	/* send satellite list to zapit */
	g_Zapit->setScanSatelliteList( satList);

        /* send scantype to zapit */
        g_Zapit->setScanType((CZapitClient::scanType) CNeutrinoApp::getInstance()->getScanSettings().scanType );
	
	paint(test);
	/* go */
	if(test) {
		int w = x + width - xpos2;
		char buffer[128];
		char * f, *s, *m;
		if(frontend->getInfo()->type == FE_QPSK) {
			frontend->getDelSys(get_set.TP_fec, dvbs_get_modulation((fe_code_rate_t)get_set.TP_fec), f, s, m);
			sprintf(buffer, "%u %c %d %s %s %s", atoi(get_set.TP_freq)/1000, get_set.TP_pol == 0 ? 'H' : 'V', atoi(get_set.TP_rate)/1000, f, s, m);
		} else if(frontend->getInfo()->type == FE_QAM) {
			frontend->getDelSys(get_set.TP_fec, get_set.TP_mod, f, s, m);
			sprintf(buffer, "%u %d %s %s %s", atoi(get_set.TP_freq)/1000, atoi(get_set.TP_rate)/1000, f, s, m);
		}
		paintLine(xpos2, ypos_cur_satellite, w - 95, get_set.satNameNoDiseqc);
		paintLine(xpos2, ypos_frequency, w, buffer);
		success = g_Zapit->tune_TP(TP);
	} else if(manual)
		success = g_Zapit->scan_TP(TP);
	else
		success = g_Zapit->startScan(scan_mode);

	//paint();

	/* poll for messages */
	istheend = !success;
	while (!istheend) {
		paintRadar();

		unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd_MS( 250 );

		do {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
			if (test && (msg <= CRCInput::RC_MaxRC)) {
				istheend = true;
				msg = CRCInput::RC_timeout;
			}
			else if(msg == CRCInput::RC_home) {
				if(manual && get_set.scan_mode)
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
	}

	if(!manual) {
                if (system(NEUTRINO_SCAN_STOP_SCRIPT) != 0)
                	perror(NEUTRINO_SCAN_STOP_SCRIPT " failed");
		g_RCInput->open_click();
	}
	if(!test) {
		//ShowLocalizedMessage(LOCALE_MESSAGEBOX_INFO, success ? LOCALE_SCANTS_FINISHED : LOCALE_SCANTS_FAILED, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");

		const char * text = g_Locale->getText(success ? LOCALE_SCANTS_FINISHED : LOCALE_SCANTS_FAILED);
		//paintLine(xpos2, ypos_frequency, xpos_frequency, text);
		frameBuffer->paintBoxRel(x, y, width, hheight, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, 1);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(xpos1, y + hheight, width, text, COL_MENUHEAD, 0, true); // UTF-8
		unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(0xFFFF);
		do {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
			if ( msg <= CRCInput::RC_MaxRC )
				msg = CRCInput::RC_timeout;
			else
				CNeutrinoApp::getInstance()->handleMsg( msg, data );
		} while (!(msg == CRCInput::RC_timeout));
	}

	hide();
	
	scan_pids = _scan_pids;
	videoDecoder->StopPicture();
	frameBuffer->ClearFrameBuffer();
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
			paintLine(xpos2, ypos_cur_satellite, w - 95, (char *)data);
			break;
			
		case NeutrinoMessages::EVT_SCAN_NUM_TRANSPONDERS:
			sprintf(buffer, "%d", data);
			paintLine(xpos2, ypos_transponder, w - 95, buffer);
			total = data;
			snprintf(str, 255, "scan: %d/%d", done, total);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
			break;
			
		case NeutrinoMessages::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS:
			if (total == 0) data = 0;
			done = data;
			sprintf(buffer, "%d/%d", done, total);
			paintLine(xpos2, ypos_transponder, w - 95, buffer);
			snprintf(str, 255, "scan %d/%d", done, total);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
			break;

		case NeutrinoMessages::EVT_SCAN_REPORT_FREQUENCY:
			freqready = 1;
			sprintf(buffer, "%u", data);
			xpos_frequency = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(buffer, true);
			paintLine(xpos2, ypos_frequency, xpos_frequency, buffer);
			break;
			
		case NeutrinoMessages::EVT_SCAN_REPORT_FREQUENCYP: 
			{
				int pol = data & 0xFF;
				int fec = (data >> 8) & 0xFF;
				int rate = data >> 16;
				char * f, *s, *m;
				frontend->getDelSys(fec, (fe_modulation_t)0, f, s, m); // FIXME
				sprintf(buffer, " %c %d %s %s %s", pol == 0 ? 'H' : 'V', rate, f, s, m);
				//(pol == 0) ? sprintf(buffer, "-H") : sprintf(buffer, "-V");
				paintLine(xpos2 + xpos_frequency, ypos_frequency, w - xpos_frequency - 80, buffer);
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
			paintLine(xpos1 + 3 * 72, ypos_service_numbers + mheight, width - 3 * 72 - 10, buffer);
			break;
			
		case NeutrinoMessages::EVT_SCAN_FOUND_TV_CHAN:
			sprintf(buffer, "%d", data);
			paintLine(xpos1, ypos_service_numbers + mheight, 72, buffer);
			break;
			
		case NeutrinoMessages::EVT_SCAN_FOUND_RADIO_CHAN:
			sprintf(buffer, "%d", data);
			paintLine(xpos1 + 72, ypos_service_numbers + mheight, 72, buffer);
			break;
			
		case NeutrinoMessages::EVT_SCAN_FOUND_DATA_CHAN:
			sprintf(buffer, "%d", data);
			paintLine(xpos1 + 2 * 72, ypos_service_numbers + mheight, 72, buffer);
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
			CNeutrinoApp::getInstance()->setVolume(msg, true, true);
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
	
	sprintf(filename, "radar%d.raw", radar);
	radar = (radar + 1) % 10;
	frameBuffer->paintIcon8(filename, xpos_radar, ypos_radar, 17);
}

void CScanTs::hide()
{
	//frameBuffer->loadPal("radiomode.pal", 18, COL_MAXFREE);
	//frameBuffer->paintBackgroundBoxRel(0, 0, 720, 576);
	frameBuffer->paintBackground();
	freqready = 0;
}

void CScanTs::paintLineLocale(int x, int * y, int width, const neutrino_locale_t l)
{
	frameBuffer->paintBoxRel(x, *y, width, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x, *y + mheight, width, g_Locale->getText(l), COL_MENUCONTENTINACTIVE, 0, true); // UTF-8
	*y += mheight;
}

void CScanTs::paintLine(int x, int y, int w, const char * const txt)
{
//printf("CScanTs::paintLine x %d y %d w %d width %d xpos2 %d: %s\n", x, y, w, width, xpos2, txt);
	frameBuffer->paintBoxRel(x, y, w, mheight, COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x, y + mheight, w, txt, COL_MENUCONTENT, 0, true); // UTF-8
}

void CScanTs::paint(bool fortest)
{
	int ypos;

	ypos = y;
	
	//frameBuffer->paintBoxRel(x, ypos, width, hheight, COL_MENUHEAD_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos, width, hheight, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, 1);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(xpos1, ypos + hheight, width, fortest ? g_Locale->getText(LOCALE_SCANTS_TEST) : g_Locale->getText(LOCALE_SCANTS_HEAD), COL_MENUHEAD, 0, true); // UTF-8
	//frameBuffer->paintBoxRel(x, ypos + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0, ROUND_RADIUS, 2);
	
	frameBuffer->loadPal("radar.pal", 17, 37);
	 
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

	ypos_service_numbers = ypos; paintLineLocale(xpos1         , &ypos, 72                 , LOCALE_SCANTS_NUMBEROFTVSERVICES   );
	ypos = ypos_service_numbers; paintLineLocale(xpos1 +     72, &ypos, 72                 , LOCALE_SCANTS_NUMBEROFRADIOSERVICES);
	ypos = ypos_service_numbers; paintLineLocale(xpos1 + 2 * 72, &ypos, 72                 , LOCALE_SCANTS_NUMBEROFDATASERVICES );
	ypos = ypos_service_numbers; paintLineLocale(xpos1 + 3 * 72, &ypos, width - 3 * 72 - 10, LOCALE_SCANTS_NUMBEROFTOTALSERVICES);
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

	ssig = frontend->getSignalStrength();
	ssnr = frontend->getSignalNoiseRatio();

	snr = (ssnr & 0xFFFF) * 100 / 65535;
	sig = (ssig & 0xFFFF) * 100 / 65535;

	posy = y + height - mheight - 5;

	if(sigscale->getPercent() != sig) {
		posx = x + 20;
		sprintf(percent, "%d%% SIG", sig);
		sw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("100% SIG");

		sigscale->paint(posx - 1, posy+2, sig);

		posx = posx + barwidth + 3;
		sw = x + 247 - posx;
		frameBuffer->paintBoxRel(posx, posy - 2, sw, mheight, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx+2, posy + mheight, sw, percent, COL_MENUCONTENT);
	}
	if(snrscale->getPercent() != snr) {
		posx = x + 20 + 260;
		sprintf(percent, "%d%% SNR", snr);
		sw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("100% SNR");
		snrscale->paint(posx - 1, posy+2, snr);

		posx = posx + barwidth + 3;
		sw = x + width - posx;
		frameBuffer->paintBoxRel(posx, posy - 2, sw, mheight, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString (posx+2, posy + mheight, sw, percent, COL_MENUCONTENT);
	}
}
