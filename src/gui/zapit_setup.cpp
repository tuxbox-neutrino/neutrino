/*
	zapit_setup settings menu - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/
	License: GPL

	Copyright (C) 2011-2012 Stefan Seyfried

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


#include "zapit_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <driver/screen_max.h>


CZapitSetup::CZapitSetup()
{
	width = w_max (40, 10); //%
}

CZapitSetup::~CZapitSetup()
{
}

int CZapitSetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	printf("[neutrino] init zapit menu setup...\n");
	if (parent)
		parent->hide();

	return showMenu();
}

int CZapitSetup::showMenu()
{
	//menue init
	CMenuWidget *zapit = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_ZAPIT);
	zapit->addIntroItems(LOCALE_ZAPITSETUP_INFO);
	COnOffNotifier* miscZapitNotifier = new COnOffNotifier(1);
	//zapit
	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_ZAPITSETUP_LAST_USE, &g_settings.uselastchannel, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, miscZapitNotifier, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	mc->setHint("", LOCALE_MENU_HINT_LAST_USE);

	CSelectChannelWidget select;

	CMenuForwarder 	*zapit1 = new CMenuForwarder(LOCALE_ZAPITSETUP_LAST_TV    , !g_settings.uselastchannel, g_settings.StartChannelTV, &select, "tv", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN );
	zapit1->setHint("", LOCALE_MENU_HINT_LAST_TV);

	CMenuForwarder 	*zapit2 = new CMenuForwarder(LOCALE_ZAPITSETUP_LAST_RADIO , !g_settings.uselastchannel, g_settings.StartChannelRadio, &select, "radio", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW );
	zapit2->setHint("", LOCALE_MENU_HINT_LAST_RADIO);

	miscZapitNotifier->addItem(zapit1);
	miscZapitNotifier->addItem(zapit2);

	zapit->addItem(mc);
	zapit->addItem(GenericMenuSeparatorLine);
	zapit->addItem(zapit1);
	zapit->addItem(zapit2);

	int res = zapit->exec(NULL, "");
	delete miscZapitNotifier;
	delete zapit;
	return res;
}

//select menu
CSelectChannelWidget::CSelectChannelWidget()
{
	width = w_max (40, 10); //%
}

CSelectChannelWidget::~CSelectChannelWidget()
{

}

int CSelectChannelWidget::exec(CMenuTarget* parent, const std::string& actionKey)
{
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if(actionKey == "tv")
	{
		return InitZapitChannelHelper(CZapitClient::MODE_TV);
	}
	else if(actionKey == "radio")
	{
		return InitZapitChannelHelper(CZapitClient::MODE_RADIO);
	}
	else if (strncmp(actionKey.c_str(), "ZCT:", 4) == 0 || strncmp(actionKey.c_str(), "ZCR:", 4) == 0)
	{
		unsigned int cnr = 0;
		t_channel_id channel_id = 0;
		sscanf(&(actionKey[4]),"%u|%" SCNx64 "", &cnr, &channel_id);

		if (strncmp(actionKey.c_str(), "ZCT:", 4) == 0)//...tv
		{
			g_settings.StartChannelTV = actionKey.substr(actionKey.find_first_of("#")+1);
			g_settings.startchanneltv_id = channel_id;
		}
		else if (strncmp(actionKey.c_str(), "ZCR:", 4) == 0)//...radio
		{
			g_settings.StartChannelRadio = actionKey.substr(actionKey.find_first_of("#")+1);
			g_settings.startchannelradio_id= channel_id;
		}

		// ...leave bouquet/channel menu and show a refreshed zapit menu with current start channel(s)
		g_RCInput->postMsg(CRCInput::RC_timeout, 0);
		return menu_return::RETURN_EXIT;
	}

	return res;
}

extern CBouquetManager *g_bouquetManager;
int CSelectChannelWidget::InitZapitChannelHelper(CZapitClient::channelsMode mode)
{
	std::vector<CMenuWidget *> toDelete;
	CMenuWidget mctv(LOCALE_TIMERLIST_BOUQUETSELECT, NEUTRINO_ICON_SETTINGS, width);
	mctv.addIntroItems();

	for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++) {
		CMenuWidget* mwtv = new CMenuWidget(LOCALE_TIMERLIST_CHANNELSELECT, NEUTRINO_ICON_SETTINGS, width);
		toDelete.push_back(mwtv);
		mwtv->addIntroItems();
		ZapitChannelList channels;
		if (mode == CZapitClient::MODE_RADIO)
			g_bouquetManager->Bouquets[i]->getRadioChannels(channels);
		else
			g_bouquetManager->Bouquets[i]->getTvChannels(channels);
		for(int j = 0; j < (int) channels.size(); j++) {
			CZapitChannel * channel = channels[j];
			char cChannelId[60] = {0};
			snprintf(cChannelId, sizeof(cChannelId), "ZC%c:%d|%" PRIx64 "#", (mode==CZapitClient::MODE_TV)?'T':'R', channel->number, channel->channel_id);

			CMenuForwarderNonLocalized * chan_item = new CMenuForwarderNonLocalized(channel->getName().c_str(), true, NULL, this, (std::string(cChannelId) + channel->getName()).c_str(), CRCInput::RC_nokey, NULL, channel->scrambled ?NEUTRINO_ICON_SCRAMBLED:NULL);
			chan_item->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
			mwtv->addItem(chan_item);

		}
		if(!channels.empty() && (!g_bouquetManager->Bouquets[i]->bHidden ))
		{
			mctv.addItem(new CMenuForwarderNonLocalized(g_bouquetManager->Bouquets[i]->Name.c_str(), true, NULL, mwtv));
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
