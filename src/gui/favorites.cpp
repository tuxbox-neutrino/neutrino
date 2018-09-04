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

#include <gui/channellist.h>

#include <global.h>
#include <neutrino.h>

#include <gui/favorites.h>

#include <gui/widget/hintbox.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>

#include <zapit/client/zapitclient.h>
#include <zapit/zapit.h>


#include <gui/bouquetlist.h>

extern CBouquetList * bouquetList;       /* neutrino.cpp */
extern CBouquetManager *g_bouquetManager;
//
// -- Add current channel to Favorites-Bouquet
// -- Return Status (bit-Status):A
// --    1 = Bouquet created
// --    2 = Channel added   (if not set, channel already in BQ)
// -- rasc
//

int CFavorites::addChannelToFavorites(bool show_list)
{
	signed int   bouquet_id;
	t_channel_id channel_id;
	//const char * fav_bouquetname;
	int          status = 0;

#if 0
	// no bouquet-List?  do nothing
	if (!bouquetList) return status;
#endif
	if(show_list)
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
		// -- check if Favorite Bouquet exists: if not, create it.
		bouquet_id = g_bouquetManager->existsUBouquet(DEFAULT_BQ_NAME_FAV, true);
		if (bouquet_id == -1) {
			g_bouquetManager->addBouquet(DEFAULT_BQ_NAME_FAV, true);
			bouquet_id = g_bouquetManager->existsUBouquet(DEFAULT_BQ_NAME_FAV, true);
			// status |= 1;
		}
	}

	channel_id = CZapit::getInstance()->GetCurrentChannelID();;

	if(!g_bouquetManager->existsChannelInBouquet(bouquet_id, channel_id)) {
		CZapit::getInstance()->addChannelToBouquet(bouquet_id, channel_id);
		status |= 2;
	}

	// -- tell zapit to save Boquets and reinit (if changed)
	if (status)
	{
		g_Zapit->saveBouquets();
	}

	return status;
}

//
// -- Menue Handler Interface
// -- to fit the MenueClasses from McClean
// -- Add current channel to Favorites and display user messagebox
//

int CFavorites::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int         status;
	std::string str;
	int         res = menu_return::RETURN_EXIT_ALL;
	bool	    show_list;
	//printf("[favorites] key %s\n", actionKey.c_str()); 
	show_list = (actionKey == "showlist");
	if (parent)
		parent->hide();

#if 0
	if (!bouquetList) {
		ShowMsg(LOCALE_FAVORITES_BOUQUETNAME, LOCALE_FAVORITES_NOBOUQUETS, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
		return res;
	}
#endif

	CHintBox hintBox(LOCALE_FAVORITES_BOUQUETNAME, g_Locale->getText(LOCALE_FAVORITES_ADDCHANNEL), 380); // UTF-8
	if(!show_list)
		hintBox.paint();

	status = addChannelToFavorites(show_list);

	hintBox.hide();

	// -- Display result

	//printf("[favorites] status %d\n", status);
	if(status < 0)
		return menu_return::RETURN_REPAINT;

	str = "";
	if(show_list)
	{
		if (status & 2)  str += g_Locale->getText(LOCALE_EXTRA_CHADDED);
		else	str += g_Locale->getText(LOCALE_EXTRA_CHALREADYINBQ);
		ShowMsg(LOCALE_EXTRA_ADD_TO_BOUQUET, str, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO); // UTF-8
	}
	else
	{
		if (status & 1)  str += g_Locale->getText(LOCALE_FAVORITES_BQCREATED);
		if (status & 2)  str += g_Locale->getText(LOCALE_FAVORITES_CHADDED);
		else             str += g_Locale->getText(LOCALE_FAVORITES_CHALREADYINBQ);
		if (status) str +=  g_Locale->getText(LOCALE_FAVORITES_FINALHINT);
		ShowMsg(LOCALE_FAVORITES_BOUQUETNAME, str, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO); // UTF-8
	}



	//	if (status) {
	//		g_RCInput->postMsg( NeutrinoMessages::EVT_BOUQUETSCHANGED, 0 );
	//	}

	return res;
}
