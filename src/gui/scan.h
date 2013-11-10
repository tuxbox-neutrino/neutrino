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

#ifndef __scants__
#define __scants__

#include <gui/widget/menue.h>
#include <gui/components/cc.h>
#include <driver/framebuffer.h>
#include <system/localize.h>
#include <string>
#include <system/settings.h>

class CScanTs : public CMenuTarget
{
	private:
		CFrameBuffer *frameBuffer;
		int x;
		int y;
		int width;
		int height;
		int hheight, mheight; // head/menu font height
		int fw;
		int xpos1; //x position for first column
		int xpos2; //x position for second column
		int radar; 
		int xpos_radar;
		int ypos_radar;
		int ypos_cur_satellite;
		int ypos_transponder;
		int ypos_frequency;
		int xpos_frequency;
		int ypos_provider;
		int ypos_channel;
		int ypos_service_numbers;
		bool success;
		bool istheend;
		uint32_t total;
		uint32_t done;
		int lastsnr, lastsig;
		int tuned;
		CProgressBar *snrscale, *sigscale;

		void paint(bool fortest = false);
		void paintLineLocale(int x, int * y, int width, const neutrino_locale_t l);
		void paintLine(int x, int y, int width, const char * const txt);
		void paintRadar(void);
		neutrino_msg_t handleMsg(neutrino_msg_t msg, neutrino_msg_data_t data);
		int greater_xpos(int xpos, const neutrino_locale_t txt);
		bool freqready;
		void showSNR();
		void testFunc();
		void prev_next_TP(bool);
		TP_params TP;
		int deltype;
		char * pname;

	public:
		CScanTs(int dtype = FE_QPSK);
		~CScanTs();
		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};
#endif
