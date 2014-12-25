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


#include "mediaplayer.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <neutrinoMessages.h>

#include <gui/audiomute.h>
#include <gui/infoclock.h>
#include <gui/movieplayer.h>
#include <gui/pictureviewer.h>
#if ENABLE_UPNP
#include <gui/upnpbrowser.h>
#endif

#include <gui/widget/icons.h>

#include <driver/screen_max.h>

#include <system/debug.h>
#include <video.h>
extern cVideo * videoDecoder;
extern CInfoClock *InfoClock;

CMediaPlayerMenu::CMediaPlayerMenu()
{
	setMenuTitel();
	setUsageMode();

	width = w_max (40, 10); //%
	
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

	if (parent)
		parent->hide();
	
	CAudioMute *audiomute = CAudioMute::getInstance();
	if (actionKey == "audioplayer")
	{
		if (audioPlayer == NULL)
			audioPlayer = new CAudioPlayerGui();
		int res = audioPlayer->exec(NULL, "init");
		return res /*menu_return::RETURN_REPAINT*/;
	}
	else if	(actionKey == "inetplayer")
	{
		if (inetPlayer == NULL)
			inetPlayer = new CAudioPlayerGui(true);
		int res = inetPlayer->exec(NULL, "init");
		return res; //menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "movieplayer")
	{
		audiomute->enableMuteIcon(false);
		InfoClock->enableInfoClock(false);
		int mode = CNeutrinoApp::getInstance()->getMode();
		if( mode == NeutrinoMessages::mode_radio )
			videoDecoder->StopPicture();
		int res = CMoviePlayerGui::getInstance().exec(NULL, "tsmoviebrowser");
		if( mode == NeutrinoMessages::mode_radio )
			videoDecoder->ShowPicture(DATADIR "/neutrino/icons/radiomode.jpg");
		audiomute->enableMuteIcon(true);
		InfoClock->enableInfoClock(true);
		return res;
	}
	
	int res = initMenuMedia();
	
	return res;
}


//show selectable mediaplayer items
int CMediaPlayerMenu::initMenuMedia(CMenuWidget *m, CPersonalizeGui *p)
{	
	CPersonalizeGui *personalize = p;
	CMenuWidget 	*media = m;
	
	bool show = (personalize == NULL || media == NULL);

	if (personalize == NULL)
		 personalize = new CPersonalizeGui();
	
	if (media == NULL)
		 media = new CMenuWidget(menu_title, NEUTRINO_ICON_MULTIMEDIA, width, MN_WIDGET_ID_MEDIA);

	personalize->addWidget(media);
	personalize->addIntroItems(media);
	
	CMenuForwarder *fw_audio = NULL;
	CMenuForwarder *fw_inet = NULL;
	CMenuForwarder *fw_mp = NULL;
	CMenuForwarder *fw_pviewer = NULL;
	CPictureViewerGui *pictureviewergui = NULL;
#if ENABLE_UPNP
	static CUpnpBrowserGui *upnpbrowsergui = NULL;
	CMenuForwarder *fw_upnp = NULL;
#endif
	CMenuWidget *moviePlayer = NULL;

	if (usage_mode != MODE_VIDEO)
	{
		//audio player
		neutrino_msg_t audio_rc = usage_mode == MODE_AUDIO ? CRCInput::RC_audio : g_settings.easymenu ? CRCInput::RC_green : CRCInput::RC_red;
		fw_audio = new CMenuForwarder(LOCALE_MAINMENU_AUDIOPLAYER, true, NULL, this, "audioplayer", audio_rc);
		fw_audio->setHint(NEUTRINO_ICON_HINT_APLAY, LOCALE_MENU_HINT_APLAY);

		//internet player
		neutrino_msg_t inet_rc = usage_mode == MODE_AUDIO ? CRCInput::RC_www : g_settings.easymenu ? CRCInput::RC_blue : CRCInput::RC_green;
		fw_inet = new CMenuForwarder(LOCALE_INETRADIO_NAME, true, NULL, this, "inetplayer", inet_rc);
		fw_inet->setHint(NEUTRINO_ICON_HINT_INET_RADIO, LOCALE_MENU_HINT_INET_RADIO);
	}

	if (usage_mode == MODE_DEFAULT)
	{
		//movieplayer
		if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) {
			moviePlayer = new CMenuWidget(LOCALE_MAINMENU_MOVIEPLAYER, NEUTRINO_ICON_MULTIMEDIA, width, MN_WIDGET_ID_MEDIA_MOVIEPLAYER);
			personalize->addWidget(moviePlayer);
			if (g_settings.easymenu)
				fw_mp = new CMenuForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, moviePlayer, NULL, CRCInput::RC_red);
			else
				fw_mp = new CMenuForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, moviePlayer, NULL, CRCInput::RC_yellow);
			fw_mp->setHint(NEUTRINO_ICON_HINT_MOVIE, LOCALE_MENU_HINT_MOVIE);
		}

 		//pictureviewer
		pictureviewergui = new CPictureViewerGui();
		if (g_settings.easymenu)
			fw_pviewer = new CMenuForwarder(LOCALE_MAINMENU_PICTUREVIEWER, true, NULL, pictureviewergui, NULL, CRCInput::RC_1);
		else
			fw_pviewer = new CMenuForwarder(LOCALE_MAINMENU_PICTUREVIEWER, true, NULL, pictureviewergui, NULL, CRCInput::RC_blue);
		fw_pviewer->setHint(NEUTRINO_ICON_HINT_PICVIEW, LOCALE_MENU_HINT_PICVIEW);
#if ENABLE_UPNP
		//upnp browser
		if (!g_settings.easymenu) {
			if (!upnpbrowsergui)
				upnpbrowsergui = new CUpnpBrowserGui();
			fw_upnp = new CMenuForwarder(LOCALE_UPNPBROWSER_HEAD, true, NULL, upnpbrowsergui, NULL, CRCInput::RC_0);
			fw_upnp->setHint(NEUTRINO_ICON_HINT_A_PIC, LOCALE_MENU_HINT_UPNP);
		}
#endif
//  		media->addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, usage_mode == MODE_AUDIO ? CMenuWidget::BTN_TYPE_CANCEL : CMenuWidget::BTN_TYPE_BACK);
	}

	if (usage_mode == MODE_AUDIO)
	{
 		//audio player	
		personalize->addItem(media, fw_audio, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_AUDIO]);
 		
 		//internet player
		personalize->addItem(media, fw_inet, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_INETPLAY]);
	}
	else if (usage_mode == MODE_VIDEO)
	{
 		showMoviePlayer(media, personalize);
	}
	else
	{
		if (g_settings.easymenu) {
			//movieplayer
			if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) {
				showMoviePlayer(moviePlayer,  personalize);
				personalize->addItem(media, fw_mp, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_MPLAYER]);
			}
			//audio player
			personalize->addItem(media, fw_audio, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_AUDIO]);
			if (g_settings.easymenu) {
				CMenuForwarder *fw_yt = new CMenuForwarder(LOCALE_MOVIEPLAYER_YTPLAYBACK, true, NULL, &CMoviePlayerGui::getInstance(), "ytplayback", CRCInput::RC_yellow);
				fw_yt->setHint(NEUTRINO_ICON_HINT_YTPLAY, LOCALE_MENU_HINT_YTPLAY);
				personalize->addItem(media, fw_yt, &g_settings.personalize[SNeutrinoSettings::P_MPLAYER_YTPLAY]);
			}
			//internet player
			personalize->addItem(media, fw_inet, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_INETPLAY]);
			//picture viewer
			personalize->addItem(media, fw_pviewer, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_PVIEWER]);
		} else {
			//audio player
			personalize->addItem(media, fw_audio, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_AUDIO]);

			//internet player
			personalize->addItem(media, fw_inet, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_INETPLAY]);

			//movieplayer
			if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) {
				showMoviePlayer(moviePlayer,  personalize);
				personalize->addItem(media, fw_mp, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_MPLAYER], false, CPersonalizeGui::PERSONALIZE_SHOW_AS_ACCESS_OPTION);
			}

			//picture viewer
			personalize->addItem(media, fw_pviewer, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_PVIEWER]);
#if ENABLE_UPNP		
			//upnp browser
			personalize->addItem(media, fw_upnp, &g_settings.personalize[SNeutrinoSettings::P_MEDIA_UPNP]);
#endif
		}
	}
	
	int res = menu_return::RETURN_NONE;
	
	if (show)
	{
 		//adding personalized items
		personalize->addPersonalizedItems();
		
		//add I_TYPE_MULTIMEDIA plugins
		CPluginsExec pluginsExec;
		unsigned int nextShortcut = (unsigned int)media->getNextShortcut();
		media->integratePlugins(&pluginsExec, CPlugins::I_TYPE_MULTIMEDIA, nextShortcut);

		res = media->exec(NULL, "");
		delete media;
		delete moviePlayer;
		delete personalize;
		delete pictureviewergui;
#if ENABLE_UPNP
		//delete upnpbrowsergui;
#endif

		setUsageMode();//set default usage_mode
	}
	return res;
}

//show movieplayer submenu with selectable items for moviebrowser or filebrowser
void CMediaPlayerMenu::showMoviePlayer(CMenuWidget *moviePlayer, CPersonalizeGui *p)
{ 
	CMenuForwarder *fw_mbrowser = new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, this, "movieplayer", CRCInput::RC_red);
	fw_mbrowser->setHint(NEUTRINO_ICON_HINT_MB, LOCALE_MENU_HINT_MB);
	CMenuForwarder *fw_file = new CMenuForwarder(LOCALE_MOVIEPLAYER_FILEPLAYBACK, true, NULL, &CMoviePlayerGui::getInstance(), "fileplayback", CRCInput::RC_green);
	fw_file->setHint(NEUTRINO_ICON_HINT_FILEPLAY, LOCALE_MENU_HINT_FILEPLAY);
	
	p->addIntroItems(moviePlayer);
	
	//moviebrowser
	p->addItem(moviePlayer, fw_mbrowser, &g_settings.personalize[SNeutrinoSettings::P_MPLAYER_MBROWSER]);
	
	//fileplayback
	p->addItem(moviePlayer, fw_file, &g_settings.personalize[SNeutrinoSettings::P_MPLAYER_FILEPLAY]);
	//ytplayback
	if (!g_settings.easymenu) {
		CMenuForwarder *fw_yt = new CMenuForwarder(LOCALE_MOVIEPLAYER_YTPLAYBACK, true, NULL, &CMoviePlayerGui::getInstance(), "ytplayback", CRCInput::RC_yellow);
		fw_yt->setHint(NEUTRINO_ICON_HINT_YTPLAY, LOCALE_MENU_HINT_YTPLAY);
		p->addItem(moviePlayer, fw_yt, &g_settings.personalize[SNeutrinoSettings::P_MPLAYER_YTPLAY]);
	}

// #if 0
// 	//moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_PESPLAYBACK, true, NULL, moviePlayerGui, "pesplayback"));
// 	//moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_TSPLAYBACK_PC, true, NULL, moviePlayerGui, "tsplayback_pc"));
// 	moviePlayer->addItem(new CLockedMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "tsmoviebrowser"));
// 	moviePlayer->addItem(new CLockedMenuForwarder(LOCALE_MOVIEPLAYER_TSPLAYBACK, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "tsplayback", CRCInput::RC_green));
// 
// 	moviePlayer->addItem(new CLockedMenuForwarder(LOCALE_MOVIEPLAYER_BOOKMARK, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "bookmarkplayback"));
// 	moviePlayer->addItem(GenericMenuSeparator);
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_FILEPLAYBACK, true, NULL, moviePlayerGui, "fileplayback", CRCInput::RC_red));
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_DVDPLAYBACK, true, NULL, moviePlayerGui, "dvdplayback", CRCInput::RC_yellow));
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_VCDPLAYBACK, true, NULL, moviePlayerGui, "vcdplayback", CRCInput::RC_blue));
// 	moviePlayer->addItem(GenericMenuSeparatorLine);
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, &streamingSettings, NULL, CRCInput::RC_help));
// 	moviePlayer->addItem(new CMenuForwarder(LOCALE_NFSMENU_HEAD, true, NULL, new CNFSSmallMenu(), NULL, CRCInput::RC_setup));
// #endif
}
