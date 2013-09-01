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

CMessageBox::CMessageBox(const neutrino_locale_t Caption, const char * const Text, const int Width, const char * const Icon, const CMessageBox::result_ &Default, const uint32_t ShowButtons) : CHintBoxExt(Caption, Text, Width, Icon)
{
	Init(Default, ShowButtons);
}

CMessageBox::CMessageBox(const neutrino_locale_t Caption, ContentLines& Lines, const int Width, const char * const Icon, const CMessageBox::result_ &Default, const uint32_t ShowButtons) : CHintBoxExt(Caption, Lines, Width, Icon)
{
	Init(Default, ShowButtons);
}

void CMessageBox::Init(const CMessageBox::result_ &Default, const uint32_t ShowButtons)
{
#define BtnCount 3
	returnDefaultOnTimeout = false;
	ButtonSpacing          = 15;
	int w = 0, h = 0, ih = 0;
	i_maxw = 0;
	std::string Btns[BtnCount] = {NEUTRINO_ICON_BUTTON_RED, NEUTRINO_ICON_BUTTON_GREEN, NEUTRINO_ICON_BUTTON_HOME};
	for (int i = 0; i < BtnCount; i++) {
		CFrameBuffer::getInstance()->getIconSize(Btns[i].c_str(), &w, &h);
		ih = std::max(h, ih);
		i_maxw = std::max(w, i_maxw);
	}
	fh                     = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
	b_height               = std::max(fh, ih) + 8 + (RADIUS_LARGE / 2);
	m_bbheight             = b_height + fh/2 + ButtonSpacing;
	result                 = Default;
	b_width                = getButtonWidth();
	if (ShowButtons        & CMessageBox::mbBtnAlignCenter1)
		mbBtnAlign     = CMessageBox::mbBtnAlignCenter1; // centered, large distances
	else if (ShowButtons   & CMessageBox::mbBtnAlignCenter2)
		mbBtnAlign     = CMessageBox::mbBtnAlignCenter2; // centered, small distances
	else if (ShowButtons   & CMessageBox::mbBtnAlignLeft)
		mbBtnAlign     = CMessageBox::mbBtnAlignLeft;
	else if (ShowButtons   & CMessageBox::mbBtnAlignRight)
		mbBtnAlign     = CMessageBox::mbBtnAlignRight;
	else
		mbBtnAlign     = CMessageBox::mbBtnAlignCenter1; // or g_settings.mbBtnAlign? ;-)
	showbuttons            = ShowButtons & 0xFF;

	ButtonCount = 0;
	if (showbuttons & mbYes) {
		Buttons[ButtonCount].icon = NEUTRINO_ICON_BUTTON_RED;
		Buttons[ButtonCount].text = g_Locale->getText(LOCALE_MESSAGEBOX_YES);
		ButtonCount++;
	}
	if (showbuttons & mbNo) {
		Buttons[ButtonCount].icon = NEUTRINO_ICON_BUTTON_GREEN;
		Buttons[ButtonCount].text = g_Locale->getText(LOCALE_MESSAGEBOX_NO);
		ButtonCount++;
	}
	if (showbuttons & (mbCancel | mbBack | mbOk)) {
		Buttons[ButtonCount].icon = NEUTRINO_ICON_BUTTON_HOME;
		Buttons[ButtonCount].text = g_Locale->getText((showbuttons & mbCancel) ? LOCALE_MESSAGEBOX_CANCEL : (showbuttons & mbOk) ? LOCALE_MESSAGEBOX_OK : LOCALE_MESSAGEBOX_BACK);
		ButtonCount++;
	}

	ButtonDistance = ButtonSpacing;
	bb_width = b_width * ButtonCount + ButtonDistance * (ButtonCount - 1);
	if(bb_width > m_width)
		m_width = bb_width; /* FIXME: what if bigger than screen area? */
	else
		if (mbBtnAlign == CMessageBox::mbBtnAlignCenter1)
			ButtonDistance = (m_width - b_width * ButtonCount) / (ButtonCount + 1);

	/* this is ugly: re-init (CHintBoxExt) to recalculate the number of lines and pages */
	init(m_caption, m_width, m_iconfile == "" ? NULL : m_iconfile.c_str());
	m_height += m_bbheight;
}

void CMessageBox::returnDefaultValueOnTimeout(bool returnDefault)
{
	returnDefaultOnTimeout = returnDefault;
}

int CMessageBox::getButtonWidth()
{
#define localeMsgCount 5
	neutrino_locale_t localeMsg[localeMsgCount] = {LOCALE_MESSAGEBOX_YES, LOCALE_MESSAGEBOX_NO, LOCALE_MESSAGEBOX_CANCEL, LOCALE_MESSAGEBOX_OK, LOCALE_MESSAGEBOX_BACK};
	int MaxButtonTextWidth = 0;
	for (int i = 0; i < localeMsgCount; i++)
		MaxButtonTextWidth = std::max(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(g_Locale->getText(localeMsg[i]), true), MaxButtonTextWidth);
	return MaxButtonTextWidth + i_maxw + 36 + (RADIUS_LARGE / 2);
}

void CMessageBox::paintButtons()
{
	fb_pixel_t color;
	fb_pixel_t bgcolor;
	int iw, ih, i;

	int xpos = (m_width - bb_width) / 2;
	if (mbBtnAlign == CMessageBox::mbBtnAlignCenter1)
		xpos = ButtonDistance;
	else if (mbBtnAlign == CMessageBox::mbBtnAlignCenter2)
		xpos = (m_width - bb_width) / 2;
	else if (mbBtnAlign == CMessageBox::mbBtnAlignLeft)
		xpos = ButtonSpacing;
	else if (mbBtnAlign == CMessageBox::mbBtnAlignRight)
		xpos = m_width - bb_width - ButtonSpacing;

	int ypos = (m_height - m_bbheight) + fh/2;

	m_window->paintBoxRel(0, m_height - m_bbheight, m_width, m_bbheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	i = 0;
	if (showbuttons & mbYes) {
		Buttons[i].def = (result == mbrYes) ? true : false;
		i++;
	}
	if (showbuttons & mbNo) {
		Buttons[i].def = (result == mbrNo) ? true : false;
		i++;
	}
	if (showbuttons & (mbCancel | mbBack | mbOk))
		Buttons[i].def = (result >= mbrCancel) ? true : false;

	for (i = 0; i < ButtonCount; i++) {
		if (Buttons[i].def) {
			color   = COL_MENUCONTENTSELECTED_TEXT;
			bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		} else {
			color   = COL_INFOBAR_SHADOW_TEXT;
			bgcolor = COL_INFOBAR_SHADOW_PLUS_0;
		}
		CFrameBuffer::getInstance()->getIconSize(Buttons[i].icon, &iw, &ih);
		m_window->paintBoxRel(xpos, ypos, b_width, b_height, (CFBWindow::color_t)bgcolor, RADIUS_LARGE);
		m_window->paintIcon(Buttons[i].icon, xpos + ((b_height - ih) / 2), ypos + ((b_height - ih) / 2), ih);
		m_window->RenderString(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], xpos + iw + 17, ypos + fh + ((b_height - fh) / 2), 
			               b_width - (iw + 21), Buttons[i].text, (CFBWindow::color_t)color, 0, true);
		xpos += b_width + ButtonDistance;
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

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd( timeout );

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
			 (showbuttons & (mbCancel | mbBack | mbOk)))
		{
			result = (showbuttons & mbCancel) ? mbrCancel : (showbuttons & mbOk) ? mbrOk: mbrBack;
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
			{
				scroll_up();
				paintButtons();
			}
			else
			{
				scroll_down();
				paintButtons();
			}
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

int ShowMsgUTF(const neutrino_locale_t Caption, const char * const Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon, const int Width, const int timeout, bool returnDefaultOnTimeout)
{
   	CMessageBox* messageBox = new CMessageBox(Caption, Text, Width, Icon, Default, ShowButtons);
	messageBox->returnDefaultValueOnTimeout(returnDefaultOnTimeout);
	messageBox->exec(timeout);
	int res = messageBox->result;
	delete messageBox;

	return res;
}

int ShowLocalizedMessage(const neutrino_locale_t Caption, const neutrino_locale_t Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon, const int Width, const int timeout, bool returnDefaultOnTimeout)
{
	return ShowMsgUTF(Caption, g_Locale->getText(Text), Default, ShowButtons, Icon, Width, timeout,returnDefaultOnTimeout);
}

int ShowMsgUTF(const neutrino_locale_t Caption, const std::string & Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon, const int Width, const int timeout, bool returnDefaultOnTimeout)
{
	return ShowMsgUTF(Caption, Text.c_str(), Default, ShowButtons, Icon, Width, timeout,returnDefaultOnTimeout);
}

void DisplayErrorMessage(const char * const ErrorMsg)
{
	ShowMsgUTF(LOCALE_MESSAGEBOX_ERROR, ErrorMsg, CMessageBox::mbrCancel, CMessageBox::mbCancel, NEUTRINO_ICON_ERROR);
}

void DisplayInfoMessage(const char * const ErrorMsg)
{
	ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, ErrorMsg, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
}
