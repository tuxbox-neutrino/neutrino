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


#ifndef __infoview_bb__
#define __infoview_bb__

#include <sectionsdclient/sectionsdclient.h>

#include <driver/rcinput.h>
#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <driver/fade.h>
#include <system/settings.h>
#include "widget/menue.h"
#include <gui/components/cc_item_progressbar.h>
#include <string>

class CInfoViewerBB
{
	public:
		enum
		{
			// The order of icons from left to right. Here nothing changes!
			BUTTON_EPG	= 0,
			BUTTON_AUDIO	= 1,
			BUTTON_SUBS	= 2,
			BUTTON_FEAT	= 3,
			BUTTON_MAX	= 4
		};

	private:
		CFrameBuffer * frameBuffer;

		enum
		{
			// The order of icons from right to left. Here nothing changes!
			ICON_SUBT	= 0,
			ICON_VTXT	= 1,
			ICON_RT		= 2,
			ICON_DD		= 3,
			ICON_16_9	= 4,
			ICON_RES	= 5,
			ICON_CA		= 6,
			ICON_TUNER	= 7,
			ICON_MAX	= 8
		};

		typedef struct
		{
			bool paint;
			int w;
			int x;
			int cx;
			int h;
			std::string icon;
			std::string text;
		} bbButtonInfoStruct;

		typedef struct
		{
			int x;
			int h;
		} bbIconInfoStruct;

		bbIconInfoStruct bbIconInfo[CInfoViewerBB::ICON_MAX];
		bbButtonInfoStruct bbButtonInfo[CInfoViewerBB::BUTTON_MAX];
		std::string tmp_bbButtonInfoText[CInfoViewerBB::BUTTON_MAX];
		int bbIconMinX, bbButtonMaxX, bbIconMaxH, bbButtonMaxH;

		int BBarY, BBarFontY;
		int hddwidth;
		//int lasthdd, lastsys;
		bool fta;
		int minX;

		bool scrambledErr, scrambledErrSave;
		bool scrambledNoSig, scrambledNoSigSave;
		pthread_t scrambledT;

		CProgressBar *hddscale, *sysscale;

		void showBBIcons(const int modus, const std::string & icon);
		void getBBIconInfo(void);
		bool checkBBIcon(const char * const icon, int *w, int *h);
		void showIcon_DD(void);

		void paint_ca_icons(int, char*, int&);
		void paintCA_bar(int,int);
		void showOne_CAIcon();
		void changePB(void);

		static void* scrambledThread(void *arg);
		void scrambledCheck(bool force=false);

		void showBarSys(int percent = 0);
		void showBarHdd(int percent = 0);

		int		hddpercent;
		pthread_t	hddperT;
		static void*	hddperThread(void *arg);
		bool		hddperTflag;
		CInfoViewerBB();

	public:
		~CInfoViewerBB();
		static CInfoViewerBB* getInstance();
		void Init(void);

		int bottom_bar_offset, InfoHeightY_Info;
		bool is_visible;

		void showSysfsHdd(void);
		void showIcon_CA_Status(int);
		void showIcon_16_9();
		void showIcon_RadioText(bool rt_available);
		void showIcon_VTXT();
		void showIcon_SubT();
		void showIcon_Resolution();
		void showIcon_Tuner(void);
		void showBBButtons(const int modus=-1);
		void paintshowButtonBar();
		void getBBButtonInfo(void);
		void reset_allScala(void);
		void setBBOffset(void);

};

#endif // __infoview_bb__
