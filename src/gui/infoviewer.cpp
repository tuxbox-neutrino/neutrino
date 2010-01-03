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
#include <string>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>

#include <fcntl.h>

#include <gui/infoviewer.h>

#include <gui/widget/icons.h>
#include <gui/widget/hintbox.h>

#include <daemonc/remotecontrol.h>

#include <global.h>
#include <neutrino.h>
#include <gui/customcolor.h>
#include <gui/pictureviewer.h>


#include <sys/timeb.h>
#include <time.h>
#include <sys/param.h>
#include <zapit/satconfig.h>
#include <zapit/frontend_c.h>
#include <video_cs.h>

void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "");
void sectionsd_getCurrentNextServiceKey(t_channel_id uniqueServiceKey, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next );

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */
extern CPictureViewer * g_PicViewer;
extern CFrontend * frontend;
extern cVideo * videoDecoder;
extern t_channel_id live_channel_id; //zapit

#define COL_INFOBAR_BUTTONS            (COL_INFOBAR_SHADOW + 1)
#define COL_INFOBAR_BUTTONS_BACKGROUND (COL_INFOBAR_SHADOW_PLUS_1)

#define ICON_LARGE_WIDTH 26
#define ICON_SMALL_WIDTH 16
#define ICON_LARGE 30
#define ICON_SMALL 18
#define ICON_Y_1 18

#define ICON_OFFSET (2 + ICON_LARGE_WIDTH + 2 + ICON_LARGE_WIDTH + 2 + ICON_SMALL_WIDTH + 2)

#ifdef NO_BLINKENLIGHTS
#define BOTTOM_BAR_OFFSET 0
#else
/* BOTTOM_BAR_OFFSET is used for the blinkenlights iconbar */
#define BOTTOM_BAR_OFFSET 22
#endif

#define borderwidth 4
#define LEFT_OFFSET 5
#define ASIZE 100

// in us
#define FADE_TIME 40000

#define LCD_UPDATE_TIME_TV_MODE (60 * 1000 * 1000)

#define ROUND_RADIUS 7

int time_left_width;
int time_dot_width;
int time_width;
int time_height;
bool newfreq = true;
char old_timestr[10];
static event_id_t last_curr_id = 0, last_next_id = 0;

extern CZapitClient::SatelliteList satList;
static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
        return a.startTime < b.startTime;
}

extern int timeshift;
extern unsigned char file_prozent;
extern std::string g_file_epg;
extern std::string g_file_epg1;
extern bool autoshift;
extern uint32_t shift_timer;

#define RED_BAR 40
#define YELLOW_BAR 70
#define GREEN_BAR 100
#define BAR_BORDER 1
#define BAR_WIDTH 72 //(68+BAR_BORDER*2)
#define BAR_HEIGHT 12 //(13 + BAR_BORDER*2)
#define TIME_BAR_HEIGHT 12
// InfoViewer: H 63 W 27
#define NUMBER_H 63
#define NUMBER_W 27
extern char act_emu_str[20];
extern std::string ext_channel_name;
int m_CA_Status;
extern bool timeset;

CInfoViewer::CInfoViewer ()
{
	sigscale = NULL;
	snrscale = NULL;
	hddscale = NULL;
	varscale = NULL;
	timescale = NULL;
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
  CA_Status = false;
  virtual_zap_mode = false;
  chanready = 1;
  fileplay = 0;

	/* after font size changes, Init() might be called multiple times */
	if (sigscale != NULL)
		delete sigscale;
	sigscale = new CProgressBar(BAR_WIDTH, 10, PB_COLORED, RED_BAR, GREEN_BAR, YELLOW_BAR);
	if (snrscale != NULL)
		delete snrscale;
	snrscale = new CProgressBar(BAR_WIDTH, 10, PB_COLORED, RED_BAR, GREEN_BAR, YELLOW_BAR);
	if (hddscale != NULL)
		delete hddscale;
	hddscale = new CProgressBar(100,        6, PB_COLORED, 50,      GREEN_BAR, 75, true);
	if (varscale != NULL)
		delete varscale;
	varscale = new CProgressBar(100,        6, PB_COLORED, 50,      GREEN_BAR, 75, true);
	if (timescale != NULL)
		delete timescale;
	timescale = new CProgressBar(PB_COLORED, 30, GREEN_BAR, 70, true);

  channel_id = live_channel_id;
  lcdUpdateTimer = 0;
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
                |                     optional blinkenlights iconbar |  BOTTOM_BAR_OFFSET
     BBarY------+----------------------------------------------------+--+
                | * red   * green  * yellow  * blue ====== [DD][16:9]|  InfoHeightY_Info
                +----------------------------------------------------+--+
                                                                     |
                                                             BoxEndX-/
*/
void CInfoViewer::start ()
{
	InfoHeightY = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight()*9/8 +
		2*g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight() +
		25;
	InfoHeightY_Info = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+ 5;

#if 0
	ChanWidth = 4* g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getRenderWidth(widest_number) + 10;
	ChanHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight()*9/8;
#else
	ChanWidth = 122;
	ChanHeight = 74;
#endif
	BoxStartX = g_settings.screen_StartX + 10;
	BoxEndX = g_settings.screen_EndX - 10;
	BoxEndY = g_settings.screen_EndY - 10 - InfoHeightY_Info - BOTTOM_BAR_OFFSET;
	BoxStartY = BoxEndY - InfoHeightY - ChanHeight / 2;

	BBarY = BoxEndY + BOTTOM_BAR_OFFSET;
	BBarFontY = BBarY + InfoHeightY_Info - (InfoHeightY_Info - g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()) / 2; /* center in buttonbar */

	ChanNameX = BoxStartX + ChanWidth + SHADOW_OFFSET;
	ChanNameY = BoxStartY + (ChanHeight / 2) + SHADOW_OFFSET;	//oberkante schatten?
	ChanInfoX = BoxStartX + (ChanWidth / 3);

	time_height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight()+5;
	time_left_width = 2* g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(widest_number);
	time_dot_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(":");
	time_width = time_left_width* 2+ time_dot_width;

	if (lcdUpdateTimer == 0)
		lcdUpdateTimer = g_RCInput->addTimer (LCD_UPDATE_TIME_TV_MODE, false, true);
}

void CInfoViewer::paintTime (bool show_dot, bool firstPaint)
{
	if (! gotTime)
		return;

//	int ChanNameY = BoxStartY + (ChanHeight >> 1) + 5;	//oberkante schatten?

	char timestr[10];
	struct timeb tm;

	ftime (&tm);
	strftime ((char *) &timestr, 20, "%H:%M", localtime (&tm.time));

	if ((!firstPaint) && (strcmp (timestr, old_timestr) == 0)) {
	  if (show_dot)
		frameBuffer->paintBoxRel (BoxEndX - time_width + time_left_width - LEFT_OFFSET, ChanNameY, time_dot_width, time_height / 2 + 2, COL_INFOBAR_PLUS_0);
	  else
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_width + time_left_width - LEFT_OFFSET, ChanNameY + time_height, time_dot_width, ":", COL_INFOBAR);
	  strcpy (old_timestr, timestr);
	} else {
	  strcpy (old_timestr, timestr);

	  if (!firstPaint) {
		frameBuffer->paintBoxRel(BoxEndX - time_width - LEFT_OFFSET, ChanNameY, time_width + LEFT_OFFSET, time_height, COL_INFOBAR_PLUS_0, ROUND_RADIUS, CORNER_TOP);
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
  recordModeActive = CNeutrinoApp::getInstance ()->recordingstatus || shift_timer;
  if (recordModeActive) {
	ChanNameX = BoxStartX + ChanWidth + 20;
	if (show) {
	  frameBuffer->paintIcon (autoshift ? "ats.raw" : NEUTRINO_ICON_BUTTON_RED, ChanNameX, BoxStartY + 12);
	  if(!autoshift && !shift_timer) {
	  	int chanH = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight ();
	  	frameBuffer->paintBoxRel (ChanNameX + 28 + SHADOW_OFFSET, BoxStartY + 12 + SHADOW_OFFSET, 300, 20, COL_INFOBAR_SHADOW_PLUS_0);
	  	frameBuffer->paintBoxRel (ChanNameX + 28, BoxStartY + 12, 300, 20, COL_INFOBAR_PLUS_0);
	  	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (ChanNameX + 30, BoxStartY + 12 + chanH, 300, ext_channel_name.c_str (), COL_INFOBAR, 0, true);
	  } else
	  	frameBuffer->paintBackgroundBoxRel (ChanNameX + 28, BoxStartY + 12, 300 + SHADOW_OFFSET, 20 + SHADOW_OFFSET);
	} else {
	  frameBuffer->paintBackgroundBoxRel (ChanNameX, BoxStartY + 10, 20, 20);
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
		BoxEndInfoY += InfoHeightY_Info + BOTTOM_BAR_OFFSET;
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

void CInfoViewer::showTitle (const int ChanNum, const std::string & Channel, const t_satellite_position satellitePosition, const t_channel_id new_channel_id, const bool calledFromNumZap, int epgpos)
{
	last_curr_id = last_next_id = 0;
	std::string ChannelName = Channel;
	bool show_dot = true;
	bool fadeOut = false;
	int fadeValue;
	bool new_chan = false;
//printf("CInfoViewer::showTitle ************************* chan num %d name %s\n", ChanNum, Channel.c_str());
	//aspectRatio = videoDecoder->getAspectRatio() > 2 ? 1 : 0;
	aspectRatio = 0;

	showButtonBar = !calledFromNumZap;
	bool fadeIn = g_settings.widget_fade && (!is_visible) && showButtonBar;

	is_visible = true;
	if (!calledFromNumZap && fadeIn)
		fadeTimer = g_RCInput->addTimer (FADE_TIME, false);

	fileplay = (ChanNum == 0);
	newfreq = true;

	sigscale->reset(); snrscale->reset(); timescale->reset(); hddscale->reset(); varscale->reset();
	lastsig = lastsnr = lasthdd = lastvar = -1;
#if 0
	InfoHeightY = NUMBER_H * 9 / 8 + 2 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight () + 25;
	InfoHeightY_Info = 40;

	time_height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight () + 5;
	time_left_width = 2 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (widest_number);
	time_dot_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth (":");
	time_width = time_left_width * 2 + time_dot_width;

	BoxStartX = g_settings.screen_StartX + 10;
	BoxEndX = g_settings.screen_EndX - 10;
	BoxEndY = g_settings.screen_EndY - 10;

	int BoxEndInfoY = showButtonBar ? (BoxEndY - InfoHeightY_Info) : (BoxEndY);
	BoxStartY = BoxEndInfoY - InfoHeightY;
#endif

	if (!gotTime)
		gotTime = timeset;

	if (fadeIn) {
		fadeValue = 0x10;
		frameBuffer->setBlendLevel(fadeValue, fadeValue);
	} else
		fadeValue = g_settings.gtx_alpha1;

#if 0
	// kill linke seite
	frameBuffer->paintBackgroundBox (BoxStartX, BoxStartY + ChanHeight, BoxStartX + (ChanWidth / 3), BoxStartY + ChanHeight + InfoHeightY_Info + 10);
	// kill progressbar
	frameBuffer->paintBackgroundBox (BoxEndX - 120, BoxStartY, BoxEndX, BoxStartY + ChanHeight);
#endif

	int col_NumBoxText;
	int col_NumBox;
	if (virtual_zap_mode) {
		col_NumBoxText = COL_MENUHEAD;
		col_NumBox = COL_MENUHEAD_PLUS_0;
		if ((channel_id != new_channel_id) || (evtlist.empty())) {
			evtlist.clear();
			//evtlist = g_Sectionsd->getEventsServiceKey(new_channel_id & 0xFFFFFFFFFFFFULL);
			sectionsd_getEventsServiceKey(new_channel_id & 0xFFFFFFFFFFFFULL, evtlist);
			if (!evtlist.empty())
				sort(evtlist.begin(),evtlist.end(), sortByDateTime);
			new_chan = true;
		}
	} else {
		col_NumBoxText = COL_INFOBAR;
		col_NumBox = COL_INFOBAR_PLUS_0;
	}
	if (! calledFromNumZap && !(g_RemoteControl->subChannels.empty()) && (g_RemoteControl->selected_subchannel > 0))
	{
		channel_id = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].getChannelID();
		ChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
	} else {
		channel_id = new_channel_id;
	}

	asize = (BoxEndX - (2*ICON_LARGE_WIDTH + 2*ICON_SMALL_WIDTH + 4*2) - 102) - ChanInfoX;
	asize = asize - (NEUTRINO_ICON_BUTTON_RED_WIDTH+6)*4;
	asize = asize / 4;

#if 0
	//Shadow
	frameBuffer->paintBox(BoxEndX-20, ChanNameY + SHADOW_OFFSET, BoxEndX + SHADOW_OFFSET, BoxEndY, COL_INFOBAR_SHADOW_PLUS_0, ROUND_RADIUS, CORNER_TOP);
	frameBuffer->paintBox(ChanInfoX + SHADOW_OFFSET, BoxEndY -20, BoxEndX + SHADOW_OFFSET, BoxEndY + SHADOW_OFFSET, COL_INFOBAR_SHADOW_PLUS_0, ROUND_RADIUS, CORNER_BOTTOM); //round

	//infobox
	frameBuffer->paintBoxRel(ChanNameX-10, ChanNameY, BoxEndX-ChanNameX+10, BoxEndInfoY-ChanNameY, COL_INFOBAR_PLUS_0, ROUND_RADIUS, CORNER_TOP); // round

	//number box
	frameBuffer->paintBoxRel (BoxStartX + SHADOW_OFFSET, BoxStartY + SHADOW_OFFSET, ChanWidth, ChanHeight + 4, COL_INFOBAR_SHADOW_PLUS_0, ROUND_RADIUS); // round
	frameBuffer->paintBoxRel (BoxStartX, BoxStartY, ChanWidth, ChanHeight + 4, COL_INFOBAR_PLUS_0, ROUND_RADIUS); // round
#endif
	paintBackground(col_NumBox);

	int ChanNumYPos = BoxStartY + ChanHeight;
	if (g_settings.infobar_sat_display && satellitePosition != 0 && satellitePositions.size()) {
		sat_iterator_t sit = satellitePositions.find(satellitePosition);

		if(sit != satellitePositions.end()) {
			int satNameWidth = g_SignalFont->getRenderWidth (sit->second.name);
			if (satNameWidth > (ChanWidth - 4))
				satNameWidth = ChanWidth - 4;

			int chanH = g_SignalFont->getHeight ();
			g_SignalFont->RenderString (3 + BoxStartX + ((ChanWidth - satNameWidth) / 2), BoxStartY + chanH, satNameWidth, sit->second.name, COL_INFOBAR);
		}
		ChanNumYPos += 5;
	}
	paintTime (show_dot, true);
	showRecordIcon (show_dot);
	show_dot = !show_dot;

	char strChanNum[10];
	sprintf (strChanNum, "%d", ChanNum);

	int ChanNumWidth = 0;
	bool logo_ok = false;
	if(showButtonBar) {
#define PIC_W 52
#define PIC_H 39
#define PIC_X (ChanNameX + 10)
#define PIC_Y (ChanNameY + time_height - PIC_H)
		logo_ok = g_PicViewer->DisplayLogo(channel_id, PIC_X, PIC_Y, PIC_W, PIC_H);
		ChanNumWidth = PIC_W + 10;
	}
	if (ChanNum && !logo_ok) {
		int ChanNumHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getHeight ();
		ChanNumWidth = 5 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->getRenderWidth (strChanNum);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->RenderString (ChanNameX + 5, ChanNameY + ChanNumHeight, ChanNumWidth, strChanNum, col_NumBoxText);
	}
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (ChanNameX + 10 + ChanNumWidth, ChanNameY + time_height, BoxEndX - (ChanNameX + 20) - time_width - LEFT_OFFSET - 5 - ChanNumWidth, ChannelName, COL_INFOBAR, 0, true);	// UTF-8

//	int ChanInfoY = BoxStartY + ChanHeight + 10;
	ButtonWidth = (BoxEndX - ChanInfoX - ICON_OFFSET) >> 2;

//	frameBuffer->paintBox (ChanInfoX, ChanInfoY, ChanNameX, BoxEndInfoY, COL_INFOBAR_PLUS_0);

	if (showButtonBar) {
		sec_timer_id = g_RCInput->addTimer (1*1000*1000, false);

		if (BOTTOM_BAR_OFFSET > 0)
		{ // FIXME
			frameBuffer->paintBox(ChanInfoX, BoxEndY, BoxEndX, BoxEndY + BOTTOM_BAR_OFFSET, COL_BLACK);
			int xcnt = (BoxEndX - ChanInfoX) / 4;
			int ycnt = (BOTTOM_BAR_OFFSET) / 4;
			for(int i = 0; i < xcnt; i++) {
				for(int j = 0; j < ycnt; j++)
					/* BoxEndY + 2 is the magic number that also appears in paint_ca_icons */
					frameBuffer->paintBoxRel(ChanInfoX + i*4, BoxEndY + 2 + j*4, 2, 2, COL_INFOBAR_PLUS_1);
			}
		}
		frameBuffer->paintBoxRel(ChanInfoX, BBarY, BoxEndX - ChanInfoX, InfoHeightY_Info, COL_INFOBAR_BUTTONS_BACKGROUND, ROUND_RADIUS, CORNER_BOTTOM); //round

		showSNR();
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_BLUE, ChanInfoX + 16*3 + asize * 3 + 2*7,
				       BBarY, InfoHeightY_Info);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(ChanInfoX + 16*4 + asize * 3 + 2*8, BBarFontY, ButtonWidth - (2 + NEUTRINO_ICON_BUTTON_BLUE_WIDTH + 2 + 2), g_Locale->getText(LOCALE_INFOVIEWER_STREAMINFO), COL_INFOBAR_BUTTONS, 0, true); // UTF-8

		showButton_Audio ();
		showButton_SubServices ();
		showIcon_CA_Status(0);
		showIcon_16_9 ();
		showIcon_VTXT ();
		showIcon_SubT();
	}

	if (fileplay) {
		show_Data ();
	} else {
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
	showLcdPercentOver ();

	if ((g_RemoteControl->current_channel_id == channel_id) && !(((info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (info_CurrentNext.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_no_current))) || (info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast))) {
		g_Sectionsd->setServiceChanged (channel_id & 0xFFFFFFFFFFFFULL, true);
	}

	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	CNeutrinoApp *neutrino = CNeutrinoApp::getInstance ();
	if (!calledFromNumZap) {

		bool hideIt = true;
		virtual_zap_mode = false;
		unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR]);

		int res = messages_return::none;
//		time_t ta, tb;

		while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) {
			g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);
#if 0
			if (!(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_current))) {
				if (difftime (time (&tb), ta) > 1.1) {
					time (&ta);
					info_CurrentNext = getEPG (channel_id);
					if ((info_CurrentNext.flags & (CSectionsdClient::epgflags::has_current))) {
						show_Data ();
						showLcdPercentOver ();
					}
				}
			}
#endif
			if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
				g_RCInput->postMsg (msg, 0);
				res = messages_return::cancel_info;
			}
			else if (msg == CRCInput::RC_help || msg == CRCInput::RC_info) {
				g_RCInput->postMsg (NeutrinoMessages::SHOW_EPG, 0);
				res = messages_return::cancel_info;
			} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fadeTimer)) {
				if (fadeOut) { // disappear
					fadeValue -= 0x10;
					if (fadeValue <= 0x10) {
						fadeValue = g_settings.gtx_alpha1;
						g_RCInput->killTimer (fadeTimer);
						res = messages_return::cancel_info;
					} else
						frameBuffer->setBlendLevel(fadeValue, fadeValue);
				} else { // appears
					fadeValue += 0x10;
					if (fadeValue >= g_settings.gtx_alpha1) {
						fadeValue = g_settings.gtx_alpha1;
						g_RCInput->killTimer (fadeTimer);
						fadeIn = false;
						frameBuffer->setBlendLevel(g_settings.gtx_alpha1, g_settings.gtx_alpha2);
					} else
						frameBuffer->setBlendLevel(fadeValue, fadeValue);
				}
			} else if ((msg == CRCInput::RC_ok) || (msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) {
				if (fadeIn) {
					g_RCInput->killTimer (fadeTimer);
					fadeIn = false;
				}
				if ((!fadeOut) && g_settings.widget_fade) {
					fadeOut = true;
					fadeTimer = g_RCInput->addTimer (FADE_TIME, false);
					timeoutEnd = CRCInput::calcTimeoutEnd (1);
				} else {
#if 0
					if ((msg != CRCInput::RC_timeout) && (msg != CRCInput::RC_ok))
						if (!fileplay && !timeshift)
							g_RCInput->postMsg (msg, data);
#endif
					res = messages_return::cancel_info;
				}
			} else if ((msg == NeutrinoMessages::EVT_TIMER) && (data == sec_timer_id)) {
				showSNR ();
				paintTime (show_dot, false);
				showRecordIcon (show_dot);
				show_dot = !show_dot;

				showIcon_16_9();
			} else if ( g_settings.virtual_zap_mode && ((msg == CRCInput::RC_right) || msg == CRCInput::RC_left )) {
				virtual_zap_mode = true;
				res = messages_return::cancel_all;
				hideIt = true;
			} else if (!fileplay && !timeshift) {
				if ((msg == (neutrino_msg_t) g_settings.key_quickzap_up) || (msg == (neutrino_msg_t) g_settings.key_quickzap_down) || (msg == CRCInput::RC_0) || (msg == NeutrinoMessages::SHOW_INFOBAR)) {
					hideIt = false;
					//hideIt = (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0) ? true : false;
					g_RCInput->postMsg (msg, data);
					res = messages_return::cancel_info;
				} else if (msg == NeutrinoMessages::EVT_TIMESET) {
					// Handle anyway!
					neutrino->handleMsg (msg, data);
					g_RCInput->postMsg (NeutrinoMessages::SHOW_INFOBAR, 0);
					hideIt = false;
					res = messages_return::cancel_all;
				} else {
					if (msg == CRCInput::RC_standby) {
						g_RCInput->killTimer (sec_timer_id);
						if (fadeIn || fadeOut)
							g_RCInput->killTimer (fadeTimer);
					}
					res = neutrino->handleMsg (msg, data);
					if (res & messages_return::unhandled) {
						// raus hier und im Hauptfenster behandeln...
						g_RCInput->postMsg (msg, data);
						res = messages_return::cancel_info;
					}
				}
			}
		}

		if (hideIt)
			killTitle ();

		g_RCInput->killTimer (sec_timer_id);
		sec_timer_id = 0;
		if (fadeIn || fadeOut) {
			g_RCInput->killTimer (fadeTimer);
			frameBuffer->setBlendLevel(g_settings.gtx_alpha1, g_settings.gtx_alpha2);
		}
		if (virtual_zap_mode)
			CNeutrinoApp::getInstance()->channelList->virtual_zap_mode(msg == CRCInput::RC_right);

	}
	aspectRatio = 0;
	fileplay = 0;
}

void CInfoViewer::showSubchan ()
{
  CFrameBuffer *lframeBuffer = CFrameBuffer::getInstance ();
  CNeutrinoApp *neutrino = CNeutrinoApp::getInstance ();

  std::string subChannelName;	// holds the name of the subchannel/audio channel
  int subchannel = 0;				// holds the channel index

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
	char text[100];
	sprintf (text, "%d - %s", subchannel, subChannelName.c_str ());

	int dx = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth (text) + 20;
	int dy = 25;

	if (g_RemoteControl->director_mode) {
	  int w = 20 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth (g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE), true) + 20;	// UTF-8
	  if (w > dx)
		dx = w;
	  dy = dy * 2;
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
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + 30, dx - 20, text, COL_MENUCONTENT);

	if (g_RemoteControl->director_mode) {
	  lframeBuffer->paintIcon (NEUTRINO_ICON_BUTTON_YELLOW, x + 8, y + dy - 20);
	  g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (x + 30, y + dy - 2, dx - 40, g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE), COL_MENUCONTENT, 0, true);	// UTF-8
	}

	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd (2);
	int res = messages_return::none;

	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) {
	  g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);

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
  } else {
	g_RCInput->postMsg (NeutrinoMessages::SHOW_INFOBAR, 0);
  }
}

void CInfoViewer::showIcon_16_9 ()
{
	if((aspectRatio == 0) || (aspectRatio != videoDecoder->getAspectRatio())) {
		aspectRatio = videoDecoder->getAspectRatio();
		frameBuffer->paintIcon((aspectRatio > 2) ? "16_9.raw" : "16_9_gray.raw",
				       BoxEndX - (2*ICON_LARGE_WIDTH + 2*ICON_SMALL_WIDTH + 4*2), BBarY,
				       InfoHeightY_Info);
	}
}

extern "C" void tuxtxt_start(int tpid);
extern "C" int  tuxtxt_stop();

void CInfoViewer::showIcon_VTXT () const
{
	frameBuffer->paintIcon((g_RemoteControl->current_PIDs.PIDs.vtxtpid != 0) ? "vtxt.raw" : "vtxt_gray.raw",
			       BoxEndX - (2*ICON_SMALL_WIDTH + 2*2), BBarY, InfoHeightY_Info);
}

void CInfoViewer::showIcon_SubT() const
{
        bool have_sub = false;
	CZapitChannel * cc = CNeutrinoApp::getInstance()->channelList->getChannel(CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber());
	if(cc && cc->getSubtitleCount())
		have_sub = true;

        frameBuffer->paintIcon(have_sub ? "subt.raw" : "subt_gray.raw", BoxEndX - (ICON_SMALL_WIDTH + 2),
			       BBarY, InfoHeightY_Info);
}

void CInfoViewer::showFailure ()
{
  ShowHintUTF (LOCALE_MESSAGEBOX_ERROR, g_Locale->getText (LOCALE_INFOVIEWER_NOTAVAILABLE), 430);	// UTF-8
}

void CInfoViewer::showMotorMoving (int duration)
{
  char text[256];
  char buffer[10];

  sprintf (buffer, "%d", duration);
  strcpy (text, g_Locale->getText (LOCALE_INFOVIEWER_MOTOR_MOVING));
  strcat (text, " (");
  strcat (text, buffer);
  strcat (text, " s)");

  ShowHintUTF (LOCALE_MESSAGEBOX_INFO, text, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth (text, true) + 10, duration);	// UTF-8
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
  } else if (msg == NeutrinoMessages::EVT_TIMER) {
	if (data == fadeTimer) {
	  // hierher kann das event nur dann kommen, wenn ein anderes Fenster im Vordergrund ist!
	  g_RCInput->killTimer (fadeTimer);
	  frameBuffer->setBlendLevel(g_settings.gtx_alpha1, g_settings.gtx_alpha2);

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
	if(is_visible) showRecordIcon(true);
  } else if (msg == NeutrinoMessages::EVT_ZAP_GOTAPIDS) {
	if ((*(t_channel_id *) data) == channel_id) {
	  if (is_visible && showButtonBar)
		showButton_Audio ();
	}
	return messages_return::handled;
  } else if (msg == NeutrinoMessages::EVT_ZAP_GOTPIDS) {
	if ((*(t_channel_id *) data) == channel_id) {
	  if (is_visible && showButtonBar) {
		showIcon_VTXT ();
		showIcon_SubT();
		showIcon_CA_Status (0);
	  }
	}
	return messages_return::handled;
  } else if (msg == NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES) {
	if ((*(t_channel_id *) data) == channel_id) {
	  if (is_visible && showButtonBar)
		showButton_SubServices ();
	}
	return messages_return::handled;
  } else if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) ||
		  (msg == NeutrinoMessages::EVT_ZAP_ISNVOD))
  {
	  channel_id = (*(t_channel_id *)data);
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
  } else if (msg == NeutrinoMessages::EVT_ZAP_CA_ID) {
	chanready = 1;
	Set_CA_Status (data);
	showSNR ();
	return messages_return::handled;
  }

  return messages_return::unhandled;
}

void CInfoViewer::showButton_SubServices ()
{
  if (!(g_RemoteControl->subChannels.empty ())) {
        frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW,
		ChanInfoX + 2 + NEUTRINO_ICON_BUTTON_RED_WIDTH + 2 + asize + 2 + NEUTRINO_ICON_BUTTON_GREEN_WIDTH + 2 + asize + 2, BBarY, InfoHeightY_Info);
        g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(
		ChanInfoX + 2 + NEUTRINO_ICON_BUTTON_RED_WIDTH + 2 + asize + 2 + NEUTRINO_ICON_BUTTON_GREEN_WIDTH + 2 + asize + 2 + NEUTRINO_ICON_BUTTON_YELLOW_WIDTH + 2,
		BBarFontY, asize, g_Locale->getText((g_RemoteControl->are_subchannels) ? LOCALE_INFOVIEWER_SUBSERVICE : LOCALE_INFOVIEWER_SELECTTIME), COL_INFOBAR_BUTTONS, 0, true); // UTF-8
  }
}

CSectionsdClient::CurrentNextInfo CInfoViewer::getEPG (const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info)
{
	static CSectionsdClient::CurrentNextInfo oldinfo;

	//g_Sectionsd->getCurrentNextServiceKey (for_channel_id & 0xFFFFFFFFFFFFULL, info);
	sectionsd_getCurrentNextServiceKey(for_channel_id & 0xFFFFFFFFFFFFULL, info);

//printf("CInfoViewer::getEPG: old uniqueKey %llx new %llx\n", oldinfo.current_uniqueKey, info.current_uniqueKey);
	if (info.current_uniqueKey != oldinfo.current_uniqueKey || info.next_uniqueKey != oldinfo.next_uniqueKey) {
		if (info.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_next)) {
			CSectionsdClient::CurrentNextInfo * _info = new CSectionsdClient::CurrentNextInfo;
			*_info = info;
			neutrino_msg_t msg;
			if (info.flags & CSectionsdClient::epgflags::has_current)
				msg = NeutrinoMessages::EVT_CURRENTEPG;
			else
				msg = NeutrinoMessages::EVT_NEXTEPG;
			g_RCInput->postMsg(msg, (unsigned) _info, false );
		} else {
			t_channel_id *p = new t_channel_id;
			*p = for_channel_id;
			g_RCInput->postMsg (NeutrinoMessages::EVT_NOEPG_YET, (const neutrino_msg_data_t) p, false);	// data is pointer to allocated memory
		}
		oldinfo = info;
	}

	return info;
}

#define get_set CNeutrinoApp::getInstance()->getScanSettings()

void CInfoViewer::showSNR ()
{
  char percent[10];
  uint16_t ssig, ssnr;
  int sw, snr, sig, posx, posy;
  int height, ChanNumYPos;
  int barwidth = BAR_WIDTH;

	if (! is_visible)
		return;

  if (! fileplay) {
	if (newfreq && chanready) {
	  char freq[20];

	  newfreq = false;
	  CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo ();
	  sprintf (freq, "%d.%d MHz %c", si.tsfrequency / 1000, si.tsfrequency % 1000, si.polarisation ? 'V' : 'H');
#if 0
//FIXME this sets default params for scan menu
	  sprintf (get_set.TP_freq, "%d", si.tsfrequency);
	  sprintf (get_set.TP_rate, "%d", si.rate);
	  get_set.TP_pol = si.polarisation;
	  get_set.TP_fec = si.fec;
#endif

	  int chanH = g_SignalFont->getHeight ();
	  int satNameWidth = g_SignalFont->getRenderWidth (freq);
	  g_SignalFont->RenderString (3 + BoxStartX + ((ChanWidth - satNameWidth) / 2), BoxStartY + 2 * chanH - 3, satNameWidth, freq, COL_INFOBAR);
	}
	ssig = frontend->getSignalStrength();
	ssnr = frontend->getSignalNoiseRatio();

	sig = (ssig & 0xFFFF) * 100 / 65535;
	snr = (ssnr & 0xFFFF) * 100 / 65535;
	height = g_SignalFont->getHeight () - 1;
	ChanNumYPos = BoxStartY + ChanHeight /*+ 4*/ - 2 * height;

	if (lastsig != sig) {
	  lastsig = sig;
	  posx = BoxStartX + 4;
	  posy = ChanNumYPos + 3;
	  sigscale->paintProgressBar(posx, posy+4, BAR_WIDTH, 10, sig, 100);

	  sprintf (percent, "%d%%S", sig);
	  posx = posx + barwidth + 2;
	  sw = BoxStartX + ChanWidth - posx;
	  frameBuffer->paintBoxRel (posx, posy, sw, height, COL_INFOBAR_PLUS_0);
	  g_SignalFont->RenderString (posx, posy + height, sw, percent, COL_INFOBAR);
	}
	if (lastsnr != snr) {
	  lastsnr = snr;
	  posx = BoxStartX + 4;
	  posy = ChanNumYPos + 3 + height - 2;

	  snrscale->paintProgressBar(posx, posy+4, BAR_WIDTH, 10, snr, 100);

	  sprintf (percent, "%d%%Q", snr);
	  posx = posx + barwidth + 2;
	  sw = BoxStartX + ChanWidth - posx -4;
	  frameBuffer->paintBoxRel (posx, posy, sw, height-2, COL_INFOBAR_PLUS_0);
	  g_SignalFont->RenderString (posx, posy + height, sw, percent, COL_INFOBAR);
	}
  }

	struct statfs s;
	int per = 0;
	if (::statfs("/var", &s) == 0) {
		per = (s.f_blocks - s.f_bfree) / (s.f_blocks/100);
	}
	/* center the scales in the button bar. BBarY + InfoHeightY_Info / 2 is middle,
	   scales are 6 pixels high, icons are 16 pixels, so keep 4 pixels free between
	   the scales */
	varscale->paintProgressBar(BoxEndX - (2*ICON_LARGE_WIDTH + 2*ICON_SMALL_WIDTH + 4*2) - 102,
				   BBarY + InfoHeightY_Info / 2 - 2 - 6, 100, 6, per, 100);
	per = 0;
	//HD info
	if (::statfs(g_settings.network_nfs_recordingdir, &s) == 0) {
		switch(s.f_type)
		{
			case (int) 0xEF53:      /*EXT2 & EXT3*/
			case (int) 0x6969:      /*NFS*/
			case (int) 0xFF534D42:  /*CIFS*/
			case (int) 0x517B:      /*SMB*/
			case (int) 0x52654973:  /*REISERFS*/
			case (int) 0x65735546:  /*fuse for ntfs*/
			case (int) 0x58465342:  /*xfs*/
			case (int) 0x4d44:      /*msdos*/
				per = (s.f_blocks - s.f_bfree) / (s.f_blocks/100);
			break;
			default:
				fprintf( stderr,"%s Unknow File system type: %i\n",g_settings.network_nfs_recordingdir ,s.f_type);
			break;
		}
	}

	hddscale->paintProgressBar(BoxEndX - (2*ICON_LARGE_WIDTH + 2*ICON_SMALL_WIDTH + 4*2) - 102,
			BBarY + InfoHeightY_Info / 2 + 2, 100, 6, per, 100);
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

	if (starttimes)
		xStart = BoxStartX + ChanWidth;
	else
		xStart = InfoX;

	if (pb_pos > -1)
	{
		int pb_w = 112;
		int pb_p = pb_pos * pb_w / 100;
		int pb_h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight() - 4;
		if (pb_p > pb_w)
			pb_p = pb_w;
		timescale->paintProgressBar(BoxEndX - pb_w - SHADOW_OFFSET, BoxStartY + 12, pb_w, pb_h, pb_p, pb_w,
					    0, 0, COL_INFOBAR_PLUS_0, COL_INFOBAR_SHADOW_PLUS_0, "", COL_INFOBAR);
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
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(InfoX, CurrInfoY, 100, runningStart, COL_INFOBAR, 0, UTF8);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(xStart, CurrInfoY, currTimeX - xStart - 5, current, COL_INFOBAR, 0, UTF8);
		oldCurrTimeX = currTimeX;
	}

	if (currTimeX < oldCurrTimeX)
		oldCurrTimeX = currTimeX;
	frameBuffer->paintBox(oldCurrTimeX, CurrInfoY-height, BoxEndX, CurrInfoY, COL_INFOBAR_PLUS_0);
	oldCurrTimeX = currTimeX;
	if (currTimeW != 0)
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(currTimeX, CurrInfoY, currTimeW, runningRest, COL_INFOBAR, 0, UTF8);

	if (next != NULL && update_next)
	{
		frameBuffer->paintBox(InfoX, NextInfoY-height, BoxEndX, NextInfoY, COL_INFOBAR_PLUS_0);
		if (nextStart != NULL)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(InfoX, NextInfoY, 100, nextStart, COL_INFOBAR, 0, UTF8);
		if (starttimes)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(xStart, NextInfoY, nextTimeX - xStart - 5, next, COL_INFOBAR, 0, UTF8);
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(xStart, NextInfoY, nextTimeX - xStart - 5, next, COL_INFOBAR, 0, UTF8);
		if (nextTimeW != 0)
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(nextTimeX, NextInfoY, nextTimeW, nextDuration, COL_INFOBAR, 0, UTF8);
	}
}

void CInfoViewer::show_Data (bool calledFromEvent)
{
  char runningStart[10];
  char runningRest[20];
  char runningPercent = 0;
  static char oldrunningPercent = 255;

  char nextStart[10];
  char nextDuration[10];

  int is_nvod = false;
  int b114 = BoxEndX - 114 + 7;
  int b112 = BoxEndX - 112 + 7;

  if (fileplay) {
	int posy = BoxStartY + 12;
	runningPercent = file_prozent;
	if(runningPercent > 100)
		runningPercent = 100;

	if(!calledFromEvent || (oldrunningPercent != runningPercent)) {
		frameBuffer->paintBoxRel (b114+4, posy, 102, 18, COL_INFOBAR_SHADOW_PLUS_0);
		frameBuffer->paintBoxRel (b112+4, posy + 2, 98, 14, COL_INFOBAR_PLUS_0);
		oldrunningPercent = file_prozent;
	}
	timescale->paintProgressBar2(b112+4, posy + 2, runningPercent);

	int xStart = BoxStartX + ChanWidth;
	int ChanInfoY = BoxStartY + ChanHeight + 15;	//+10
	int height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight ();
	int duration1TextPos = BoxEndX - 30;

	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration1TextPos - xStart - 5, g_file_epg, COL_INFOBAR, 0, true);
	ChanInfoY += height;
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration1TextPos - xStart - 5, g_file_epg1, COL_INFOBAR, 0, true);

	return;
  }

	if (! is_visible)
		return;

	if ((g_RemoteControl->current_channel_id == channel_id) && (g_RemoteControl->subChannels.size () > 0) && (!g_RemoteControl->are_subchannels)) {
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
	  int seit = (jetzt - info_CurrentNext.current_zeit.startzeit + 30) / 60;
	  int rest = (info_CurrentNext.current_zeit.dauer / 60) - seit;
	  if (seit < 0) {
		runningPercent = 0;
		sprintf (runningRest, "in %d min", -seit);
	  } else {
		runningPercent = (unsigned) ((float) (jetzt - info_CurrentNext.current_zeit.startzeit) / (float) info_CurrentNext.current_zeit.dauer * 100.);
		if(runningPercent > 100)
			runningPercent = 100;
		sprintf (runningRest, "%d / %d min", seit, rest);
	  }

	  struct tm *pStartZeit = localtime (&info_CurrentNext.current_zeit.startzeit);
	  sprintf (runningStart, "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
	} else
		last_curr_id = 0;

	if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) {
	  unsigned dauer = info_CurrentNext.next_zeit.dauer / 60;
	  sprintf (nextDuration, "%d min", dauer);
	  struct tm *pStartZeit = localtime (&info_CurrentNext.next_zeit.startzeit);
	  sprintf (nextStart, "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
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
		if(!calledFromEvent || (oldrunningPercent != runningPercent)) {
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
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_RED, ChanInfoX + 2, BBarY, InfoHeightY_Info);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(ChanInfoX + (2 + NEUTRINO_ICON_BUTTON_RED_WIDTH + 2), BBarFontY, asize, g_Locale->getText(LOCALE_INFOVIEWER_EVENTLIST), COL_INFOBAR_BUTTONS, 0, true); // UTF-8
	  }
	}

	if ((info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast) ||
	    (calledFromEvent && !(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_next|CSectionsdClient::epgflags::has_current))))
	{
		// no EPG available
		display_Info(NULL, g_Locale->getText(gotTime ? LOCALE_INFOVIEWER_NOEPG : LOCALE_INFOVIEWER_WAITTIME));
		/* send message. Parental pin check gets triggered on EPG events... */
		char *p = new char[sizeof(t_channel_id)];
		memcpy(p, &channel_id, sizeof(t_channel_id));
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

		if(last_next_id != info_CurrentNext.next_uniqueKey) {
			frameBuffer->paintBox (ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (ChanInfoX + 10, ChanInfoY + height, 100, nextStart, COL_INFOBAR);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration2TextPos - xStart - 5, info_CurrentNext.next_name, COL_INFOBAR, 0, true);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration2TextPos, ChanInfoY + height, duration2Width, nextDuration, COL_INFOBAR);
			last_next_id = info_CurrentNext.next_uniqueKey;
		}
	  } else {
		  if(last_curr_id != info_CurrentNext.current_uniqueKey) {
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
			if(last_next_id != info_CurrentNext.next_uniqueKey) {
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

void CInfoViewer::showButton_Audio ()
{
	// green, in case of several APIDs
	uint32_t count = g_RemoteControl->current_PIDs.APIDs.size ();
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN,
			       ChanInfoX + 2 + NEUTRINO_ICON_BUTTON_RED_WIDTH + 2 + asize + 2,
			       BBarY, InfoHeightY_Info);

	if (count > 0) {
		int selected = g_RemoteControl->current_PIDs.PIDs.selected_apid;
		int sx = ChanInfoX + 2 + NEUTRINO_ICON_BUTTON_RED_WIDTH + 2 + asize + 2 + NEUTRINO_ICON_BUTTON_GREEN_WIDTH + 2;

		frameBuffer->paintBoxRel(sx, BBarY, asize, InfoHeightY_Info, COL_INFOBAR_BUTTONS_BACKGROUND);

		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(sx, BBarFontY, asize,
			g_RemoteControl->current_PIDs.APIDs[selected].desc, COL_INFOBAR_BUTTONS, 0, true); // UTF-8
	}
	const char *dd_icon;
	if ((g_RemoteControl->current_PIDs.PIDs.selected_apid < count) && (g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].is_ac3))
		dd_icon = "dd.raw";
	else if (g_RemoteControl->has_ac3)
		dd_icon = "dd_avail.raw";
	else
		dd_icon = "dd_gray.raw";

	frameBuffer->paintIcon(dd_icon, BoxEndX - (ICON_LARGE_WIDTH + 2*ICON_SMALL_WIDTH + 3*2),
			       BBarY, InfoHeightY_Info);
}

void CInfoViewer::killTitle()
{
	if (is_visible)
	{
		is_visible = false;
		int bottom = BoxEndY + SHADOW_OFFSET + BOTTOM_BAR_OFFSET;
		if (showButtonBar)
			bottom += InfoHeightY_Info;
		frameBuffer->paintBackgroundBox(BoxStartX, BoxStartY, BoxEndX+ SHADOW_OFFSET, bottom);
	}
	showButtonBar = false;
}

void CInfoViewer::Set_CA_Status (int Status)
{
  CA_Status = Status;
  m_CA_Status = Status;
  if (is_visible && showButtonBar)
	showIcon_CA_Status (1);
}

void CInfoViewer::showLcdPercentOver ()
{
  if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] != 1) {
	if (fileplay || (NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode())) {
	  CVFD::getInstance ()->showPercentOver (file_prozent);
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
		runningPercent = MIN ((unsigned) ((float) (jetzt - info_CurrentNext.current_zeit.startzeit) / (float) info_CurrentNext.current_zeit.dauer * 100.), 100);
	}
	CVFD::getInstance ()->showPercentOver (runningPercent);
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



#define ICON_H 16
#define ICON_Y_2 (16 + 2 + ICON_H)
#define MAX_EW 146


void CInfoViewer::paint_ca_icons(int caid, char * icon)
{
	if (BOTTOM_BAR_OFFSET == 0)
		return;

	char buf[20];
	int endx = ChanInfoX - 8 + ((BoxEndX - ChanInfoX)/4)*4;
	int py = BoxEndY + 2; /* hand-crafted, should be automatic */
	int px = 0;

	switch ( caid & 0xFF00 ) {
		case 0x0E00:
			px = endx - 48 - 38 - 9*10 - 7*14 - 10; sprintf(buf, "%s_%s.raw", "powervu", icon);
			break;
		case 0x4A00:
			px = endx - 48 - 38 - 8*10 - 6*14 - 10; sprintf(buf, "%s_%s.raw", "d", icon);
			break;
		case 0x2600:
			px = endx - 48 - 38 - 7*10 - 5*14 - 10; sprintf(buf, "%s_%s.raw", "biss", icon);
			break;
		case 0x600:
		case 0x602:
		case 0x1700:
			px = endx - 48 - 38 - 6*10 - 4*14 - 10; sprintf(buf, "%s_%s.raw", "ird", icon);
			break;
		case 0x100:
			px = endx - 48 - 38 - 5*10 - 4*14; sprintf(buf, "%s_%s.raw", "seca", icon);
			break;
		case 0x500:
			px = endx - 48 - 38 - 4*10 - 3*14; sprintf(buf, "%s_%s.raw", "via", icon);
			break;
		case 0x1800:
		case 0x1801:
			px = endx - 48 - 38 - 3*10 - 2*14; sprintf(buf, "%s_%s.raw", "nagra", icon);
			break;
		case 0xB00:
			px = endx - 48 - 38 - 2*10 - 1*14; sprintf(buf, "%s_%s.raw", "conax", icon);
			break;
		case 0xD00:
			px = endx - 48 - 38 - 1*10; sprintf(buf, "%s_%s.raw", "cw", icon);
			break;
		case 0x900:
			px = endx - 48; sprintf(buf, "%s_%s.raw", "nds", icon);
			break;
		default:
			break;
         }/*case*/
	 if(px) {
		frameBuffer->paintIcon(buf, px, py );
	}
}

static char * gray = (char *) "white";
//static char * green = (char *) "green";
static char * white = (char *) "yellow";
extern int pmt_caids[10];

void CInfoViewer::showIcon_CA_Status (int notfirst)
{
#if 0
 FILE *f;
 char input[256];
 char buf[256];
 int acaid = 0;
 int py = BoxEndY - InfoHeightY_Info;
#endif
 int i;
 int caids[] = { 0x1700, 0x0100, 0x0500, 0x1800, 0xB00, 0xD00, 0x900, 0x2600, 0x4a00, 0x0E00 };

 if(!notfirst) {
	 for(i=0; i < (int)(sizeof(caids)/sizeof(int)); i++) {
		 paint_ca_icons(caids[i], (char *) (pmt_caids[i] ? white : gray));
	 }
 }
}
