/*
	$port: mediaplayer_setup.cpp,v 1.5 2010/12/06 21:00:15 tuxbox-cvs Exp $

	Neutrino-GUI  -   DBoxII-Project

	media player setup implementation - Neutrino-GUI

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

#include "mediaplayer_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>

#include <gui/audioplayer_setup.h>
#include <gui/pictureviewer_setup.h>
#include <gui/webtv_setup.h>
#include <gui/xmltv_setup.h>
#include <gui/moviebrowser/mb.h>

#include <driver/screen_max.h>

#include <system/debug.h>

CMediaPlayerSetup::CMediaPlayerSetup()
{
	width = 40;
	selected = -1;
}

CMediaPlayerSetup::~CMediaPlayerSetup()
{
}

int CMediaPlayerSetup::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init mediaplayer setup menu\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();


	res = showMediaPlayerSetup();
	
	return res;
}

/*shows media setup menue entries*/
int CMediaPlayerSetup::showMediaPlayerSetup()
{
	CMenuWidget* mediaSetup = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width);
	mediaSetup->setSelected(selected);

	// intros
	mediaSetup->addIntroItems(LOCALE_MAINMENU_MEDIA);

	CMenuForwarder *mf;

	CWebTVSetup wsetup;
	mf = new CMenuForwarder(LOCALE_WEBTV_HEAD, true, NULL, &wsetup, "webtv_menu", CRCInput::RC_red);
	mf->setHint(NEUTRINO_ICON_HINT_WEBTV, LOCALE_MENU_HINT_WEBTV_SETUP);
	mediaSetup->addItem(mf);

	mf = new CMenuForwarder(LOCALE_WEBRADIO_HEAD, true, NULL, &wsetup, "webradio_menu", CRCInput::RC_green);
	mf->setHint(NEUTRINO_ICON_HINT_WEBTV /*FIXME*/, LOCALE_MENU_HINT_WEBRADIO_SETUP);
	mediaSetup->addItem(mf);

	CXMLTVSetup xmltvsetup;
	mf = new CMenuForwarder(LOCALE_XMLTV_HEAD, true, NULL, &xmltvsetup, "show_menu", CRCInput::RC_yellow);
	mf->setHint(NEUTRINO_ICON_HINT_XMLTV, LOCALE_MENU_HINT_XMLTV_SETUP);
	mediaSetup->addItem(mf);

	mediaSetup->addItem(GenericMenuSeparatorLine);

	int shortcut = 1;

	CAudioPlayerSetup asetup;
	mf = new CMenuForwarder(LOCALE_AUDIOPLAYER_INTERNETRADIO_NAME, true, NULL, &asetup, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_APLAY, LOCALE_MENU_HINT_APLAY_SETUP);
	mediaSetup->addItem(mf);

	CPictureViewerSetup psetup;
	mf = new CMenuForwarder(LOCALE_PICTUREVIEWER_HEAD, true, NULL, &psetup, "", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_PICVIEW, LOCALE_MENU_HINT_PICTUREVIEWER_SETUP);
	mediaSetup->addItem(mf);

	mediaSetup->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_MOVIEPLAYER));

	CMovieBrowser msetup;
	mf = new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, &msetup, "show_menu", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_MB, LOCALE_MENU_HINT_MOVIEBROWSER_SETUP);
	mediaSetup->addItem(mf);

	mf = new CMenuForwarder(LOCALE_MOVIEPLAYER_YTPLAYBACK, true, NULL, &msetup, "show_ytmenu", CRCInput::convertDigitToKey(shortcut++));
	mf->setHint(NEUTRINO_ICON_HINT_YTPLAY, LOCALE_MENU_HINT_YTPLAY_SETUP);
	mediaSetup->addItem(mf);

	mediaSetup->addItem(GenericMenuSeparator);

	CMenuOptionChooser *mc;
	mc = new CMenuOptionChooser(LOCALE_MOVIEPLAYER_DISPLAY_PLAYTIME, &g_settings.movieplayer_display_playtime, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_info.hw_caps->display_xres >= 8);
	mc->setHint("", LOCALE_MENU_HINT_MOVIEPLAYER_DISPLAY_PLAYTIME);
	mediaSetup->addItem(mc);

	int res = mediaSetup->exec (NULL, "");
	selected = mediaSetup->getSelected();
	delete mediaSetup;
	return res;
}
