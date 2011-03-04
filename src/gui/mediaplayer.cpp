/*
	Mediaplayer selection menu - Neutrino-GUI

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


#include "gui/mediaplayer.h"

#include <global.h>
#include <neutrino.h>


#include "gui/movieplayer.h"
#include "gui/pictureviewer.h"
#include "gui/upnpbrowser.h"

#include <gui/widget/icons.h>

#include <driver/screen_max.h>

#include <system/debug.h>


CMediaPlayerMenu::CMediaPlayerMenu()
{
	setMenuTitel();
	setUsageMode();

	width = w_max (40, 10); //%
	selected = -1;
	
	audioPlayer 	= NULL;
	inetPlayer 	= NULL;
}

CMediaPlayerMenu* CMediaPlayerMenu::getInstance()
{
	static CMediaPlayerMenu* mpm = NULL;

	if(!mpm) {
		mpm = new CMediaPlayerMenu();
		printf("[neutrino] mediaplayer menu instance created\n");
	}
	return mpm;
}

CMediaPlayerMenu::~CMediaPlayerMenu()
{
	delete audioPlayer ;
	delete inetPlayer ;
}

int CMediaPlayerMenu::exec(CMenuTarget* parent, const std::string &actionKey)
{
	printf("init mediaplayer menu in usage mode %d\n", usage_mode);
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();
	
	if (actionKey == "audioplayer")
	{
		if (audioPlayer == NULL)
			audioPlayer = new CAudioPlayerGui();
		audioPlayer->exec(NULL, "init");
		
		return res;
	}
	else if	(actionKey == "inetplayer")
	{
		if (inetPlayer == NULL)
			inetPlayer = new CAudioPlayerGui(true);
		inetPlayer->exec(NULL, "init");
		
		return res;
	}
	
	showMenu(); 
	
	return res;
}

//show selectable mediaplayer items
void CMediaPlayerMenu::showMenu()
{
	CMenuWidget *media = new CMenuWidget(menu_title, NEUTRINO_ICON_MULTIMEDIA, width);

	CMenuForwarder *fw_audio = NULL;
	CMenuForwarder *fw_inet = NULL;
	CMenuForwarder *fw_mp = NULL;
	CMenuForwarder *fw_pviewer = NULL;
	CMenuForwarder *fw_upnp = NULL;
	CMenuWidget *moviePlayer = NULL;

	if (usage_mode != MODE_VIDEO)
	{
		//menue init
		media->setSelected(selected);

		//audio player
		neutrino_msg_t audio_rc = usage_mode == MODE_AUDIO ? CRCInput::RC_audio:CRCInput::RC_red;
		const char* audio_btn = usage_mode == MODE_AUDIO ? "" : NEUTRINO_ICON_BUTTON_RED;
		fw_audio = new CMenuForwarder(LOCALE_MAINMENU_AUDIOPLAYER, true, NULL, this, "audioplayer", audio_rc, audio_btn);
		//internet player
		neutrino_msg_t inet_rc = usage_mode == MODE_AUDIO ? CRCInput::RC_www : CRCInput::RC_green;
		const char* inet_btn = usage_mode == MODE_AUDIO ? "" : NEUTRINO_ICON_BUTTON_GREEN;
		fw_inet = new CMenuForwarder(LOCALE_INETRADIO_NAME, true, NULL, this, "inetplayer", inet_rc, inet_btn);
	}

	if (usage_mode == MODE_DEFAULT)
	{
		//movieplayer
		moviePlayer = new CMenuWidget(LOCALE_MAINMENU_MEDIA, NEUTRINO_ICON_MULTIMEDIA, width);
		fw_mp = new CMenuForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, moviePlayer, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);

		//pictureviewer
		fw_pviewer = new CMenuForwarder(LOCALE_MAINMENU_PICTUREVIEWER, true, NULL, new CPictureViewerGui(), NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);

		//upnp browser
		fw_upnp = new CMenuForwarder(LOCALE_UPNPBROWSER_HEAD, true, NULL, new CUpnpBrowserGui(), NULL, CRCInput::RC_0, NEUTRINO_ICON_BUTTON_0);

		media->addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, usage_mode == MODE_AUDIO ? CMenuWidget::BTN_TYPE_CANCEL : CMenuWidget::BTN_TYPE_BACK);
	}

	if (usage_mode == MODE_AUDIO)
	{
		//audio player	
		media->addItem(fw_audio);
		
		//internet player
		media->addItem(fw_inet);
	}
	else if (usage_mode == MODE_VIDEO)
	{
		showMoviePlayer(media);
	}
	else
	{
		//audio player
		media->addItem(fw_audio);
		
		//internet player
		media->addItem(fw_inet);
		
		//movieplayer
		showMoviePlayer(moviePlayer);
		media->addItem(fw_mp);
		
		//pictureviewer
		media->addItem(fw_pviewer);
		
		//upnp browser
		media->addItem(fw_upnp);
	}
	
	media->exec(NULL, "");
	media->hide();
	selected = media->getSelected();
	delete media;
	setUsageMode();//set default usage_mode
}

//show movieplayer submenu with selectable items for moviebrowser or filebrowser
void CMediaPlayerMenu::showMoviePlayer(CMenuWidget *moviePlayer)
{ 
	CMoviePlayerGui *movieplayer_gui = &CMoviePlayerGui::getInstance();

	moviePlayer->addIntroItems(LOCALE_MAINMENU_MOVIEPLAYER);
	moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, movieplayer_gui, "tsmoviebrowser", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_FILEPLAYBACK, true, NULL, movieplayer_gui, "fileplayback", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

// #if 0
// 	//moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_PESPLAYBACK, true, NULL, moviePlayerGui, "pesplayback"));
// 	//moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_TSPLAYBACK_PC, true, NULL, moviePlayerGui, "tsplayback_pc"));
// 	moviePlayer->addItem(new CLockedMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "tsmoviebrowser"));
// 	moviePlayer->addItem(new CLockedMenuForwarder(LOCALE_MOVIEPLAYER_TSPLAYBACK, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "tsplayback", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
// 
// 	moviePlayer->addItem(new CLockedMenuForwarder(LOCALE_MOVIEPLAYER_BOOKMARK, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "bookmarkplayback"));
// 	moviePlayer->addItem(GenericMenuSeparator);
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_FILEPLAYBACK, true, NULL, moviePlayerGui, "fileplayback", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_DVDPLAYBACK, true, NULL, moviePlayerGui, "dvdplayback", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_VCDPLAYBACK, true, NULL, moviePlayerGui, "vcdplayback", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
// 	moviePlayer->addItem(GenericMenuSeparatorLine);
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, &streamingSettings, NULL, CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL));
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_NFSMENU_HEAD, true, NULL, new CNFSSmallMenu(), NULL, CRCInput::RC_setup, NEUTRINO_ICON_BUTTON_DBOX_SMALL));
// #endif
}
