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


#ifndef __messagebox__
#define __messagebox__

#include <gui/widget/hintboxext.h>

#include <stdint.h>
#include <string>

#define MaxButtons 3

class CMessageBox : public CHintBoxExt
{
 private:
	struct mbButtons
	{
		bool def;
		const char* icon;
		const char* text;
	};
	struct mbButtons Buttons[MaxButtons];

	int  showbuttons;
	bool returnDefaultOnTimeout;
	int  mbBtnAlign;
	int  ButtonSpacing, ButtonDistance;
	int  fh, i_maxw;
	int  b_height, b_width, bb_width;
	int ButtonCount;

	void paintButtons();
	int getButtonWidth();

 public:
	enum result_
		{
			mbrYes    = 0,
			mbrNo     = 1,
			mbrCancel = 2,
			mbrBack   = 3,
			mbrOk     = 4
		} result;
	
	enum buttons_
		{
			mbYes             = 0x01,
			mbNo              = 0x02,
			mbCancel          = 0x04,
			mbAll             = 0x07,
			mbBack            = 0x08,
			mbOk              = 0x10,
			mbBtnAlignCenter1 = 0x0100, /* centered, large distances */
			mbBtnAlignCenter2 = 0x0200, /* centered, small distances */
			mbBtnAlignLeft    = 0x0400,
			mbBtnAlignRight   = 0x0800
		} buttons;
	
	// Text & Caption are always UTF-8 encoded
	CMessageBox(const neutrino_locale_t Caption, const char * const Text, const int Width = 500, const char * const Icon = NULL, const CMessageBox::result_ &Default = mbrYes, const uint32_t ShowButtons = mbAll);
	CMessageBox(const std::string &Caption, const char * const Text, const int Width = 500, const char * const Icon = NULL, const CMessageBox::result_ &Default = mbrYes, const uint32_t ShowButtons = mbAll);
	CMessageBox(const neutrino_locale_t Caption, ContentLines& Lines, const int Width = 500, const char * const Icon = NULL, const CMessageBox::result_ &Default = mbrYes, const uint32_t ShowButtons = mbAll);
	CMessageBox(const std::string &Caption, ContentLines& Lines, const int Width = 500, const char * const Icon = NULL, const CMessageBox::result_ &Default = mbrYes, const uint32_t ShowButtons = mbAll);

	int exec(int timeout = -1);
	void returnDefaultValueOnTimeout(bool returnDefault);

 private:
	void Init(const CMessageBox::result_ &Default, const uint32_t ShowButtons);
};

// Text is always UTF-8 encoded
int ShowMsg(const neutrino_locale_t Caption, const char * const Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon = NULL, const int Width = 450, const int timeout = -1, bool returnDefaultOnTimeout = false); // UTF-8
int ShowMsg(const neutrino_locale_t Caption, const std::string & Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon = NULL, const int Width = 450, const int timeout = -1, bool returnDefaultOnTimeout = false); // UTF-8
int ShowMsg(const neutrino_locale_t Caption, const neutrino_locale_t Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon = NULL, const int Width = 450, const int timeout = -1, bool returnDefaultOnTimeout = false); // UTF-8
int ShowMsg(const std::string &Caption, const char * const Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon = NULL, const int Width = 450, const int timeout = -1, bool returnDefaultOnTimeout = false); // UTF-8
int ShowMsg(const std::string &Caption, const std::string & Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon = NULL, const int Width = 450, const int timeout = -1, bool returnDefaultOnTimeout = false); // UTF-8
int ShowMsg(const std::string &Caption, const neutrino_locale_t Text, const CMessageBox::result_ &Default, const uint32_t ShowButtons, const char * const Icon = NULL, const int Width = 450, const int timeout = -1, bool returnDefaultOnTimeout = false); // UTF-8

void DisplayErrorMessage(const char * const ErrorMsg); // UTF-8
void DisplayInfoMessage(const char * const InfoMsg); // UTF-8

#endif
