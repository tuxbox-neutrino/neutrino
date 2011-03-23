/*
	info menue - Neutrino-GUI

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

#include <global.h>
#include <neutrino.h>

#include "gui/info_menue.h"
#include "gui/imageinfo.h"
#include "gui/dboxinfo.h"
#include <gui/streaminfo2.h>

#include <driver/screen_max.h>


CInfoMenu::CInfoMenu()
{
	width = w_max (40, 10);
	selected = -1;
}


CInfoMenu::~CInfoMenu()
{


}

int CInfoMenu::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	printf("[neutrino] CInfoMenu %s: init...\n",__FUNCTION__);
	int   res = menu_return::RETURN_REPAINT;
		
	if (parent)
		parent->hide();
	
	showMenu();

	return res;
}



void CInfoMenu::showMenu()
{
	printf("[neutrino] CInfoMenu call %s...\n", __FUNCTION__);

	CMenuWidget *info = new CMenuWidget(LOCALE_MESSAGEBOX_INFO, NEUTRINO_ICON_INFO);
	info->setSelected(selected);

	info->addIntroItems();
	info->addItem(new CMenuForwarder(LOCALE_SERVICEMENU_IMAGEINFO,  true, NULL, new CImageInfo(), NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED ), false);
	info->addItem(new CMenuForwarder(LOCALE_EXTRA_DBOXINFO, 	true, NULL, new CDBoxInfoWidget, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	info->addItem(new CMenuForwarder(LOCALE_STREAMINFO_HEAD, 	true, NULL, new CStreamInfo2Handler(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
		
	info->exec(NULL, "");
	info->hide();
	selected = info->getSelected();
	delete info;
}



