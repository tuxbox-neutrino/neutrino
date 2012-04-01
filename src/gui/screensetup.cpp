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
#include "gui/widget/icons.h"

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <system/settings.h>

#include <global.h>
#include <neutrino.h>

//int x_box = 15 * 5;

inline unsigned int make16color(__u32 rgb)
{
	return 0xFF000000 | rgb;
}

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

	x_box = 15*5;
	y_box = frameBuffer->getScreenHeight(true) / 2;

        int icol_w, icol_h;
        frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);

	BoxHeight = std::max(icol_h+4, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());
	BoxWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCREENSETUP_UPPERLEFT));

	int tmp = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_SCREENSETUP_LOWERRIGHT));
	if (tmp > BoxWidth)
		BoxWidth = tmp;

	BoxWidth += 10 + icol_w;

	x_coord[0] = g_settings.screen_StartX;
	x_coord[1] = g_settings.screen_EndX;
	y_coord[0] = g_settings.screen_StartY;
	y_coord[1] = g_settings.screen_EndY;

	paint();
	frameBuffer->blit();

	selected = 0;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
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
				if(g_settings.screen_preset) {
					g_settings.screen_StartX_lcd = g_settings.screen_StartX;
					g_settings.screen_StartY_lcd = g_settings.screen_StartY;
					g_settings.screen_EndX_lcd = g_settings.screen_EndX;
					g_settings.screen_EndY_lcd = g_settings.screen_EndY;
				} else {
					g_settings.screen_StartX_crt = g_settings.screen_StartX;
					g_settings.screen_StartY_crt = g_settings.screen_StartY;
					g_settings.screen_EndX_crt = g_settings.screen_EndX;
					g_settings.screen_EndY_crt = g_settings.screen_EndY;
				}
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

					frameBuffer->paintBoxRel(x_box, y_box, BoxWidth, BoxHeight,
						(selected == 0)?COL_MENUCONTENTSELECTED_PLUS_0:COL_MENUCONTENT_PLUS_0);
					frameBuffer->paintBoxRel(x_box, y_box + BoxHeight, BoxWidth, BoxHeight,
						(selected ==1 )?COL_MENUCONTENTSELECTED_PLUS_0:COL_MENUCONTENT_PLUS_0);

					paintIcons(selected);
					break;
				}
			case CRCInput::RC_up:
			{

				int min = (selected == 0) ? 0 : 400;
				if (y_coord[selected] <= min)
					y_coord[selected] = min;
				else
				{
					unpaintBorder(selected);
					y_coord[selected]--;
					paintBorder(selected);
				}
				break;
			}
			case CRCInput::RC_down:
			{
				int max = (selected == 0 )? 200 : frameBuffer->getScreenHeight(true) - 1;
				if (y_coord[selected] >= max)
					y_coord[selected] = max;
				else
				{
					unpaintBorder(selected);
					y_coord[selected]++;
					paintBorder(selected);
				}
				break;
			}
			case CRCInput::RC_left:
			{
				int min = (selected == 0) ? 0 : 400;
				if (x_coord[selected] <= min)
					x_coord[selected] = min;
				else
				{
					unpaintBorder(selected);
					x_coord[selected]--;
					paintBorder( selected );
				}
				break;
			}
			case CRCInput::RC_right:
			{
				int max = (selected == 0) ? 200 : frameBuffer->getScreenWidth(true) - 1;
				if (x_coord[selected] >= max)
					x_coord[selected] = max;
				else
				{
					unpaintBorder(selected);
					x_coord[selected]++;
					paintBorder( selected );
				}
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
		frameBuffer->blit();
	}

	hide();
	return res;
}

void CScreenSetup::hide()
{
	int w = (int) frameBuffer->getScreenWidth(true);
	int h = (int) frameBuffer->getScreenHeight(true);
	frameBuffer->paintBackgroundBox(0, 0, w, h);
	frameBuffer->blit();
}

void CScreenSetup::paintBorder( int pselected )
{
	if ( pselected == 0 )
		paintBorderUL();
	else
		paintBorderLR();

	paintCoords();
}

void CScreenSetup::unpaintBorder(int pselected)
{
	int cx = x_coord[pselected] - 96 * pselected;
	int cy = y_coord[pselected] - 96 * pselected;
	frameBuffer->paintBoxRel(cx, cy, 96, 96, make16color(0xA0A0A0));
}

void CScreenSetup::paintIcons(int pselected)
{
        int icol_w = 0, icol_h = 0;
        frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);

	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_RED, x_box + 5, y_box, BoxHeight);
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, x_box + 5, y_box+BoxHeight, BoxHeight);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x_box + icol_w + 10, y_box + BoxHeight, BoxWidth,
		g_Locale->getText(LOCALE_SCREENSETUP_UPPERLEFT ), (pselected == 0) ? COL_MENUCONTENTSELECTED:COL_MENUCONTENT , 0, true); // UTF-8
        g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x_box + icol_w + 10, y_box + BoxHeight * 2, BoxWidth,
		g_Locale->getText(LOCALE_SCREENSETUP_LOWERRIGHT), (pselected == 1) ? COL_MENUCONTENTSELECTED:COL_MENUCONTENT, 0, true); // UTF-8
}

void CScreenSetup::paintBorderUL()
{
	frameBuffer->paintIcon(NEUTRINO_ICON_BORDER_UL, x_coord[0], y_coord[0] );
}

void CScreenSetup::paintBorderLR()
{
	frameBuffer->paintIcon(NEUTRINO_ICON_BORDER_LR, x_coord[1]- 96, y_coord[1]- 96 );
}

void CScreenSetup::paintCoords()
{
	Font *f = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	int w = f->getRenderWidth("EX: 2222") * 5 / 4;	/* half glyph border left and right */
	int fh = f->getHeight();
	int h = fh * 4;		/* 4 lines, fonts have enough space around them, no extra border */

	int x1 = (frameBuffer->getScreenWidth(true) - w) / 2;	/* centered */
	int y1 = frameBuffer->getScreenHeight(true) / 2 - h;	/* above center, to avoid conflict */
	int x2 = x1 + w / 10;
	int y2 = y1 + fh;

	frameBuffer->paintBoxRel(x1, y1, w, h, COL_MENUCONTENT_PLUS_0);

	char str[4][16];
	snprintf(str[0], 16, "SX: %d", x_coord[0]);
	snprintf(str[1], 16, "SY: %d", y_coord[0]);
	snprintf(str[2], 16, "EX: %d", x_coord[1]);
	snprintf(str[3], 16, "EY: %d", y_coord[1]);
	/* the code is smaller with this loop instead of open-coded 4x RenderString() :-) */
	for (int i = 0; i < 4; i++)
	{
		f->RenderString(x2, y2, w, str[i], COL_MENUCONTENT);
		y2 += fh;
	}
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

	frameBuffer->paintBox(0, 0, w/3, h/3, make16color(0xA0A0A0));
	frameBuffer->paintBox(w-w/3, h-h/3, w-1, h-1, make16color(0xA0A0A0));

	frameBuffer->paintBoxRel(x_box, y_box, BoxWidth, BoxHeight, COL_MENUCONTENTSELECTED_PLUS_0);   //upper selected box
	frameBuffer->paintBoxRel(x_box, y_box + BoxHeight, BoxWidth, BoxHeight, COL_MENUCONTENT_PLUS_0); //lower selected box

	paintIcons(0);
	paintBorderUL();
	paintBorderLR();
	paintCoords();
}
