/*
	QuadPiP setup implementation - Neutrino-GUI

	Copyright (C) 2021 Mike Bremer (BPanther)
	Homepage: https://forum.mbremer.de

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

#include "quadpip_setup.h"

#include <global.h>
#include <mymenu.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <driver/fontrenderer.h>

#include <zapit/capmt.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>

#include <hardware/audio.h>
#include <hardware/video.h>

#include <system/debug.h>

extern cVideo * videoDecoder;		// VIDEO0
extern cAudio * audioDecoder;		// AUDIO0
extern cDemux * videoDemux;		// VIDEO DEMUX0
extern cDemux * audioDemux;		// AUDIO DEMUX0

extern cVideo * pipVideoDecoder[3];	// VIDEO 1..3
extern cAudio * pipAudioDecoder[3];	// AUDIO 1..3
extern cDemux * pipVideoDemux[3];	// VIDEO DEMUX 1..3
extern cDemux * pipAudioDemux[3];	// AUDIO DEMUX 1..3

extern CBouquetManager * g_bouquetManager;
CComponentsShapeSquare * quad_win = NULL;
int quadpip = 0;
unsigned int pip_devs = 0;
int wn = 0;
int aw = 0;
int fb_w;
int fb_h;

CQuadPiPSetup::CQuadPiPSetup()
{
	pip_devs = g_info.hw_caps->pip_devs + 1;
	fb_w = CFrameBuffer::getInstance()->getScreenWidth(true) / 2;
	fb_h = CFrameBuffer::getInstance()->getScreenHeight(true) / 2;
}

CQuadPiPSetup::~CQuadPiPSetup()
{
}

int CQuadPiPSetup::exec(CMenuTarget* parent, const std::string & actionKey)
{
	dprintf(DEBUG_DEBUG, "init quadpip setup\n");
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	res = showQuadPiPSetup();
	return res;
}

/*shows the QuadPiP setup menue*/
int CQuadPiPSetup::showQuadPiPSetup()
{
	CMenuWidget * quadpipsetup = new CMenuWidget(LOCALE_QUADPIP, NEUTRINO_ICON_SETTINGS);

	// menu head
	quadpipsetup->addItem(GenericMenuSeparator);
	quadpipsetup->addItem(GenericMenuBack);
	quadpipsetup->addItem(GenericMenuSeparatorLine);

	// quadpip on/off
	quadpipsetup->addItem(new CMenuOptionChooser(LOCALE_QUADPIP, &quadpip, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, new CQuadPiPSetupNotifier(), CRCInput::RC_green));
	quadpipsetup->addItem(GenericMenuSeparatorLine);

	// quadpip window channel settings
	CQuadPiPSetupSelectChannelWidget select;
	quadpipsetup->addItem(new CMenuForwarder(LOCALE_QUADPIP_1, true, g_settings.quadpip_channel_window[0], &select, "window_1", CRCInput::RC_1));
	if (pip_devs >= 2) quadpipsetup->addItem(new CMenuForwarder(LOCALE_QUADPIP_2, true, g_settings.quadpip_channel_window[1], &select, "window_2", CRCInput::RC_2));
	if (pip_devs >= 3) quadpipsetup->addItem(new CMenuForwarder(LOCALE_QUADPIP_3, true, g_settings.quadpip_channel_window[2], &select, "window_3", CRCInput::RC_3));
	if (pip_devs >= 4) quadpipsetup->addItem(new CMenuForwarder(LOCALE_QUADPIP_4, true, g_settings.quadpip_channel_window[3], &select, "window_4", CRCInput::RC_4));

	// reset all channel settings
	quadpipsetup->addItem(GenericMenuSeparatorLine);
	quadpipsetup->addItem(new CMenuForwarder(LOCALE_RESET_CHANNELS, true, NULL, &select, "reset", CRCInput::RC_red));

	// select channel
	quadpipsetup->addItem(GenericMenuSeparatorLine);
	quadpipsetup->addItem(new CMenuForwarder(LOCALE_FONTSIZE_CHANNEL_NUM_ZAP, true, NULL, &select, "select_window", CRCInput::RC_blue));

	int res = quadpipsetup->exec(NULL, "");

	if (quad_win != NULL && quad_win->isPainted())
			quad_win->kill();

	delete quadpipsetup;
	return res;
}

bool CQuadPiPSetupNotifier::changeNotify(const neutrino_locale_t, void * Data)
{
	if (quadpip)
	{
		videoDecoder->QuadPiP(true);
		for (unsigned i=0; i < pip_devs; i++)
		{
			usleep (200);	// delay time for zap etc.
			if (i == 0)
			{
				CNeutrinoApp::getInstance()->channelList->zapTo_ChannelID(g_settings.quadpip_channel_id_window[i]);
				CCamManager::getInstance()->Start(CZapit::getInstance()->GetCurrentChannel()->getChannelID(), CCamManager::PLAY);
			}
			if (i >= 1 && g_settings.quadpip_channel_id_window[i] != 0)
			{
				CZapit::getInstance()->StartPip(g_settings.quadpip_channel_id_window[i], i-1);
				g_Zapit->zapTo_pip(g_settings.quadpip_channel_id_window[i], i-1);
				pipAudioDemux[i-1]->Start();
				pipAudioDecoder[i-1]->Start();
				usleep(100);			// delay for audio start/stop for audio later at window selection
				pipAudioDemux[i-1]->Stop();
				pipAudioDecoder[i-1]->Stop();

				if (i== 1)
					pipVideoDecoder[i-1]->Pig(fb_w, 0, fb_w, fb_h, CFrameBuffer::getInstance()->getScreenWidth(true), CFrameBuffer::getInstance()->getScreenHeight(true));
				else if (i== 2)
					pipVideoDecoder[i-1]->Pig(0, fb_h, fb_w, fb_h, CFrameBuffer::getInstance()->getScreenWidth(true), CFrameBuffer::getInstance()->getScreenHeight(true));
				else if (i== 3)
					pipVideoDecoder[i-1]->Pig(fb_w, fb_h, fb_w, fb_h, CFrameBuffer::getInstance()->getScreenWidth(true), CFrameBuffer::getInstance()->getScreenHeight(true));

				pipVideoDecoder[i-1]->ShowPig(1);
			}
		}
		g_Zapit->Rezap();
	}
	else
	{
		videoDecoder->QuadPiP(false);
		for (unsigned i=0; i < (unsigned int) g_info.hw_caps->pip_devs; i++)
		{
			CCamManager::getInstance()->Stop(g_settings.quadpip_channel_id_window[i], CCamManager::PIP);
			if (pipVideoDemux[i])
				pipVideoDemux[i]->Stop();
			if (pipAudioDemux[i])
				pipAudioDemux[i]->Stop();
			if (pipVideoDecoder[i])
			{
				pipVideoDecoder[i]->ShowPig(0);
				pipVideoDecoder[i]->Stop();
			}
			g_Zapit->stopPip(i);
		}
		g_Zapit->Rezap();
		aw = 0;
	}
	return true;
}

static void switchAudio(int _old, int _new)
{
	if (_old == -1)
	{
		audioDemux->Stop();
		audioDecoder->Stop();
	}
	else
	{
		if (pipAudioDemux[_old])
		{
			pipAudioDemux[_old]->Stop();
			pipAudioDecoder[_old]->Stop();
		}
	}

	if (_new == -1)
	{
		videoDecoder->SetSyncMode((AVSYNC_TYPE) g_settings.avsync);
		audioDecoder->SetSyncMode((AVSYNC_TYPE) g_settings.avsync);
		audioDemux->Start();
		audioDecoder->Start();
	}
	else
	{
		if (pipAudioDemux[_new])
		{
			pipVideoDecoder[_new]->SetSyncMode((AVSYNC_TYPE) g_settings.avsync);
			pipAudioDecoder[_new]->SetSyncMode((AVSYNC_TYPE) g_settings.avsync);
			pipAudioDemux[_new]->Start();
			pipAudioDecoder[_new]->Start();
		}
	}
}

static void paintWindowSize(int x, int y, int w, int h)
{
	if (quad_win == NULL)
	{
		quad_win = new CComponentsShapeSquare(0, 0, 0, 0);
		quad_win->setFrameThickness(OFFSET_INNER_SMALL);
		quad_win->disableShadow();
		quad_win->setColorBody(COL_BACKGROUND);
		quad_win->setColorFrame(COL_RED);
		quad_win->doPaintBg(true);
	}
	else
	{
		if (quad_win->isPainted())
			quad_win->kill();
	}

	quad_win->setXPos(x);
	quad_win->setYPos(y);
	quad_win->setWidth(w);
	quad_win->setHeight(h);

	quad_win->paint(CC_SAVE_SCREEN_NO);

	// show channel name
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x + OFFSET_INNER_SMALL * 2, y + h - OFFSET_INNER_SMALL * 2, w, g_settings.quadpip_channel_window[aw], COL_MENUCONTENT_TEXT);
}

static void paintWindow(int num)
{
	switch (num)
	{
		case 0:
			aw=0;
			paintWindowSize(0, 0, fb_w, fb_h);
			break;
		case 1:
			aw=1;
			paintWindowSize(fb_w, 0, fb_w, fb_h);
			break;
		case 2:
			aw=2;
			paintWindowSize(0, fb_h, fb_w, fb_h);
			break;
		case 3:
			aw=3;
			paintWindowSize(fb_w, fb_h, fb_w, fb_h);
			break;
	}
}

CQuadPiPSetupSelectChannelWidget::CQuadPiPSetupSelectChannelWidget()
{
}

CQuadPiPSetupSelectChannelWidget::~CQuadPiPSetupSelectChannelWidget()
{
}

int CQuadPiPSetupSelectChannelWidget::InitZapitChannelHelper(CZapitClient::channelsMode mode)
{
	std::vector<CMenuWidget *> toDelete;
	CMenuWidget mctv(LOCALE_TIMERLIST_BOUQUETSELECT, NEUTRINO_ICON_SETTINGS);
	mctv.addIntroItems();

	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++)
	{
		CMenuWidget* mwtv = new CMenuWidget(LOCALE_TIMERLIST_CHANNELSELECT, NEUTRINO_ICON_SETTINGS);
		toDelete.push_back(mwtv);
		mwtv->addIntroItems();
		ZapitChannelList channels;
		g_bouquetManager->Bouquets[i]->getTvChannels(channels);

		for(int j = 0; j < (int) channels.size(); j++)
		{
			CZapitChannel * channel = channels[j];
			if (!IS_WEBCHAN(channel->getChannelID()))	// no web channels for pip
			{
				char cChannelId[60] = {0};
				snprintf(cChannelId, sizeof(cChannelId), "ZCT:%d|%" PRIx64 "#", channel->number, channel->getChannelID());
				CMenuForwarder * chan_item = new CMenuForwarder(channel->getName(), true, NULL, this, (std::string(cChannelId) + channel->getName()).c_str(), CRCInput::RC_nokey, NULL, channel->scrambled ? NEUTRINO_ICON_SCRAMBLED: (channel->getUrl().empty() ? NULL : NEUTRINO_ICON_STREAMING));
				chan_item->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
				mwtv->addItem(chan_item);
			}
		}
		if(!channels.empty() && (!g_bouquetManager->Bouquets[i]->bHidden ))
		{
			mctv.addItem(new CMenuForwarder(g_bouquetManager->Bouquets[i]->Name.c_str(), true, NULL, mwtv));
		}
	}
	int res = mctv.exec (NULL, "");

	// delete dynamic created objects
	for(unsigned int count=0;count<toDelete.size();count++)
	{
		delete toDelete[count];
	}
	toDelete.clear();
	return res;
}

int CQuadPiPSetupSelectChannelWidget::exec(CMenuTarget* parent, const std::string& actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (actionKey.find("window_") != std::string::npos)
	{
		wn = atoi(actionKey.substr(7, 1).c_str()) - 1;
		return InitZapitChannelHelper(CZapitClient::MODE_TV);
	}
	else if (strncmp(actionKey.c_str(), "ZCT:", 4) == 0)
	{
		unsigned int cnr = 0;
		t_channel_id channel_id = 0;
		sscanf(&(actionKey[4]),"%u|%" SCNx64 "", &cnr, &channel_id);
		g_settings.quadpip_channel_window[wn] = actionKey.substr(actionKey.find_first_of("#") + 1);
		g_settings.quadpip_channel_id_window[wn] = channel_id;

		// leave bouquet/channel menu and show a refreshed menu with current channel(s)
		g_RCInput->postMsg(CRCInput::RC_timeout, 0);

		// switch channel only if quadpip active
		if (quadpip)
			CNeutrinoApp::getInstance()->channelList->zapTo_ChannelID(g_settings.quadpip_channel_id_window[wn]);

		return menu_return::RETURN_EXIT;
	}
	else if(actionKey == "reset")
	{
		for (unsigned i=0; i < pip_devs; i++)
		{
			g_settings.quadpip_channel_window[i] = "-";
			g_settings.quadpip_channel_id_window[i] = 0;
		}
	}

	else if(actionKey=="select_window")
	{
		if (!quadpip)
			return res;

		if (aw == 0)
			paintWindow(0);
		else if (aw == 1)
			paintWindow(1);
		else if (aw == 2)
			paintWindow(2);
		else if (aw == 3)
			paintWindow(3);

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;

		bool loop=true;
		while (loop)
		{
			frameBuffer->blit();
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

			if ( msg <= CRCInput::RC_MaxRC )
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

			if ( msg == CRCInput::RC_ok )
			{
				// switch to selected channel
				videoDecoder->QuadPiP(false);
				quadpip = 0;
				for (unsigned i=0; i < (unsigned int) g_info.hw_caps->pip_devs; i++)
				{
					CCamManager::getInstance()->Stop(g_settings.quadpip_channel_id_window[i], CCamManager::PIP);
					if (pipVideoDemux[i])
						pipVideoDemux[i]->Stop();
					if (pipVideoDecoder[i])
					{
						pipVideoDecoder[i]->ShowPig(0);
						pipVideoDecoder[i]->Stop();
					}
					if (pipAudioDemux[i])
						pipAudioDemux[i]->Stop();
					if (pipAudioDecoder[i])
						pipAudioDecoder[i]->Stop();
					g_Zapit->stopPip(i);
				}
				CNeutrinoApp::getInstance()->channelList->zapTo_ChannelID(g_settings.quadpip_channel_id_window[aw]);
				g_Zapit->Rezap();
				loop = false;
				aw = 0;
				res = menu_return::RETURN_EXIT_ALL;
				break;
			}
			else if ((msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout))
			{
				// exit - back to menu
				loop = false;
			}
			else if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right) || (msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
			{
				// channel change 1..4 - cursor keys
				if (msg == CRCInput::RC_left)
				{
					if (aw == 1)
					{
						switchAudio(0, -1);
						paintWindow(0);
					}
					else if ((aw == 3) && (g_settings.quadpip_channel_id_window[2]))
					{
						switchAudio(2, 1);
						paintWindow(2);
					}
				}
				if (msg == CRCInput::RC_right)
				{
					if ((aw == 0) && (g_settings.quadpip_channel_id_window[1]))
					{
						switchAudio(-1, 0);
						paintWindow(1);
					}
					else if ((aw == 2) && (g_info.hw_caps->pip_devs > 2) && (g_settings.quadpip_channel_id_window[3]))
					{
						switchAudio(1, 2);
						paintWindow(3);
					}
				}
				if (msg == CRCInput::RC_up)
				{
					if (aw == 2)
					{
						switchAudio(1, -1);
						paintWindow(0);
					}
					else if ((aw == 3) && (g_settings.quadpip_channel_id_window[1]))
					{
						switchAudio(2, 0);
						paintWindow(1);
					}
				}
				if (msg == CRCInput::RC_down)
				{
					if ((aw == 0) && (g_info.hw_caps->pip_devs > 1) && (g_settings.quadpip_channel_id_window[2]))
					{
						switchAudio(-1, 1);
						paintWindow(2);
					}
					else if ((aw == 1) && (g_info.hw_caps->pip_devs > 2) && (g_settings.quadpip_channel_id_window[3]))
					{
						switchAudio(0, 2);
						paintWindow(3);
					}
				}
			}
			else if ((msg == CRCInput::RC_1) || (msg == CRCInput::RC_2) || (msg == CRCInput::RC_3) || (msg == CRCInput::RC_4))
			{
				// channel change 1..4 - direct keys
				if ((msg == CRCInput::RC_1) && (aw != 0))
				{
					switchAudio(aw-1, -1);
					paintWindow(0);
				}
				if ((msg == CRCInput::RC_2) && (aw != 1) && (g_settings.quadpip_channel_id_window[1]))
				{
					switchAudio(aw-1, 0);
					paintWindow(1);
				}
				if ((msg == CRCInput::RC_3) && (aw != 2) && (g_settings.quadpip_channel_id_window[2]))
				{
					switchAudio(aw-1, 1);
					paintWindow(2);
				}
				if ((msg == CRCInput::RC_4) && (aw != 3) && (g_settings.quadpip_channel_id_window[3]))
				{
					switchAudio(aw-1, 2);
					paintWindow(3);
				}
			}
			else if (msg > CRCInput::RC_MaxRC)
			{
				if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}
		}
		quad_win->kill();
		return res;
	}
	return res;
}
