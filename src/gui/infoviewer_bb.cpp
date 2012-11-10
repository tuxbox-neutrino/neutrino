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
#include "infoviewer_bb.h"

#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sys/timeb.h>
#include <sys/param.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <global.h>
#include <neutrino.h>

#include <gui/infoviewer.h>
#include <gui/bouquetlist.h>
#include <gui/widget/icons.h>
#include <gui/widget/hintbox.h>
#include <gui/customcolor.h>
#include <gui/pictureviewer.h>
#include <gui/movieplayer.h>
#include <system/helpers.h>
#include <daemonc/remotecontrol.h>

#include <zapit/femanager.h>
#include <zapit/zapit.h>

#include <video.h>

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
extern cVideo * videoDecoder;

//#define SHOW_RADIOTEXT_ICON
#define COL_INFOBAR_BUTTONS            (COL_INFOBAR_SHADOW + 1)
#define COL_INFOBAR_BUTTONS_BACKGROUND (COL_INFOBAR_SHADOW_PLUS_1)

CInfoViewerBB::CInfoViewerBB()
{
	frameBuffer = CFrameBuffer::getInstance();

	is_visible		= false;
	scrambledErr		= false;
	scrambledErrSave	= false;
	scrambledNoSig		= false;
	scrambledNoSigSave	= false;
	scrambledT		= 0;
#if 0
	if(!scrambledT) {
		pthread_create(&scrambledT, NULL, scrambledThread, (void*) this) ;
		pthread_detach(scrambledT);
	}
#endif
	hddpercent		= 0;
	hddperT			= 0;
	hddperTflag		= false;
	hddscale 		= NULL;
	sysscale 		= NULL;
	bbIconInfo[0].x = 0;
	bbIconInfo[0].h = 0;
	BBarY = 0;
	BBarFontY = 0;

	Init();
}

void CInfoViewerBB::Init()
{
	hddwidth		= 0;
	bbIconMaxH 		= 0;
	bbButtonMaxH 		= 0;
	bbIconMinX 		= 0;
	bbButtonMaxX 		= 0;
	fta			= true;
	minX			= 0;

	for (int i = 0; i < CInfoViewerBB::BUTTON_MAX; i++) {
		tmp_bbButtonInfoText[i] = "";
	}

	InfoHeightY_Info = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight() + 5;
	setBBOffset();

	changePB();
}

CInfoViewerBB::~CInfoViewerBB()
{
	if(scrambledT) {
		pthread_cancel(scrambledT);
		scrambledT = 0;
	}
	if(hddperT) {
		pthread_cancel(hddperT);
		hddperT = 0;
	}
	if (hddscale)
		delete hddscale;
	if (sysscale)
		delete sysscale;
}

CInfoViewerBB* CInfoViewerBB::getInstance()
{
	static CInfoViewerBB* InfoViewerBB = NULL;

	if(!InfoViewerBB) {
		InfoViewerBB = new CInfoViewerBB();
	}
	return InfoViewerBB;
}

bool CInfoViewerBB::checkBBIcon(const char * const icon, int *w, int *h)
{
	frameBuffer->getIconSize(icon, w, h);
	if ((*w != 0) && (*h != 0))
		return true;
	return false;
}

void CInfoViewerBB::getBBIconInfo()
{
	bbIconMaxH 		= 0;
	BBarY 			= g_InfoViewer->BoxEndY + bottom_bar_offset;
	BBarFontY 		= BBarY + InfoHeightY_Info - (InfoHeightY_Info - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()) / 2; /* center in buttonbar */
	bbIconMinX 		= g_InfoViewer->BoxEndX;
	CNeutrinoApp* neutrino	= CNeutrinoApp::getInstance();

	for (int i = 0; i < CInfoViewerBB::ICON_MAX; i++) {
		int w = 0, h = 0;
		bool iconView = false;
		switch (i) {
		case CInfoViewerBB::ICON_SUBT:  //no radio
			if (neutrino->getMode() != NeutrinoMessages::mode_radio)
				iconView = checkBBIcon(NEUTRINO_ICON_SUBT, &w, &h);
			break;
		case CInfoViewerBB::ICON_VTXT:  //no radio
			if (neutrino->getMode() != NeutrinoMessages::mode_radio)
				iconView = checkBBIcon(NEUTRINO_ICON_VTXT, &w, &h);
			break;
#ifdef SHOW_RADIOTEXT_ICON
		case CInfoViewerBB::ICON_RT:
			if (neutrino->getMode() == NeutrinoMessages::mode_radio)
				iconView = checkBBIcon(NEUTRINO_ICON_RT, &w, &h);
			break;
#endif
		case CInfoViewerBB::ICON_DD:
			if( g_settings.infobar_show_dd_available )
				iconView = checkBBIcon(NEUTRINO_ICON_DD, &w, &h);
			break;
		case CInfoViewerBB::ICON_16_9:  //no radio
			if (neutrino->getMode() != NeutrinoMessages::mode_radio)
				iconView = checkBBIcon(NEUTRINO_ICON_16_9, &w, &h);
			break;
		case CInfoViewerBB::ICON_RES:  //no radio
			if ((g_settings.infobar_show_res < 2) && (neutrino->getMode() != NeutrinoMessages::mode_radio))
				iconView = checkBBIcon(NEUTRINO_ICON_RESOLUTION_1280, &w, &h);
			break;
		case CInfoViewerBB::ICON_CA:
			if (g_settings.casystem_display == 2)
				iconView = checkBBIcon(NEUTRINO_ICON_SCRAMBLED2, &w, &h);
			break;
		case CInfoViewerBB::ICON_TUNER:
			if (CFEManager::getInstance()->getMode() != CFEManager::FE_MODE_SINGLE) {
				if (g_settings.infobar_show_tuner == 1) {
					iconView = checkBBIcon(NEUTRINO_ICON_TUNER_1, &w, &h);
				}
			}
			break;
		default:
			break;
		}
		if (iconView) {
			bbIconMinX -= w + 2;
			bbIconInfo[i].x = bbIconMinX;
			bbIconInfo[i].h = h;
		}
		else
			bbIconInfo[i].x = -1;
	}
	for (int i = 0; i < CInfoViewerBB::ICON_MAX; i++) {
		if (bbIconInfo[i].x != -1)
			bbIconMaxH = std::max(bbIconMaxH, bbIconInfo[i].h);
	}
	if (g_settings.infobar_show_sysfs_hdd)
		bbIconMinX -= hddwidth + 2;
}

void CInfoViewerBB::getBBButtonInfo()
{
	bbButtonMaxH = 0;
	bbButtonMaxX = g_InfoViewer->ChanInfoX;
	int bbButtonMaxW = 0;
	for (int i = 0; i < CInfoViewerBB::BUTTON_MAX; i++) {
		int w = 0, h = 0;
		std::string text, icon;
		switch (i) {
		case CInfoViewerBB::BUTTON_EPG:
			icon = NEUTRINO_ICON_BUTTON_RED;
			frameBuffer->getIconSize(icon.c_str(), &w, &h);
			text = g_settings.usermenu_text[SNeutrinoSettings::BUTTON_RED];
			if (text.empty())
				text = g_Locale->getText(LOCALE_INFOVIEWER_EVENTLIST);
			break;
		case CInfoViewerBB::BUTTON_AUDIO:
			icon = NEUTRINO_ICON_BUTTON_GREEN;
			frameBuffer->getIconSize(icon.c_str(), &w, &h);
			text = g_settings.usermenu_text[SNeutrinoSettings::BUTTON_GREEN];
			if (text == g_Locale->getText(LOCALE_AUDIOSELECTMENUE_HEAD))
				text = "";
			if(NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode() && !CMoviePlayerGui::getInstance().timeshift){
				text = CMoviePlayerGui::getInstance().CurrentAudioName();
			}else if (!g_RemoteControl->current_PIDs.APIDs.empty()) {
				int selected = g_RemoteControl->current_PIDs.PIDs.selected_apid;
				if (text.empty()){
					text = g_RemoteControl->current_PIDs.APIDs[selected].desc;
				}
			}
			break;
		case CInfoViewerBB::BUTTON_SUBS:
			icon = NEUTRINO_ICON_BUTTON_YELLOW;
			frameBuffer->getIconSize(icon.c_str(), &w, &h);
			text = g_settings.usermenu_text[SNeutrinoSettings::BUTTON_YELLOW];
			if (text.empty())
				text = g_Locale->getText((g_RemoteControl->are_subchannels) ? LOCALE_INFOVIEWER_SUBSERVICE : LOCALE_INFOVIEWER_SELECTTIME);
			break;
		case CInfoViewerBB::BUTTON_FEAT:
			icon = NEUTRINO_ICON_BUTTON_BLUE;
			frameBuffer->getIconSize(icon.c_str(), &w, &h);
			text = g_settings.usermenu_text[SNeutrinoSettings::BUTTON_BLUE];
			if (text.empty())
				text = g_Locale->getText(LOCALE_INFOVIEWER_STREAMINFO);
			break;
		default:
			break;
		}
		bbButtonInfo[i].w = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(text) + w + 10;
		bbButtonInfo[i].cx = w + 5;
		bbButtonInfo[i].h = h;
		bbButtonInfo[i].text = text;
		bbButtonInfo[i].icon = icon;
	}
	// Calculate position/size of buttons
	minX = std::min(bbIconMinX, g_InfoViewer->ChanInfoX + (((g_InfoViewer->BoxEndX - g_InfoViewer->ChanInfoX) * 75) / 100));
	int MaxBr = minX - (g_InfoViewer->ChanInfoX + 10);
	bbButtonMaxX = g_InfoViewer->ChanInfoX + 10;
	int br = 0;
	for (int i = 0; i < CInfoViewerBB::BUTTON_MAX; i++) {
		if ((i == CInfoViewerBB::BUTTON_SUBS) && (g_RemoteControl->subChannels.empty())) { // no subchannels
			bbButtonInfo[i].paint = false;
//			bbButtonInfo[i].x = -1;
//			continue;
		}
		else
			bbButtonInfo[i].paint = true;
		br += bbButtonInfo[i].w;
		bbButtonInfo[i].x = bbButtonMaxX;
		bbButtonMaxX += bbButtonInfo[i].w;
		bbButtonMaxW = std::max(bbButtonMaxW, bbButtonInfo[i].w);
	}
	if (br > MaxBr) { // TODO: Cut to long strings
		printf("[infoviewer.cpp - %s, line #%d] width ColorButtons (%d) > MaxBr (%d)\n", __FUNCTION__, __LINE__, br, MaxBr);
	}
#if 0
	int Btns = 0;
	// counting buttons
	for (int i = 0; i < CInfoViewerBB::BUTTON_MAX; i++) {
		if (bbButtonInfo[i].x != -1) {
			Btns++;
		}
	}
	bbButtonMaxX = g_InfoViewer->ChanInfoX + 10;

	bbButtonInfo[CInfoViewerBB::BUTTON_EPG].x = bbButtonMaxX;
	bbButtonInfo[CInfoViewerBB::BUTTON_FEAT].x = minX - bbButtonInfo[CInfoViewerBB::BUTTON_FEAT].w;

	int x1 = bbButtonInfo[CInfoViewerBB::BUTTON_EPG].x + bbButtonInfo[CInfoViewerBB::BUTTON_EPG].w;
	int rest = bbButtonInfo[CInfoViewerBB::BUTTON_FEAT].x - x1;

	if (Btns < 4) {
		rest -= bbButtonInfo[CInfoViewerBB::BUTTON_AUDIO].w;
		bbButtonInfo[CInfoViewerBB::BUTTON_AUDIO].x = x1 + rest / 2;
	}
	else {
		rest -= bbButtonInfo[CInfoViewerBB::BUTTON_AUDIO].w + bbButtonInfo[CInfoViewerBB::BUTTON_SUBS].w;
		rest = rest / 3;
		bbButtonInfo[CInfoViewerBB::BUTTON_AUDIO].x = x1 + rest;
		bbButtonInfo[CInfoViewerBB::BUTTON_SUBS].x = bbButtonInfo[CInfoViewerBB::BUTTON_AUDIO].x + 
								bbButtonInfo[CInfoViewerBB::BUTTON_AUDIO].w + rest;
	}
#endif
#if 1
	bbButtonMaxX = g_InfoViewer->ChanInfoX + 10;
	int step = MaxBr / 4;
	bbButtonInfo[CInfoViewerBB::BUTTON_EPG].x   = bbButtonMaxX;
	bbButtonInfo[CInfoViewerBB::BUTTON_AUDIO].x = bbButtonMaxX + step;
	bbButtonInfo[CInfoViewerBB::BUTTON_SUBS].x  = bbButtonMaxX + 2*step;
	bbButtonInfo[CInfoViewerBB::BUTTON_FEAT].x  = bbButtonMaxX + 3*step;
#endif
}

void CInfoViewerBB::showBBButtons(const int modus)
{
	if (!is_visible)
		return;
	int i;
	bool paint = false;

	getBBButtonInfo();
	for (i = 0; i < CInfoViewerBB::BUTTON_MAX; i++) {
		if (tmp_bbButtonInfoText[i] != bbButtonInfo[i].text) {
			paint = true;
			break;
		}
	}

	if (paint) {
		frameBuffer->paintBoxRel(g_InfoViewer->ChanInfoX, BBarY, minX - g_InfoViewer->ChanInfoX, InfoHeightY_Info, COL_INFOBAR_BUTTONS_BACKGROUND, RADIUS_SMALL, CORNER_BOTTOM); //round
		for (i = 0; i < CInfoViewerBB::BUTTON_MAX; i++) {
			if ((bbButtonInfo[i].x <= g_InfoViewer->ChanInfoX) || (bbButtonInfo[i].x >= g_InfoViewer->BoxEndX) || (!bbButtonInfo[i].paint))
				continue;
			if ((bbButtonInfo[i].x > 0) && ((bbButtonInfo[i].x + bbButtonInfo[i].w) <= minX)) {
				
				frameBuffer->paintIcon(bbButtonInfo[i].icon, bbButtonInfo[i].x, BBarY, InfoHeightY_Info);

				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(bbButtonInfo[i].x + bbButtonInfo[i].cx, BBarFontY, 
				       bbButtonInfo[i].w - bbButtonInfo[i].cx, bbButtonInfo[i].text, COL_INFOBAR_BUTTONS, 0, true); // UTF-8
			}
		}

		if (modus == CInfoViewerBB::BUTTON_AUDIO)
			showIcon_DD();

		for (i = 0; i < CInfoViewerBB::BUTTON_MAX; i++) {
			tmp_bbButtonInfoText[i] = bbButtonInfo[i].text;
		}
	}
}

void CInfoViewerBB::showBBIcons(const int modus, const std::string & icon)
{
	if ((bbIconInfo[modus].x <= g_InfoViewer->ChanInfoX) || (bbIconInfo[modus].x >= g_InfoViewer->BoxEndX))
		return;
	if ((modus >= CInfoViewerBB::ICON_SUBT) && (modus < CInfoViewerBB::ICON_MAX) && (bbIconInfo[modus].x != -1) && (is_visible)) {
		frameBuffer->paintIcon(icon, bbIconInfo[modus].x, BBarY, 
				       InfoHeightY_Info, 1, true, true, COL_INFOBAR_BUTTONS_BACKGROUND);
	}
}

void CInfoViewerBB::paintshowButtonBar()
{
	if (!is_visible)
		return;
	getBBIconInfo();
	for (int i = 0; i < CInfoViewerBB::BUTTON_MAX; i++) {
		tmp_bbButtonInfoText[i] = "";
	}
	g_InfoViewer->sec_timer_id = g_RCInput->addTimer(1*1000*1000, false);

	if (g_settings.casystem_display < 2)
		paintCA_bar(0,0);

	frameBuffer->paintBoxRel(g_InfoViewer->ChanInfoX, BBarY, g_InfoViewer->BoxEndX - g_InfoViewer->ChanInfoX, InfoHeightY_Info, COL_INFOBAR_BUTTONS_BACKGROUND, RADIUS_SMALL, CORNER_BOTTOM); //round

	g_InfoViewer->showSNR();

	// Buttons
	showBBButtons();

	// Icons, starting from right
	showIcon_SubT();
	showIcon_VTXT();
	showIcon_DD();
	showIcon_16_9();
#if 0
	scrambledCheck(true);
#endif
	showIcon_CA_Status(0);
	showIcon_Resolution();
	showIcon_Tuner();
	showSysfsHdd();
}

void CInfoViewerBB::showIcon_SubT()
{
	if (!is_visible)
		return;
	bool have_sub = false;
	CZapitChannel * cc = CNeutrinoApp::getInstance()->channelList->getChannel(CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber());
	if (cc && cc->getSubtitleCount())
		have_sub = true;

	showBBIcons(CInfoViewerBB::ICON_SUBT, (have_sub) ? NEUTRINO_ICON_SUBT : NEUTRINO_ICON_SUBT_GREY);
}

void CInfoViewerBB::showIcon_VTXT()
{
	if (!is_visible)
		return;
	showBBIcons(CInfoViewerBB::ICON_VTXT, (g_RemoteControl->current_PIDs.PIDs.vtxtpid != 0) ? NEUTRINO_ICON_VTXT : NEUTRINO_ICON_VTXT_GREY);
}

void CInfoViewerBB::showIcon_DD()
{
	if (!is_visible || !g_settings.infobar_show_dd_available)
		return;
	std::string dd_icon;
	if ((g_RemoteControl->current_PIDs.PIDs.selected_apid < g_RemoteControl->current_PIDs.APIDs.size()) && 
	    (g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].is_ac3))
		dd_icon = NEUTRINO_ICON_DD;
	else 
		dd_icon = g_RemoteControl->has_ac3 ? NEUTRINO_ICON_DD_AVAIL : NEUTRINO_ICON_DD_GREY;

	showBBIcons(CInfoViewerBB::ICON_DD, dd_icon);
}

#ifdef SHOW_RADIOTEXT_ICON
void CInfoViewerBB::showIcon_RadioText(bool rt_available)
{
	// TODO: display radiotext icon
	if ((showButtonBar) && (is_visible))
	{
		int mode = CNeutrinoApp::getInstance()->getMode();

		showBBIcons(CInfoViewerBB::ICON_RT, rt_icon);
	}
}
#else
void CInfoViewerBB::showIcon_RadioText(bool /*rt_available*/)
{
}
#endif

void CInfoViewerBB::showIcon_16_9()
{
	if (!is_visible)
		return;
	if ((g_InfoViewer->aspectRatio == 0) || ( g_RemoteControl->current_PIDs.PIDs.vpid == 0 ) || (g_InfoViewer->aspectRatio != videoDecoder->getAspectRatio())) {
		if (g_InfoViewer->chanready && g_RemoteControl->current_PIDs.PIDs.vpid > 0 ) {
			g_InfoViewer->aspectRatio = videoDecoder->getAspectRatio();
		}
		else
			g_InfoViewer->aspectRatio = 0;

		showBBIcons(CInfoViewerBB::ICON_16_9, (g_InfoViewer->aspectRatio > 2) ? NEUTRINO_ICON_16_9 : NEUTRINO_ICON_16_9_GREY);
	}
}

void CInfoViewerBB::showIcon_Resolution()
{
	if ((!is_visible) || (g_settings.infobar_show_res == 2)) //show resolution icon is off
		return;
	const char *icon_name = NULL;
#if 0
	if ((scrambledNoSig) || ((!fta) && (scrambledErr)))
#else
	if (!g_InfoViewer->chanready || videoDecoder->getBlank())
#endif
	{
		icon_name = NEUTRINO_ICON_RESOLUTION_000;
	} else {
		int xres, yres, framerate;
		if (g_settings.infobar_show_res == 0) {//show resolution icon on infobar
			videoDecoder->getPictureInfo(xres, yres, framerate);
			switch (yres) {
			case 1920:
				icon_name = NEUTRINO_ICON_RESOLUTION_1920;
				break;
			case 1088:
			case 1080:
				icon_name = NEUTRINO_ICON_RESOLUTION_1080;
				break;
			case 1440:
				icon_name = NEUTRINO_ICON_RESOLUTION_1440;
				break;
			case 1280:
				icon_name = NEUTRINO_ICON_RESOLUTION_1280;
				break;
			case 720:
				icon_name = NEUTRINO_ICON_RESOLUTION_720;
				break;
			case 704:
				icon_name = NEUTRINO_ICON_RESOLUTION_704;
				break;
			case 576:
				icon_name = NEUTRINO_ICON_RESOLUTION_576;
				break;
			case 544:
				icon_name = NEUTRINO_ICON_RESOLUTION_544;
				break;
			case 528:
				icon_name = NEUTRINO_ICON_RESOLUTION_528;
				break;
			case 480:
				icon_name = NEUTRINO_ICON_RESOLUTION_480;
				break;
			case 382:
				icon_name = NEUTRINO_ICON_RESOLUTION_382;
				break;
			case 352:
				icon_name = NEUTRINO_ICON_RESOLUTION_352;
				break;
			case 288:
				icon_name = NEUTRINO_ICON_RESOLUTION_288;
				break;
			default:
				icon_name = NEUTRINO_ICON_RESOLUTION_000;
				break;
			}
		}
		if (g_settings.infobar_show_res == 1) {//show simple resolution icon on infobar
			videoDecoder->getPictureInfo(xres, yres, framerate);
			if (yres > 576)
				icon_name = NEUTRINO_ICON_RESOLUTION_HD;
			else if (yres > 0)
				icon_name = NEUTRINO_ICON_RESOLUTION_SD;
			else
				icon_name = NEUTRINO_ICON_RESOLUTION_000;
		}
	}
	showBBIcons(CInfoViewerBB::ICON_RES, icon_name);
}

void CInfoViewerBB::showOne_CAIcon()
{
	std::string sIcon = "";
#if 0
	if (CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_radio) {
		if (scrambledNoSig)
			sIcon = NEUTRINO_ICON_SCRAMBLED2_BLANK;
		else {	
			if (fta)
				sIcon = NEUTRINO_ICON_SCRAMBLED2_GREY;
			else
				sIcon = (scrambledErr) ? NEUTRINO_ICON_SCRAMBLED2_RED : NEUTRINO_ICON_SCRAMBLED2;
		}
	}
	else
#endif
		sIcon = (fta) ? NEUTRINO_ICON_SCRAMBLED2_GREY : NEUTRINO_ICON_SCRAMBLED2;
	showBBIcons(CInfoViewerBB::ICON_CA, sIcon);
}

void CInfoViewerBB::showIcon_Tuner()
{
	std::string icon_name;
	switch (CFEManager::getInstance()->getLiveFE()->getNumber()) {
		case 1:
			icon_name = NEUTRINO_ICON_TUNER_2;
			break;
		case 0:
		default:
			icon_name = NEUTRINO_ICON_TUNER_1;
			break;
	}
	showBBIcons(CInfoViewerBB::ICON_TUNER, icon_name);
}

void CInfoViewerBB::showSysfsHdd()
{
	if (g_settings.infobar_show_sysfs_hdd) {
		//sysFS info
		int percent = 0;
		long t, u;
		if (get_fs_usage("/", t, u))
			percent = (u * 100ULL) / t;
		showBarSys(percent);

		//HDD info in a seperate thread
		if(!hddperTflag) {
			hddperTflag=true;
			pthread_create(&hddperT, NULL, hddperThread, (void*) this);
			pthread_detach(hddperT);
		}

		if (check_dir(g_settings.network_nfs_recordingdir) == 0)
			showBarHdd(hddpercent);
		else
			showBarHdd(-1);
	}
}

void* CInfoViewerBB::hddperThread(void *arg)
{
	CInfoViewerBB *infoViewerBB = (CInfoViewerBB*) arg;

	infoViewerBB->hddpercent = 0;
	long t, u;
	if (get_fs_usage(g_settings.network_nfs_recordingdir, t, u))
		infoViewerBB->hddpercent = (u * 100ULL) / t;

	infoViewerBB->hddperTflag=false;
	pthread_exit(NULL);
}

void CInfoViewerBB::showBarSys(int percent)
{
	if (is_visible)
		sysscale->paintProgressBar(bbIconMinX, BBarY + InfoHeightY_Info / 2 - 2 - 6, hddwidth, 6, percent, 100);
}

void CInfoViewerBB::showBarHdd(int percent)
{
	if (is_visible) {
		if (percent >= 0)
			hddscale->paintProgressBar(bbIconMinX, BBarY + InfoHeightY_Info / 2 + 2 + 0, hddwidth, 6, percent, 100);
		else {
			frameBuffer->paintBoxRel(bbIconMinX, BBarY + InfoHeightY_Info / 2 + 2 + 0, hddwidth, 6, COL_INFOBAR_BUTTONS_BACKGROUND);
			hddscale->reset();
		}
	}
}

void CInfoViewerBB::paint_ca_icons(int caid, char * icon, int &icon_space_offset)
{
	char buf[20];
	int endx = g_InfoViewer->BoxEndX -3;
	int py = g_InfoViewer->BoxEndY + 2; /* hand-crafted, should be automatic */
	int px = 0;
	static map<int, std::pair<int,const char*> > icon_map;
	const int icon_space = 10, icon_number = 10;

	static int icon_offset[icon_number] = {0,0,0,0,0,0,0,0,0,0};
	static int icon_sizeW [icon_number] = {0,0,0,0,0,0,0,0,0,0};
	static bool init_flag = false;

	if (!init_flag) {
		init_flag = true;
		int icon_sizeH = 0, index = 0;
		map<int, std::pair<int,const char*> >::const_iterator it;

		icon_map[0x0E00] = std::make_pair(index++,"powervu");
		icon_map[0x4A00] = std::make_pair(index++,"d");
		icon_map[0x2600] = std::make_pair(index++,"biss");
		icon_map[0x0600] = std::make_pair(index++,"ird");
		icon_map[0x0100] = std::make_pair(index++,"seca");
		icon_map[0x0500] = std::make_pair(index++,"via");
		icon_map[0x1800] = std::make_pair(index++,"nagra");
		icon_map[0x0B00] = std::make_pair(index++,"conax");
		icon_map[0x0D00] = std::make_pair(index++,"cw");
		icon_map[0x0900] = std::make_pair(index  ,"nds");

		for (it=icon_map.begin(); it!=icon_map.end(); ++it) {
			snprintf(buf, sizeof(buf), "%s_%s", (*it).second.second, icon);
			frameBuffer->getIconSize(buf, &icon_sizeW[(*it).second.first], &icon_sizeH);
		}

		for (int j = 0; j < icon_number; j++) {
			for (int i = j; i < icon_number; i++) {
				icon_offset[j] += icon_sizeW[i] + icon_space;
			}
		}
	}
	caid &= 0xFF00;

	if (icon_offset[icon_map[caid].first] == 0)
		return;

	if (g_settings.casystem_display == 0) {
		px = endx - (icon_offset[icon_map[caid].first] - icon_space );
	} else {
		icon_space_offset += icon_sizeW[icon_map[caid].first];
		px = endx - icon_space_offset;
		icon_space_offset += 4;
	}

	if (px) {
		snprintf(buf, sizeof(buf), "%s_%s", icon_map[caid].second, icon);
		if ((px >= (endx-8)) || (px <= 0))
			printf("#####[%s:%d] Error paint icon %s, px: %d,  py: %d, endx: %d, icon_offset: %d\n", 
				__FUNCTION__, __LINE__, buf, px, py, endx, icon_offset[icon_map[caid].first]);
		else
			frameBuffer->paintIcon(buf, px, py);
	}
}

void CInfoViewerBB::showIcon_CA_Status(int notfirst)
{
	if (g_settings.casystem_display == 3)
		return;
	if(NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode() && !CMoviePlayerGui::getInstance().timeshift){
		if (g_settings.casystem_display == 2) {
			fta = true;
			showOne_CAIcon();
		}
		return;
	}

	int caids[] = {  0x900, 0xD00, 0xB00, 0x1800, 0x0500, 0x0100, 0x600,  0x2600, 0x4a00, 0x0E00 };
	const char * white = (char *) "white";
	const char * yellow = (char *) "yellow";
	int icon_space_offset = 0;

	if(!g_InfoViewer->chanready) {
		if (g_settings.casystem_display == 2) {
			fta = true;
			showOne_CAIcon();
		}
		else if(g_settings.casystem_display == 0) {
			for (int i = 0; i < (int)(sizeof(caids)/sizeof(int)); i++) {
				paint_ca_icons(caids[i], (char *) white, icon_space_offset);
			}
		}
		return;
	}

	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	if(!channel)
		return;

	if (g_settings.casystem_display == 2) {
		fta = channel->camap.empty();
		showOne_CAIcon();
		return;
	}

	if(!notfirst) {
#if 0
		static int icon_space_offset = 0;
		if ((g_settings.casystem_display == 1) && (icon_space_offset)) {
			paintCA_bar(0,icon_space_offset);
			icon_space_offset = 0;
		}
#endif
		for (int i = 0; i < (int)(sizeof(caids)/sizeof(int)); i++) {
			bool found = false;
			for(casys_map_iterator_t it = channel->camap.begin(); it != channel->camap.end(); ++it) {
				int caid = (*it) & 0xFF00;
				if (caid == 0x1700)
					caid = 0x0600;
				if((found = (caid == caids[i])))
					break;
			}
			if(g_settings.casystem_display == 0)
				paint_ca_icons(caids[i], (char *) (found ? yellow : white), icon_space_offset);
			else if(found)
				paint_ca_icons(caids[i], (char *) yellow, icon_space_offset);
		}
	}
}

void CInfoViewerBB::paintCA_bar(int left, int right)
{
	int xcnt = (g_InfoViewer->BoxEndX - g_InfoViewer->ChanInfoX) / 4;
	int ycnt = bottom_bar_offset / 4;
	if (right)
		right = xcnt - ((right/4)+1);
	if (left)
		left =  xcnt - ((left/4)-1);

	frameBuffer->paintBox(g_InfoViewer->ChanInfoX + (right*4), g_InfoViewer->BoxEndY, g_InfoViewer->BoxEndX - (left*4), g_InfoViewer->BoxEndY + bottom_bar_offset, COL_BLACK);

	if (left)
		left -= 1;

	for (int i = 0  + right; i < xcnt - left; i++) {
		for (int j = 0; j < ycnt; j++) {
			frameBuffer->paintBoxRel((g_InfoViewer->ChanInfoX + 2) + i*4, g_InfoViewer->BoxEndY + 2 + j*4, 2, 2, COL_INFOBAR_PLUS_1);
		}
	}
}

void CInfoViewerBB::changePB()
{
	hddwidth = frameBuffer->getScreenWidth(true) * ((g_settings.screen_preset == 1) ? 10 : 8) / 128; /* 80(CRT)/100(LCD) pix if screen is 1280 wide */
	if (hddscale != NULL)
		delete hddscale;
	hddscale = new CProgressBar(true, hddwidth, 6, 50, 100, 75, true);
	if (sysscale != NULL)
		delete sysscale;
	sysscale = new CProgressBar(true, hddwidth, 6, 50, 100, 75, true);
}

void CInfoViewerBB::reset_allScala()
{
	hddscale->reset();
	sysscale->reset();
	//lasthdd = lastsys = -1;
}

void CInfoViewerBB::setBBOffset()
{
	bottom_bar_offset = (g_settings.casystem_display < 2) ? 22 : 0;
}

void* CInfoViewerBB::scrambledThread(void *arg)
{
 	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	CInfoViewerBB *infoViewerBB = static_cast<CInfoViewerBB*>(arg);
	while(1) {
		if (infoViewerBB->is_visible)
			infoViewerBB->scrambledCheck();
		usleep(500*1000);
	}
	return 0;
}

void CInfoViewerBB::scrambledCheck(bool force)
{
	scrambledErr = false;
	scrambledNoSig = false;
	if (videoDecoder->getBlank()) {
		if (videoDecoder->getPlayState())
			scrambledErr = true;
		else
			scrambledNoSig = true;
	}
	
	if ((scrambledErr != scrambledErrSave) || (scrambledNoSig != scrambledNoSigSave) || force) {
		showIcon_CA_Status(0);
		showIcon_Resolution();
		scrambledErrSave = scrambledErr;
		scrambledNoSigSave = scrambledNoSig;
	}
}
