/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Bugfixes/cleanups (C) 2007-2012 Stefan Seyfried
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
#include <string>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sys/timeb.h>
#include <sys/param.h>
#include <time.h>
#include <fcntl.h>

#include <global.h>
#include <neutrino.h>

#include <gui/infoviewer.h>
#include <gui/bouquetlist.h>
#include <gui/widget/icons.h>
#include <gui/widget/hintbox.h>
#include <gui/customcolor.h>
#include <gui/pictureviewer.h>
#include <gui/movieplayer.h>

#include <daemonc/remotecontrol.h>
#include <driver/record.h>

#include <zapit/satconfig.h>
#include <zapit/frontend_c.h>
#include <zapit/zapit.h>
#include <video.h>

void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "");
void sectionsd_getCurrentNextServiceKey(t_channel_id uniqueServiceKey, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next );

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
extern CBouquetList * bouquetList;       /* neutrino.cpp */
extern CPictureViewer * g_PicViewer;
extern cVideo * videoDecoder;

#define COL_INFOBAR_BUTTONS            (COL_INFOBAR_SHADOW + 1)
#define COL_INFOBAR_BUTTONS_BACKGROUND (COL_INFOBAR_SHADOW_PLUS_1)

#define LEFT_OFFSET 5


event_id_t CInfoViewer::last_curr_id = 0, CInfoViewer::last_next_id = 0;


extern CZapitClient::SatelliteList satList;
static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}

extern bool autoshift;
extern uint32_t shift_timer;
//extern std::string ext_channel_name;
extern bool timeset;

CInfoViewer::CInfoViewer ()
	: fader(g_settings.infobar_alpha)
{
	sigscale = NULL;
	snrscale = NULL;
	hddscale = NULL;
	varscale = NULL;
	timescale = NULL;
	info_CurrentNext.current_zeit.startzeit = 0;
	info_CurrentNext.current_zeit.dauer = 0;
	info_CurrentNext.flags = 0;
	Init();
}

void CInfoViewer::Init()
{
	frameBuffer = CFrameBuffer::getInstance ();

	BoxStartX = BoxStartY = BoxEndX = BoxEndY = 0;
	recordModeActive = false;
	is_visible = false;
	showButtonBar = false;
	//gotTime = g_Sectionsd->getIsTimeSet ();
	gotTime = timeset;
	virtual_zap_mode = false;
	newfreq = true;
	chanready = 1;
	fileplay = 0;
	SDT_freq_update = false;

	/* maybe we should not tie this to the blinkenlights settings? */
	if (g_settings.casystem_display < 2)
		bottom_bar_offset = 22;
	else
		bottom_bar_offset = 0;
	/* after font size changes, Init() might be called multiple times */
	changePB();

	casysChange = g_settings.casystem_display;
	channellogoChange = g_settings.infobar_show_channellogo;

	/* we need to calculate this only once */
	info_time_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth("22:22") + 10;

	channel_id = CZapit::getInstance()->GetCurrentChannelID();;
	lcdUpdateTimer = 0;

	int dummy_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_16_9_GREY, &icon_large_width, &dummy_h);
	if (icon_large_width == 0)
		icon_large_width = 26;

	frameBuffer->getIconSize(NEUTRINO_ICON_VTXT_GREY, &icon_small_width, &dummy_h);
	if (icon_small_width == 0)
		icon_small_width = 16;

	frameBuffer->getIconSize(NEUTRINO_ICON_RESOLUTION_000, &icon_xres_width, &dummy_h);
	if (icon_xres_width == 0)
		icon_xres_width = 28;

	if (g_settings.infobar_show_res >= 2)
		icon_xres_width = 0;

	frameBuffer->getIconSize(NEUTRINO_ICON_SCRAMBLED2_GREY, &icon_crypt_width, &dummy_h);
	if (icon_crypt_width == 0)
		icon_crypt_width = 24;
}

/*
 * This nice ASCII art should hopefully explain how all the variables play together ;)
 *

              ___BoxStartX
             |-ChanWidth-|
             |           |  _recording icon                 _progress bar
 BoxStartY---+-----------+ |                               |
     |       |           | *                              #######____
     |       |           |-------------------------------------------+--+-ChanNameY
     |       |           | Channelname                               |  |
 ChanHeight--+-----------+                                           |  |
                |                                                    |  InfoHeightY
                |01:23     Current Event                             |  |
                |02:34     Next Event                                |  |
                |                                                    |  |
     BoxEndY----+----------------------------------------------------+--+
                |                     optional blinkenlights iconbar |  bottom_bar_offset
     BBarY------+----------------------------------------------------+--+
                | * red   * green  * yellow  * blue ====== [DD][16:9]|  InfoHeightY_Info
                +----------------------------------------------------+--+
                                                                     |
                                                             BoxEndX-/
*/
void CInfoViewer::start ()
{
	InfoHeightY = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight()*9/8 +
		      2*g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight() +
		      25;
	InfoHeightY_Info = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+ 5;

	if ( g_settings.infobar_show_channellogo != 3 && g_settings.infobar_show_channellogo != 5  && g_settings.infobar_show_channellogo != 6) /* 3 & 5 & 6 is "default" with sigscales etc. */
	{
		ChanWidth = 4 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getRenderWidth(widest_number) + 10;
		ChanHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight() * 9 / 8;
	}
	else
	{	/* default mode, with signal bars etc. */
		ChanWidth = 122;
		ChanHeight = 74;
		int test = g_SignalFont->getWidth() * 14;
		if (test > ChanWidth) {
			ChanWidth = test;
		}
		test = (g_SignalFont->getHeight() * 2) + (36 * g_settings.screen_yres / 100);
		if (test > ChanHeight) {
			ChanHeight = test;
		}
	}

	BoxStartX = g_settings.screen_StartX + 10;
	BoxEndX = g_settings.screen_EndX - 10;
	BoxEndY = g_settings.screen_EndY - 10 - InfoHeightY_Info - bottom_bar_offset;
	BoxStartY = BoxEndY - InfoHeightY - ChanHeight / 2;

	BBarY = BoxEndY + bottom_bar_offset;
	BBarFontY = BBarY + InfoHeightY_Info - (InfoHeightY_Info - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()) / 2; /* center in buttonbar */

	ChanNameY = BoxStartY + (ChanHeight / 2) + SHADOW_OFFSET;	//oberkante schatten?
	ChanInfoX = BoxStartX + (ChanWidth / 3);
	/* assuming all color icons must have same size */
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);
	asize = (BoxEndX - (2*icon_large_width + 2*icon_small_width + 4*2) - 102) - ChanInfoX;
	asize = asize - (icol_w+6)*4;
	asize = asize / 4;

	time_height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight()+5;
	time_left_width = 2 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getWidth(); /* still a kludge */
	time_dot_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(":");
	time_width = time_left_width* 2+ time_dot_width;

	const int lcd_update_time_tv_mode = (60 * 1000 * 1000);
	if (lcdUpdateTimer == 0)
		lcdUpdateTimer = g_RCInput->addTimer (lcd_update_time_tv_mode, false, true);
}

void CInfoViewer::changePB()
{
	const short red_bar = 40;
	const short yellow_bar = 70;
	const short green_bar = 100;
	int w = 0, h = 0;

	frameBuffer->getIconSize(NEUTRINO_ICON_16_9, &w, &h);
	if (w > 26) { // larger icons
		if (g_settings.screen_preset == 1)
			w = (g_settings.casystem_display == 2) ? 6 : 10; // LCD
		else
			w = (g_settings.casystem_display == 2) ? 4 : 7; // CRT
	}
	else // org. icons
		w = 10;
	hddwidth = frameBuffer->getScreenWidth(true) * w / 128; /* 40...100 pix if screen is 1280 wide */
	if (sigscale != NULL)
		delete sigscale;
	sigscale = new CProgressBar(true, bar_width, 10, red_bar, green_bar, yellow_bar);
	if (snrscale != NULL)
		delete snrscale;
	snrscale = new CProgressBar(true, bar_width, 10, red_bar, green_bar, yellow_bar);
	if (hddscale != NULL)
		delete hddscale;
	hddscale = new CProgressBar(true, hddwidth,   6, 50,      green_bar, 75, true);
	if (varscale != NULL)
		delete varscale;
	varscale = new CProgressBar(true, hddwidth,   6, 50,      green_bar, 75, true);
	if (timescale != NULL)
		delete timescale;
	timescale = new CProgressBar(true, -1,       -1, 30,      green_bar, yellow_bar, true);
}

void CInfoViewer::paintTime (bool show_dot, bool firstPaint)
{
	if (! gotTime)
		return;

	char timestr[10];
	time_t rawtime = time(NULL);
	strftime ((char *) &timestr, sizeof(timestr), "%H:%M", localtime(&rawtime));

	if ((!firstPaint) && (strcmp (timestr, old_timestr) == 0)) {
		if (show_dot)
			frameBuffer->paintBoxRel (BoxEndX - time_width + time_left_width - LEFT_OFFSET, ChanNameY, time_dot_width, time_height / 2 + 2, COL_INFOBAR_PLUS_0);
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_width + time_left_width - LEFT_OFFSET, ChanNameY + time_height, time_dot_width, ":", COL_INFOBAR);
		strcpy (old_timestr, timestr);
	} else {
		strcpy (old_timestr, timestr);

		if (!firstPaint) {
			frameBuffer->paintBoxRel(BoxEndX - time_width - LEFT_OFFSET, ChanNameY, time_width + LEFT_OFFSET, time_height, COL_INFOBAR_PLUS_0, RADIUS_SMALL, CORNER_TOP);
		}

		timestr[2] = 0;
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_width - LEFT_OFFSET, ChanNameY + time_height, time_left_width, timestr, COL_INFOBAR);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_left_width - LEFT_OFFSET, ChanNameY + time_height, time_left_width, &timestr[3], COL_INFOBAR);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_width + time_left_width - LEFT_OFFSET, ChanNameY + time_height, time_dot_width, ":", COL_INFOBAR);
		if (show_dot)
			frameBuffer->paintBoxRel (BoxEndX - time_left_width - time_dot_width - LEFT_OFFSET, ChanNameY, time_dot_width, time_height / 2 + 2, COL_INFOBAR_PLUS_0);
	}
}

void CInfoViewer::showRecordIcon (const bool show)
{
	CRecordManager * crm		= CRecordManager::getInstance();
	int rec_mode 			= crm->GetRecordMode();
	
	recordModeActive		= rec_mode != CRecordManager::RECMODE_OFF; 
	if (recordModeActive)
	{
		std::string Icon_Rec = NEUTRINO_ICON_REC_GRAY, Icon_Ts = NEUTRINO_ICON_AUTO_SHIFT_GRAY;

		t_channel_id cci	= g_RemoteControl->current_channel_id;
		bool status_ts		= crm->GetRecordMode(cci) == CRecordManager::RECMODE_TSHIFT;
		bool status_rec		= crm->GetRecordMode(cci) == CRecordManager::RECMODE_REC && !status_ts;
		if (status_ts)
			Icon_Ts		= NEUTRINO_ICON_AUTO_SHIFT;
		if (status_rec)
			Icon_Rec	= NEUTRINO_ICON_REC;
		int records		= crm->GetRecordCount();
		
		const int radius = RADIUS_MIN;
		const int ChanName_X = BoxStartX + ChanWidth + SHADOW_OFFSET;
		const int icon_space = 3, box_posY = 12;
		int box_len = 0, rec_icon_posX = 0, ts_icon_posX = 0;
		
		int rec_icon_w = 0, rec_icon_h = 0, ts_icon_w = 0, ts_icon_h = 0;
		frameBuffer->getIconSize(Icon_Rec.c_str(), &rec_icon_w, &rec_icon_h);
		frameBuffer->getIconSize(Icon_Ts.c_str(), &ts_icon_w, &ts_icon_h);
		
		int chanH = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight () * (g_settings.screen_yres / 100);
		if (chanH < rec_icon_h)
			chanH = rec_icon_h;
		const int box_posX = ChanName_X + SHADOW_OFFSET;
		
		char records_msg[8];
		snprintf(records_msg, sizeof(records_msg)-1, "%d%s", records, "x");
		int TextWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(records_msg) 
					* (g_settings.screen_xres / 100);
					
		if (rec_mode == CRecordManager::RECMODE_REC)
		{
			box_len = rec_icon_w + TextWidth + icon_space*5;
			rec_icon_posX = box_posX + icon_space*2;
		}
		else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
		{
			box_len = ts_icon_w + icon_space*4;
			ts_icon_posX = box_posX + icon_space*2;
		}
		else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
		{
			box_len = ts_icon_w + rec_icon_w + TextWidth + icon_space*7;
			ts_icon_posX = box_posX + icon_space*2;
			rec_icon_posX = ts_icon_posX + ts_icon_w + icon_space*2;
			records--;
			snprintf(records_msg, sizeof(records_msg)-1, "%d%s", records, "x");
		}
		
		if (show)
		{
			frameBuffer->paintBoxRel(box_posX + SHADOW_OFFSET, BoxStartY + box_posY + SHADOW_OFFSET, box_len, chanH, COL_INFOBAR_SHADOW_PLUS_0, radius);
			frameBuffer->paintBoxRel(box_posX, BoxStartY + box_posY , box_len, chanH, COL_INFOBAR_PLUS_0, radius);
			
			if (rec_mode != CRecordManager::RECMODE_TSHIFT)
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (rec_icon_posX + rec_icon_w + icon_space, BoxStartY + box_posY + chanH, box_len, records_msg, COL_INFOBAR, 0, true);
			
			if (rec_mode == CRecordManager::RECMODE_REC)
			{
				frameBuffer->paintIcon(Icon_Rec, rec_icon_posX, BoxStartY + box_posY + (chanH - rec_icon_h)/2);
			}
			else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
			{
				frameBuffer->paintIcon(Icon_Ts, ts_icon_posX, BoxStartY + box_posY + (chanH - ts_icon_h)/2);
			}
			else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
			{
				frameBuffer->paintIcon(Icon_Rec, rec_icon_posX, BoxStartY + box_posY + (chanH - rec_icon_h)/2);
				frameBuffer->paintIcon(Icon_Ts, ts_icon_posX, BoxStartY + box_posY + (chanH - ts_icon_h)/2);
			}
		}
		else
		{
			if (rec_mode == CRecordManager::RECMODE_REC)
				frameBuffer->paintBoxRel(rec_icon_posX, BoxStartY + box_posY + (chanH - rec_icon_h)/2, rec_icon_w, rec_icon_h, COL_INFOBAR_PLUS_0);
			else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
				frameBuffer->paintBoxRel(ts_icon_posX, BoxStartY + box_posY + (chanH - ts_icon_h)/2, ts_icon_w, ts_icon_h, COL_INFOBAR_PLUS_0);
			else if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
				frameBuffer->paintBoxRel(ts_icon_posX, BoxStartY + box_posY + (chanH - rec_icon_h)/2, ts_icon_w + rec_icon_w + icon_space*2, rec_icon_h, COL_INFOBAR_PLUS_0);
		}
	}
}

void CInfoViewer::paintBackground(int col_NumBox)
{
	int c_rad_large = RADIUS_LARGE;
	int c_shadow_width = (c_rad_large * 2) + 1;
	int c_rad_mid = RADIUS_MID;
	int BoxEndInfoY = BoxEndY;
	if (showButtonBar) // add button bar and blinkenlights
		BoxEndInfoY += InfoHeightY_Info + bottom_bar_offset;
	// kill left side
	frameBuffer->paintBackgroundBox(BoxStartX,
					BoxStartY + ChanHeight - 6,
					BoxStartX + ChanWidth / 3,
					BoxEndInfoY + SHADOW_OFFSET);
	// kill progressbar + info-line
	frameBuffer->paintBackgroundBox(BoxStartX + ChanWidth + 40, // 40 for the recording icon!
					BoxStartY, BoxEndX, BoxStartY + ChanHeight);

	// shadow for channel name, epg data...
	frameBuffer->paintBox(BoxEndX - c_shadow_width, ChanNameY + SHADOW_OFFSET,
			      BoxEndX + SHADOW_OFFSET,  BoxEndInfoY + SHADOW_OFFSET,
			      COL_INFOBAR_SHADOW_PLUS_0, c_rad_large, CORNER_RIGHT);
	frameBuffer->paintBox(ChanInfoX + SHADOW_OFFSET, BoxEndInfoY - c_shadow_width,
			      BoxEndX - c_shadow_width, BoxEndInfoY + SHADOW_OFFSET,
			      COL_INFOBAR_SHADOW_PLUS_0, c_rad_large, CORNER_BOTTOM_LEFT);

	// background for channel name, epg data
	frameBuffer->paintBox(ChanInfoX, ChanNameY, BoxEndX, BoxEndY,
			      COL_INFOBAR_PLUS_0, c_rad_large,
			      CORNER_TOP_RIGHT | (showButtonBar ? 0 : CORNER_BOTTOM));

	// number box
	frameBuffer->paintBoxRel(BoxStartX + SHADOW_OFFSET, BoxStartY + SHADOW_OFFSET,
				 ChanWidth, ChanHeight,
				 COL_INFOBAR_SHADOW_PLUS_0, c_rad_mid);
	frameBuffer->paintBoxRel(BoxStartX, BoxStartY,
				 ChanWidth, ChanHeight,
				 col_NumBox, c_rad_mid);
}
void CInfoViewer::paintCA_bar(int left, int right)
{
	int xcnt = (BoxEndX - ChanInfoX) / 4;
	int ycnt = bottom_bar_offset / 4;
	if (right)
		right = xcnt - ((right/4)+1);
	if (left)
		left =  xcnt - ((left/4)-1);

	frameBuffer->paintBox(ChanInfoX + (right*4), BoxEndY, BoxEndX - (left*4), BoxEndY + bottom_bar_offset, COL_BLACK);

	if (left)
		left -= 1;

	for (int i = 0  + right; i < xcnt - left; i++) {
		for (int j = 0; j < ycnt; j++) {
			/* BoxEndY + 2 is the magic number that also appears in paint_ca_icons */
			frameBuffer->paintBoxRel((ChanInfoX + 2) + i*4, BoxEndY + 2 + j*4, 2, 2, COL_INFOBAR_PLUS_1);
		}
	}
}

void CInfoViewer::paintshowButtonBar()
{
	sec_timer_id = g_RCInput->addTimer (1*1000*1000, false);

	if (g_settings.casystem_display < 2) {
		paintCA_bar(0,0);
	}
	frameBuffer->paintBoxRel(ChanInfoX, BBarY, BoxEndX - ChanInfoX, InfoHeightY_Info, COL_INFOBAR_BUTTONS_BACKGROUND, RADIUS_SMALL, CORNER_BOTTOM); //round

	showSNR();
	//frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_BLUE, ChanInfoX + 16*3 + asize * 3 + 2*6,
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_BLUE, ChanInfoX + 10 + (icol_w + 4 + asize + 2) * 3,
			       BBarY, InfoHeightY_Info);

	std::string txt = g_settings.usermenu_text[SNeutrinoSettings::BUTTON_BLUE];
	if (txt.empty())
		txt = g_Locale->getText(LOCALE_INFOVIEWER_STREAMINFO);

	int icons_offset = (2*(icon_large_width + 2)) + icon_small_width +2 +2;
	ButtonWidth = (BoxEndX - ChanInfoX - icons_offset) >> 2;
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(ChanInfoX + 10 + (icol_w + 4 + asize + 2) * 3 + icol_w + 4,
			BBarFontY, ButtonWidth - (2 + icol_w + 4 + 2), txt, COL_INFOBAR_BUTTONS, 0, true); // UTF-8

	showButton_Audio ();
	showButton_SubServices ();
	showIcon_CA_Status(0);
	showIcon_16_9 ();
	showIcon_VTXT ();
	showIcon_SubT();
	showIcon_Resolution();

}

void CInfoViewer::show_current_next(bool new_chan, int  epgpos)
{
	//info_CurrentNext = getEPG (channel_id);
	sectionsd_getCurrentNextServiceKey(channel_id & 0xFFFFFFFFFFFFULL, info_CurrentNext);
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
		// nicht gefunden / noch nicht geladen
		/* see the comment in display_Info() for a reasoning for this calculation */
		int CurrInfoY = (BoxEndY + ChanNameY + time_height) / 2; // lower end of current info box
		neutrino_locale_t loc;
		if (! gotTime)
			loc = LOCALE_INFOVIEWER_WAITTIME;
		else if (showButtonBar)
			loc = LOCALE_INFOVIEWER_EPGWAIT;
		else
			loc = LOCALE_INFOVIEWER_EPGNOTLOAD;
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(ChanNameX+ 10, CurrInfoY, BoxEndX- (ChanNameX+ 20), g_Locale->getText(loc), COL_INFOBAR, 0, true); // UTF-8
	} else {
		show_Data ();
	}
}

void CInfoViewer::showMovieTitle(const int playState, const std::string &Channel,
				 const std::string &g_file_epg, const std::string &g_file_epg1,
				 const int duration, const int curr_pos)
{
	check_channellogo_ca_SettingsChange();
	aspectRatio = 0;
	last_curr_id = last_next_id = 0;
	showButtonBar = true;


	fileplay = true;
	reset_allScala();
	if (!gotTime)
		gotTime = timeset;

	if (g_settings.radiotext_enable && g_Radiotext) {
		g_Radiotext->RT_MsgShow = true;
	}

	if(!is_visible)
		fader.StartFadeIn();

	is_visible = true;

	ChannelName = Channel;
	channel_id = 0;

	/* showChannelLogo() changes this, so better reset it every time... */
	ChanNameX = BoxStartX + ChanWidth + SHADOW_OFFSET;

	paintBackground(COL_INFOBAR_PLUS_0);

	bool show_dot = true;
	paintTime (show_dot, true);
	showRecordIcon (show_dot);
	show_dot = !show_dot;
	showInfoFile();
	paintshowButtonBar();

	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(ChanNameX + 10 , ChanNameY + time_height,BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 5 ,ChannelName, COL_INFOBAR, 0, true);	// UTF-8

	// show_Data
	if (CMoviePlayerGui::getInstance().file_prozent > 100)
		CMoviePlayerGui::getInstance().file_prozent = 100;

	char runningRest[32]; // %d can be 10 digits max...
	sprintf(runningRest, "%d / %d min", (curr_pos + 30000) / 60000, (duration + 30000) / 60000);
	display_Info(g_file_epg.c_str(), g_file_epg1.c_str(), true, false, CMoviePlayerGui::getInstance().file_prozent, NULL, runningRest);

	const char *playicon = NULL;
	switch (playState) {
	case CMoviePlayerGui::PLAY:
		playicon = NEUTRINO_ICON_PLAY;
		break;
	case CMoviePlayerGui::PAUSE:
		playicon = NEUTRINO_ICON_PAUSE;
		break;
	case CMoviePlayerGui::REW:
		playicon = NEUTRINO_ICON_REW;
		break;
	case CMoviePlayerGui::FF:
		playicon = NEUTRINO_ICON_FF;
		break;
	default:
		/* NULL crashes in getIconSize, just use something */
		playicon = NEUTRINO_ICON_BUTTON_HELP;
		break;
	}
	int icon_w = 0,icon_h = 0;
	frameBuffer->getIconSize(playicon, &icon_w, &icon_h);
	int icon_x = BoxStartX + ChanWidth / 2 - icon_w / 2;
	int icon_y = BoxStartY + ChanHeight / 2 - icon_h / 2;
	frameBuffer->paintIcon(playicon, icon_x, icon_y);

	showLcdPercentOver ();
	//loop(fadeValue, show_dot , fadeIn);
	loop(show_dot);
	aspectRatio = 0;
	fileplay = 0;
}

void CInfoViewer::reset_allScala()
{
	sigscale->reset();
	snrscale->reset();
	timescale->reset();
	hddscale->reset();
	varscale->reset();
	lastsig = lastsnr = lasthdd = lastvar = -1;
}

void CInfoViewer::check_channellogo_ca_SettingsChange()
{
	if (casysChange != g_settings.casystem_display || channellogoChange != g_settings.infobar_show_channellogo) {
		casysChange = g_settings.casystem_display;
		channellogoChange = g_settings.infobar_show_channellogo;

		if (g_settings.casystem_display < 2)
			bottom_bar_offset = 22;
		else
			bottom_bar_offset = 0;
		start();
	}
}

void CInfoViewer::showTitle (const int ChanNum, const std::string & Channel, const t_satellite_position satellitePosition, const t_channel_id new_channel_id, const bool calledFromNumZap, int epgpos)
{
	check_channellogo_ca_SettingsChange();
	aspectRatio = 0;
	last_curr_id = last_next_id = 0;
	showButtonBar = !calledFromNumZap;

	fileplay = (ChanNum == 0);
	newfreq = true;

	reset_allScala();
	if (!gotTime)
		gotTime = timeset;

	if(!is_visible && !calledFromNumZap)
		fader.StartFadeIn();

	is_visible = true;

	int col_NumBoxText = COL_INFOBAR;
	int col_NumBox = COL_INFOBAR_PLUS_0;
	ChannelName = Channel;
	bool new_chan = false;

	if (virtual_zap_mode) {
		if (g_RemoteControl->current_channel_id != new_channel_id) {
			col_NumBoxText = COL_MENUHEAD;
		}
		if ((channel_id != new_channel_id) || (evtlist.empty())) {
			evtlist.clear();
			//evtlist = g_Sectionsd->getEventsServiceKey(new_channel_id & 0xFFFFFFFFFFFFULL);
			sectionsd_getEventsServiceKey(new_channel_id, evtlist);
			if (!evtlist.empty())
				sort(evtlist.begin(),evtlist.end(), sortByDateTime);
			new_chan = true;
		}
	}
	if (! calledFromNumZap && !(g_RemoteControl->subChannels.empty()) && (g_RemoteControl->selected_subchannel > 0))
	{
		channel_id = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].getChannelID();
		ChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
	} else {
		channel_id = new_channel_id;
	}

	/* showChannelLogo() changes this, so better reset it every time... */
	ChanNameX = BoxStartX + ChanWidth + SHADOW_OFFSET;


	paintBackground(col_NumBox);

	bool show_dot = true;
	paintTime (show_dot, true);
	showRecordIcon (show_dot);
	show_dot = !show_dot;

	showInfoFile();
	if (showButtonBar) {
		paintshowButtonBar();
	}

	int ChanNumWidth = 0;
	int ChannelLogoMode = 0;
	bool logo_ok = false;
	if (ChanNum) /* !fileplay */
	{
		char strChanNum[10];
		snprintf (strChanNum, sizeof(strChanNum), "%d", ChanNum);
		const int channel_number_width =(g_settings.infobar_show_channellogo == 6) ? 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (strChanNum) : 0;
		ChannelLogoMode = showChannelLogo(channel_id,channel_number_width); // get logo mode, paint channel logo if adjusted
		logo_ok = ( g_settings.infobar_show_channellogo != 0 && ChannelLogoMode != 0);
		fprintf(stderr, "after showchannellogo, mode = %d ret = %d logo_ok = %d\n",g_settings.infobar_show_channellogo, ChannelLogoMode, logo_ok);

		int ChanNumYPos = BoxStartY + ChanHeight;
		if (g_settings.infobar_sat_display && !satellitePositions.empty()) {
			sat_iterator_t sit = satellitePositions.find(satellitePosition);

			if (sit != satellitePositions.end()) {
				int satNameWidth = g_SignalFont->getRenderWidth (sit->second.name);
				std::string satname_tmp = sit->second.name;
				if (satNameWidth > (ChanWidth - 4)) {
					satNameWidth = ChanWidth - 4;
					size_t pos1 = sit->second.name.find("(") ;
					size_t pos2 = sit->second.name.find_last_of(")");
					size_t pos0 = sit->second.name.find(" ") ;
					if ((pos1 != std::string::npos) && (pos2 != std::string::npos) && (pos0 != std::string::npos)) {
						pos1++;
						satname_tmp = sit->second.name.substr(0, pos0 );

						if(satname_tmp == "Hot")
							satname_tmp = "Hotbird";

						satname_tmp +=" ";
						satname_tmp += sit->second.name.substr( pos1,pos2-pos1 );
						satNameWidth = g_SignalFont->getRenderWidth (satname_tmp);
						if (satNameWidth > (ChanWidth - 4)) 
							satNameWidth = ChanWidth - 4;
					}
				}
				int chanH = g_SignalFont->getHeight ();
				g_SignalFont->RenderString (3 + BoxStartX + ((ChanWidth - satNameWidth) / 2) , BoxStartY + chanH, satNameWidth, satname_tmp, COL_INFOBAR);
			}
			ChanNumYPos += 10;
		}

		/* TODO: the logic will get much easier once we decouple channellogo and signal bars */
		if ((!logo_ok && g_settings.infobar_show_channellogo < 2) || g_settings.infobar_show_channellogo == 2 || g_settings.infobar_show_channellogo == 4) // no logo in numberbox
		{
			// show number in numberbox
			int tmpwidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getRenderWidth(strChanNum);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->RenderString(
				BoxStartX + (ChanWidth - tmpwidth) / 2, ChanNumYPos,
				ChanWidth, strChanNum, col_NumBoxText);
		}
		if (ChannelLogoMode == 1 || ( g_settings.infobar_show_channellogo == 3 && !logo_ok) || g_settings.infobar_show_channellogo == 6 ) /* channel number besides channel name */
		{
			ChanNumWidth = 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (strChanNum);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(
				ChanNameX + 5, ChanNameY + time_height,
				ChanNumWidth, strChanNum, col_NumBoxText);
		}
	}

	if (g_settings.infobar_show_channellogo < 5 || !logo_ok) {
		if (ChannelLogoMode != 2) {
			//FIXME good color to display inactive for zap ?
			//uint8_t    color = CNeutrinoApp::getInstance ()->channelList->SameTP(new_channel_id) ? COL_INFOBAR : COL_INFOBAR_SHADOW;
			uint8_t    color = COL_INFOBAR;
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(
				ChanNameX + 10 + ChanNumWidth, ChanNameY + time_height,
				BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 5 - ChanNumWidth,
				ChannelName, color /*COL_INFOBAR*/, 0, true);	// UTF-8
		}
	}

	if (fileplay) {
		show_Data ();
	} else {
		show_current_next(new_chan,epgpos);
	}
	showLcdPercentOver ();

	if ((g_RemoteControl->current_channel_id == channel_id) && !(((info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (info_CurrentNext.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_no_current))) || (info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast))) {
		g_Sectionsd->setServiceChanged (channel_id & 0xFFFFFFFFFFFFULL, true);
	}

	// Radiotext
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio)
	{
		if ((g_settings.radiotext_enable) && (!recordModeActive) && (!calledFromNumZap))
			showRadiotext();
		else
			showIcon_RadioText(false);
	}


	if (!calledFromNumZap) {
		//loop(fadeValue, show_dot , fadeIn);
		loop(show_dot);
	}
	else
		frameBuffer->blit();
	aspectRatio = 0;
	fileplay = 0;
}
void CInfoViewer::setInfobarTimeout(int timeout_ext)
{
	int mode = CNeutrinoApp::getInstance()->getMode();
	//define timeouts
	switch (mode)
	{
		case NeutrinoMessages::mode_tv:
				timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] + timeout_ext);
				break;
		case NeutrinoMessages::mode_radio:
				timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_RADIO] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_RADIO] + timeout_ext);
				break;
		case NeutrinoMessages::mode_ts:
				timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_MOVIE] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR_MOVIE] + timeout_ext);
				break;
		default:
				timeoutEnd = CRCInput::calcTimeoutEnd(6 + timeout_ext); 
				break;
	}
}
void CInfoViewer::loop(bool show_dot)
{
	bool hideIt = true;
	virtual_zap_mode = false;
	//bool fadeOut = false;
	timeoutEnd=0;;
	setInfobarTimeout();

	int res = messages_return::none;
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) {
		frameBuffer->blit();
		g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);

		if (msg == (neutrino_msg_t) g_settings.key_screenshot) {
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
		  
		} else if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
			g_RCInput->postMsg (msg, 0);
			res = messages_return::cancel_info;
		}
		else if (msg == CRCInput::RC_help || msg == CRCInput::RC_info) {
			g_RCInput->postMsg (NeutrinoMessages::SHOW_EPG, 0);
			res = messages_return::cancel_info;
		} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetTimer())) {
			if(fader.Fade())
				res = messages_return::cancel_info;
		} else if ((msg == CRCInput::RC_ok) || (msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) {
			if(fader.StartFadeOut())
				timeoutEnd = CRCInput::calcTimeoutEnd (1);
			else
				res = messages_return::cancel_info;
		} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == sec_timer_id)) {
			showSNR ();
			paintTime (show_dot, false);
			showRecordIcon (show_dot);
			show_dot = !show_dot;
			if ((g_settings.radiotext_enable) && (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio))
				showRadiotext();

			showIcon_16_9();
			showIcon_Resolution();
		} else if ((g_settings.mode_left_right_key_tv == SNeutrinoSettings::VZAP) && ((msg == CRCInput::RC_right) || (msg == CRCInput::RC_left ))) {
			virtual_zap_mode = true;
			res = messages_return::cancel_all;
			hideIt = true;
		} else if ((msg == NeutrinoMessages::EVT_RECORDMODE) && 
			   (CMoviePlayerGui::getInstance().timeshift) && (CRecordManager::getInstance()->GetRecordCount() == 1)) {
			res = CNeutrinoApp::getInstance()->handleMsg(msg, data);
		} else if (!fileplay && !CMoviePlayerGui::getInstance().timeshift) {
			CNeutrinoApp *neutrino = CNeutrinoApp::getInstance ();
			if ((msg == (neutrino_msg_t) g_settings.key_quickzap_up) || (msg == (neutrino_msg_t) g_settings.key_quickzap_down) || (msg == CRCInput::RC_0) || (msg == NeutrinoMessages::SHOW_INFOBAR)) {
				if ((g_settings.radiotext_enable) && (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio))
					hideIt =  true;
				else
					hideIt = false;
				int rec_mode = CRecordManager::getInstance()->GetRecordMode();
				if ((rec_mode == CRecordManager::RECMODE_REC) || (rec_mode == CRecordManager::RECMODE_REC_TSHIFT))
					hideIt = true;
				//hideIt = (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0) ? true : false;
				g_RCInput->postMsg (msg, data);
				res = messages_return::cancel_info;
			} else if (msg == NeutrinoMessages::EVT_TIMESET) {
				/* handle timeset event in upper layer, ignore here */
				res = neutrino->handleMsg (msg, data);
			} else {
				if (msg == CRCInput::RC_standby) {
					g_RCInput->killTimer (sec_timer_id);
					fader.Stop();
				}
				res = neutrino->handleMsg (msg, data);
				if (res & messages_return::unhandled) {
					// raus hier und im Hauptfenster behandeln...
					g_RCInput->postMsg (msg, data);
					res = messages_return::cancel_info;
				}
			}
		} else if (fileplay && !CMoviePlayerGui::getInstance().timeshift /* && ( (msg == (neutrino_msg_t) g_settings.mpkey_pause) || (msg == (neutrino_msg_t) g_settings.mpkey_rewind) || (msg == (neutrino_msg_t) g_settings.mpkey_play) || (msg == (neutrino_msg_t) g_settings.mpkey_forward) || (msg == (neutrino_msg_t) g_settings.mpkey_stop)) */ ) {
			/* this debug message will only hit in movieplayer mode, where console is
			 * spammed to death anyway... */
			printf("%s:%d msg:%08lx, data: %08lx\n", __func__, __LINE__, (long)msg, (long)data);
			if (msg < CRCInput::RC_Events) /* RC / Keyboard event */
			{
				g_RCInput->postMsg (msg, data);
				res = messages_return::cancel_info;
			}
			else
				res = CNeutrinoApp::getInstance()->handleMsg(msg, data);

		} 
#if 0
		else if (CMoviePlayerGui::getInstance().start_timeshift && (msg == NeutrinoMessages::EVT_TIMER)) {
			CMoviePlayerGui::getInstance().start_timeshift = false;
		} 
#endif
		else if (CMoviePlayerGui::getInstance().timeshift && ((msg == (neutrino_msg_t) g_settings.mpkey_rewind)  || \
		   							(msg == (neutrino_msg_t) g_settings.mpkey_forward) || \
		   							(msg == (neutrino_msg_t) g_settings.mpkey_pause)   || \
		   							(msg == (neutrino_msg_t) g_settings.mpkey_stop)    || \
		   							(msg == (neutrino_msg_t) g_settings.mpkey_play)    || \
		   							(msg == (neutrino_msg_t) g_settings.mpkey_time)    || \
		   							(msg == (neutrino_msg_t) g_settings.key_timeshift))) {
			g_RCInput->postMsg (msg, data);
			res = messages_return::cancel_info;
		}
	}

	if (hideIt)
		killTitle ();

	g_RCInput->killTimer (sec_timer_id);
	fader.Stop();
	if (virtual_zap_mode) {
		/* if bouquet cycle set, do virtual over current bouquet */
		if (g_settings.zap_cycle && (bouquetList != NULL) && !(bouquetList->Bouquets.empty()))
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
				int w = 20 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth (g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE), true) + 20;	// UTF-8
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
			//g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + 30, dx - 20, text, COL_MENUCONTENT, 0, true);

			if (g_RemoteControl->director_mode) {
				lframeBuffer->paintIcon (NEUTRINO_ICON_BUTTON_YELLOW, x + 8, y + dy - 20);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (x + 30, y + dy - 2, dx - 40, g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE), COL_MENUCONTENT, 0, true);	// UTF-8
				int h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + dy - h - 2, dx - 20, text, COL_MENUCONTENT, 0, true);
			} else
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + dy - 2, dx - 20, text, COL_MENUCONTENT, 0, true);

			uint64_t timeoutEnd_tmp = CRCInput::calcTimeoutEnd (2);
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

void CInfoViewer::showIcon_RadioText(bool /*rt_available*/) const
// painting the icon for radiotext mode
{
#if 0
	if (showButtonBar)
	{
		int mode = CNeutrinoApp::getInstance()->getMode();
		std::string rt_icon = "radiotextoff.raw";
		if ((!virtual_zap_mode) && (!recordModeActive) && (mode == NeutrinoMessages::mode_radio))
		{
			if (g_settings.radiotext_enable){
					rt_icon = rt_available ? "radiotextget.raw" : "radiotextwait.raw";
				}
		}
		frameBuffer->paintIcon(rt_icon, BoxEndX - (ICON_LARGE_WIDTH + 2 + ICON_LARGE_WIDTH + 2 + ICON_SMALL_WIDTH + 2 + ICON_SMALL_WIDTH + 6),BoxEndY + (InfoHeightY_Info - ICON_HEIGHT) / 2);
	}
#endif
}

void CInfoViewer::showIcon_16_9 ()
{
	if ((aspectRatio == 0) || ( g_RemoteControl->current_PIDs.PIDs.vpid == 0 ) || (aspectRatio != videoDecoder->getAspectRatio())) {
		if ( g_RemoteControl->current_PIDs.PIDs.vpid > 0 ) {
			aspectRatio = videoDecoder->getAspectRatio();
		}
		else {
			aspectRatio = 0;
		}
		frameBuffer->paintIcon((aspectRatio > 2) ? NEUTRINO_ICON_16_9 : NEUTRINO_ICON_16_9_GREY,
				       BoxEndX - (2*icon_large_width + 2*icon_small_width + 4*2), BBarY,
				       InfoHeightY_Info, 1, true, true, COL_INFOBAR_BUTTONS_BACKGROUND);
	}
}

void CInfoViewer::showIcon_VTXT () const
{
	frameBuffer->paintIcon((g_RemoteControl->current_PIDs.PIDs.vtxtpid != 0) ? NEUTRINO_ICON_VTXT : NEUTRINO_ICON_VTXT_GREY,
			       BoxEndX - (2*icon_small_width + 2*2), BBarY, InfoHeightY_Info, 1, true, true, COL_INFOBAR_BUTTONS_BACKGROUND);
}

void CInfoViewer::showIcon_Resolution() const
{
	int xres, yres, framerate;
	const char *icon_name = NULL;
	if (videoDecoder->getBlank()) {
		icon_name = NEUTRINO_ICON_RESOLUTION_000;
	} else {
		if (g_settings.infobar_show_res == 0) {//show resolution icon on infobar
			videoDecoder->getPictureInfo(xres, yres, framerate);
			switch (yres) {
			case 1920:
				icon_name = NEUTRINO_ICON_RESOLUTION_1920;
				break;
			case 1088:
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
			switch (yres) {
			case 1920:
			case 1440:
			case 1280:
			case 1088:
			case 720:
				icon_name = NEUTRINO_ICON_RESOLUTION_HD;
				break;
			case 704:
			case 576:
			case 544:
			case 528:
			case 480:
			case 382:
			case 352:
			case 288:
				icon_name = NEUTRINO_ICON_RESOLUTION_SD;
				break;
			default:
				icon_name = NEUTRINO_ICON_RESOLUTION_000;
				break;
			}
		}
	}
	if (g_settings.infobar_show_res < 2)
		frameBuffer->paintIcon(icon_name, BoxEndX - (icon_xres_width + 2*icon_large_width + 2*icon_small_width + 5*2), BBarY, 
				       InfoHeightY_Info, 1, true, true, COL_INFOBAR_BUTTONS_BACKGROUND);
}

void CInfoViewer::showIcon_SubT() const
{
	bool have_sub = false;
	CZapitChannel * cc = CNeutrinoApp::getInstance()->channelList->getChannel(CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber());
	if (cc && cc->getSubtitleCount())
		have_sub = true;

	frameBuffer->paintIcon(have_sub ? NEUTRINO_ICON_SUBT : NEUTRINO_ICON_SUBT_GREY, BoxEndX - (icon_small_width + 2),
			       BBarY, InfoHeightY_Info, 1, true, true, COL_INFOBAR_BUTTONS_BACKGROUND);
}

void CInfoViewer::showFailure ()
{
	ShowHintUTF (LOCALE_MESSAGEBOX_ERROR, g_Locale->getText (LOCALE_INFOVIEWER_NOTAVAILABLE), 430);	// UTF-8
}

void CInfoViewer::showMotorMoving (int duration)
{
	setInfobarTimeout(duration + 1);

	char text[256];
	snprintf(text, sizeof(text), "%s (%ds)", g_Locale->getText (LOCALE_INFOVIEWER_MOTOR_MOVING), duration);
	ShowHintUTF (LOCALE_MESSAGEBOX_INFO, text, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth (text, true) + 10, duration);	// UTF-8
}

void CInfoViewer::killRadiotext()
{
	frameBuffer->paintBackgroundBox(rt_x, rt_y, rt_w, rt_h);
	frameBuffer->blit();
}

void CInfoViewer::showRadiotext()
{
	char stext[3][100];
//	int yoff = 8;
	int ii = 0;
	bool RTisIsUTF = false;

	if (g_Radiotext == NULL) return;
	showIcon_RadioText(g_Radiotext->haveRadiotext());

	if (g_Radiotext->S_RtOsd) {
		// dimensions of radiotext window
		rt_dx = BoxEndX - BoxStartX;
		rt_dy = 25;
		rt_x = BoxStartX;
		rt_y = g_settings.screen_StartY + 10;
		rt_h = rt_y + 7 + rt_dy*(g_Radiotext->S_RtOsdRows+1)+SHADOW_OFFSET;
		rt_w = rt_x+rt_dx+SHADOW_OFFSET;
		
		int lines = 0;
		for (int i = 0; i < g_Radiotext->S_RtOsdRows; i++) {
			if (g_Radiotext->RT_Text[i][0] != '\0') lines++;
		}
		if (lines == 0)
		{
			frameBuffer->paintBackgroundBox(rt_x, rt_y, rt_w, rt_h);
			frameBuffer->blit();
		}
		if (g_Radiotext->RT_MsgShow) {

			if (g_Radiotext->S_RtOsdTitle == 1) {

		// Title
		//	sprintf(stext[0], g_Radiotext->RT_PTY == 0 ? "%s - %s %s%s" : "%s - %s (%s)%s",
		//	g_Radiotext->RT_Titel, tr("Radiotext"), g_Radiotext->RT_PTY == 0 ? g_Radiotext->RDS_PTYN : g_Radiotext->ptynr2string(g_Radiotext->RT_PTY), g_Radiotext->RT_MsgShow ? ":" : tr("  [waiting ...]"));
				if ((lines) || (g_Radiotext->RT_PTY !=0)) {
					sprintf(stext[0], g_Radiotext->RT_PTY == 0 ? "%s %s%s" : "%s (%s)%s", tr("Radiotext"), g_Radiotext->RT_PTY == 0 ? g_Radiotext->RDS_PTYN : g_Radiotext->ptynr2string(g_Radiotext->RT_PTY), ":");
					
					// shadow
					frameBuffer->paintBoxRel(rt_x+SHADOW_OFFSET, rt_y+SHADOW_OFFSET, rt_dx, rt_dy, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_LARGE, CORNER_TOP);
					frameBuffer->paintBoxRel(rt_x, rt_y, rt_dx, rt_dy, COL_INFOBAR_PLUS_0, RADIUS_LARGE, CORNER_TOP);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rt_x+10, rt_y+ 30, rt_dx-20, stext[0], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
					frameBuffer->blit();
				}
//				yoff = 17;
				ii = 1;
#if 0
			// RDS- or Rass-Symbol, ARec-Symbol or Bitrate
			int inloff = (ftitel->Height() + 9 - 20) / 2;
			if (Rass_Flags[0][0]) {
				osd->DrawBitmap(Setup.OSDWidth-51, inloff, rass, bcolor, fcolor);
				if (ARec_Record)
					osd->DrawBitmap(Setup.OSDWidth-107, inloff, arec, bcolor, 0xFFFC1414);	// FG=Red
				else
					inloff = (ftitel->Height() + 9 - ftext->Height()) / 2;
				osd->DrawText(4, inloff, RadioAudio->bitrate, fcolor, clrTransparent, ftext, Setup.OSDWidth-59, ftext->Height(), taRight);
			}
			else {
				osd->DrawBitmap(Setup.OSDWidth-84, inloff, rds, bcolor, fcolor);
				if (ARec_Record)
					osd->DrawBitmap(Setup.OSDWidth-140, inloff, arec, bcolor, 0xFFFC1414);	// FG=Red
				else
					inloff = (ftitel->Height() + 9 - ftext->Height()) / 2;
				osd->DrawText(4, inloff, RadioAudio->bitrate, fcolor, clrTransparent, ftext, Setup.OSDWidth-92, ftext->Height(), taRight);
			}
#endif
			}
			// Body
			if (lines) {
				frameBuffer->paintBoxRel(rt_x+SHADOW_OFFSET, rt_y+rt_dy+SHADOW_OFFSET, rt_dx, 7+rt_dy* g_Radiotext->S_RtOsdRows, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);
				frameBuffer->paintBoxRel(rt_x, rt_y+rt_dy, rt_dx, 7+rt_dy* g_Radiotext->S_RtOsdRows, COL_INFOBAR_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

				// RT-Text roundloop
				int ind = (g_Radiotext->RT_Index == 0) ? g_Radiotext->S_RtOsdRows - 1 : g_Radiotext->RT_Index - 1;
				int rts_x = rt_x+10;
				int rts_y = rt_y+ 30;
				int rts_dx = rt_dx-20;
				if (g_Radiotext->S_RtOsdLoop == 1) { // latest bottom
					for (int i = ind+1; i < g_Radiotext->S_RtOsdRows; i++)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
					for (int i = 0; i <= ind; i++)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
				}
				else { // latest top
					for (int i = ind; i >= 0; i--)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
					for (int i = g_Radiotext->S_RtOsdRows-1; i > ind; i--)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
				}
				frameBuffer->blit();
			}
#if 0
			// + RT-Plus or PS-Text = 2 rows
			if ((S_RtOsdTags == 1 && RT_PlusShow) || S_RtOsdTags >= 2) {
				if (!RDS_PSShow || !strstr(RTP_Title, "---") || !strstr(RTP_Artist, "---")) {
					sprintf(stext[1], "> %s  %s", tr("Title  :"), RTP_Title);
					sprintf(stext[2], "> %s  %s", tr("Artist :"), RTP_Artist);
					osd->DrawText(4, 6+yoff+fheight*(ii++), stext[1], fcolor, clrTransparent, ftext, Setup.OSDWidth-4, ftext->Height());
					osd->DrawText(4, 3+yoff+fheight*(ii++), stext[2], fcolor, clrTransparent, ftext, Setup.OSDWidth-4, ftext->Height());
				}
				else {
					char *temp = "";
					int ind = (RDS_PSIndex == 0) ? 11 : RDS_PSIndex - 1;
					for (int i = ind+1; i < 12; i++)
						asprintf(&temp, "%s%s ", temp, RDS_PSText[i]);
					for (int i = 0; i <= ind; i++)
						asprintf(&temp, "%s%s ", temp, RDS_PSText[i]);
					snprintf(stext[1], 6*9, "%s", temp);
					snprintf(stext[2], 6*9, "%s", temp+(6*9));
					free(temp);
					osd->DrawText(6, 6+yoff+fheight*ii, "[", fcolor, clrTransparent, ftext, 12, ftext->Height());
					osd->DrawText(Setup.OSDWidth-12, 6+yoff+fheight*ii, "]", fcolor, clrTransparent, ftext, Setup.OSDWidth-6, ftext->Height());
					osd->DrawText(16, 6+yoff+fheight*(ii++), stext[1], fcolor, clrTransparent, ftext, Setup.OSDWidth-16, ftext->Height(), taCenter);
					osd->DrawText(6, 3+yoff+fheight*ii, "[", fcolor, clrTransparent, ftext, 12, ftext->Height());
					osd->DrawText(Setup.OSDWidth-12, 3+yoff+fheight*ii, "]", fcolor, clrTransparent, ftext, Setup.OSDWidth-6, ftext->Height());
					osd->DrawText(16, 3+yoff+fheight*(ii++), stext[2], fcolor, clrTransparent, ftext, Setup.OSDWidth-16, ftext->Height(), taCenter);
				}
			}
#endif
		}
#if 0
// framebuffer can only display raw images
		// show mpeg-still
		char *image;
		if (g_Radiotext->Rass_Archiv >= 0)
			asprintf(&image, "%s/Rass_%d.mpg", DataDir, g_Radiotext->Rass_Archiv);
		else
			asprintf(&image, "%s/Rass_show.mpg", DataDir);
		frameBuffer->useBackground(frameBuffer->loadBackground(image));// set useBackground true or false
		frameBuffer->paintBackground();
//		RadioAudio->SetBackgroundImage(image);
		free(image);
#endif
	}
	g_Radiotext->RT_MsgShow = false;

}


int CInfoViewer::handleMsg (const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if ((msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG) || (msg == NeutrinoMessages::EVT_NEXTPROGRAM)) {
//printf("CInfoViewer::handleMsg: NeutrinoMessages::EVT_CURRENTNEXT_EPG data %llx current %llx\n", *(t_channel_id *) data, channel_id & 0xFFFFFFFFFFFFULL);
		if ((*(t_channel_id *) data) == (channel_id & 0xFFFFFFFFFFFFULL)) {
			getEPG (*(t_channel_id *) data, info_CurrentNext);
			if (is_visible)
				show_Data (true);
			showLcdPercentOver ();
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOTPIDS) {
		if ((*(t_channel_id *) data) == channel_id) {
			if (is_visible && showButtonBar) {
				showIcon_VTXT ();
				showIcon_SubT();
				showIcon_CA_Status (0);
				showIcon_Resolution();
			}
		}
		return messages_return::handled;
	} else if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) ||
			(msg == NeutrinoMessages::EVT_ZAP_ISNVOD)) {
		channel_id = (*(t_channel_id *)data);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_ID) {
		chanready = 1;
		Set_CA_Status (data);
		showSNR ();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_TIMER) {
		if (data == fader.GetTimer()) {
			// here, the event can only come if there is another window in the foreground!
			fader.Stop();
			return messages_return::handled;
		} else if (data == lcdUpdateTimer) {
//printf("CInfoViewer::handleMsg: lcdUpdateTimer\n");
			if ( is_visible )
				show_Data( true );
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
		if ((*(t_channel_id *) data) == channel_id) {
			if (is_visible && showButtonBar)
				showButton_Audio ();
			if (g_settings.radiotext_enable && g_Radiotext && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio))
				g_Radiotext->setPid(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES) {
		if ((*(t_channel_id *) data) == channel_id) {
			if (is_visible && showButtonBar)
				showButton_SubServices ();
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE) {
		chanready = 1;
		showSNR ();
		//if ((*(t_channel_id *)data) == channel_id)
		{
			if (is_visible && showButtonBar && (!g_RemoteControl->are_subchannels))
				show_Data (true);
		}
		showLcdPercentOver ();
		eventname = info_CurrentNext.current_name;
		CVFD::getInstance()->setEPGTitle(eventname);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_SUB_FAILED) {
		chanready = 1;
		showSNR ();
		// show failure..!
		CVFD::getInstance ()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')');
		printf ("zap failed!\n");
		showFailure ();
		CVFD::getInstance ()->showPercentOver (255);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_FAILED) {
		chanready = 1;
		showSNR ();
		if ((*(t_channel_id *) data) == channel_id) {
			// show failure..!
			CVFD::getInstance ()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')');
			printf ("zap failed!\n");
			showFailure ();
			CVFD::getInstance ()->showPercentOver (255);
		}
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_MOTOR) {
		chanready = 0;
		showMotorMoving (data);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_MODECHANGED) {
		aspectRatio = data;
		if (is_visible && showButtonBar)
			showIcon_16_9 ();
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_TIMESET) {
		gotTime = true;
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_CLEAR) {
		Set_CA_Status (false);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_LOCK) {
		Set_CA_Status (true);
		return messages_return::handled;
	} else if (msg == NeutrinoMessages::EVT_ZAP_CA_FTA) {
		Set_CA_Status (false);
		return messages_return::handled;
	}

	return messages_return::unhandled;
}

void CInfoViewer::showButton_SubServices ()
{
	if (!(g_RemoteControl->subChannels.empty ())) {
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW,
				       ChanInfoX + 10 + (icol_w + 4 + asize + 2) * 2, BBarY, InfoHeightY_Info);
		/*ChanInfoX + 10 + NEUTRINO_ICON_BUTTON_RED_WIDTH + 4 + asize + 2 + NEUTRINO_ICON_BUTTON_GREEN_WIDTH + 4 + asize + 2, BBarY, InfoHeightY_Info);*/
		std::string txt = g_settings.usermenu_text[SNeutrinoSettings::BUTTON_YELLOW];
		if (txt.empty())
			txt = g_Locale->getText((g_RemoteControl->are_subchannels) ? LOCALE_INFOVIEWER_SUBSERVICE : LOCALE_INFOVIEWER_SELECTTIME);

		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(
			ChanInfoX + 10 + (icol_w + 4 + asize + 2) * 2 + icol_w + 4,
			/*ChanInfoX + 10 + NEUTRINO_ICON_BUTTON_RED_WIDTH + 4 + asize + 2 + NEUTRINO_ICON_BUTTON_GREEN_WIDTH + 4 + asize + 2 + NEUTRINO_ICON_BUTTON_YELLOW_WIDTH + 4,*/
			BBarFontY, asize, txt, COL_INFOBAR_BUTTONS, 0, true); // UTF-8
	}
}

void CInfoViewer::getEPG(const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info)
{
	static CSectionsdClient::CurrentNextInfo oldinfo;

	/* to clear the oldinfo for channels without epg, call getEPG() with for_channel_id = 0 */
	if (for_channel_id == 0)
	{
		oldinfo.current_uniqueKey = 0;
		return;
	}

	sectionsd_getCurrentNextServiceKey(for_channel_id & 0xFFFFFFFFFFFFULL, info);

	/* of there is no EPG, send an event so that parental lock can work */
	if (info.current_uniqueKey == 0 && info.next_uniqueKey == 0) {
		memcpy(&oldinfo, &info, sizeof(CSectionsdClient::CurrentNextInfo));
		char *p = new char[sizeof(t_channel_id)];
		memcpy(p, &for_channel_id, sizeof(t_channel_id));
		g_RCInput->postMsg (NeutrinoMessages::EVT_NOEPG_YET, (const neutrino_msg_data_t) p, false);
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
	char percent[10];
	uint16_t ssig, ssnr;
	/* right now, infobar_show_channellogo == 3 is the trigger for signal bars etc.
	   TODO: decouple this  */
	if (! fileplay && ( g_settings.infobar_show_channellogo == 3 || g_settings.infobar_show_channellogo == 5 || g_settings.infobar_show_channellogo == 6 )) {
		int chanH = g_SignalFont->getHeight();
		int freqStartY = BoxStartY + 2 * chanH - 3;
		if ((newfreq && chanready) || SDT_freq_update) {
			char freq[20];
			newfreq = false;

			CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo ();
			std::string polarisation;
			if (g_info.delivery_system == DVB_S)
				polarisation = (si.polarisation) ? "V" : "H";
			else
				polarisation = "";
			snprintf (freq, sizeof(freq), "%d.%d MHz %s", si.tsfrequency / 1000, si.tsfrequency % 1000, polarisation.c_str());

			int satNameWidth = g_SignalFont->getRenderWidth (freq);
			g_SignalFont->RenderString (3 + BoxStartX + ((ChanWidth - satNameWidth) / 2), BoxStartY + 2 * chanH - 3, satNameWidth, freq, SDT_freq_update ? COL_COLORED_EVENTS_INFOBAR:COL_INFOBAR);
			SDT_freq_update = false;
		}
		int sw, snr, sig, posx, posy;
		int height;
		ssig = CFrontend::getInstance()->getSignalStrength();
		ssnr = CFrontend::getInstance()->getSignalNoiseRatio();

		sig = (ssig & 0xFFFF) * 100 / 65535;
		snr = (ssnr & 0xFFFF) * 100 / 65535;
		height = g_SignalFont->getHeight () - 1;

		if (lastsig != sig) {
			lastsig = sig;
			posx = BoxStartX + (ChanWidth - (bar_width + 2 + (g_SignalFont->getWidth() * 4))) / 2;
			posy = freqStartY;
			sigscale->paintProgressBar(posx, posy+4, bar_width, 10 * g_settings.screen_yres / 100, sig, 100);
			snprintf (percent, sizeof(percent), "%d%%S", sig);
			posx = posx + bar_width + 2;
			sw = BoxStartX + ChanWidth - posx;
			frameBuffer->paintBoxRel (posx, posy, sw, height, COL_INFOBAR_PLUS_0);
			g_SignalFont->RenderString (posx, posy + height, sw, percent, COL_INFOBAR);
		}
		if (lastsnr != snr) {
			lastsnr = snr;
			posx = BoxStartX + (ChanWidth - (bar_width + 2 + (g_SignalFont->getWidth() * 4))) / 2;
			posy = freqStartY + height - (2 * g_settings.screen_yres / 100);
			snrscale->paintProgressBar(posx, posy+4, bar_width, 10 * g_settings.screen_yres / 100, snr, 100);
			snprintf (percent, sizeof(percent), "%d%%Q", snr);
			posx = posx + bar_width + 2;
			sw = BoxStartX + ChanWidth - posx -4;
			frameBuffer->paintBoxRel (posx, posy, sw, height-2, COL_INFOBAR_PLUS_0);
			g_SignalFont->RenderString (posx, posy + height, sw, percent, COL_INFOBAR);
		}
	}
	if (g_settings.infobar_show_var_hdd) {
		struct statfs s;
		int per = 0;
		if (::statfs("/var", &s) == 0) {
			per = (s.f_blocks - s.f_bfree) / (s.f_blocks/100);
		}
		/* center the scales in the button bar. BBarY + InfoHeightY_Info / 2 is middle,
		   scales are 6 pixels high, icons are 16 pixels, so keep 4 pixels free between
		   the scales */
		varscale->paintProgressBar(BoxEndX - (((g_settings.casystem_display !=2) ? 0:icon_crypt_width )+ icon_xres_width + 2*icon_large_width + 2*icon_small_width + ((g_settings.casystem_display !=2) ?5:6)*2) - hddwidth - 2,
					   BBarY + InfoHeightY_Info / 2 - 2 - 6, hddwidth , 6, per, 100);
		per = 0;
		//HD info
		if(!check_dir(g_settings.network_nfs_recordingdir)){
			if (::statfs(g_settings.network_nfs_recordingdir, &s) == 0) {
				per = (s.f_blocks - s.f_bfree) / (s.f_blocks/100);
			}
		}
		hddscale->paintProgressBar(BoxEndX - (((g_settings.casystem_display !=2) ? 0:icon_crypt_width )+ icon_xres_width + 2*icon_large_width + 2*icon_small_width + ((g_settings.casystem_display !=2) ?5:6)*2) - hddwidth - 2,
					   BBarY + InfoHeightY_Info / 2 + 2, hddwidth, 6, per, 100);
	}
}

void CInfoViewer::display_Info(const char *current, const char *next,
			       bool UTF8, bool starttimes, const int pb_pos,
			       const char *runningStart, const char *runningRest,
			       const char *nextStart, const char *nextDuration,
			       bool update_current, bool update_next)
{
	/* dimensions of the two-line current-next "box":
	   top of box    == ChanNameY + time_height (bottom of channel name)
	   bottom of box == BoxEndY
	   height of box == BoxEndY - (ChanNameY + time_height)
	   middle of box == top + height / 2
			 == ChanNameY + time_height + (BoxEndY - (ChanNameY + time_height))/2
			 == ChanNameY + time_height + (BoxEndY - ChanNameY - time_height)/2
			 == ChanNameY / 2 + time_height / 2 + BoxEndY / 2
			 == (BoxEndY + ChanNameY + time_height)/2
	   The bottom of current info and the top of next info is == middle of box.
	 */

	int height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight();
	int CurrInfoY = (BoxEndY + ChanNameY + time_height) / 2;
	int NextInfoY = CurrInfoY + height;	// lower end of next info box
	int xStart;
	int InfoX = ChanInfoX + 10;

	xStart = InfoX;
	if (starttimes)
		xStart += info_time_width + 10;

	//colored_events init
	bool colored_event_C = false;
	bool colored_event_N = false;
	if (g_settings.colored_events_infobar == 1)
		colored_event_C = true;
	if (g_settings.colored_events_infobar == 2)
		colored_event_N = true;

	if (pb_pos > -1)
	{
		int pb_w = 112;
		int pb_p = pb_pos * pb_w / 100;
		int pb_h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight() - 4;
		if (pb_p > pb_w)
			pb_p = pb_w;
		timescale->paintProgressBar(BoxEndX - pb_w - SHADOW_OFFSET, ChanNameY - (pb_h + 10) , pb_w, pb_h, pb_p, pb_w,
					    0, 0, g_settings.progressbar_color ? COL_INFOBAR_SHADOW_PLUS_0 : COL_INFOBAR_PLUS_0, COL_INFOBAR_SHADOW_PLUS_0, "", COL_INFOBAR);
		//printf("paintProgressBar(%d, %d, %d, %d)\n", BoxEndX - pb_w - SHADOW_OFFSET, ChanNameY - (pb_h + 10) , pb_w, pb_h);
	}

	int currTimeW = 0;
	int nextTimeW = 0;
	if (runningRest != NULL)
		currTimeW = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(runningRest, UTF8);
	if (nextDuration != NULL)
		nextTimeW = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(nextDuration, UTF8);
	int currTimeX = BoxEndX - currTimeW - 10;
	int nextTimeX = BoxEndX - nextTimeW - 10;
	static int oldCurrTimeX = currTimeX; // remember the last pos. of remaining time, in case we change from 20/100min to 21/99min

	if (current != NULL && update_current)
	{
		frameBuffer->paintBox(InfoX, CurrInfoY - height, currTimeX, CurrInfoY, COL_INFOBAR_PLUS_0);
		if (runningStart != NULL)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(InfoX, CurrInfoY, info_time_width, runningStart, colored_event_C ? COL_COLORED_EVENTS_INFOBAR : COL_INFOBAR, 0, UTF8);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(xStart, CurrInfoY, currTimeX - xStart - 5, current, colored_event_C ? COL_COLORED_EVENTS_INFOBAR : COL_INFOBAR, 0, UTF8);
		oldCurrTimeX = currTimeX;
	}

	if (currTimeX < oldCurrTimeX)
		oldCurrTimeX = currTimeX;
	frameBuffer->paintBox(oldCurrTimeX, CurrInfoY-height, BoxEndX, CurrInfoY, COL_INFOBAR_PLUS_0);
	oldCurrTimeX = currTimeX;
	if (currTimeW != 0)
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(currTimeX, CurrInfoY, currTimeW, runningRest, colored_event_C ? COL_COLORED_EVENTS_INFOBAR : COL_INFOBAR, 0, UTF8);

	if (next != NULL && update_next)
	{
		frameBuffer->paintBox(InfoX, NextInfoY-height, BoxEndX, NextInfoY, COL_INFOBAR_PLUS_0);
		if (nextStart != NULL)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(InfoX, NextInfoY, info_time_width, nextStart, colored_event_N ? COL_COLORED_EVENTS_INFOBAR : COL_INFOBAR, 0, UTF8);
		if (starttimes)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(xStart, NextInfoY, nextTimeX - xStart - 5, next, colored_event_N ? COL_COLORED_EVENTS_INFOBAR : COL_INFOBAR, 0, UTF8);
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(xStart, NextInfoY, nextTimeX - xStart - 5, next, colored_event_N ? COL_COLORED_EVENTS_INFOBAR : COL_INFOBAR, 0, UTF8);
		if (nextTimeW != 0)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(nextTimeX, NextInfoY, nextTimeW, nextDuration, colored_event_N ? COL_COLORED_EVENTS_INFOBAR : COL_INFOBAR, 0, UTF8);
	}
}

void CInfoViewer::show_Data (bool calledFromEvent)
{
	if (! is_visible)
		return;

	/* EPG data is not useful in movieplayer mode ;) */
	if (fileplay && !CMoviePlayerGui::getInstance().timeshift)
		return;

	char runningStart[10];
	char runningRest[20];
	char runningPercent = 0;

	char nextStart[10];
	char nextDuration[10];

	int is_nvod = false;

	if ((g_RemoteControl->current_channel_id == channel_id) && (!g_RemoteControl->subChannels.empty()) && (!g_RemoteControl->are_subchannels)) {
		is_nvod = true;
		info_CurrentNext.current_zeit.startzeit = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].startzeit;
		info_CurrentNext.current_zeit.dauer = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].dauer;
	} else {
		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (showButtonBar)) {
			if ((uint) info_CurrentNext.next_zeit.startzeit < (info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)) {
				is_nvod = true;
			}
		}
	}

	time_t jetzt = time (NULL);

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
		int seit = (abs(jetzt - info_CurrentNext.current_zeit.startzeit) + 30) / 60;
		int rest = (info_CurrentNext.current_zeit.dauer / 60) - seit;
		runningPercent = 0;
		if (!gotTime)
			snprintf(runningRest, sizeof(runningRest), "%d min", info_CurrentNext.current_zeit.dauer / 60);
		else if (jetzt < info_CurrentNext.current_zeit.startzeit)
			snprintf (runningRest, sizeof(runningRest), "in %d min", seit);
		else {
			runningPercent = (jetzt - info_CurrentNext.current_zeit.startzeit) * 100 / info_CurrentNext.current_zeit.dauer;
			if (runningPercent > 100)
				runningPercent = 100;
			if (rest >= 0)
				snprintf(runningRest, sizeof(runningRest), "%d / %d min", seit, rest);
			else
				snprintf(runningRest, sizeof(runningRest), "%d +%d min", info_CurrentNext.current_zeit.dauer / 60, -rest);
		}

		struct tm *pStartZeit = localtime (&info_CurrentNext.current_zeit.startzeit);
		snprintf (runningStart, sizeof(runningStart), "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
	} else
		last_curr_id = 0;

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) {
		unsigned dauer = info_CurrentNext.next_zeit.dauer / 60;
		snprintf (nextDuration, sizeof(nextDuration), "%d min", dauer);
		struct tm *pStartZeit = localtime (&info_CurrentNext.next_zeit.startzeit);
		snprintf (nextStart, sizeof(nextStart), "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
	} else
		last_next_id = 0;

//	int ChanInfoY = BoxStartY + ChanHeight + 15;	//+10

	if (showButtonBar) {
#if 0
		int posy = BoxStartY + 16;
		int height2 = 20;
		//percent
		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
//printf("CInfoViewer::show_Data: ************************************************* runningPercent %d\n", runningPercent);
			if (!calledFromEvent || (oldrunningPercent != runningPercent)) {
				frameBuffer->paintBoxRel(BoxEndX - 104, posy + 6, 108, 14, COL_INFOBAR_SHADOW_PLUS_0, 1);
				frameBuffer->paintBoxRel(BoxEndX - 108, posy + 2, 108, 14, COL_INFOBAR_PLUS_0, 1);
				oldrunningPercent = runningPercent;
			}
			timescale->paint(BoxEndX - 102, posy + 2, runningPercent);
		} else {
			oldrunningPercent = 255;
			frameBuffer->paintBackgroundBoxRel (BoxEndX - 108, posy, 112, height2);
		}
#endif
		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_anything) {
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_RED, ChanInfoX + 10, BBarY, InfoHeightY_Info);
			std::string txt = g_settings.usermenu_text[SNeutrinoSettings::BUTTON_RED];
			if (txt.empty())
				txt = g_Locale->getText(LOCALE_INFOVIEWER_EVENTLIST);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(ChanInfoX + (10 + icol_w + 4), BBarFontY, asize, txt, COL_INFOBAR_BUTTONS, 0, true); // UTF-8
		}
	}

	if ((info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast) ||
			(calledFromEvent && !(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_next|CSectionsdClient::epgflags::has_current))))
	{
		// no EPG available
		display_Info(NULL, g_Locale->getText(gotTime ? LOCALE_INFOVIEWER_NOEPG : LOCALE_INFOVIEWER_WAITTIME));
		/* send message. Parental pin check gets triggered on EPG events... */
		char *p = new char[sizeof(t_channel_id)];
		memmove(p, &channel_id, sizeof(t_channel_id));
		/* clear old info in getEPG */
		CSectionsdClient::CurrentNextInfo dummy;
		getEPG(0, dummy);
		g_RCInput->postMsg(NeutrinoMessages::EVT_NOEPG_YET, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory
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
		if (info_CurrentNext.current_uniqueKey != last_curr_id)
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
	display_Info(current, next, true, true, runningPercent,
		     curr_time, curr_rest, next_time, next_dur, curr_upd, next_upd);

#if 0
	int height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight ();
	int xStart = BoxStartX + ChanWidth;

	//frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);

	if ((info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast) || ((calledFromEvent) && !(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_next | CSectionsdClient::epgflags::has_current)))) {
		// no EPG available
		ChanInfoY += height;
		frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (BoxStartX + ChanWidth + 20, ChanInfoY + height, BoxEndX - (BoxStartX + ChanWidth + 20), g_Locale->getText (gotTime ? LOCALE_INFOVIEWER_NOEPG : LOCALE_INFOVIEWER_WAITTIME), COL_INFOBAR, 0, true);	// UTF-8
	} else {
		// irgendein EPG gefunden
		int duration1Width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth (runningRest);
		int duration1TextPos = BoxEndX - duration1Width - LEFT_OFFSET;

		int duration2Width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth (nextDuration);
		int duration2TextPos = BoxEndX - duration2Width - LEFT_OFFSET;

		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (!(info_CurrentNext.flags & CSectionsdClient::epgflags::has_current))) {
			// there are later events available - yet no current
			frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, BoxEndX - xStart, g_Locale->getText (LOCALE_INFOVIEWER_NOCURRENT), COL_INFOBAR, 0, true);	// UTF-8

			ChanInfoY += height;

			//info next
			//frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);

			if (last_next_id != info_CurrentNext.next_uniqueKey) {
				frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (ChanInfoX + 10, ChanInfoY + height, 100, nextStart, COL_INFOBAR);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration2TextPos - xStart - 5, info_CurrentNext.next_name, COL_INFOBAR, 0, true);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration2TextPos, ChanInfoY + height, duration2Width, nextDuration, COL_INFOBAR);
				last_next_id = info_CurrentNext.next_uniqueKey;
			}
		} else {
			if (last_curr_id != info_CurrentNext.current_uniqueKey) {
				frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (ChanInfoX + 10, ChanInfoY + height, 100, runningStart, COL_INFOBAR);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration1TextPos - xStart - 5, info_CurrentNext.current_name, COL_INFOBAR, 0, true);

				last_curr_id = info_CurrentNext.current_uniqueKey;
			}
			frameBuffer->paintBox (BoxEndX - 80, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);//FIXME duration1TextPos not really good
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration1TextPos, ChanInfoY + height, duration1Width, runningRest, COL_INFOBAR);

			ChanInfoY += height;

			//info next
			//frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);

			if ((!is_nvod) && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next)) {
				if (last_next_id != info_CurrentNext.next_uniqueKey) {
					frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (ChanInfoX + 10, ChanInfoY + height, 100, nextStart, COL_INFOBAR);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration2TextPos - xStart - 5, info_CurrentNext.next_name, COL_INFOBAR, 0, true);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration2TextPos, ChanInfoY + height, duration2Width, nextDuration, COL_INFOBAR);
					last_next_id = info_CurrentNext.next_uniqueKey;
				}
			} //else
			//frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);//why this...
		}
	}
}
#endif
}

void CInfoViewer::showInfoFile()
{
	/*if (recordModeActive)
		return;*/
	char infotext[80];
	int fd, xStart, xOffset, yStart, width, height, tWidth, tIndent;
	ssize_t cnt;

	fd = open("/tmp/infobar.txt", O_RDONLY); //read textcontent from this file

	if (fd < 0)
		return;

	cnt = read(fd, infotext, 79);
	close(fd);
	if (cnt < 1) { //EOF == 0
		fprintf(stderr, "CInfoViewer::showInfoFile: could not read from infobar.txt: %m");
		return;
	}
	infotext[cnt-1] = '\0';

	xStart	= BoxStartX + ChanWidth + 140;	//140px space for the little rec/ts-bar
	xOffset	= 5;				//same value as the used RADIUS_SMALL
	yStart	= BoxStartY;
	width	= BoxEndX - xStart - 225;	//225px space for the progress-bar
	height	= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight() + 2;
	tWidth	= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(infotext);
	if (tWidth < (width - (xOffset * 2)) )
		tIndent	= (width - (xOffset * 2) - tWidth) / 2;
	else
		tIndent	= 0;
	//shadow
	frameBuffer->paintBoxRel(xStart + SHADOW_OFFSET, yStart + SHADOW_OFFSET, width, height, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_SMALL, CORNER_ALL);
	//background
	frameBuffer->paintBoxRel(xStart, yStart, width, height, COL_INFOBAR_PLUS_0, RADIUS_SMALL, CORNER_ALL);
	//content
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(
		xStart + xOffset + tIndent, yStart + height, width - xOffset, (std::string)infotext, COL_INFOBAR, height, false);
}


void CInfoViewer::showButton_Audio ()
{
	// green, in case of several APIDs
	uint32_t count = g_RemoteControl->current_PIDs.APIDs.size ();
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN,
			       ChanInfoX + 10 + icol_w + 4 + asize + 2,
			       BBarY, InfoHeightY_Info);

	std::string txt = g_settings.usermenu_text[SNeutrinoSettings::BUTTON_GREEN];
	if (count > 0) {
		int selected = g_RemoteControl->current_PIDs.PIDs.selected_apid;
		/*int sx = ChanInfoX + 10 + NEUTRINO_ICON_BUTTON_RED_WIDTH + 4 + asize + 2 + NEUTRINO_ICON_BUTTON_GREEN_WIDTH + 4;*/
		int sx = ChanInfoX + 10 + (icol_w + 4)*2 + asize + 2;

		frameBuffer->paintBoxRel(sx, BBarY, asize, InfoHeightY_Info, COL_INFOBAR_BUTTONS_BACKGROUND);

		if (txt.empty() || (txt == g_Locale->getText(LOCALE_AUDIOSELECTMENUE_HEAD)))
			txt = g_RemoteControl->current_PIDs.APIDs[selected].desc;

		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(sx, BBarFontY, asize,
				txt, COL_INFOBAR_BUTTONS, 0, true); // UTF-8
	}
	const char *dd_icon;
	if ((g_RemoteControl->current_PIDs.PIDs.selected_apid < count) && (g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].is_ac3))
		dd_icon = NEUTRINO_ICON_DD;
	else if (g_RemoteControl->has_ac3)
		dd_icon = NEUTRINO_ICON_DD_AVAIL;
	else
		dd_icon = NEUTRINO_ICON_DD_GREY;

	frameBuffer->paintIcon(dd_icon, BoxEndX - (icon_large_width + 2*icon_small_width + 3*2),
			       BBarY, InfoHeightY_Info, 1, true, true, COL_INFOBAR_BUTTONS_BACKGROUND);
}

void CInfoViewer::killTitle()
{
	if (is_visible)
	{
		is_visible = false;
		int bottom = BoxEndY + SHADOW_OFFSET + bottom_bar_offset;
		if (showButtonBar)
			bottom += InfoHeightY_Info;
		//printf("killTitle(%d, %d, %d, %d)\n", BoxStartX, BoxStartY, BoxEndX+ SHADOW_OFFSET-BoxStartX, bottom-BoxStartY);
		frameBuffer->paintBackgroundBox(BoxStartX, BoxStartY, BoxEndX+ SHADOW_OFFSET, bottom);
		frameBuffer->blit();
		if (g_settings.radiotext_enable && g_Radiotext) {
			g_Radiotext->S_RtOsd = g_Radiotext->haveRadiotext() ? 1 : 0;
			killRadiotext();
		}
	}
	showButtonBar = false;
}

void CInfoViewer::Set_CA_Status (int /*Status*/)
{
	if (is_visible && showButtonBar)
		showIcon_CA_Status (1);
}

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
	int chan_w = BoxEndX- (start_x+ 20)- time_width- 15;

	bool logo_available = g_PicViewer->GetLogoName(logo_channel_id, ChannelName, strAbsIconPath, &logo_w, &logo_h);

	fprintf(stderr, "%s: logo_available: %d file: %s\n", __FUNCTION__, logo_available, strAbsIconPath.c_str());

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
		y_mid = BoxStartY + (satNameHeight + ChanHeight) / 2;

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
		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, chan_w, time_height);
		// hide channel name
// this is too ugly...		ChannelName = "";
		// calculate logo position
		y_mid = ChanNameY + time_height / 2;
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
		g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, Logo_max_width, time_height);
		// calculate logo position
		y_mid = ChanNameY + time_height / 2;
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
	/* TODO: g_settings.infobar_channellogo_background*/
#if 0
	// paint logo background (shaded/framed)
	if ((g_settings.infobar_channellogo_background !=0) && (res !=0)) // with background
	{
		int frame_w = 2, logo_bg_x=0, logo_bg_y=0, logo_bg_w=0, logo_bg_h=0;

		if (g_settings.infobar_channellogo_background == 1) // framed
		{
			//sh_offset = 2;
			logo_bg_x = logo_x-frame_w;
			logo_bg_y = logo_y-frame_w;
			logo_bg_w = logo_w+frame_w*2;
			logo_bg_h = logo_h+frame_w*2;
		}
		else if (g_settings.infobar_channellogo_background == 2) // shaded
		{
			//sh_offset = 3;
			logo_bg_x = logo_x+SHADOW_OFFSET;
			logo_bg_y = logo_y+SHADOW_OFFSET;
			logo_bg_w = logo_w;
			logo_bg_h = logo_h;
		}
		frameBuffer->paintBoxRel(logo_bg_x, logo_bg_y, logo_bg_w, logo_bg_h, COL_INFOBAR_BUTTONS_BACKGROUND);
	}
#endif
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
		if (fileplay || NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode()) {
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
	}
}
#else
void CInfoViewer::showLcdPercentOver ()
{
	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] != 1) {
		if (fileplay || (NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode())) {
			CVFD::getInstance ()->showPercentOver (CMoviePlayerGui::getInstance().file_prozent);
			return;
		}
		int runningPercent = -1;
		time_t jetzt = time (NULL);
#if 0
		if (!(info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) || jetzt > (int) (info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)) {
			info_CurrentNext = getEPG (channel_id);
		}
#endif
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
#if 0
		/* let's check if this is still needed */
		else
			/* don't show anything, but update the LCD
			   TODO: we should not have to update the LCD from the _infoviewer_.
				 they have nothing to do with each other */
			showLcdPercentOver();
#endif
	}
}


int CInfoViewerHandler::exec (CMenuTarget * parent, const std::string & /*actionkey*/)
{
	int res = menu_return::RETURN_EXIT_ALL;
	CChannelList *channelList;
	CInfoViewer *i;

	if (parent) {
		parent->hide ();
	}

	i = new CInfoViewer;

	channelList = CNeutrinoApp::getInstance ()->channelList;
	i->start ();
	i->showTitle (channelList->getActiveChannelNumber (), channelList->getActiveChannelName (), channelList->getActiveSatellitePosition (), channelList->getActiveChannel_ChannelID ());	// UTF-8
	delete i;

	return res;
}

void CInfoViewer::paint_ca_icons(int caid, char * icon, int &icon_space_offset)
{
	char buf[20];
	int endx = BoxEndX -3;
	int py = BoxEndY + 2; /* hand-crafted, should be automatic */
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

		for (it=icon_map.begin(); it!=icon_map.end(); it++) {
			snprintf(buf, sizeof(buf), "%s_%s", (*it).second.second, icon);
			frameBuffer->getIconSize(buf, &icon_sizeW[(*it).second.first], &icon_sizeH);
		}

		for (int j = 0; j < icon_number; j++) {
			for (int i = j; i < icon_number; i++) {
				icon_offset[j] += icon_sizeW[i] + icon_space;
			}
		}
	}
	if (( caid & 0xFF00 ) == 0x1700)
		caid = 0x0600;

	if (g_settings.casystem_display == 0) {
		px = endx - (icon_offset[icon_map[( caid & 0xFF00 )].first] - icon_space );
	} else {
		icon_space_offset += icon_sizeW[icon_map[( caid & 0xFF00 )].first];
		px = endx - icon_space_offset;
		icon_space_offset += 4;
	}

	if (px) {
		snprintf(buf, sizeof(buf), "%s_%s", icon_map[( caid & 0xFF00 )].second, icon);
		frameBuffer->paintIcon(buf, px, py );
	}
}

void CInfoViewer::showOne_CAIcon(bool fta)
{
	frameBuffer->paintIcon(fta ? NEUTRINO_ICON_SCRAMBLED2_GREY : NEUTRINO_ICON_SCRAMBLED2, BoxEndX - (icon_xres_width + icon_crypt_width + 2*icon_large_width + 2*icon_small_width + 6*2), BBarY,
			       InfoHeightY_Info, 1, true, true, COL_INFOBAR_BUTTONS_BACKGROUND);
}

void CInfoViewer::showIcon_CA_Status (int notfirst)
{
	extern int pmt_caids[4][11];
	int caids[] = { 0x600, 0x1700, 0x0100, 0x0500, 0x1800, 0xB00, 0xD00, 0x900, 0x2600, 0x4a00, 0x0E00 };
	int i = 0;
	if (g_settings.casystem_display == 2) {
		bool fta = true;
		for (i=0; i < (int)(sizeof(caids)/sizeof(int)); i++) {
			if (pmt_caids[0][i]) {
				fta = false;
				break;
			}
		}
		showOne_CAIcon(fta);
		return;
	}
	else if (g_settings.casystem_display == 3) {
		return;
	}

	const char * white = (char *) "white";
	const char * yellow = (char *) "yellow";
	static int icon_space_offset = 0;
	bool paintIconFlag = false;

	if (pmt_caids[0][0] != 0 && pmt_caids[0][1] != 0)
		pmt_caids[0][1] = 0;

	if (!notfirst) {
		if ((g_settings.casystem_display == 1) && (icon_space_offset)) {
			paintCA_bar(0,icon_space_offset);
			icon_space_offset = 0;
		}
		for (i=0; i < (int)(sizeof(caids)/sizeof(int)); i++) {
			if (!(i == 1 && pmt_caids[0][0] != 0 && pmt_caids[0][1] == 0 )) {
				if ((g_settings.casystem_display == 1 )  && pmt_caids[0][i]) {
					paintIconFlag = true;
				} else if (g_settings.casystem_display == 0 )
					paintIconFlag = true;
			}
			if (paintIconFlag) {
				paint_ca_icons(caids[i], (char *) (pmt_caids[0][i] ? yellow : white),icon_space_offset);
				paintIconFlag = false;
			}
		}
	}
}
