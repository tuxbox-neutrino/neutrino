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

#include <gui/widget/colorchooser.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>

#include <gui/color.h>
#include <gui/widget/messagebox.h>

#define ROUND_RADIUS 8

#define VALUE_R     0
#define VALUE_G     1
#define VALUE_B     2
#define VALUE_ALPHA 3

static const char * const iconnames[4] = {
	"volumeslider2red.raw",
	"volumeslider2green.raw",
	"volumeslider2blue.raw",
	"volumeslider2alpha.raw"
};

static const neutrino_locale_t colorchooser_names[4] =
{
	LOCALE_COLORCHOOSER_RED  ,
	LOCALE_COLORCHOOSER_GREEN,
	LOCALE_COLORCHOOSER_BLUE ,
	LOCALE_COLORCHOOSER_ALPHA
};

CColorChooser::CColorChooser(const neutrino_locale_t Name, unsigned char *R, unsigned char *G, unsigned char *B, unsigned char* Alpha, CChangeObserver* Observer) // UTF-8
{
	frameBuffer = CFrameBuffer::getInstance();
	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	observer = Observer;
	name = Name;
	width = w_max(360, 0);
	height = h_max(hheight+ mheight* 4, 0);

	x = frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth()-width) >> 1);
	y = frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight()-height)>>1);

	value[VALUE_R]     = R;
	value[VALUE_G]     = G;
	value[VALUE_B]     = B;
	value[VALUE_ALPHA] = Alpha;
}

void CColorChooser::setColor()
{
	int color = convertSetupColor2RGB(*(value[VALUE_R]), *(value[VALUE_G]), *(value[VALUE_B]));
	int tAlpha = (value[VALUE_ALPHA]) ? (convertSetupAlpha2Alpha(*(value[VALUE_ALPHA]))) : 0;

	if(!value[VALUE_ALPHA]) tAlpha = 0xFF;

	fb_pixel_t col = ((tAlpha << 24) & 0xFF000000) | color;
		//((tAlpha << 24) & 0xFF000000) | ((color << 16) & 0x00FF0000) | (color & 0x0000FF00) | ((color >> 16) & 0xFF);
	frameBuffer->paintBoxRel(x+222,y+hheight+2+5,  mheight*4-4 ,mheight*4-4-10, col);
}

int CColorChooser::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;
	if (parent)
		parent->hide();

	unsigned char r_alt= *value[VALUE_R];
	unsigned char g_alt= *value[VALUE_G];
	unsigned char b_alt= *value[VALUE_B];
	unsigned char a_alt = (value[VALUE_ALPHA]) ? (*(value[VALUE_ALPHA])) : 0;

	paint();
	setColor();

	int selected = 0;

	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
			::TIMING_MENU]);

	bool loop=true;
	while (loop) {
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
					::TIMING_MENU]);

		switch ( msg ) {
			case CRCInput::RC_down:
				{
					if (selected < ((value[VALUE_ALPHA]) ? 3 : 2))
					{
						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], false);
						selected++;
						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], true);
					} else {
						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], false);
						selected = 0;
						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], true);
					}
					break;

				}
			case CRCInput::RC_up:
				{
					if (selected > 0)
					{
						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], false);
						selected--;
						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], true);
					} else {
						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], false);
						selected = ((value[VALUE_ALPHA]) ? 3 : 2);
						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], true);
					}
					break;
				}
			case CRCInput::RC_right:
				{
					if ((*value[selected]) < 100)
					{
						if ((*value[selected]) < 98)
							(*value[selected]) += 2;
						else
							(*value[selected]) = 100;

						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], true);
						setColor();
					}
					break;
				}
			case CRCInput::RC_left:
				{
					if ((*value[selected]) > 0)
					{
						if ((*value[selected]) > 2)
							(*value[selected]) -= 2;
						else
							(*value[selected]) = 0;

						paintSlider(x + 10, y + hheight + mheight * selected, value[selected], colorchooser_names[selected], iconnames[selected], true);
						setColor();
					}
					break;
				}
			case CRCInput::RC_home:
				if (((*value[VALUE_R] != r_alt) || (*value[VALUE_G] != g_alt) || (*value[VALUE_B] != b_alt) || ((value[VALUE_ALPHA]) && (*(value[VALUE_ALPHA]) != a_alt))) &&
						(ShowLocalizedMessage(name, LOCALE_MESSAGEBOX_DISCARD, CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbCancel) == CMessageBox::mbrCancel))
					break;

				// sonst abbruch...
				*value[VALUE_R] = r_alt;
				*value[VALUE_G] = g_alt;
				*value[VALUE_B] = b_alt;
				if (value[VALUE_ALPHA])
					*value[VALUE_ALPHA] = a_alt;

			case CRCInput::RC_sat:
			case CRCInput::RC_favorites:
				break;
			case CRCInput::RC_timeout:
			case CRCInput::RC_ok:
				loop = false;
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

	if(observer)
		observer->changeNotify(name, NULL);

	return res;
}

void CColorChooser::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
}

void CColorChooser::paint()
{
	//frameBuffer->paintBoxRel(x,y, width,hheight, COL_MENUHEAD_PLUS_0);
	frameBuffer->paintBoxRel(x,y, width,hheight, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, 1); //round
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+10,y+hheight, width, g_Locale->getText(name), COL_MENUHEAD, 0, true); // UTF-8
	//frameBuffer->paintBoxRel(x,y+hheight, width,height-hheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x,y+hheight, width,height-hheight, COL_MENUCONTENT_PLUS_0, ROUND_RADIUS, 2);//round

	for (int i = 0; i < 4; i++)
		paintSlider(x + 10, y + hheight + mheight * i, value[i], colorchooser_names[i], iconnames[i], (i == 0));

	//color preview
	frameBuffer->paintBoxRel(x+220,y+hheight+5,    mheight*4,   mheight*4-10, COL_MENUHEAD_PLUS_0);
	frameBuffer->paintBoxRel(x+222,y+hheight+2+5,  mheight*4-4 ,mheight*4-4-10, 254);
}

void CColorChooser::paintSlider(int x, int y, unsigned char *spos, const neutrino_locale_t text, const char * const iconname, const bool selected)
{
	if (!spos)
		return;
	frameBuffer->paintBoxRel(x+70,y,120,mheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintIcon("volumebody.raw",x+70,y+2+mheight/4);
	frameBuffer->paintIcon(selected ? iconname : "volumeslider2.raw",x+73+(*spos),y+mheight/4);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x,y+mheight, width, g_Locale->getText(text), COL_MENUCONTENT, 0, true); // UTF-8
}
