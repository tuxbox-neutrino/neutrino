/*
	$port: pictureviewer_setup.cpp,v 1.4 2010/12/08 18:03:23 tuxbox-cvs Exp $

	pictureviewer setup implementation - Neutrino-GUI

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

#include "pictureviewer_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>

#include <gui/pictureviewer.h>
#include <gui/filebrowser.h>

#include <driver/screen_max.h>

#include <system/debug.h>


CPictureViewerSetup::CPictureViewerSetup()
{
	width = w_max (40, 10);
}

CPictureViewerSetup::~CPictureViewerSetup()
{

}

int CPictureViewerSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init pctureviwer setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();


	if(actionKey == "picturedir")
	{
		parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.network_nfs_picturedir.c_str()))
			g_settings.network_nfs_picturedir = b.getSelectedFile()->Name;
		return res;
	}

	res = showPictureViewerSetup();

	return res;
}

#define PICTUREVIEWER_SCALING_OPTION_COUNT 3
const CMenuOptionChooser::keyval PICTUREVIEWER_SCALING_OPTIONS[PICTUREVIEWER_SCALING_OPTION_COUNT] =
{
	{ CPictureViewer::SIMPLE, LOCALE_PICTUREVIEWER_RESIZE_SIMPLE        },
	{ CPictureViewer::COLOR , LOCALE_PICTUREVIEWER_RESIZE_COLOR_AVERAGE },
	{ CPictureViewer::NONE  , LOCALE_PICTUREVIEWER_RESIZE_NONE          }
};

/*shows the picviewer setup menue*/
int CPictureViewerSetup::showPictureViewerSetup()
{
	CMenuWidget* picviewsetup = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_PVIEWERSETUP);

	// intros: back ande save
	picviewsetup->addIntroItems(LOCALE_PICTUREVIEWER_HEAD);

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_PICTUREVIEWER_SCALING, &g_settings.picviewer_scaling, PICTUREVIEWER_SCALING_OPTIONS, PICTUREVIEWER_SCALING_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_PICTUREVIEWER_SCALING);
	picviewsetup->addItem(mc);

	CMenuOptionNumberChooser *cc = new CMenuOptionNumberChooser(LOCALE_PICTUREVIEWER_SLIDE_TIME, &g_settings.picviewer_slide_time, true, 0, 999);
	cc->setNumberFormat(std::string("%d ") + g_Locale->getText(LOCALE_UNIT_SHORT_SECOND));
	cc->setHint("", LOCALE_MENU_HINT_PICTUREVIEWER_SLIDE_TIME);
	picviewsetup->addItem(cc);

	CMenuForwarder *mf = new CMenuForwarder(LOCALE_PICTUREVIEWER_DEFDIR, true, g_settings.network_nfs_picturedir, this, "picturedir");
	mf->setHint("", LOCALE_MENU_HINT_PICTUREVIEWER_DEFDIR);
	picviewsetup->addItem(mf);

	int res = picviewsetup->exec(NULL, "");
	delete picviewsetup;
	return res;
}
