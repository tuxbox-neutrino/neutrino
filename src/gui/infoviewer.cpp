/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Bugfixes/cleanups (C) 2007-2013,2015 Stefan Seyfried
	(C) 2008 Novell, Inc. Author: Stefan Seyfried

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
#include "infoviewer.h"
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

#include <gui/bouquetlist.h>
#include <gui/color_custom.h>
#include <gui/widget/icons.h>
#include <gui/widget/hintbox.h>
#include <gui/pictureviewer.h>
#include <gui/movieplayer.h>
#include <gui/infoclock.h>

#include <system/helpers.h>

#include <daemonc/remotecontrol.h>
#include <driver/record.h>
#include <driver/display.h>
#include <driver/volume.h>
#include <driver/radiotext.h>
#include <driver/fontrenderer.h>

#include <zapit/satconfig.h>
#include <zapit/femanager.h>
#include <zapit/zapit.h>
#include <eitd/sectionsd.h>
#include <video.h>

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
extern CBouquetList * bouquetList;       /* neutrino.cpp */
extern CPictureViewer * g_PicViewer;
extern cVideo * videoDecoder;


#define LEFT_OFFSET 5

event_id_t CInfoViewer::last_curr_id = 0, CInfoViewer::last_next_id = 0;

static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}

extern bool timeset;

CInfoViewer::CInfoViewer ()
	: fader(g_settings.theme.infobar_alpha)
{
	sigbox = NULL;
	header = numbox = body = rec = NULL;
	txt_cur_start = txt_cur_event = txt_cur_event_rest = txt_next_start = txt_next_event = txt_next_in = NULL;
	timescale = NULL;
	clock = NULL;
	info_CurrentNext.current_zeit.startzeit = 0;
	info_CurrentNext.current_zeit.dauer = 0;
	info_CurrentNext.flags = 0;
	frameBuffer = CFrameBuffer::getInstance();
	infoViewerBB = CInfoViewerBB::getInstance();
	InfoHeightY = 0;
	ButtonWidth = 0;
	ChanNameX = 0;
	ChanNameY = 0;
	ChanWidth = 0;
	ChanHeight = 0;
	numbox_offset = 0;
	time_width = 0;
	time_height = header_height = 0;
	lastsnr = 0;
	lastsig = 0;
	lasttime = 0;
	aspectRatio = 0;
	ChanInfoX = 0;
	Init();
	infoViewerBB->Init();
	oldinfo.current_uniqueKey = 0;
	oldinfo.next_uniqueKey = 0;
	isVolscale = false;
	info_time_width = 0;
	timeoutEnd = 0;
	sec_timer_id = 0;
}

CInfoViewer::~CInfoViewer()
{
	ResetModules();
}

void CInfoViewer::Init()
{
	BoxStartX = BoxStartY = BoxEndX = BoxEndY = 0;
	initClock();
	recordModeActive = false;
	is_visible = false;
	showButtonBar = false;
	zap_mode = IV_MODE_DEFAULT;
	newfreq = true;
	chanready = 1;
	fileplay = 0;
	SDT_freq_update = false;

	/* maybe we should not tie this to the blinkenlights settings? */
	infoViewerBB->initBBOffset();
	/* after font size changes, Init() might be called multiple times */
	changePB();

	casysChange = g_settings.infobar_casystem_display;
	channellogoChange = g_settings.infobar_show_channellogo;

	current_channel_id = CZapit::getInstance()->GetCurrentChannelID();;
	current_epg_id = 0;
	lcdUpdateTimer = 0;
	rt_x = rt_y = rt_h = rt_w = 0;

	infobar_txt = NULL;

	_livestreamInfo1.clear();
	_livestreamInfo2.clear();
}

/*
 * This nice ASCII art should hopefully explain how all the variables play together ;)
 *

              ___BoxStartX
             |-ChanWidth-|
             |           |  _recording icon                 _progress bar
 BoxStartY---+-----------+ |                               |
     |       |           | *                              #######____
     |       |           |-------------------------------------------+--+-ChanNameY-----+
     |       |           | Channelname (header)              | clock |  | header height |
 ChanHeight--+-----------+-------------------------------------------+--+               |
                |                 B---O---D---Y                      |                  |InfoHeightY
                |01:23     Current Event                             |                  |
                |02:34     Next Event                                |                  |
                |                                                    |                  |
     BoxEndY----+----------------------------------------------------+--+---------------+
                |                     optional blinkenlights iconbar |  bottom_bar_offset
     BBarY------+----------------------------------------------------+--+
                | * red   * green  * yellow  * blue ====== [DD][16:9]|  InfoHeightY_Info
                +----------------------------------------------------+--+
                |              asize               |                 |
                                                             BoxEndX-/
*/
void CInfoViewer::start ()
{
	info_time_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth("22:22") + 10;

	InfoHeightY = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight() * 9/8 +
		      2 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight() + 25;
	infoViewerBB->Init();

	ChanWidth = std::max(125, 4 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getMaxDigitWidth() + 10);

	ChanHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight()/* * 9/8*/;
	ChanHeight += g_SignalFont->getHeight()/2;
	ChanHeight = std::max(75, ChanHeight);
	numbox_offset = 3;

	BoxStartX = g_settings.screen_StartX + 10;
	BoxEndX = g_settings.screen_EndX - 10;
	BoxEndY = g_settings.screen_EndY - 10 - infoViewerBB->InfoHeightY_Info - infoViewerBB->bottom_bar_offset;
	BoxStartY = BoxEndY - InfoHeightY - ChanHeight / 2;

	ChanNameY = BoxStartY + (ChanHeight / 2) + OFFSET_SHADOW;
	ChanInfoX = BoxStartX + (ChanWidth / 3);

	initClock();
	time_height = std::max(ChanHeight / 2, clock->getHeight());
	time_width = clock->getWidth();
}

void CInfoViewer::ResetPB()
{
	if (sigbox){
		delete sigbox;
		sigbox = NULL;
	}

	if (timescale){
		timescale->reset();
	}
}

void CInfoViewer::changePB()
{
	ResetPB();
	if (!timescale){
		timescale = new CProgressBar();
		timescale->setType(CProgressBar::PB_TIMESCALE);
	}
}

void CInfoViewer::initClock()
{

	int gradient_top = g_settings.theme.infobar_gradient_top;

	//basic init for clock object
	if (clock == NULL){
		clock = new CComponentsFrmClock();
		clock->setClockFormat("%H:%M", "%H %M");
	}

	CInfoClock::getInstance()->disableInfoClock();
	clock->clear();
	clock->enableColBodyGradient(gradient_top, COL_INFOBAR_PLUS_0);
	clock->doPaintBg(!gradient_top);
	clock->enableTboxSaveScreen(gradient_top);
	clock->setColorBody(COL_INFOBAR_PLUS_0);
	clock->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);
	clock->setClockFont(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]);
	clock->setPos(BoxEndX - 10 - clock->getWidth(), ChanNameY);
	clock->setTextColor(COL_INFOBAR_TEXT);
}


void CInfoViewer::showRecordIcon (const bool show)
{
	/* FIXME if record or timeshift stopped while infobar visible, artifacts */

	CRecordManager * crm		= CRecordManager::getInstance();
	
	recordModeActive		= crm->RecordingStatus();
	if (recordModeActive)
	{
		std::string rec_icon = NEUTRINO_ICON_REC_GRAY;
		std::string ts_icon  = NEUTRINO_ICON_AUTO_SHIFT_GRAY;

		t_channel_id cci	= g_RemoteControl->current_channel_id;
		/* global record mode */
		int rec_mode 		= crm->GetRecordMode();
		/* channel record mode */
		int ccrec_mode 		= crm->GetRecordMode(cci);

		/* set 'active' icons for current channel */
		if (ccrec_mode & CRecordManager::RECMODE_REC)
			rec_icon = NEUTRINO_ICON_REC;

		if (ccrec_mode & CRecordManager::RECMODE_TSHIFT)
			ts_icon  = NEUTRINO_ICON_AUTO_SHIFT;

		int records = crm->GetRecordCount();
		
		int txt_h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
		int txt_w = 0;

		int box_x = BoxStartX + ChanWidth + 2*OFFSET_SHADOW;
		int box_y = BoxStartY + OFFSET_SHADOW;
		int box_w = 0;
		int box_h = txt_h;

		int icon_space = OFFSET_SHADOW/2;

		int rec_icon_x = 0, rec_icon_w = 0, rec_icon_h = 0;
		int ts_icon_x  = 0, ts_icon_w  = 0, ts_icon_h  = 0;

		frameBuffer->getIconSize(rec_icon.c_str(), &rec_icon_w, &rec_icon_h);
		frameBuffer->getIconSize(ts_icon.c_str(), &ts_icon_w, &ts_icon_h);
		
		int icon_h = std::max(rec_icon_h, ts_icon_h);
		box_h = std::max(box_h, icon_h+icon_space*2);
		
		int icon_y = box_y + (box_h - icon_h)/2;
		int txt_y  = box_y + (box_h + txt_h)/2;

		char records_msg[8];
					
		if (rec_mode == CRecordManager::RECMODE_REC)
		{
			snprintf(records_msg, sizeof(records_msg)-1, "%d%s", records, "x");
			txt_w = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(records_msg);

			box_w = rec_icon_w + txt_w + icon_space*5;
			rec_icon_x = box_x + icon_space*2;
		}
		else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
		{
			box_w = ts_icon_w + icon_space*4;
			ts_icon_x = box_x + icon_space*2;
		}
		else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
		{
			//subtract ts
			records--;
			snprintf(records_msg, sizeof(records_msg)-1, "%d%s", records, "x");
			txt_w = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(records_msg);

			box_w = ts_icon_w + rec_icon_w + txt_w + icon_space*7;
			ts_icon_x = box_x + icon_space*2;
			rec_icon_x = ts_icon_x + ts_icon_w + icon_space*2;
		}
		
		if (show)
		{
			if (rec == NULL){ //TODO: full refactoring of this icon handler
				rec = new CComponentsShapeSquare(box_x, box_y , box_w, box_h, NULL, CC_SHADOW_ON, COL_RED, COL_INFOBAR_PLUS_0);
				rec->setFrameThickness(FRAME_WIDTH_NONE);
				rec->setShadowWidth(OFFSET_SHADOW/2);
				rec->setCorner(RADIUS_MIN, CORNER_ALL);
			}
			if (rec->getWidth() != box_w)
				rec->setWidth(box_w);

			if (!rec->isPainted())
				rec->paint(CC_SAVE_SCREEN_NO);
			
			if (rec_mode != CRecordManager::RECMODE_TSHIFT)
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rec_icon_x + rec_icon_w + icon_space, txt_y, txt_w, records_msg, COL_INFOBAR_TEXT);
			
			if (rec_mode == CRecordManager::RECMODE_REC)
			{
				frameBuffer->paintIcon(rec_icon, rec_icon_x, icon_y);
			}
			else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
			{
				frameBuffer->paintIcon(ts_icon, ts_icon_x, icon_y);
			}
			else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
			{
				frameBuffer->paintIcon(rec_icon, rec_icon_x, icon_y);
				frameBuffer->paintIcon(ts_icon, ts_icon_x, icon_y);
			}
		}
		else
		{
			if (rec_mode == CRecordManager::RECMODE_REC)
				frameBuffer->paintBoxRel(rec_icon_x, icon_y, rec_icon_w, icon_h, COL_INFOBAR_PLUS_0);
			else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
				frameBuffer->paintBoxRel(ts_icon_x, icon_y, ts_icon_w, icon_h, COL_INFOBAR_PLUS_0);
			else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
				frameBuffer->paintBoxRel(ts_icon_x, icon_y, ts_icon_w + rec_icon_w + icon_space*2, icon_h, COL_INFOBAR_PLUS_0);
		}
	}
}

void CInfoViewer::paintBackground(int col_NumBox)
{
	int c_rad_mid = RADIUS_MID;

	// background for channel name/logo and clock
	paintHead();

	// background for epg data
	paintBody();

	// number box
	int y_numbox = body->getYPos()-ChanHeight-OFFSET_SHADOW;
	if (numbox == NULL){ //TODO: move into an own member, paintNumBox() or so...
		numbox = new CComponentsShapeSquare(BoxStartX, y_numbox, ChanWidth, ChanHeight);
		numbox->enableShadow(CC_SHADOW_ON, OFFSET_SHADOW, true);
	}else
		numbox->setDimensionsAll(BoxStartX, y_numbox, ChanWidth, ChanHeight);
	numbox->setColorBody(g_settings.theme.infobar_gradient_top ? COL_MENUHEAD_PLUS_0 : col_NumBox);
	numbox->enableColBodyGradient(g_settings.theme.infobar_gradient_top, g_settings.theme.infobar_gradient_top ? COL_INFOBAR_PLUS_0 : col_NumBox, g_settings.theme.infobar_gradient_top_direction);
	numbox->setCorner(c_rad_mid, CORNER_ALL);
	numbox->paint(CC_SAVE_SCREEN_NO);
}

void CInfoViewer::paintHead()
{
	int head_x = BoxStartX+ChanWidth -1; /*Ugly: -1 to avoid background shine through round borders*/
	int head_w = BoxEndX-head_x;
	if (header == NULL){
		header = new CComponentsShapeSquare(head_x, ChanNameY, head_w, time_height, NULL, CC_SHADOW_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT | CC_SHADOW_CORNER_BOTTOM_RIGHT);
		header->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);
	}else
		header->setDimensionsAll(head_x, ChanNameY, head_w, time_height);

	header->setColorBody(g_settings.theme.infobar_gradient_top ? COL_MENUHEAD_PLUS_0 : COL_INFOBAR_PLUS_0);
	header->enableColBodyGradient(g_settings.theme.infobar_gradient_top, COL_INFOBAR_PLUS_0, g_settings.theme.infobar_gradient_top_direction);
	clock->setColorBody(header->getColorBody());

	header->paint(CC_SAVE_SCREEN_NO);
	header_height = header->getHeight();
}

void CInfoViewer::paintBody()
{
	int h_body = InfoHeightY - header_height - OFFSET_SHADOW;
	if(h_body < 0)
		h_body = 0;

	infoViewerBB->initBBOffset();
	if (!zap_mode)
		h_body += infoViewerBB->bottom_bar_offset;

	int y_body = ChanNameY + header_height;

	if (body == NULL){
		body = new CComponentsShapeSquare(ChanInfoX, y_body, BoxEndX-ChanInfoX, h_body);
	} else {
		if (txt_cur_event && txt_cur_start && txt_cur_event_rest &&
				txt_next_event && txt_next_start && txt_next_in) {
			if (h_body != body->getHeight() || y_body != body->getYPos()){
				txt_cur_start->getCTextBoxObject()->clearScreenBuffer();
				txt_cur_event->getCTextBoxObject()->clearScreenBuffer();
				txt_cur_event_rest->getCTextBoxObject()->clearScreenBuffer();
				txt_next_start->getCTextBoxObject()->clearScreenBuffer();
				txt_next_event->getCTextBoxObject()->clearScreenBuffer();
				txt_next_in->getCTextBoxObject()->clearScreenBuffer();
			}
		}
		body->setDimensionsAll(ChanInfoX, y_body, BoxEndX-ChanInfoX, h_body);
	}

	//set corner and shadow modes, consider virtual zap mode
	body->setCorner(RADIUS_LARGE, (zap_mode) ? CORNER_BOTTOM : CORNER_NONE);
	body->enableShadow(zap_mode ? CC_SHADOW_ON : CC_SHADOW_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT | CC_SHADOW_CORNER_BOTTOM_RIGHT);

	body->setColorBody(g_settings.theme.infobar_gradient_body ? COL_MENUHEAD_PLUS_0 : COL_INFOBAR_PLUS_0);
	body->enableColBodyGradient(g_settings.theme.infobar_gradient_body, COL_INFOBAR_PLUS_0, g_settings.theme.infobar_gradient_body_direction);

	body->paint(CC_SAVE_SCREEN_NO);
}

void CInfoViewer::show_current_next(bool new_chan, int  epgpos)
{
	CEitManager::getInstance()->getCurrentNextServiceKey(current_epg_id, info_CurrentNext);
	if (!evtlist.empty()) {
		if (new_chan) {
			for ( eli=evtlist.begin(); eli!=evtlist.end(); ++eli ) {
				if ((uint)eli->startTime >= info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)
					break;
			}
			if (eli == evtlist.end()) // the end is not valid, so go back
				--eli;
		}

		if (epgpos != 0) {
			info_CurrentNext.flags = 0;
			if ((epgpos > 0) && (eli != evtlist.end())) {
				++eli; // next epg
				if (eli == evtlist.end()) // the end is not valid, so go back
					--eli;
			}
			else if ((epgpos < 0) && (eli != evtlist.begin())) {
				--eli; // prev epg
			}
			info_CurrentNext.flags = CSectionsdClient::epgflags::has_current;
			info_CurrentNext.current_uniqueKey      = eli->eventID;
			info_CurrentNext.current_zeit.startzeit = eli->startTime;
			info_CurrentNext.current_zeit.dauer     = eli->duration;
			if (eli->description.empty())
				info_CurrentNext.current_name   = g_Locale->getText(LOCALE_INFOVIEWER_NOEPG);
			else
				info_CurrentNext.current_name   = eli->description;
			info_CurrentNext.current_fsk            = '\0';

			if (eli != evtlist.end()) {
				++eli;
				if (eli != evtlist.end()) {
					info_CurrentNext.flags                  = CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_next;
					info_CurrentNext.next_uniqueKey         = eli->eventID;
					info_CurrentNext.next_zeit.startzeit    = eli->startTime;
					info_CurrentNext.next_zeit.dauer        = eli->duration;
					if (eli->description.empty())
						info_CurrentNext.next_name      = g_Locale->getText(LOCALE_INFOVIEWER_NOEPG);
					else
						info_CurrentNext.next_name      = eli->description;
				}
				--eli;
			}
		}
	}

	if (!(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_later | CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::not_broadcast))) {
		neutrino_locale_t loc;
		if (!timeset)
			loc = LOCALE_INFOVIEWER_WAITTIME;
		else if (showButtonBar)
			loc = LOCALE_INFOVIEWER_EPGWAIT;
		else
			loc = LOCALE_INFOVIEWER_EPGNOTLOAD;

		_livestreamInfo1.clear();
		_livestreamInfo2.clear();
		if (!showLivestreamInfo())
			display_Info(g_Locale->getText(loc), NULL);
	} else {
		show_Data ();
	}
}

void CInfoViewer::showMovieTitle(const int playState, const t_channel_id &Channel_Id, const std::string &Channel,
				 const std::string &g_file_epg, const std::string &g_file_epg1,
				 const int duration, const int curr_pos,
				 const int repeat_mode, const int _zap_mode)
{
	if (g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_LEFT || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_RIGHT || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_CENTER || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_HIGHER_CENTER)
		isVolscale = CVolume::getInstance()->hideVolscale();
	else
		isVolscale = false;

	check_channellogo_ca_SettingsChange();
	aspectRatio = 0;
	last_curr_id = last_next_id = 0;
	showButtonBar = true;
	fileplay = true;
	zap_mode = _zap_mode;
	reset_allScala();

	if (g_settings.radiotext_enable && g_Radiotext) {
		g_Radiotext->RT_MsgShow = true;
	}

	if(!is_visible)
		fader.StartFadeIn();

	is_visible = true;
	infoViewerBB->is_visible = true;

	ChannelName = Channel;
	t_channel_id old_channel_id = current_channel_id;
	current_channel_id = Channel_Id;

	/* showChannelLogo() changes this, so better reset it every time... */
	ChanNameX = BoxStartX + ChanWidth + OFFSET_SHADOW;

	paintBackground(COL_INFOBAR_PLUS_0);

	bool show_dot = true;
	if (timeset)
		clock->paint(CC_SAVE_SCREEN_NO);
	showRecordIcon (show_dot);
	show_dot = !show_dot;

	if (!zap_mode)
		infoViewerBB->paintshowButtonBar();

	int renderFlag = ((g_settings.theme.infobar_gradient_top) ? Font::FULLBG : 0) | Font::IS_UTF8;
	int ChannelLogoMode = 0;
	if (g_settings.infobar_show_channellogo > 1)
		ChannelLogoMode = showChannelLogo(current_channel_id, 0);
	if (ChannelLogoMode == 0 || ChannelLogoMode == 3 || ChannelLogoMode == 4)
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(ChanNameX + 10 , ChanNameY + header_height,BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 10 ,ChannelName, COL_INFOBAR_TEXT, 0, renderFlag);

	// show_Data
	if (CMoviePlayerGui::getInstance().file_prozent > 100)
		CMoviePlayerGui::getInstance().file_prozent = 100;

	const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
 	char runningRest[32]; // %d can be 10 digits max...
	snprintf(runningRest, sizeof(runningRest), "%d / %d %s", (curr_pos + 30000) / 60000, (duration - curr_pos + 30000) / 60000, unit_short_minute);
	display_Info(g_file_epg.c_str(), g_file_epg1.c_str(), false, CMoviePlayerGui::getInstance().file_prozent, NULL, runningRest);

	int speed = CMoviePlayerGui::getInstance().GetSpeed();
	const char *playicon = NULL;
	switch (playState) {
	case CMoviePlayerGui::PLAY:
		switch (repeat_mode) {
		case CMoviePlayerGui::REPEAT_ALL:
			playicon = NEUTRINO_ICON_PLAY_REPEAT_ALL;
			break;
		case CMoviePlayerGui::REPEAT_TRACK:
			playicon = NEUTRINO_ICON_PLAY_REPEAT_TRACK;
			break;
		default:
			playicon = NEUTRINO_ICON_PLAY;
		}
		speed = 0;
		break;
	case CMoviePlayerGui::PAUSE:
		playicon = NEUTRINO_ICON_PAUSE;
		break;
	case CMoviePlayerGui::REW:
		playicon = NEUTRINO_ICON_REW;
		speed = abs(speed);
		break;
	case CMoviePlayerGui::FF:
		playicon = NEUTRINO_ICON_FF;
		speed = abs(speed);
		break;
	default:
		/* NULL crashes in getIconSize, just use something */
		playicon = NEUTRINO_ICON_BUTTON_HELP;
		break;
	}
	int icon_w = 0,icon_h = 0;
	frameBuffer->getIconSize(playicon, &icon_w, &icon_h);
	int speedw = 0;
	if (speed) {
		sprintf(runningRest, "%dx", speed);
		speedw = 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(runningRest);
		icon_w += speedw;
	}
	int icon_x = BoxStartX + ChanWidth / 2 - icon_w / 2;
	int icon_y = BoxStartY + ChanHeight / 2 - icon_h / 2;
	if (speed) {
		int sh = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight();
		int sy = BoxStartY + ChanHeight/2 - sh/2 + sh;
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(icon_x, sy, ChanHeight, runningRest, COL_INFOBAR_TEXT, 0, renderFlag);
		icon_x += speedw;
	}
	frameBuffer->paintIcon(playicon, icon_x, icon_y);
	showLcdPercentOver ();
	showInfoFile();
	//loop(fadeValue, show_dot , fadeIn);
	loop(show_dot);
	aspectRatio = 0;
	fileplay = 0;
	current_channel_id = old_channel_id;
}

void CInfoViewer::reset_allScala()
{
	changePB();
	lastsig = lastsnr = -1;
	infoViewerBB->changePB();
	infoViewerBB->reset_allScala();
	if(!clock)
		initClock();
}

void CInfoViewer::check_channellogo_ca_SettingsChange()
{
	if (casysChange != g_settings.infobar_casystem_display || channellogoChange != g_settings.infobar_show_channellogo) {
		casysChange = g_settings.infobar_casystem_display;
		channellogoChange = g_settings.infobar_show_channellogo;
		infoViewerBB->initBBOffset();
		start();
	}
}

void CInfoViewer::showTitle(t_channel_id chid, const bool calledFromNumZap, int epgpos, bool forcePaintButtonBar/*=false*/)
{
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(chid);

	if(channel)
		showTitle(channel, calledFromNumZap, epgpos, forcePaintButtonBar);
}

void CInfoViewer::showTitle(CZapitChannel * channel, const bool calledFromNumZap, int epgpos, bool forcePaintButtonBar/*=false*/)
{
	if(!calledFromNumZap && !(zap_mode & IV_MODE_DEFAULT))
		resetSwitchMode();
	int renderFlag = ((g_settings.theme.infobar_gradient_top) ? Font::FULLBG : 0) | Font::IS_UTF8;

	std::string Channel = channel->getName();
	t_satellite_position satellitePosition = channel->getSatellitePosition();
	t_channel_id new_channel_id = channel->getChannelID();
	int ChanNum = channel->number;

	current_epg_id = channel->getEpgID();

	if (g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_LEFT || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_RIGHT || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_BOTTOM_CENTER || 
	    g_settings.volume_pos == CVolumeBar::VOLUMEBAR_POS_HIGHER_CENTER)
		isVolscale = CVolume::getInstance()->hideVolscale();
	else
		isVolscale = false;

	check_channellogo_ca_SettingsChange();
	aspectRatio = 0;
	last_curr_id = last_next_id = 0;
	showButtonBar = (!calledFromNumZap || forcePaintButtonBar);
	bool noTimer  = (calledFromNumZap && forcePaintButtonBar);

	fileplay = (ChanNum == 0);
	newfreq = true;

	reset_allScala();

	if(!is_visible && !calledFromNumZap)
		fader.StartFadeIn();

	is_visible = true;
	infoViewerBB->is_visible = true;

	fb_pixel_t col_NumBoxText = COL_INFOBAR_TEXT;
	fb_pixel_t col_NumBox = COL_INFOBAR_PLUS_0;
	ChannelName = Channel;
	bool new_chan = false;

	if (zap_mode & IV_MODE_VIRTUAL_ZAP) {
		if (g_RemoteControl->current_channel_id != new_channel_id) {
			col_NumBoxText = COL_MENUHEAD_TEXT;
		}
		if ((current_channel_id != new_channel_id) || (evtlist.empty())) {
			CEitManager::getInstance()->getEventsServiceKey(current_epg_id, evtlist);
			if (!evtlist.empty())
				sort(evtlist.begin(),evtlist.end(), sortByDateTime);
			new_chan = true;
		}
	}
	if (! calledFromNumZap && !(g_RemoteControl->subChannels.empty()) && (g_RemoteControl->selected_subchannel > 0))
	{
		current_channel_id = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].getChannelID();
		ChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
	} else {
		current_channel_id = new_channel_id;
	}

	/* showChannelLogo() changes this, so better reset it every time... */
	ChanNameX = BoxStartX + ChanWidth + OFFSET_SHADOW;


	paintBackground(col_NumBox);

	bool show_dot = true;
	if (timeset)
		clock->paint(CC_SAVE_SCREEN_NO);
	showRecordIcon (show_dot);
	show_dot = !show_dot;

	if (showButtonBar) {
		infoViewerBB->paintshowButtonBar(noTimer);
	}

	int ChanNumWidth = 0;
	int ChannelLogoMode = 0;
	bool logo_ok = false;
	if (ChanNum) /* !fileplay */
	{
		char strChanNum[10];
		snprintf (strChanNum, sizeof(strChanNum), "%d", ChanNum);
		const int channel_number_width =(g_settings.infobar_show_channellogo == 6) ? 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (strChanNum) : 0;
		ChannelLogoMode = showChannelLogo(current_channel_id,channel_number_width); // get logo mode, paint channel logo if adjusted
		logo_ok = ( g_settings.infobar_show_channellogo != 0 && ChannelLogoMode != 0);
		//fprintf(stderr, "after showchannellogo, mode = %d ret = %d logo_ok = %d\n",g_settings.infobar_show_channellogo, ChannelLogoMode, logo_ok);

		if (g_settings.infobar_sat_display) {
			// TODO split into WebTV/WebRadio
			std::string name = (IS_WEBCHAN(current_channel_id))? "Web-Channel" : CServiceManager::getInstance()->GetSatelliteName(satellitePosition);
			int satNameWidth = g_SignalFont->getRenderWidth (name);
			std::string satname_tmp = name;
			if (satNameWidth > (ChanWidth - numbox_offset*2)) {
				satNameWidth = ChanWidth - numbox_offset*2;
				size_t pos1 = name.find("(") ;
				size_t pos2 = name.find_last_of(")");
				size_t pos0 = name.find(" ") ;
				if ((pos1 != std::string::npos) && (pos2 != std::string::npos) && (pos0 != std::string::npos)) {
					pos1++;
					satname_tmp = name.substr(0, pos0 );

					if(satname_tmp == "Hot")
						satname_tmp = "Hotbird";

					satname_tmp +=" ";
					satname_tmp += name.substr( pos1,pos2-pos1 );
					satNameWidth = g_SignalFont->getRenderWidth (satname_tmp);
					if (satNameWidth > (ChanWidth - numbox_offset*2))
						satNameWidth = ChanWidth - numbox_offset*2;
				}
			}
			int h_sfont = g_SignalFont->getHeight();
			g_SignalFont->RenderString (BoxStartX + numbox_offset + ((ChanWidth - satNameWidth) / 2) , numbox->getYPos() + h_sfont, satNameWidth, satname_tmp, COL_INFOBAR_TEXT, 0, renderFlag);
		}

		/* TODO: the logic will get much easier once we decouple channellogo and signal bars */
		if ((!logo_ok && g_settings.infobar_show_channellogo < 2) || g_settings.infobar_show_channellogo == 2 || g_settings.infobar_show_channellogo == 4) // no logo in numberbox
		{
			// show number in numberbox
			int h_tmp 	= numbox->getHeight();
			int y_tmp 	= numbox->getYPos() + 5*100/h_tmp; //5%
			if (g_settings.infobar_sat_display){
				int h_sfont = g_SignalFont->getHeight();
				h_tmp -= h_sfont;
				y_tmp += h_sfont;
			}
			y_tmp += h_tmp/2 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight()/2;
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->RenderString(BoxStartX + numbox_offset + (ChanWidth-g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getRenderWidth(strChanNum))/2,
											y_tmp,
											ChanWidth - 2*numbox_offset,
											strChanNum,
											col_NumBoxText, 0, renderFlag);
		}
		if (ChannelLogoMode == 1 || ( g_settings.infobar_show_channellogo == 3 && !logo_ok) || g_settings.infobar_show_channellogo == 6 ) /* channel number besides channel name */
		{
			ChanNumWidth = 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (strChanNum);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(
				ChanNameX + 5, ChanNameY + header_height,
				ChanNumWidth, strChanNum, col_NumBoxText, 0, renderFlag);
		}
	}

	if (g_settings.infobar_show_channellogo < 5 || !logo_ok) {
		if (ChannelLogoMode != 2) {
			//FIXME good color to display inactive for zap ?
			//fb_pixel_t color = CNeutrinoApp::getInstance ()->channelList->SameTP(new_channel_id) ? COL_INFOBAR_TEXT : COL_MENUFOOT_TEXT;
			fb_pixel_t color = COL_INFOBAR_TEXT;
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(
				ChanNameX + 10 + ChanNumWidth, ChanNameY + header_height,
				BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 10 - ChanNumWidth,
				ChannelName, color /*COL_INFOBAR_TEXT*/, 0, renderFlag);
			//provider name
			if(g_settings.infobar_show_channeldesc && channel->pname){
				std::string prov_name = channel->pname;
				prov_name=prov_name.substr(prov_name.find_first_of("]")+1);

				int chname_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (ChannelName);
				unsigned int chann_size = ChannelName.size();
				if(ChannelName.empty())
					chann_size = 1;
				chname_width += (chname_width/chann_size/2);

				int tmpY = ((ChanNameY + header_height) - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getDigitOffset()
						+ g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getDigitOffset());
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(
					ChanNameX + 10 + ChanNumWidth + chname_width, tmpY,
					BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 10 - ChanNumWidth - chname_width,
					prov_name, color /*COL_INFOBAR_TEXT*/, 0, renderFlag);
			}

		}
	}

	if (fileplay) {
		show_Data ();
#if 0
	} else if (IS_WEBCHAN(new_channel_id)) {
		if (channel) {
			const char *current = channel->getDesc().c_str();
			const char *next = channel->getUrl().c_str();
			if (!current) {
				current = next;
				next = "";
			}
			display_Info(current, next, false, 0, NULL, NULL, NULL, NULL, true, true);
		}
#endif
	} else {
		show_current_next(new_chan, epgpos);
	}

	showLcdPercentOver ();
	showInfoFile();

	// Radiotext
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_radio || CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio)
	{
		if ((g_settings.radiotext_enable) && (!recordModeActive) && (!calledFromNumZap))
			showRadiotext();
		else if (showButtonBar)
			infoViewerBB->showIcon_RadioText(false);
	}

	if (!calledFromNumZap) {
		//loop(fadeValue, show_dot , fadeIn);
		loop(show_dot);
	}
	aspectRatio = 0;
	fileplay = 0;
}
void CInfoViewer::setInfobarTimeout(int timeout_ext)
{
	int mode = CNeutrinoApp::getInstance()->getMode();
	//define timeouts
	switch (mode)
	{
		case NeutrinoModes::mode_radio:
		case NeutrinoModes::mode_webradio:
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_RADIO] + timeout_ext);
				break;
		case NeutrinoModes::mode_ts:
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_MOVIE] + timeout_ext);
				break;
		case NeutrinoModes::mode_tv:
		case NeutrinoModes::mode_webtv:
		default:
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] + timeout_ext);
				break;
	}
}

bool CInfoViewer::showLivestreamInfo()
{
	CZapitChannel * cc = CZapit::getInstance()->GetCurrentChannel();
	bool web_mode = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv || CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio);
	if (web_mode && cc->getEpgID() == 0)
	{
		std::string livestreamInfo1 = "";
		std::string livestreamInfo2 = "";

		if (!cc->getScriptName().empty())
		{
			std::string tmp1 = "";
			CMoviePlayerGui::getInstance().getLivestreamInfo(&livestreamInfo1, &tmp1);

			if (!(videoDecoder->getBlank()))
			{
				int xres, yres, framerate;
				std::string tmp2;
				videoDecoder->getPictureInfo(xres, yres, framerate);
				switch (framerate)
				{
				case 0:
					tmp2 = "23.976fps";
					break;
				case 1:
					tmp2 = "24fps";
					break;
				case 2:
					tmp2 = "25fps";
					break;
				case 3:
					tmp2 = "29,976fps";
					break;
				case 4:
					tmp2 = "30fps";
					break;
				case 5:
					tmp2 = "50fps";
					break;
				case 6:
					tmp2 = "50,94fps";
					break;
				case 7:
					tmp2 = "60fps";
					break;
				default:
					tmp2 = g_Locale->getText(LOCALE_STREAMINFO_FRAMERATE_UNKNOWN);
					break;
				}
				livestreamInfo2 = to_string(xres) + "x" + to_string(yres) + ", " + tmp2;
				if (!tmp1.empty())
					livestreamInfo2 += (std::string)", " + tmp1;
			}
		}
		else
		{
			// try to get meta data
			std::string artist = "";
			std::string title = "";
			std::vector<std::string> keys, values;
			cPlayback *playback = CMoviePlayerGui::getInstance().getPlayback();
			if (playback)
				playback->GetMetadata(keys, values);
			size_t count = keys.size();
			if (count > 0)
			{
				for (size_t i = 0; i < count; i++)
				{
					std::string key = trim(keys[i]);
					if (!strcasecmp("artist", key.c_str()))
					{
						artist = isUTF8(values[i]) ? values[i] : convertLatin1UTF8(values[i]);
						continue;
					}
					if (!strcasecmp("title", key.c_str()))
					{
						title = isUTF8(values[i]) ? values[i] : convertLatin1UTF8(values[i]);
						continue;
					}
				}
			}
			if (!artist.empty())
			{
				livestreamInfo1 = artist;
			}
			if (!title.empty())
			{
				if (!livestreamInfo1.empty())
					livestreamInfo1 += " - ";
				livestreamInfo1 += title;
			}
		}

		if (livestreamInfo1 != _livestreamInfo1 || livestreamInfo2 != _livestreamInfo2)
		{
			display_Info(livestreamInfo1.c_str(), livestreamInfo2.c_str(), false);
			_livestreamInfo1 = livestreamInfo1;
			_livestreamInfo2 = livestreamInfo2;
			infoViewerBB->showBBButtons(true /*paintFooter*/);
		}
		return true;
	}
	return false;
}

void CInfoViewer::loop(bool show_dot)
{
	bool hideIt = true;
	resetSwitchMode(); //no virtual zap
	//bool fadeOut = false;
	timeoutEnd=0;;
	setInfobarTimeout();

	int res = messages_return::none;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	if (isVolscale)
		CVolume::getInstance()->showVolscale();

	_livestreamInfo1.clear();
	_livestreamInfo2.clear();

	while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) {
		g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);

		showLivestreamInfo();

#ifdef ENABLE_PIP
		if ((msg == (neutrino_msg_t) g_settings.key_pip_close) || 
		    (msg == (neutrino_msg_t) g_settings.key_pip_setup) || 
		    (msg == (neutrino_msg_t) g_settings.key_pip_swap)) {
			g_RCInput->postMsg(msg, data);
			res = messages_return::cancel_info;
		} else
#endif
		if (msg == (neutrino_msg_t) g_settings.key_screenshot) {
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);

		} else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			g_RCInput->postMsg (msg, 0);
			res = messages_return::cancel_info;
		} else if (msg == CRCInput::RC_help || msg == CRCInput::RC_info) {
			if (fileplay)
			{
				CMoviePlayerGui::getInstance().setFromInfoviewer(true);
				g_RCInput->postMsg (msg, data);
				hideIt = true;
			}
			else
				g_RCInput->postMsg (NeutrinoMessages::SHOW_EPG, 0);
			res = messages_return::cancel_info;
		} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer())) {
			if(fader.FadeDone())
				res = messages_return::cancel_info;
		} else if ((msg == CRCInput::RC_ok) || (msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) {
			if ((g_settings.mode_left_right_key_tv == SNeutrinoSettings::VZAP) && (msg == CRCInput::RC_ok))
			{
				if (fileplay)
				{
					// in movieplayer mode process vzap keys in movieplayer.cpp
					//printf("%s:%d: imitate VZAP; RC_ok\n", __func__, __LINE__);
					CMoviePlayerGui::getInstance().setFromInfoviewer(true);
					g_RCInput->postMsg (msg, data);
					hideIt = true;
				}
			}
			if(fader.StartFadeOut())
				timeoutEnd = CRCInput::calcTimeoutEnd(1);
			else
				res = messages_return::cancel_info;
		} else if ((g_settings.mode_left_right_key_tv == SNeutrinoSettings::VZAP) && ((msg == CRCInput::RC_right) || (msg == CRCInput::RC_left ))) {
			if (fileplay)
			{
				// in movieplayer mode process vzap keys in movieplayer.cpp
				//printf("%s:%d: imitate VZAP; RC_left/right\n", __func__, __LINE__);
				CMoviePlayerGui::getInstance().setFromInfoviewer(true);
				g_RCInput->postMsg (msg, data);
			}
			else
				setSwitchMode(IV_MODE_VIRTUAL_ZAP);
			res = messages_return::cancel_all;
			hideIt = true;
		} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == sec_timer_id)) {
			showSNR ();
			if (timeset)
				clock->paint(CC_SAVE_SCREEN_NO);
			showRecordIcon (show_dot);
			show_dot = !show_dot;
			showInfoFile();
			if ((g_settings.radiotext_enable) && (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_radio))
				showRadiotext();

			infoViewerBB->showIcon_16_9();
			//infoViewerBB->showIcon_CA_Status(0);
			infoViewerBB->showIcon_Resolution();
		} else if ((msg == NeutrinoMessages::EVT_RECORDMODE) && 
			   (CMoviePlayerGui::getInstance().timeshift) && (CRecordManager::getInstance()->GetRecordCount() == 1)) {
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
		} else if (!fileplay && !CMoviePlayerGui::getInstance().timeshift) {
			CNeutrinoApp *neutrino = CNeutrinoApp::getInstance ();
			if ((msg == (neutrino_msg_t) g_settings.key_quickzap_up) || (msg == (neutrino_msg_t) g_settings.key_quickzap_down) || (msg == CRCInput::RC_0) || (msg == NeutrinoMessages::SHOW_INFOBAR)) {
				hideIt = false; // default
				if ((g_settings.radiotext_enable) && (neutrino->getMode() == NeutrinoModes::mode_radio))
					hideIt =  true;

				int rec_mode = CRecordManager::getInstance()->GetRecordMode();
				/* hide, if record (not timeshift only) is running -> neutrino will show channel list */
				if (rec_mode & CRecordManager::RECMODE_REC)
					hideIt = true;

				g_RCInput->postMsg (msg, data);
				res = messages_return::cancel_info;
			} else if (msg == NeutrinoMessages::EVT_TIMESET) {
				/* handle timeset event in upper layer, ignore here */
				res = neutrino->handleMsg (msg, data);
			} else {
				if (msg == CRCInput::RC_standby) {
					g_RCInput->killTimer (sec_timer_id);
					fader.StopFade();
				}
				res = neutrino->handleMsg (msg, data);
				if (res & messages_return::unhandled) {
					// raus hier und im Hauptfenster behandeln...
					g_RCInput->postMsg (msg, data);
					res = messages_return::cancel_info;
				}
			}
		} else if (fileplay || CMoviePlayerGui::getInstance().timeshift) {

			/* this debug message will only hit in movieplayer mode, where console is
			 * spammed to death anyway... */
			printf("%s:%d msg->MP: %08lx, data: %08lx\n", __func__, __LINE__, (long)msg, (long)data);

			bool volume_keys = (
				   msg == CRCInput::RC_spkr
				|| msg == (neutrino_msg_t) g_settings.key_volumeup
				|| msg == (neutrino_msg_t) g_settings.key_volumedown
			);

			if (msg < CRCInput::RC_Events && !volume_keys)
			{
				g_RCInput->postMsg (msg, data);
				res = messages_return::cancel_info;
			}
			else
				res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
	}

	if (hideIt) {
		CVolume::getInstance()->hideVolscale();
		killTitle ();
	}

	g_RCInput->killTimer (sec_timer_id);
	fader.StopFade();
	if (zap_mode & IV_MODE_VIRTUAL_ZAP) {
		/* if bouquet cycle set, do virtual over current bouquet */
		if (/*g_settings.zap_cycle && */ /* (bouquetList != NULL) && */ !(bouquetList->Bouquets.empty()))
			bouquetList->Bouquets[bouquetList->getActiveBouquetNumber()]->channelList->virtual_zap_mode(msg == CRCInput::RC_right);
		else
			CNeutrinoApp::getInstance()->channelList->virtual_zap_mode(msg == CRCInput::RC_right);
	}
}

void CInfoViewer::showSubchan ()
{
	CFrameBuffer *lframeBuffer = CFrameBuffer::getInstance ();
	CNeutrinoApp *neutrino = CNeutrinoApp::getInstance ();

	std::string subChannelName;	// holds the name of the subchannel/audio channel
	int subchannel = 0;				// holds the channel index
	const int borderwidth = 4;

	if (!(g_RemoteControl->subChannels.empty ())) {
		// get info for nvod/subchannel
		subchannel = g_RemoteControl->selected_subchannel;
		if (g_RemoteControl->selected_subchannel >= 0)
			subChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
	} else if (g_RemoteControl->current_PIDs.APIDs.size () > 1 && g_settings.audiochannel_up_down_enable) {
		// get info for audio channel
		subchannel = g_RemoteControl->current_PIDs.PIDs.selected_apid;
		subChannelName = g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].desc;
	}

	if (!(subChannelName.empty ())) {
		if ( g_settings.infobar_subchan_disp_pos == 4 ) {
			g_RCInput->postMsg( NeutrinoMessages::SHOW_INFOBAR , 0 );
		} else {
			char text[100];
			snprintf (text, sizeof(text), "%d - %s", subchannel, subChannelName.c_str ());

			int dx = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth (text) + 20;
			int dy = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight(); // 25;

			if (g_RemoteControl->director_mode) {
				int w = 20 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth (g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE)) + 20;
				int h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
				if (w > dx)
					dx = w;
				dy = dy + h + 5; //dy * 2;
			} else
				dy = dy + 5;

			int x = 0, y = 0;
			if (g_settings.infobar_subchan_disp_pos == 0) {
				// Rechts-Oben
				x = g_settings.screen_EndX - dx - 10;
				y = g_settings.screen_StartY + 10;
			} else if (g_settings.infobar_subchan_disp_pos == 1) {
				// Links-Oben
				x = g_settings.screen_StartX + 10;
				y = g_settings.screen_StartY + 10;
			} else if (g_settings.infobar_subchan_disp_pos == 2) {
				// Links-Unten
				x = g_settings.screen_StartX + 10;
				y = g_settings.screen_EndY - dy - 10;
			} else if (g_settings.infobar_subchan_disp_pos == 3) {
				// Rechts-Unten
				x = g_settings.screen_EndX - dx - 10;
				y = g_settings.screen_EndY - dy - 10;
			}

			fb_pixel_t pixbuf[(dx + 2 * borderwidth) * (dy + 2 * borderwidth)];
			lframeBuffer->SaveScreen (x - borderwidth, y - borderwidth, dx + 2 * borderwidth, dy + 2 * borderwidth, pixbuf);

			// clear border
			lframeBuffer->paintBackgroundBoxRel (x - borderwidth, y - borderwidth, dx + 2 * borderwidth, borderwidth);
			lframeBuffer->paintBackgroundBoxRel (x - borderwidth, y + dy, dx + 2 * borderwidth, borderwidth);
			lframeBuffer->paintBackgroundBoxRel (x - borderwidth, y, borderwidth, dy);
			lframeBuffer->paintBackgroundBoxRel (x + dx, y, borderwidth, dy);

			lframeBuffer->paintBoxRel (x, y, dx, dy, COL_MENUCONTENT_PLUS_0);
			//g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + 30, dx - 20, text, COL_MENUCONTENT_TEXT);

			if (g_RemoteControl->director_mode) {
				lframeBuffer->paintIcon (NEUTRINO_ICON_BUTTON_YELLOW, x + 8, y + dy - 20);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (x + 30, y + dy - 2, dx - 40, g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE), COL_MENUCONTENT_TEXT);
				int h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + dy - h - 2, dx - 20, text, COL_MENUCONTENT_TEXT);
			} else
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + dy - 2, dx - 20, text, COL_MENUCONTENT_TEXT);

			uint64_t timeoutEnd_tmp = CRCInput::calcTimeoutEnd(2);
			int res = messages_return::none;

			neutrino_msg_t msg;
			neutrino_msg_data_t data;

			while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) {
				g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd_tmp);

				if (msg == CRCInput::RC_timeout) {
					res = messages_return::cancel_info;
				} else {
					res = neutrino->handleMsg (msg, data);

					if (res & messages_return::unhandled) {
						// raus hier und im Hauptfenster behandeln...
						g_RCInput->postMsg (msg, data);
						res = messages_return::cancel_info;
					}
				}
			}
			lframeBuffer->RestoreScreen (x - borderwidth, y - borderwidth, dx + 2 * borderwidth, dy + 2 * borderwidth, pixbuf);
		}
	} else {
		g_RCInput->postMsg (NeutrinoMessages::SHOW_INFOBAR, 0);
	}
}

void CInfoViewer::showFailure ()
{
	ShowHint (LOCALE_MESSAGEBOX_ERROR, g_Locale->getText (LOCALE_INFOVIEWER_NOTAVAILABLE), 430);	// UTF-8
}

void CInfoViewer::showMotorMoving (int duration)
{
	setInfobarTimeout(duration + 1);

	char text[256];
	snprintf(text, sizeof(text), "%s (%ds)", g_Locale->getText (LOCALE_INFOVIEWER_MOTOR_MOVING), duration);
	ShowHint (LOCALE_MESSAGEBOX_INFO, text, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth (text) + 10, duration);	// UTF-8
}

void CInfoViewer::killRadiotext()
{
	if (g_Radiotext->S_RtOsd)
		frameBuffer->paintBackgroundBoxRel(rt_x, rt_y, rt_w + OFFSET_SHADOW, rt_h + OFFSET_SHADOW);
	rt_x = rt_y = rt_h = rt_w = 0;
	CInfoClock::getInstance()->enableInfoClock(true);
}

void CInfoViewer::showRadiotext()
{
	/*
	   Maybe there's a nice CComponents solution with user's gradients.
	*/

	if (g_Radiotext == NULL)
		return;

	if (showButtonBar)
		infoViewerBB->showIcon_RadioText(g_Radiotext->haveRadiotext());

	char stext[3][100];
	bool RTisUTF8 = false;

	if (g_Radiotext->S_RtOsd)
	{
		CInfoClock::getInstance()->enableInfoClock(false);
		int rt_font = SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO;
		int item_h = g_Font[rt_font]->getHeight();

		// dimensions of radiotext window
		rt_x = BoxStartX;
		rt_y = g_settings.screen_StartY + OFFSET_INNER_MID;
		rt_w = BoxEndX - BoxStartX;
		rt_h = (g_Radiotext->S_RtOsdRows + 1)*item_h + 4*OFFSET_INNER_SMALL;
		
		int item_x = rt_x + OFFSET_INNER_MID;
		int item_y = rt_y + OFFSET_INNER_SMALL + item_h;
		int item_w = rt_w - 2*OFFSET_INNER_MID;

		int item = 0;
		int lines = 0;
		for (int i = 0; i < g_Radiotext->S_RtOsdRows; i++)
		{
			if (g_Radiotext->RT_Text[i][0] != '\0')
				lines++;
		}
		if (lines == 0)
			frameBuffer->paintBackgroundBoxRel(rt_x, rt_y, rt_w + OFFSET_SHADOW, rt_h + OFFSET_SHADOW);

		if (g_Radiotext->RT_MsgShow)
		{
			// Title
			if (g_Radiotext->S_RtOsdTitle == 1)
			{
				if (lines || g_Radiotext->RT_PTY != 0)
				{
					sprintf(stext[0], g_Radiotext->RT_PTY == 0 ? "%s %s%s" : "%s (%s)%s", tr("Radiotext"), g_Radiotext->RT_PTY == 0 ? g_Radiotext->RDS_PTYN : g_Radiotext->ptynr2string(g_Radiotext->RT_PTY), ":");
					int title_w = g_Font[rt_font]->getRenderWidth(stext[0]) + 2*OFFSET_INNER_MID;

					g_Font[rt_font]->RenderString(item_x, item_y, title_w, stext[0], COL_INFOBAR_TEXT, 0, RTisUTF8);
				}
				item = 1;
			}
			// Body
			if (lines)
			{
				frameBuffer->paintBoxRel(rt_x + OFFSET_SHADOW, rt_y + 2*OFFSET_INNER_SMALL + item_h + OFFSET_SHADOW, rt_w, item_h*g_Radiotext->S_RtOsdRows + 2*OFFSET_INNER_SMALL, COL_SHADOW_PLUS_0, RADIUS_LARGE, item == 1 ? CORNER_TOP_RIGHT|CORNER_BOTTOM : CORNER_ALL);
				frameBuffer->paintBoxRel(rt_x, rt_y + 2*OFFSET_INNER_SMALL + item_h, rt_w, item_h*g_Radiotext->S_RtOsdRows + 2*OFFSET_INNER_SMALL, COL_INFOBAR_PLUS_0, RADIUS_LARGE, item == 1 ? CORNER_TOP_RIGHT|CORNER_BOTTOM : CORNER_ALL);

				item_y += 2*OFFSET_INNER_SMALL;

				// RT-Text roundloop
				int index = (g_Radiotext->RT_Index == 0) ? g_Radiotext->S_RtOsdRows - 1 : g_Radiotext->RT_Index - 1;
				if (g_Radiotext->S_RtOsdLoop == 1) // latest bottom
				{
					for (int i = index + 1; i < g_Radiotext->S_RtOsdRows; i++)
						g_Font[rt_font]->RenderString(item_x, item_y + (item++)*item_h, item_w, g_Radiotext->RT_Text[i], COL_INFOBAR_TEXT, 0, RTisUTF8);
					for (int i = 0; i <= index; i++)
						g_Font[rt_font]->RenderString(item_x, item_y + (item++)*item_h, item_w, g_Radiotext->RT_Text[i], COL_INFOBAR_TEXT, 0, RTisUTF8);
				}
				else // latest top
				{
					for (int i = index; i >= 0; i--)
						g_Font[rt_font]->RenderString(item_x, item_y + (item++)*item_h, item_w, g_Radiotext->RT_Text[i], COL_INFOBAR_TEXT, 0, RTisUTF8);
					for (int i = g_Radiotext->S_RtOsdRows - 1; i > index; i--)
						g_Font[rt_font]->RenderString(item_x, item_y + (item++)*item_h, item_w, g_Radiotext->RT_Text[i], COL_INFOBAR_TEXT, 0, RTisUTF8);
				}
			}
		}
	}
	g_Radiotext->RT_MsgShow = false;
}

int CInfoViewer::handleMsg (const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if ((msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG) || (msg == NeutrinoMessages::EVT_NEXTPROGRAM)) {
//printf("CInfoViewer::handleMsg: NeutrinoMessages::EVT_CURRENTNEXT_EPG data %llx current %llx\n", *(t_channel_id *) data, current_channel_id & 0xFFFFFFFFFFFFULL);
		if ((*(t_channel_id *) data) == (current_channel_id & 0xFFFFFFFFFFFFULL)) {
			getEPG (*(t_channel_id *) data, info_CurrentNext);
			if (is_visible)
				show_Data (true);
			showLcdPercentOver ();
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOTPIDS) {
		if ((*(t_channel_id *) data) == current_channel_id) {
			if (is_visible && showButtonBar) {
				infoViewerBB->showIcon_VTXT();
				infoViewerBB->showIcon_SubT();
				//infoViewerBB->showIcon_CA_Status(0);
				infoViewerBB->showIcon_Resolution();
				infoViewerBB->showIcon_Tuner();
			}
		}
		return messages_return::handled;
	} else if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) ||
			(msg == NeutrinoMessages::EVT_ZAP_ISNVOD)) {
		current_channel_id = (*(t_channel_id *)data);
		//killInfobarText();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_ID) {
		//chanready = 1;
		showSNR ();
		if (is_visible && showButtonBar)
			infoViewerBB->showIcon_CA_Status(0);
		//Set_CA_Status (data);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_TIMER) {
		if (data == fader.GetFadeTimer()) {
			// here, the event can only come if there is another window in the foreground!
			fader.StopFade();
			return messages_return::handled;
		} else if (data == lcdUpdateTimer) {
//printf("CInfoViewer::handleMsg: lcdUpdateTimer\n");
			if (is_visible) {
				if (fileplay || CMoviePlayerGui::getInstance().timeshift)
					CMoviePlayerGui::getInstance().UpdatePosition();
				if (fileplay) {
					const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
					char runningRest[64]; // %d can be 10 digits max...
 					int curr_pos = CMoviePlayerGui::getInstance().GetPosition(); 
 					int duration = CMoviePlayerGui::getInstance().GetDuration();
					snprintf(runningRest, sizeof(runningRest), "%d / %d %s", (curr_pos + 30000) / 60000, (duration - curr_pos + 30000) / 60000, unit_short_minute);
					display_Info(NULL, NULL, false, CMoviePlayerGui::getInstance().file_prozent, NULL, runningRest);
				} else if (!IS_WEBCHAN(current_channel_id)) {
					show_Data(false);
				}
			}
			showLcdPercentOver ();
			return messages_return::handled;
		} else if (data == sec_timer_id) {
			showSNR ();
			return messages_return::handled;
		}
	} else if (msg == NeutrinoMessages::EVT_RECORDMODE) {
		recordModeActive = data;
		if (is_visible) showRecordIcon(true);
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOTAPIDS) {
		if ((*(t_channel_id *) data) == current_channel_id) {
			if (is_visible && showButtonBar) {
				infoViewerBB->showIcon_DD();
				showLivestreamInfo();
				infoViewerBB->showBBButtons(true /*paintFooter*/); // in case button text has changed
			}
			if (g_settings.radiotext_enable && g_Radiotext && !g_RemoteControl->current_PIDs.APIDs.empty() && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoModes::mode_radio))
				g_Radiotext->setPid(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES) {
		if ((*(t_channel_id *) data) == current_channel_id) {
			if (is_visible && showButtonBar)
				infoViewerBB->showBBButtons(true /*paintFooter*/); // in case button text has changed
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE) {
		//chanready = 1;
		showSNR ();
		//if ((*(t_channel_id *)data) == current_channel_id)
		{
			if (is_visible && showButtonBar && (!g_RemoteControl->are_subchannels))
				show_Data (true);
		}
		showLcdPercentOver ();
		eventname = info_CurrentNext.current_name;
		CVFD::getInstance()->setEPGTitle(eventname);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_SUB_FAILED) {
		//chanready = 1;
		showSNR ();
		// show failure..!
		CVFD::getInstance ()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')', g_RemoteControl->getCurrentChannelNumber());
		printf ("zap failed!\n");
		showFailure ();
		CVFD::getInstance ()->showPercentOver (255);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_FAILED) {
		//chanready = 1;
		showSNR ();
		if ((*(t_channel_id *) data) == current_channel_id) {
			// show failure..!
			CVFD::getInstance ()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')', g_RemoteControl->getCurrentChannelNumber());
			printf ("zap failed!\n");
			showFailure ();
			CVFD::getInstance ()->showPercentOver (255);
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_MOTOR) {
		chanready = 0;
		showMotorMoving (data);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_TUNE_COMPLETE) {
		chanready = 1;
		showSNR ();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_MODECHANGED) {
		aspectRatio = (int8_t)data;
		if (is_visible && showButtonBar)
			infoViewerBB->showIcon_16_9 ();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_TIMESET) {
		// gotTime = true;
		return messages_return::handled;
	}
#if 0
	else if (msg == NeutrinoMessages::EVT_ZAP_CA_CLEAR) {
		Set_CA_Status (false);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_LOCK) {
		Set_CA_Status (true);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_FTA) {
		Set_CA_Status (false);
		return messages_return::handled;
	}
#endif
	return messages_return::unhandled;
}

void CInfoViewer::sendNoEpg(const t_channel_id for_channel_id)
{
	if (!zap_mode/* & IV_MODE_DEFAULT*/) {
		char *p = new char[sizeof(t_channel_id)];
		memcpy(p, &for_channel_id, sizeof(t_channel_id));
		g_RCInput->postMsg (NeutrinoMessages::EVT_NOEPG_YET, (const neutrino_msg_data_t) p, false);
	}
}

void CInfoViewer::getEPG(const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info)
{
	/* to clear the oldinfo for channels without epg, call getEPG() with for_channel_id = 0 */
	if (for_channel_id == 0 || IS_WEBCHAN(for_channel_id))
	{
		oldinfo.current_uniqueKey = 0;
		return;
	}

	CEitManager::getInstance()->getCurrentNextServiceKey(current_epg_id, info);

	/* of there is no EPG, send an event so that parental lock can work */
	if (info.current_uniqueKey == 0 && info.next_uniqueKey == 0) {
		memcpy(&oldinfo, &info, sizeof(CSectionsdClient::CurrentNextInfo));
		sendNoEpg(for_channel_id);
		return;
	}

	if (info.current_uniqueKey != oldinfo.current_uniqueKey || info.next_uniqueKey != oldinfo.next_uniqueKey)
	{
		char *p = new char[sizeof(t_channel_id)];
		memcpy(p, &for_channel_id, sizeof(t_channel_id));
		neutrino_msg_t msg;
		if (info.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_next))
		{
			if (info.flags & CSectionsdClient::epgflags::has_current)
				msg = NeutrinoMessages::EVT_CURRENTEPG;
			else
				msg = NeutrinoMessages::EVT_NEXTEPG;
		}
		else
			msg = NeutrinoMessages::EVT_NOEPG_YET;
		g_RCInput->postMsg(msg, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory
		memcpy(&oldinfo, &info, sizeof(CSectionsdClient::CurrentNextInfo));
	}
}

void CInfoViewer::showSNR ()
{
	if (! is_visible)
		return;
	int renderFlag = ((g_settings.theme.infobar_gradient_top) ? Font::FULLBG : 0) | Font::IS_UTF8;
	/* right now, infobar_show_channellogo == 3 is the trigger for signal bars etc.
	   TODO: decouple this  */
	if (!fileplay && !IS_WEBCHAN(current_channel_id) && ( g_settings.infobar_show_channellogo == 3 || g_settings.infobar_show_channellogo == 5 || g_settings.infobar_show_channellogo == 6 )) {
		int y_freq = 2*g_SignalFont->getHeight();
		if (!g_settings.infobar_sat_display)
			y_freq -= g_SignalFont->getHeight()/2; //half line up to center freq vertically
		int y_numbox = numbox->getYPos();
		if ((newfreq && chanready) || SDT_freq_update) {
			char freq[20];
			newfreq = false;

			std::string polarisation = "";
			
			if (CFrontend::isSat(CFEManager::getInstance()->getLiveFE()->getCurrentDeliverySystem()))
				polarisation = transponder::pol(CFEManager::getInstance()->getLiveFE()->getPolarization());

			int frequency = CFEManager::getInstance()->getLiveFE()->getFrequency();
			snprintf (freq, sizeof(freq), "%d.%d MHz %s", frequency / 1000, frequency % 1000, polarisation.c_str());

			int freqWidth = g_SignalFont->getRenderWidth(freq);
			if (freqWidth > (ChanWidth - numbox_offset*2))
				freqWidth = ChanWidth - numbox_offset*2;
			g_SignalFont->RenderString(BoxStartX + numbox_offset + ((ChanWidth - freqWidth) / 2), y_numbox + y_freq - 3, ChanWidth - 2*numbox_offset, freq, SDT_freq_update ? COL_COLORED_EVENTS_TEXT:COL_INFOBAR_TEXT, 0, renderFlag);
			SDT_freq_update = false;
		}
		if (sigbox == NULL){
			int sigbox_offset = ChanWidth *10/100;
			sigbox = new CSignalBox(BoxStartX + sigbox_offset, y_numbox+ChanHeight/2, ChanWidth - 2*sigbox_offset, ChanHeight/2, NULL, true, NULL, "S", "Q");
			sigbox->setTextColor(COL_INFOBAR_TEXT);
			sigbox->setActiveColor(COL_INFOBAR_PLUS_7);
			sigbox->setPassiveColor(COL_INFOBAR_PLUS_3);
			sigbox->setColorBody(numbox->getColorBody());
			sigbox->doPaintBg(false);
			sigbox->enableTboxSaveScreen(numbox->getColBodyGradientMode());
		}
		sigbox->setFrontEnd(CFEManager::getInstance()->getLiveFE());
		sigbox->paint(CC_SAVE_SCREEN_NO);
	}
	if(showButtonBar)
		infoViewerBB->showSysfsHdd();
}

void CInfoViewer::display_Info(const char *current, const char *next,
			       bool starttimes, const int pb_pos,
			       const char *runningStart, const char *runningRest,
			       const char *nextStart, const char *nextDuration,
			       bool update_current, bool update_next)
{
	/* dimensions of the two-line current-next "box":
	   top of box    == ChanNameY + header_height (bottom of channel name)
	   bottom of box == BoxEndY
	   height of box == BoxEndY - (ChanNameY + header_height)
	   middle of box == top + height / 2
			 == ChanNameY + header_height + (BoxEndY - (ChanNameY + header_height))/2
			 == ChanNameY + header_height + (BoxEndY - ChanNameY - header_height)/2
			 == ChanNameY / 2 + header_height / 2 + BoxEndY / 2
			 == (BoxEndY + ChanNameY + header_height)/2
	   The bottom of current info and the top of next info is == middle of box.
	 */

	int height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight();
	int CurrInfoY = (BoxEndY + ChanNameY + header_height) / 2;
	int NextInfoY = CurrInfoY/* + height*/;	// lower end of next info box
	int InfoX = ChanInfoX + 10;

	int xStart = InfoX;
	if (starttimes)
		xStart += info_time_width + 10;

	int pb_h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight() - 4;
	switch(g_settings.infobar_progressbar)
	{
		case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME:
		case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME_SMALL:
			CurrInfoY += (pb_h/3);
			NextInfoY += (pb_h/3);
		break;
		case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BETWEEN_EVENTS:
			CurrInfoY -= (pb_h/3);
			NextInfoY += (pb_h/3);
		break;
		default:
		break;
	}

	if (pb_pos > -1)
	{
		int pb_w = 112;
		int pb_startx = BoxEndX - pb_w - OFFSET_SHADOW;
		int pb_starty = ChanNameY - (pb_h + 10);
		if (g_settings.infobar_progressbar)
		{
			pb_startx = xStart;
			pb_w = BoxEndX - 10 - xStart;
		}
		int tmpY = CurrInfoY - height - ChanNameY + header_height -
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getDigitOffset()/3+OFFSET_SHADOW;
		switch(g_settings.infobar_progressbar){ //set progressbar position
			case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME:
				pb_h = (pb_h/3);
				pb_starty = ChanNameY + (tmpY-pb_h)/2;
			break;
			case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BELOW_CH_NAME_SMALL:
				pb_h = (pb_h/5);
				pb_starty = ChanNameY + (tmpY-pb_h)/2;
			break;
			case SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_BETWEEN_EVENTS:
				pb_starty = CurrInfoY + ((pb_h / 3)-(pb_h/5)) ;
				pb_h = (pb_h/5);
			break;
			default:
			break;
		}

		int pb_p = pb_pos * pb_w / 100;
		if (pb_p > pb_w)
			pb_p = pb_w;

		timescale->setDimensionsAll(pb_startx, pb_starty, pb_w, pb_h);
		timescale->setActiveColor(COL_INFOBAR_PLUS_7);
		timescale->setPassiveColor(g_settings.infobar_progressbar ? COL_INFOBAR_PLUS_1 : COL_INFOBAR_PLUS_0);
		timescale->enableShadow(!g_settings.infobar_progressbar ? CC_SHADOW_ON : CC_SHADOW_OFF, OFFSET_SHADOW/2);
		timescale->setValues(pb_p, pb_w);

		//printf("paintProgressBar(%d, %d, %d, %d)\n", BoxEndX - pb_w - OFFSET_SHADOW, ChanNameY - (pb_h + 10) , pb_w, pb_h);
	}else{
		if (g_settings.infobar_progressbar == SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_DEFAULT)
			timescale->kill();
	}

	int currTimeW = 0;
	int nextTimeW = 0;
	if (runningRest)
		currTimeW = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(runningRest)*2;
	if (nextDuration)
		nextTimeW = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(nextDuration)*2;
	int currTimeX = BoxEndX - currTimeW - 10;
	int nextTimeX = BoxEndX - nextTimeW - 10;

	//colored_events init
	bool colored_event_C = (g_settings.theme.colored_events_infobar == 1);
	bool colored_event_N = (g_settings.theme.colored_events_infobar == 2);

	//current event
	if (current && update_current){
		if (txt_cur_event == NULL)
			txt_cur_event = new CComponentsTextTransp(NULL, xStart, CurrInfoY - height, currTimeX - xStart - 5, height);
		else
			txt_cur_event->setDimensionsAll(xStart, CurrInfoY - height, currTimeX - xStart - 5, height);

		txt_cur_event->setText(current, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
		if (txt_cur_event->isPainted())
			txt_cur_event->hide();
		txt_cur_event->paint(CC_SAVE_SCREEN_YES);

		if (runningStart && starttimes){
			if (txt_cur_start == NULL)
				txt_cur_start = new CComponentsTextTransp(NULL, InfoX, CurrInfoY - height, info_time_width, height);
			else
				txt_cur_start->setDimensionsAll(InfoX, CurrInfoY - height, info_time_width, height);
			txt_cur_start->setText(runningStart, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
			if (txt_cur_event->isPainted())
				txt_cur_event->hide();
			txt_cur_start->paint(CC_SAVE_SCREEN_YES);
		}

		if (runningRest){
			if (txt_cur_event_rest == NULL)
				txt_cur_event_rest = new CComponentsTextTransp(NULL, currTimeX, CurrInfoY - height, currTimeW, height);
			else
				txt_cur_event_rest->setDimensionsAll(currTimeX, CurrInfoY - height, currTimeW, height);
			txt_cur_event_rest->setText(runningRest, CTextBox::RIGHT, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
			if (txt_cur_event_rest->isPainted())
				txt_cur_event_rest->hide();
			txt_cur_event_rest->paint(CC_SAVE_SCREEN_YES);
		}
	}

	//next event
	if (next && update_next)
	{
		if (txt_next_event == NULL)
			txt_next_event = new CComponentsTextTransp(NULL, xStart, NextInfoY, nextTimeX - xStart - 5, height);
		else
			txt_next_event->setDimensionsAll(xStart, NextInfoY, nextTimeX - xStart - 5, height);
		txt_next_event->setText(next, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
		if (txt_next_event->isPainted())
			txt_next_event->hide();
		txt_next_event->paint(CC_SAVE_SCREEN_YES);

		if (nextStart && starttimes){
			if (txt_next_start == NULL)
				txt_next_start = new CComponentsTextTransp(NULL, InfoX, NextInfoY, info_time_width, height);
			else
				txt_next_start->setDimensionsAll(InfoX, NextInfoY, info_time_width, height);
			txt_next_start->setText(nextStart, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
			if (txt_next_start->isPainted())
				txt_next_start->hide();
			txt_next_start->paint(CC_SAVE_SCREEN_YES);
		}

		if (nextDuration){
			if (txt_next_in == NULL)
				txt_next_in = new CComponentsTextTransp(NULL, nextTimeX, NextInfoY, nextTimeW, height);
			else
				txt_next_in->setDimensionsAll(nextTimeX, NextInfoY, nextTimeW, height);
			txt_next_in->setText(nextDuration, CTextBox::RIGHT, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO], colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_INFOBAR_TEXT);
			if (txt_next_in->isPainted())
				txt_next_in->hide();
			txt_next_in->paint(CC_SAVE_SCREEN_YES);
		}
	}

	//finally paint time scale
	if (pb_pos > -1)
		timescale->paint();
}

void CInfoViewer::show_Data (bool calledFromEvent)
{
	if (! is_visible)
		return;

	/* EPG data is not useful in movieplayer mode ;) */
	if (fileplay && !CMoviePlayerGui::getInstance().timeshift)
		return;

	char runningStart[32];
	char runningRest[32];
	char runningPercent = 0;

	char nextStart[32];
	char nextDuration[32];

	int is_nvod = false;

	if ((g_RemoteControl->current_channel_id == current_channel_id) && (!g_RemoteControl->subChannels.empty()) && (!g_RemoteControl->are_subchannels)) {
		is_nvod = true;
		info_CurrentNext.current_zeit.startzeit = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].startzeit;
		info_CurrentNext.current_zeit.dauer = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].dauer;
	} else {
#if 0
/* this triggers false positives on some channels.
 * TODO: test on real NVOD channels, if this was even necessary at all */
		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (showButtonBar)) {
			if ((uint) info_CurrentNext.next_zeit.startzeit < (info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)) {
				is_nvod = true;
			}
		}
#endif
	}

	time_t jetzt = time (NULL);

	const char *unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
		int seit = (abs(jetzt - info_CurrentNext.current_zeit.startzeit) + 30) / 60;
		int rest = (info_CurrentNext.current_zeit.dauer / 60) - seit;
		runningPercent = 0;
		if (!timeset)
			snprintf(runningRest, sizeof(runningRest), "%d %s", info_CurrentNext.current_zeit.dauer / 60, unit_short_minute);
		else if (jetzt < info_CurrentNext.current_zeit.startzeit)
			snprintf(runningRest, sizeof(runningRest), "%s %d %s", g_Locale->getText(LOCALE_WORD_IN), seit, unit_short_minute);
		else {
			runningPercent = (jetzt - info_CurrentNext.current_zeit.startzeit) * 100 / info_CurrentNext.current_zeit.dauer;
			if (runningPercent > 100)
				runningPercent = 100;
			if (rest >= 0)
				snprintf(runningRest, sizeof(runningRest), "%d / %d %s", seit, rest, unit_short_minute);
			else
				snprintf(runningRest, sizeof(runningRest), "%d +%d %s", info_CurrentNext.current_zeit.dauer / 60, -rest, unit_short_minute);
		}

		struct tm *pStartZeit = localtime (&info_CurrentNext.current_zeit.startzeit);
		snprintf (runningStart, sizeof(runningStart), "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
	} else
		last_curr_id = 0;

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) {
		unsigned dauer = info_CurrentNext.next_zeit.dauer / 60;
		snprintf (nextDuration, sizeof(nextDuration), "%d %s", dauer, unit_short_minute);
		struct tm *pStartZeit = localtime (&info_CurrentNext.next_zeit.startzeit);
		snprintf (nextStart, sizeof(nextStart), "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
	} else
		last_next_id = 0;

	if (showButtonBar) {
		infoViewerBB->showBBButtons(calledFromEvent);
	}

	if ((info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast) ||
			(calledFromEvent && !(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_next|CSectionsdClient::epgflags::has_current))))
	{
		// no EPG available
		display_Info(g_Locale->getText(timeset ? LOCALE_INFOVIEWER_NOEPG : LOCALE_INFOVIEWER_WAITTIME), NULL);
		/* send message. Parental pin check gets triggered on EPG events... */
		/* clear old info in getEPG */
		CSectionsdClient::CurrentNextInfo dummy;
		getEPG(0, dummy);
		sendNoEpg(current_channel_id);
		return;
	}

	// irgendein EPG gefunden
	const char *current   = NULL;
	const char *curr_time = NULL;
	const char *curr_rest = NULL;
	const char *next      = NULL;
	const char *next_time = NULL;
	const char *next_dur  = NULL;
	bool curr_upd = true;
	bool next_upd = true;

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current)
	{
		if (!calledFromEvent || info_CurrentNext.current_uniqueKey != last_curr_id)
		{
			last_curr_id = info_CurrentNext.current_uniqueKey;
			curr_time = runningStart;
			current = info_CurrentNext.current_name.c_str();
		}
		else
			curr_upd = false;
		curr_rest = runningRest;
	}
	else
		current = g_Locale->getText(LOCALE_INFOVIEWER_NOCURRENT);

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next)
	{
		if (!(is_nvod && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current))
				&& info_CurrentNext.next_uniqueKey != last_next_id)
		{	/* if current is shown, show next only if !nvod. Why? I don't know */
			//printf("SHOWDATA: last_next_id = 0x%016llx next_id = 0x%016llx\n", last_next_id, info_CurrentNext.next_uniqueKey);
			last_next_id = info_CurrentNext.next_uniqueKey;
			next_time = nextStart;
			next = info_CurrentNext.next_name.c_str();
			next_dur = nextDuration;
		}
		else
			next_upd = false;
	}
	display_Info(current, next, true, runningPercent,
		     curr_time, curr_rest, next_time, next_dur, curr_upd, next_upd);
}

void CInfoViewer::killInfobarText()
{
	if (infobar_txt){
		if (infobar_txt->isPainted())
			infobar_txt->kill();
		delete infobar_txt;
		infobar_txt = NULL;
	}
}


void CInfoViewer::showInfoFile()
{
	//read textcontent from this file
	std::string infobar_file = "/tmp/infobar.txt"; 

	//exit if file not found, don't create an info object, delete old instance if required
	if (!file_size(infobar_file.c_str()))	{
		killInfobarText();
		return;
	}

	//get width of progressbar timescale
	int pb_w = 0;
	if ( (timescale != NULL) && (g_settings.infobar_progressbar == 0) ) {
		pb_w = timescale->getWidth();
	}

	//set position of info area
	const int oOffset	= 140; // outer left/right offset
	const int xStart	= BoxStartX + ChanWidth + oOffset;
	const int yStart	= BoxStartY;
	const int width		= BoxEndX - xStart - oOffset - pb_w;
	const int height	= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight() + 2;

	//create info object
	if (infobar_txt == NULL){
		infobar_txt = new CComponentsInfoBox();
		//set some properties for info object
		infobar_txt->setCorner(RADIUS_SMALL);
		infobar_txt->enableShadow(CC_SHADOW_ON, OFFSET_SHADOW/2);
		infobar_txt->setTextColor(COL_INFOBAR_TEXT);
		infobar_txt->setColorBody(COL_INFOBAR_PLUS_0);
		infobar_txt->doPaintTextBoxBg(false);
		infobar_txt->enableColBodyGradient(g_settings.theme.infobar_gradient_top, g_settings.theme.infobar_gradient_top ? COL_INFOBAR_PLUS_0 : header->getColorBody(), g_settings.theme.infobar_gradient_top_direction);
	}

	//get text from file and set it to info object, exit and delete object if failed
	std::string old_txt = infobar_txt->getText();
	std::string new_txt = infobar_txt->getTextFromFile(infobar_file);
	bool has_text = infobar_txt->setText(new_txt, CTextBox::CENTER, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]);
	if (new_txt.empty()){
		killInfobarText();
		return;
	}

	//check dimension, if changed then kill to force reinit
	if (infobar_txt->getWidth() != width)
		infobar_txt->kill();

	//consider possible size change
	infobar_txt->setDimensionsAll(xStart, yStart, width, height);

	//paint info if not painted or text has changed
	if (has_text || (zap_mode & IV_MODE_VIRTUAL_ZAP)){
		if ((old_txt != new_txt) || !infobar_txt->isPainted())
			infobar_txt->paint(CC_SAVE_SCREEN_NO);
	}
}

void CInfoViewer::killTitle()
{
	if (is_visible)
	{
		is_visible = false;
		infoViewerBB->is_visible = false;
		if (infoViewerBB->getFooter())
			infoViewerBB->getFooter()->kill();
		if (infoViewerBB->getCABar())
			infoViewerBB->getCABar()->kill();
		if (rec)
			rec->kill();
		//printf("killTitle(%d, %d, %d, %d)\n", BoxStartX, BoxStartY, BoxEndX+ OFFSET_SHADOW-BoxStartX, bottom-BoxStartY);
		//frameBuffer->paintBackgroundBox(BoxStartX, BoxStartY, BoxEndX+ OFFSET_SHADOW, bottom);
		if (!(zap_mode & IV_MODE_VIRTUAL_ZAP)){
			if (infobar_txt)
				infobar_txt->kill();
			numbox->kill();
		}

#if 0
		//not really required to kill sigbox, numbox does this
		if (sigbox)
			sigbox->kill();
#endif

		header->kill();

		if (clock)
		{
			clock->kill();
			delete clock;
			clock = NULL;
		}

		body->kill();

		if (txt_cur_event)
			txt_cur_event->kill();
		if (txt_cur_event_rest)
			txt_cur_event_rest->kill();
		if (txt_cur_start)
			txt_cur_start->kill();
		if (txt_next_start)
			txt_next_start->kill();
		if (txt_next_event)
			txt_next_event->kill();
		if (txt_next_in)
			txt_next_in->kill();

		if (timescale)
			if (g_settings.infobar_progressbar == SNeutrinoSettings::INFOBAR_PROGRESSBAR_ARRANGEMENT_DEFAULT)
				timescale->kill();
		if (g_settings.radiotext_enable && g_Radiotext) {
			g_Radiotext->S_RtOsd = g_Radiotext->haveRadiotext() ? 1 : 0;
			killRadiotext();
		}
	}
	showButtonBar = false;
	CInfoClock::getInstance()->enableInfoClock();
}

#if 0
void CInfoViewer::Set_CA_Status (int /*Status*/)
{
	if (is_visible && showButtonBar)
		infoViewerBB->showIcon_CA_Status(1);
}
#endif
/******************************************************************************
returns mode of painted channel logo,
0 = no logo painted
1 = in number box
2 = in place of channel name
3 = beside channel name
*******************************************************************************/
int CInfoViewer::showChannelLogo(const t_channel_id logo_channel_id, const int channel_number_width)
{
	if (!g_settings.infobar_show_channellogo) // show logo only if configured
		return 0;

	std::string strAbsIconPath;

	int logo_w, logo_h;
	int logo_x = 0, logo_y = 0;
	int res = 0;
	int start_x = ChanNameX;
	int chan_w = BoxEndX- (start_x+ 20)- time_width- LEFT_OFFSET - 10;

	bool logo_available = g_PicViewer->GetLogoName(logo_channel_id, ChannelName, strAbsIconPath, &logo_w, &logo_h);

	//fprintf(stderr, "%s: logo_available: %d file: %s\n", __FUNCTION__, logo_available, strAbsIconPath.c_str());

	if (! logo_available)
		return 0;

	if ((logo_w == 0) || (logo_h == 0)) // corrupt logo size?
	{
		printf("[infoviewer] channel logo: \n"
		       " -> %s (%s) has no size\n"
		       " -> please check logo file!\n", strAbsIconPath.c_str(), ChannelName.c_str());
		return 0;
	}
	int y_mid;

	if (g_settings.infobar_show_channellogo == 1) // paint logo in numberbox
	{
		// calculate mid of numberbox
		int satNameHeight = g_settings.infobar_sat_display ? g_SignalFont->getHeight() : 0;
		int x_mid = BoxStartX + ChanWidth / 2;
		y_mid = numbox->getYPos() + (satNameHeight + ChanHeight) / 2;

		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, ChanWidth, ChanHeight - satNameHeight);
		// channel name with number
// this is too ugly...		ChannelName = (std::string)strChanNum + ". " + ChannelName;
		// get position of channel logo, must be centered in number box
		logo_x = x_mid - logo_w / 2;
		logo_y = y_mid - logo_h / 2;
		res = 1;
	}
	else if (g_settings.infobar_show_channellogo == 2 || g_settings.infobar_show_channellogo == 5 || g_settings.infobar_show_channellogo == 6) // paint logo in place of channel name
	{
		// check logo dimensions
		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, chan_w, header_height - 2*OFFSET_INNER_MIN);
		// hide channel name
// this is too ugly...		ChannelName = "";
		// calculate logo position
		y_mid = ChanNameY + header_height / 2;
		logo_x = start_x + 10 + channel_number_width;;
		logo_y = y_mid - logo_h / 2;
		if (g_settings.infobar_show_channellogo == 2)
			res = 2;
		else
			res = 5;
	}
	else if (g_settings.infobar_show_channellogo == 3 || g_settings.infobar_show_channellogo == 4)  // paint logo beside channel name
	{
		// check logo dimensions
		int Logo_max_width = chan_w - logo_w - 10;
		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, Logo_max_width, header_height - 2*OFFSET_INNER_MIN);
		// calculate logo position
		y_mid = ChanNameY + header_height / 2;
		logo_x = start_x + 10;
		logo_y = y_mid - logo_h / 2;
		// set channel name x pos right of the logo
		ChanNameX = start_x + logo_w + 10;
		if (g_settings.infobar_show_channellogo == 3)
			res = 3;
		else
			res = 4;
	}
	else
	{
		res = 0;
	}

	// paint the logo
	if (res != 0) {
		if (!g_PicViewer->DisplayImage(strAbsIconPath, logo_x, logo_y, logo_w, logo_h))
			return 0; // paint logo failed
	}

	return res;
}

#if HAVE_TRIPLEDRAGON
/* the cheap COOLSTREAM display cannot do this, so keep the routines separate */
void CInfoViewer::showLcdPercentOver()
{
	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] != 1)
	{
		if (fileplay || NeutrinoModes::mode_ts == CNeutrinoApp::getInstance()->getMode()) {
			CVFD::getInstance()->showPercentOver(CMoviePlayerGui::getInstance().file_prozent);
			return;
		}
		static long long old_interval = 0;
		int runningPercent = -1;
		time_t jetzt = time(NULL);
		long long interval = 60000000; /* 60 seconds default update time */
		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
			if (jetzt < info_CurrentNext.current_zeit.startzeit)
				runningPercent = 0;
			else if (jetzt > (int)(info_CurrentNext.current_zeit.startzeit +
					       info_CurrentNext.current_zeit.dauer))
				runningPercent = -2; /* overtime */
			else {
				runningPercent = MIN((jetzt-info_CurrentNext.current_zeit.startzeit) * 100 /
					              info_CurrentNext.current_zeit.dauer, 100);
				interval = info_CurrentNext.current_zeit.dauer * 1000LL * (1000/100); // update every percent
				if (is_visible && interval > 60000000)	// if infobar visible, update at
					interval = 60000000;		// least once per minute (radio mode)
				if (interval < 5000000)
					interval = 5000000;		// but update only every 5 seconds
			}
		}
		if (interval != old_interval) {
			g_RCInput->killTimer(lcdUpdateTimer);
			lcdUpdateTimer = g_RCInput->addTimer(interval, false);
			//printf("lcdUpdateTimer: interval %lld old %lld\n",interval/1000000,old_interval/1000000);
			old_interval = interval;
		}
		CLCD::getInstance()->showPercentOver(runningPercent);
		int mode = CNeutrinoApp::getInstance()->getMode();
		if ((mode == NeutrinoModes::mode_radio || mode == NeutrinoModes::mode_tv))
			CVFD::getInstance()->setEPGTitle(info_CurrentNext.current_name);
	}
}
#else
void CInfoViewer::showLcdPercentOver ()
{
	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] != 1) {
		if (fileplay || (NeutrinoModes::mode_ts == CNeutrinoApp::getInstance()->getMode())) {
			CVFD::getInstance ()->showPercentOver (CMoviePlayerGui::getInstance().file_prozent);
			return;
		}
		int runningPercent = -1;
		time_t jetzt = time (NULL);
		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
			if (jetzt < info_CurrentNext.current_zeit.startzeit)
				runningPercent = 0;
			else
				runningPercent = MIN ((jetzt - info_CurrentNext.current_zeit.startzeit) * 100 / info_CurrentNext.current_zeit.dauer, 100);
		}
		CVFD::getInstance ()->showPercentOver (runningPercent);
	}
}
#endif

void CInfoViewer::showEpgInfo()   //message on event change
{
	int mode = CNeutrinoApp::getInstance()->getMode();
	/* show epg info only if we in TV- or Radio mode and current event is not the same like before */
	if ((eventname != info_CurrentNext.current_name) && (mode == 2 /*mode_radio*/ || mode == 1 /*mode_tv*/))
	{
		eventname = info_CurrentNext.current_name;
		if (g_settings.infobar_show)
			g_RCInput->postMsg(NeutrinoMessages::SHOW_INFOBAR , 0);
	}
}

void CInfoViewer::setUpdateTimer(uint64_t interval)
{
	g_RCInput->killTimer(lcdUpdateTimer);
	if (interval)
		lcdUpdateTimer = g_RCInput->addTimer(interval, false);
}


void CInfoViewer::ResetModules()
{
	delete header; 			header 		= NULL;
	delete body; 			body 		= NULL;
	delete infobar_txt; 		infobar_txt 	= NULL;
	if (clock)
	{
		delete clock;		clock 		= NULL;
	}
	delete txt_cur_start; 		txt_cur_start 	= NULL;
	delete txt_cur_event; 		txt_cur_event 	= NULL;
	delete txt_cur_event_rest; 	txt_cur_event_rest = NULL;
	delete txt_next_start; 		txt_next_start 	= NULL;
	delete txt_next_event; 		txt_next_event 	= NULL;
	delete txt_next_in; 		txt_next_in 	= NULL;
	delete numbox; 			numbox 		= NULL;
	ResetPB();
	delete rec; 			rec 		= NULL;
	infoViewerBB->ResetModules();
}
