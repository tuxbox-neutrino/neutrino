/*
	Neutrino-GUI  -   DBoxII-Project

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

#include <global.h>
#include <neutrino.h>

#include <gui/favorites.h>

#include <gui/bouquetlist.h>
#include <gui/channellist.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <zapit/client/zapitclient.h>
#include <zapit/zapit.h>

extern CBouquetList *bouquetList;
extern CBouquetManager *g_bouquetManager;

//
// -- Add current channel to Favorites-Bouquet
// -- Return Status (bit-Status):A
// --    1 = Bouquet created
// --    2 = Channel added (if not set, channel already in BQ)
// -- rasc
//

int CFavorites::addChannelToFavorites(bool show_list)
{
	signed int bouquet_id;
	t_channel_id channel_id;
	int status = 0;

	if (show_list)
	{
		bouquet_id = bouquetList->exec(false);
		if (bouquet_id < 0)
			return bouquet_id;
		printf("[favorites] bouquet name %s\n", bouquetList->Bouquets[bouquet_id]->channelList->getName());
		bouquet_id = g_Zapit->existsBouquet(bouquetList->Bouquets[bouquet_id]->channelList->getName());
		if (bouquet_id == -1)
			return bouquet_id;
	}
	else
	{
		bouquet_id = g_bouquetManager->existsUBouquet(DEFAULT_BQ_NAME_FAV, true);
		if (bouquet_id == -1)
		{
			g_bouquetManager->addBouquet(DEFAULT_BQ_NAME_FAV, true);
			bouquet_id = g_bouquetManager->existsUBouquet(DEFAULT_BQ_NAME_FAV, true);
			//status |= 1;
		}
	}

	channel_id = CZapit::getInstance()->GetCurrentChannelID();;

	if (!g_bouquetManager->existsChannelInBouquet(bouquet_id, channel_id))
	{
		CZapit::getInstance()->addChannelToBouquet(bouquet_id, channel_id);
		status |= 2;
	}

	if (status)
		g_Zapit->saveBouquets();

	return status;
}

int CFavorites::exec(CMenuTarget *parent, const std::string &actionKey)
{
	int status;
	std::string str;
	int res = menu_return::RETURN_EXIT_ALL;
	bool show_list = (actionKey == "showlist");

	if (parent)
		parent->hide();

	CHintBox hintBox(LOCALE_FAVORITES_BOUQUETNAME, g_Locale->getText(LOCALE_FAVORITES_ADDCHANNEL), 380);
	if (!show_list)
		hintBox.paint();

	status = addChannelToFavorites(show_list);

	hintBox.hide();

	if (status < 0)
		return menu_return::RETURN_REPAINT;

	str = "";
	if (show_list)
	{
		if (status & 2)
			str += g_Locale->getText(LOCALE_EXTRA_CHADDED);
		else
			str += g_Locale->getText(LOCALE_EXTRA_CHALREADYINBQ);

		ShowMsg(LOCALE_EXTRA_ADD_TO_BOUQUET, str, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
	}
	else
	{
		if (status & 1)
			str += g_Locale->getText(LOCALE_FAVORITES_BQCREATED);
		if (status & 2)
			str += g_Locale->getText(LOCALE_FAVORITES_CHADDED);
		else
			str += g_Locale->getText(LOCALE_FAVORITES_CHALREADYINBQ);
		if (status)
			str += g_Locale->getText(LOCALE_FAVORITES_FINALHINT);

		ShowMsg(LOCALE_FAVORITES_BOUQUETNAME, str, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
	}

	return res;
}
