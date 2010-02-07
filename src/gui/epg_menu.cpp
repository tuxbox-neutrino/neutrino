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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <gui/widget/icons.h>

#include <gui/epg_menu.h>

#include <gui/eventlist.h>
#include <gui/infoviewer.h>
#include <gui/epgplus.h>
#include <gui/streaminfo2.h>

//
//  -- EPG Menue Handler Class
//  -- to be used for calls from Menue
//  -- (2004-03-06 rasc)
//

int CEPGMenuHandler::exec(CMenuTarget* parent, const std::string &/*actionkey*/)
{
	int           res = menu_return::RETURN_EXIT_ALL;


	if (parent) {
		parent->hide();
	}

	doMenu ();
	return res;
}

int CEPGMenuHandler::doMenu ()
{
	CMenuWidget EPGSelector(LOCALE_EPGMENU_HEAD, NEUTRINO_ICON_FEATURES);

	// EPGSelector.addItem(GenericMenuSeparator);
	EPGSelector.addItem(new CMenuForwarder(LOCALE_EPGMENU_EVENTLIST , true, NULL, new CEventListHandler(), NULL, CRCInput::RC_red   , NEUTRINO_ICON_BUTTON_RED   ), false);
	EPGSelector.addItem(new CMenuForwarder(LOCALE_EPGMENU_EPGPLUS   , true, NULL, new CEPGplusHandler()  , NULL, CRCInput::RC_green , NEUTRINO_ICON_BUTTON_GREEN ), false);
	EPGSelector.addItem(new CMenuForwarder(LOCALE_EPGMENU_EVENTINFO , true, NULL, new CEPGDataHandler()  , NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW), false);
	EPGSelector.addItem(new CMenuForwarder(LOCALE_EPGMENU_STREAMINFO, true, NULL, new CStreamInfo2()     , NULL, CRCInput::RC_blue  , NEUTRINO_ICON_BUTTON_BLUE  ), false);

	EPGSelector.addItem(GenericMenuSeparator);

	return EPGSelector.exec(NULL, "");
}

