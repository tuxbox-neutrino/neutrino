/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.

	***********************************************************
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _PVIEWER_HELP_H_
#define _PVIEWER_HELP_H_


#include <gui/widget/helpbox.h>

// Class to show Pictureviewer help, to be used by menu
class CPictureViewerHelp : public CMenuTarget
{
	private:
		bool audioplayer;

	public:
		CPictureViewerHelp(bool enable_audioplayer_help)
		{
			audioplayer = enable_audioplayer_help;
		}
		~CPictureViewerHelp(){};

		int exec(CMenuTarget* /*parent*/, const std::string & /*actionKey*/)
		{
			Helpbox helpbox(g_Locale->getText(LOCALE_HELP_BOX_TITLE));

			helpbox.addLine(g_Locale->getText(LOCALE_PICTUREVIEWER_HELP1), CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH, 35, 10, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]);
			helpbox.addLine(NEUTRINO_ICON_BUTTON_OKAY, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP2));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_5, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP3));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_0, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP4));
			helpbox.addSeparatorLine();
			helpbox.addLine(g_Locale->getText(LOCALE_PICTUREVIEWER_HELP5), CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH, 35, 10, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]);
			helpbox.addLine(NEUTRINO_ICON_BUTTON_LEFT, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP6));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_RIGHT, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP7));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_5, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP3));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_HOME, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP8));
			helpbox.addPagebreak();
			helpbox.addLine(g_Locale->getText(LOCALE_PICTUREVIEWER_HELP9), CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH, 35, 10, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]);
			helpbox.addLine(NEUTRINO_ICON_BUTTON_OKAY, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP10));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_LEFT, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP11));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_RIGHT, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP12));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_1, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP13));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_3, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP14));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_2, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP15));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_4, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP16));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_6, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP17));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_8, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP18));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_5, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP3));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_0, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP19));
			helpbox.addLine(NEUTRINO_ICON_BUTTON_HOME, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP8));

			if(audioplayer){
				helpbox.addPagebreak();
				helpbox.addLine(g_Locale->getText(LOCALE_PICTUREVIEWER_HELP30));
				helpbox.addLine(NEUTRINO_ICON_BUTTON_PLAY, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP31));
				helpbox.addLine(NEUTRINO_ICON_BUTTON_PAUSE, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP32));
				helpbox.addLine(NEUTRINO_ICON_BUTTON_STOP, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP33));
				helpbox.addLine(NEUTRINO_ICON_BUTTON_FORWARD, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP34));
				helpbox.addLine(NEUTRINO_ICON_BUTTON_BACKWARD, g_Locale->getText(LOCALE_PICTUREVIEWER_HELP35));
			}

			helpbox.addExitKey(CRCInput::RC_ok);

			helpbox.show();
 			int ret = helpbox.exec();
			helpbox.hide();
			
			return ret;
		}
};

#endif /*_PVIEWER_HELP_H_*/

