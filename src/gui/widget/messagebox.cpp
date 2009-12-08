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

#include <gui/widget/messagebox.h>

#include <gui/widget/icons.h>
#include <driver/screen_max.h>
#include <global.h>
#include <neutrino.h>

#define ROUND_RADIUS 9

CMessageBox::CMessageBox(const neutrino_locale_t Caption, const char * const Text, const int Width, const char * const Icon, const CMessageBox::result_ Default, const uint32_t ShowButtons) : CHintBoxExt(Caption, Text, Width, Icon)
{
	returnDefaultOnTimeout = false;

	m_height += (m_fheight << 1);

	result = Default;

	showbuttons = ShowButtons;

	int MaxButtonTextWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(g_Locale->getText(LOCALE_MESSAGEBOX_CANCEL), true); // UTF-8
	int ButtonWidth = 20 + 33 + MaxButtonTextWidth + 5;
	int num = 0;
	if (showbuttons & mbYes)
		num++;
	if (showbuttons & mbNo)
		num++;
	if (showbuttons & (mbCancel | mbBack))
		num++;
	int new_width = 15 + num*ButtonWidth;
	if(new_width > m_width)
		m_width = new_width;
}

CMessageBox::CMessageBox(const neutrino_locale_t Caption, ContentLines& Lines, const int Width, const char * const Icon, const CMessageBox::result_ Default, const uint32_t ShowButtons) : CHintBoxExt(Caption, Lines, Width, Icon)
{
	returnDefaultOnTimeout = false;

	m_height += (m_fheight << 1);

	result = Default;

	showbuttons = ShowButtons;
	int MaxButtonTextWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(g_Locale->getText(LOCALE_MESSAGEBOX_CANCEL), true); // UTF-8
	int ButtonWidth = 20 + 33 + MaxButtonTextWidth + 5;
	int num = 0;
	if (showbuttons & mbYes)
		num++;
	if (showbuttons & mbNo)
		num++;
	if (showbuttons & (mbCancel | mbBack))
		num++;
	int new_width = 15 + num*ButtonWidth;
	if(new_width > m_width)
		m_width = new_width;
}

void CMessageBox::returnDefaultValueOnTimeout(bool returnDefault)
{
	returnDefaultOnTimeout = returnDefault;
}

void CMessageBox::paintButtons()
{
	uint8_t    color;
	fb_pixel_t bgcolor;

	//m_window->paintBoxRel(0, m_height - (m_fheight << 1), m_width, (m_fheight << 1), (CFBWindow::color_t)COL_MENUCONTENT_PLUS_0);
	m_window->paintBoxRel(0, m_height - (m_fheight << 1), m_width, (m_fheight << 1), (CFBWindow::color_t)COL_MENUCONTENT_PLUS_0, ROUND_RADIUS, 2);

	//irgendwann alle vergleichen - aber cancel ist sicher der längste
	int MaxButtonTextWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(g_Locale->getText(LOCALE_MESSAGEBOX_CANCEL), true); // UTF-8

	int ButtonWidth = 20 + 33 + MaxButtonTextWidth;

//	int ButtonSpacing = 40;
//	int startpos = (m_width - ((ButtonWidth*3)+(ButtonSpacing*2))) / 2;

	int ButtonSpacing = (m_width - 20 - (ButtonWidth * 3)) / 2;
	if(ButtonSpacing <= 5) ButtonSpacing = 5;

	int xpos = 10;

	if (showbuttons & mbYes)
	{
		if (result == mbrYes)
		{
			color   = COL_MENUCONTENTSELECTED;
			bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		}
		else
		{
			color   = COL_INFOBAR_SHADOW;
			bgcolor = COL_INFOBAR_SHADOW_PLUS_0;
		}
		//m_window->paintBoxRel(xpos, m_height - m_fheight - 20, ButtonWidth, m_fheight, (CFBWindow::color_t)bgcolor);
		m_window->paintBoxRel(xpos, m_height - m_fheight - 20, ButtonWidth, m_fheight, (CFBWindow::color_t)bgcolor, ROUND_RADIUS, 3);//round
		m_window->paintIcon(NEUTRINO_ICON_BUTTON_RED, xpos + 14, m_height - m_fheight - 15);
		m_window->RenderString(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], xpos + 43, m_height-m_fheight+4, ButtonWidth- 53, g_Locale->getText(LOCALE_MESSAGEBOX_YES), (CFBWindow::color_t)color, 0, true); // UTF-8
		xpos += ButtonWidth + ButtonSpacing;
	}


	if (showbuttons & mbNo)
	{
		if (result == mbrNo)
		{
			color   = COL_MENUCONTENTSELECTED;
			bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		}
		else
		{
			color   = COL_INFOBAR_SHADOW;
			bgcolor = COL_INFOBAR_SHADOW_PLUS_0;
		}

		//m_window->paintBoxRel(xpos, m_height-m_fheight-20, ButtonWidth, m_fheight, (CFBWindow::color_t)bgcolor);
		m_window->paintBoxRel(xpos, m_height-m_fheight-20, ButtonWidth, m_fheight, (CFBWindow::color_t)bgcolor, ROUND_RADIUS, 3);//round
		m_window->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, xpos+14, m_height-m_fheight-15);
		m_window->RenderString(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], xpos + 43, m_height-m_fheight+4, ButtonWidth- 53, g_Locale->getText(LOCALE_MESSAGEBOX_NO), (CFBWindow::color_t)color, 0, true); // UTF-8
		xpos += ButtonWidth + ButtonSpacing;
	}


	if (showbuttons & (mbCancel | mbBack))
	{
		if (result >= mbrCancel)
		{
			color   = COL_MENUCONTENTSELECTED;
			bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		}
		else
		{
			color   = COL_INFOBAR_SHADOW;
			bgcolor = COL_INFOBAR_SHADOW_PLUS_0;
		}

		//m_window->paintBoxRel(xpos, m_height-m_fheight-20, ButtonWidth, m_fheight, (CFBWindow::color_t)bgcolor);
		m_window->paintBoxRel(xpos, m_height-m_fheight-20, ButtonWidth, m_fheight, (CFBWindow::color_t)bgcolor, ROUND_RADIUS, 3);//round
		m_window->paintIcon(NEUTRINO_ICON_BUTTON_HOME, xpos+10, m_height-m_fheight-19);
		m_window->RenderString(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], xpos + 43, m_height-m_fheight+4, ButtonWidth- 53, g_Locale->getText((showbuttons & mbCancel) ? LOCALE_MESSAGEBOX_CANCEL : LOCALE_MESSAGEBOX_BACK), (CFBWindow::color_t)color, 0, true); // UTF-8
	}
}

int CMessageBox::exec(int timeout)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	CHintBoxExt::paint(0);

	if (m_window == NULL)
	{
		return res; /* out of memory */
	}

	paintButtons();

	if ( timeout == -1 )
		timeout = g_settings.timing[SNeutrinoSettings::TIMING_EPG];

	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd( timeout );

	bool loop=true;
	while (loop)
	{

		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );
		if (msg == CRCInput::RC_timeout && returnDefaultOnTimeout)
		{
			// return default 
			loop = false;
		}
		else if (((msg == CRCInput::RC_timeout) ||
			  (msg  == (neutrino_msg_t)g_settings.key_channelList_cancel)) &&
			 (showbuttons & (mbCancel | mbBack)))
		{
			result = (showbuttons & mbCancel) ? mbrCancel : mbrBack;
			loop   = false;
		}
		else if ((msg == CRCInput::RC_green) && (showbuttons & mbNo))
		{
			result = mbrNo;
			loop   = false;
		}
		else if ((msg == CRCInput::RC_red) && (showbuttons & mbYes))
		{
			result = mbrYes;
			loop   = false;
		}
		else if(msg==CRCInput::RC_right)
		{
			bool ok = false;
			while (!ok)
			{
				result = (CMessageBox::result_)((result + 1) & 3);
				ok = showbuttons & (1 << result);
			}

			paintButtons();
		}
		else if (has_scrollbar() && ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down)))
		{
			if (msg == CRCInput::RC_up)
				scroll_up();
			else
				scroll_down();
		}
		else if(msg==CRCInput::RC_left)
		{
			bool ok = false;
			while (!ok)
			{
				result = (CMessageBox::result_)((result - 1) & 3);
				ok = showbuttons & (1 << result);
			}

			paintButtons();

		}
		else if(msg == CRCInput::RC_ok)
		{
			loop = false;
		}
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites))
		{
		}
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
		{
			res  = menu_return::RETURN_EXIT_ALL;
			loop = false;
		}

	}

	hide();
	
	return res;
}

int ShowMsgUTF(const neutrino_locale_t Caption, const char * const Text, const CMessageBox::result_ Default, const uint32_t ShowButtons, const char * const Icon, const int Width, const int timeout, bool returnDefaultOnTimeout)
{
   	CMessageBox* messageBox = new CMessageBox(Caption, Text, Width, Icon, Default, ShowButtons);
	messageBox->returnDefaultValueOnTimeout(returnDefaultOnTimeout);
	messageBox->exec(timeout);
	int res = messageBox->result;
	delete messageBox;
	
	return res;
}

int ShowLocalizedMessage(const neutrino_locale_t Caption, const neutrino_locale_t Text, const CMessageBox::result_ Default, const uint32_t ShowButtons, const char * const Icon, const int Width, const int timeout, bool returnDefaultOnTimeout)
{
	return ShowMsgUTF(Caption, g_Locale->getText(Text), Default, ShowButtons, Icon, Width, timeout,returnDefaultOnTimeout);
}

int ShowMsgUTF(const neutrino_locale_t Caption, const std::string & Text, const CMessageBox::result_ Default, const uint32_t ShowButtons, const char * const Icon, const int Width, const int timeout, bool returnDefaultOnTimeout)
{
	return ShowMsgUTF(Caption, Text.c_str(), Default, ShowButtons, Icon, Width, timeout,returnDefaultOnTimeout);
}

void DisplayErrorMessage(const char * const ErrorMsg)
{
	ShowMsgUTF(LOCALE_MESSAGEBOX_ERROR, ErrorMsg, CMessageBox::mbrCancel, CMessageBox::mbCancel, "error.raw");
}
