/*
	$port: audioplayer_setup.cpp,v 1.4 2010/12/06 21:00:15 tuxbox-cvs Exp $

	audioplayer setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/


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


#include "audioplayer_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>

#include <gui/audioplayer.h>
#include <gui/filebrowser.h>

#include <driver/screen_max.h>

#include <system/debug.h>


CAudioPlayerSetup::CAudioPlayerSetup()
{
	width = w_max (40, 10);
}

CAudioPlayerSetup::~CAudioPlayerSetup()
{

}

int CAudioPlayerSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init audioplayer setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();


	if(actionKey == "audioplayerdir")
	{
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.network_nfs_audioplayerdir.c_str()))
			g_settings.network_nfs_audioplayerdir = b.getSelectedFile()->Name;
		return res;
	}

	res = showAudioPlayerSetup();

	return res;
}


#define AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT 2
const CMenuOptionChooser::keyval AUDIOPLAYER_DISPLAY_ORDER_OPTIONS[AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT] =
{
	{ CAudioPlayerGui::ARTIST_TITLE, LOCALE_AUDIOPLAYER_ARTIST_TITLE },
	{ CAudioPlayerGui::TITLE_ARTIST, LOCALE_AUDIOPLAYER_TITLE_ARTIST }
};


/*shows the audio setup menue*/
int CAudioPlayerSetup::showAudioPlayerSetup()
{
	CMenuOptionChooser * mc;
	CMenuForwarder * mf;

	CMenuWidget* audioplayerSetup = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_AUDIOSETUP);

	audioplayerSetup->addIntroItems(LOCALE_AUDIOPLAYER_NAME);

	// display order
	mc = new CMenuOptionChooser(LOCALE_AUDIOPLAYER_DISPLAY_ORDER, &g_settings.audioplayer_display, AUDIOPLAYER_DISPLAY_ORDER_OPTIONS, AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_ORDER);
	audioplayerSetup->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_AUDIOPLAYER_FOLLOW, &g_settings.audioplayer_follow, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_FOLLOW);
	audioplayerSetup->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SELECT_TITLE_BY_NAME, &g_settings.audioplayer_select_title_by_name, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true );
	mc->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_TITLE);
	audioplayerSetup->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_AUDIOPLAYER_REPEAT_ON, &g_settings.audioplayer_repeat_on, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true );
	mc->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_REPEAT);
	audioplayerSetup->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SHOW_PLAYLIST, &g_settings.audioplayer_show_playlist, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_PLAYLIST);
	audioplayerSetup->addItem(mc);

	CMenuOptionNumberChooser *cc = new CMenuOptionNumberChooser(LOCALE_AUDIOPLAYER_SCREENSAVER_TIMEOUT, &g_settings.audioplayer_screensaver, true, 0, 999, NULL, 0, 0, LOCALE_OPTIONS_OFF);
	cc->setNumberFormat(std::string("%d ") + g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE));
	cc->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_SCREENSAVER);
	audioplayerSetup->addItem(cc);

	mc = new CMenuOptionChooser(LOCALE_AUDIOPLAYER_HIGHPRIO, &g_settings.audioplayer_highprio, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true );
	mc->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_HIGHPRIO);
	audioplayerSetup->addItem(mc);
#if 0
	if (CVFD::getInstance()->has_lcd) //FIXME
		audioplayerSetup->addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SPECTRUM     , &g_settings.spectrum    , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
#endif
	mf = new CMenuForwarder(LOCALE_AUDIOPLAYER_DEFDIR, true, g_settings.network_nfs_audioplayerdir, this, "audioplayerdir");
	mf->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_DEFDIR);
	audioplayerSetup->addItem(mf);

	mc = new CMenuOptionChooser(LOCALE_AUDIOPLAYER_ENABLE_SC_METADATA, &g_settings.audioplayer_enable_sc_metadata, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_AUDIOPLAYER_SC_METADATA);
	audioplayerSetup->addItem(mc);

	int res = audioplayerSetup->exec (NULL, "");
	delete audioplayerSetup;
	return res;
}
