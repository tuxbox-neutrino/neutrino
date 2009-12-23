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

#include <gui/screensetup.h>

#include <gui/color.h>
#include <gui/widget/messagebox.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <system/settings.h>

#include <global.h>
#include <neutrino.h>

int x=15*5;
int y=15*25;
int BoxHeight=15*4;
int BoxWidth=15*23;

CScreenSetup::CScreenSetup()
{
	frameBuffer = CFrameBuffer::getInstance();
}

int CScreenSetup::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	x_coord[0] = g_settings.screen_StartX;
	x_coord[1] = g_settings.screen_EndX;
	y_coord[0] = g_settings.screen_StartY;
	y_coord[1] = g_settings.screen_EndY;

	paint();

	selected = 0;

	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
::TIMING_MENU]);

	bool loop=true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd, true );

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
::TIMING_MENU]);

		switch ( msg )
		{
			case CRCInput::RC_ok:
				// abspeichern
				g_settings.screen_StartX = x_coord[0];
				g_settings.screen_EndX = x_coord[1];
				g_settings.screen_StartY = y_coord[0];
				g_settings.screen_EndY = y_coord[1];
				if (g_InfoViewer) /* recalc infobar position */
					g_InfoViewer->start();
				loop = false;
				break;

			case CRCInput::RC_home:
				if ( ( ( g_settings.screen_StartX != x_coord[0] ) ||
							( g_settings.screen_EndX != x_coord[1] ) ||
							( g_settings.screen_StartY != y_coord[0] ) ||
							( g_settings.screen_EndY != y_coord[1] ) ) &&
						(ShowLocalizedMessage(LOCALE_VIDEOMENU_SCREENSETUP, LOCALE_MESSAGEBOX_DISCARD, CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbCancel) == CMessageBox::mbrCancel))
					break;

			case CRCInput::RC_timeout:
				loop = false;
				break;

			case CRCInput::RC_red:
			case CRCInput::RC_green:
				{
					selected = ( msg == CRCInput::RC_green ) ? 1 : 0 ;

					frameBuffer->paintBoxRel(x,y, BoxWidth,BoxHeight/2, (selected==0)? COL_MENUCONTENTSELECTED_PLUS_0:COL_MENUCONTENT_PLUS_0);
					frameBuffer->paintBoxRel(x,y+BoxHeight/2, BoxWidth,BoxHeight/2, (selected==1)? COL_MENUCONTENTSELECTED_PLUS_0:COL_MENUCONTENT_PLUS_0);

					g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+30,y+BoxHeight/2, BoxWidth, g_Locale->getText(LOCALE_SCREENSETUP_UPPERLEFT ), (selected == 0)?COL_MENUCONTENTSELECTED:COL_MENUCONTENT, 0, true); // UTF-8

					g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+30,y+BoxHeight, BoxWidth, g_Locale->getText(LOCALE_SCREENSETUP_LOWERRIGHT), (selected == 1)?COL_MENUCONTENTSELECTED:COL_MENUCONTENT, 0, true); // UTF-8

					paintIcons();
					break;
				}
			case CRCInput::RC_up:
				{
					y_coord[selected]--;

					int min = ( selected == 0 ) ? 0 : 400;
					if ( y_coord[selected] < min )
						y_coord[selected] = min ;
					else
						paintBorder( selected );
					break;
				}
			case CRCInput::RC_down:
				{
					y_coord[selected]++;

					//int max = ( selected == 0 ) ? 200 : 575;
					int max = ( selected == 0 ) ? 200 : frameBuffer->getScreenHeight(true)-1;
printf("selected %d y %d max %d\n", selected, y_coord[selected], max);
					if ( y_coord[selected] > max )
						y_coord[selected] = max ;
					else
						paintBorder( selected );
					break;
				}
			case CRCInput::RC_left:
				{
					x_coord[selected]--;

					int min = ( selected == 0 ) ? 0 : 400;
					if ( x_coord[selected] < min )
						x_coord[selected] = min ;
					else
						paintBorder( selected );
					break;
				}
			case CRCInput::RC_right:
				{
					x_coord[selected]++;

					//int max = ( selected == 0 ) ? 200 : 719;
					int max = ( selected == 0 ) ? 200 : frameBuffer->getScreenWidth(true)-1;
					if ( y_coord[selected] > max )
						y_coord[selected] = max ;
					else
						paintBorder( selected );
					break;
				}
			case CRCInput::RC_favorites:
			case CRCInput::RC_sat:
				break;

			default:
				if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
		}

	}

	hide();
	return res;
}

void CScreenSetup::hide()
{
	int w = (int) frameBuffer->getScreenWidth(true);
	int h = (int) frameBuffer->getScreenHeight(true);
	frameBuffer->paintBackgroundBox(0, 0, w, h);
}

void CScreenSetup::paintBorder( int pselected )
{
	if ( pselected == 0 )
		paintBorderUL();
	else
		paintBorderLR();

	paintCoords();
}

void CScreenSetup::paintIcons()
{
        frameBuffer->paintIcon("rot.raw", x+6, y+8);
        frameBuffer->paintIcon("gruen.raw", x+6, y+36 );
}

void CScreenSetup::paintBorderUL()
{
	frameBuffer->paintIcon( "border_ul.raw", x_coord[0], y_coord[0] );
}

void CScreenSetup::paintBorderLR()
{
	frameBuffer->paintIcon("border_lr.raw", x_coord[1]- 96, y_coord[1]- 96 );
}

void CScreenSetup::paintCoords()
{
	int w = 15*9;
	int h = 15*6;

	//int x=15*19;
	//int y=15*16;
	int x1 = (frameBuffer->getScreenWidth(true) - w) / 2;
	int y1 = (frameBuffer->getScreenHeight(true) - h) / 2;

	frameBuffer->paintBoxRel(x1,y1, 15*9,15*6, COL_MENUCONTENT_PLUS_0);

	char xpos[30];
	char ypos[30];
	char xepos[30];
	char yepos[30];

	sprintf((char*) &xpos, "SX: %d",x_coord[0] );
	sprintf((char*) &ypos, "SY: %d", y_coord[0] );
	sprintf((char*) &xepos, "EX: %d", x_coord[1] );
	sprintf((char*) &yepos, "EY: %d", y_coord[1] );

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x1+10,y1+30, 200, xpos, COL_MENUCONTENT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x1+10,y1+50, 200, ypos, COL_MENUCONTENT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x1+10,y1+70, 200, xepos, COL_MENUCONTENT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x1+10,y1+90, 200, yepos, COL_MENUCONTENT);
}

inline unsigned int make16color(__u32 rgb)
{
        return 0xFF000000 | rgb;
}

void CScreenSetup::paint()
{
	if (!frameBuffer->getActive())
		return;

	int w = (int) frameBuffer->getScreenWidth(true);
	int h = (int) frameBuffer->getScreenHeight(true);

	frameBuffer->paintBox(0,0, w, h, make16color(0xA0A0A0));

	for(int count = 0; count < h; count += 15)
		frameBuffer->paintHLine(0, w-1, count, make16color(0x505050) );

	for(int count = 0; count < w; count += 15)
		frameBuffer->paintVLine(count, 0, h-1, make16color(0x505050) );

	//frameBuffer->paintBox(0,0, 15*15,15*15, make16color(0xA0A0A0));
	//frameBuffer->paintBox(32*15+1, 23*15+1, 719,575, make16color(0xA0A0A0));
	frameBuffer->paintBox(0, 0, w/3, h/3, make16color(0xA0A0A0));
	frameBuffer->paintBox(w-w/3, h-h/3, w-1, h-1, make16color(0xA0A0A0));
//new
        //frameBuffer->paintBoxRel(225, 89, 496, 3, COL_MENUCONTENT_PLUS_0); //upper letterbox marker
        //frameBuffer->paintBoxRel(0, 495, 481, 3, COL_MENUCONTENT_PLUS_0);  //lower letterbox marker

        frameBuffer->paintBoxRel(x, y, BoxWidth, BoxHeight/2, COL_MENUCONTENTSELECTED_PLUS_0);   //upper selected box
        frameBuffer->paintBoxRel(x, y+BoxHeight/2, BoxWidth, BoxHeight/2, COL_MENUCONTENT_PLUS_0);  //lower selected box

        g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+30, y+BoxHeight/2, BoxWidth,
		g_Locale->getText(LOCALE_SCREENSETUP_UPPERLEFT ), COL_MENUCONTENTSELECTED , 0, true); // UTF-8
        g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+30, y+BoxHeight, BoxWidth,
		g_Locale->getText(LOCALE_SCREENSETUP_LOWERRIGHT), COL_MENUCONTENT, 0, true); // UTF-8
//new end
#if 0 // old
	int x=15*5;
	int y=15*24;
	frameBuffer->paintBoxRel(x,y, 15*23,15*4, COL_MENUCONTENT_PLUS_0);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+30,y+29, 15*23, g_Locale->getText(LOCALE_SCREENSETUP_UPPERLEFT ), COL_MENUHEAD   , 0, true); // UTF-8
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+30,y+49, 15*23, g_Locale->getText(LOCALE_SCREENSETUP_LOWERRIGHT), COL_MENUCONTENT, 0, true); // UTF-8
#endif

	paintIcons();
	paintBorderUL();
	paintBorderLR();
	paintCoords();
}
