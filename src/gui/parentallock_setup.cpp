/*
	$port: parentallock_setup.cpp,v 1.4 2010/10/01 15:59:10 tuxbox-cvs Exp $

	parentallock setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
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
	along with this program; if not, write to the 
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "parentallock_setup.h"

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>


#include <system/debug.h>


//constructor is definied in parentallock_setup.h

CParentalSetup::~CParentalSetup()
{

}

int CParentalSetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init parental setup\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (check())
		res = showParentalSetup();

	return res;
}


#if 1
#define PARENTALLOCK_PROMPT_OPTION_COUNT 3
#else
#define PARENTALLOCK_PROMPT_OPTION_COUNT 4
#endif

const CMenuOptionChooser::keyval PARENTALLOCK_PROMPT_OPTIONS[PARENTALLOCK_PROMPT_OPTION_COUNT] =
{
	{ PARENTALLOCK_PROMPT_NEVER         , LOCALE_PARENTALLOCK_NEVER          },
#if 0
	{ PARENTALLOCK_PROMPT_ONSTART       , LOCALE_PARENTALLOCK_ONSTART        },
#endif
	{ PARENTALLOCK_PROMPT_CHANGETOLOCKED, LOCALE_PARENTALLOCK_CHANGETOLOCKED },
	{ PARENTALLOCK_PROMPT_ONSIGNAL      , LOCALE_PARENTALLOCK_ONSIGNAL       }
};

#define PARENTALLOCK_LOCKAGE_OPTION_COUNT 3
const CMenuOptionChooser::keyval PARENTALLOCK_LOCKAGE_OPTIONS[PARENTALLOCK_LOCKAGE_OPTION_COUNT] =
{
	{ 12, LOCALE_PARENTALLOCK_LOCKAGE12 },
	{ 16, LOCALE_PARENTALLOCK_LOCKAGE16 },
	{ 18, LOCALE_PARENTALLOCK_LOCKAGE18 }
};

#define PARENTALLOCK_DEFAULTLOCKED_OPTION_COUNT 2
const CMenuOptionChooser::keyval PARENTALLOCK_DEFAULTLOCKED_OPTIONS[PARENTALLOCK_DEFAULTLOCKED_OPTION_COUNT] =
{
	{ false, LOCALE_PARENTALLOCK_DEFAULTUNLOCKED },
	{ true,  LOCALE_PARENTALLOCK_DEFAULTLOCKED }
};

extern bool parentallocked;
int CParentalSetup::showParentalSetup()
{
	//menue init
	CMenuWidget* plock = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_LOCK, width, MN_WIDGET_ID_PLOCKSETUP);

	// intros with sub head
	plock->addIntroItems(LOCALE_PARENTALLOCK_PARENTALLOCK);

	CMenuForwarder * mf;

	CMenuOptionChooser * mc;
	mc = new CMenuOptionChooser(LOCALE_PARENTALLOCK_PROMPT , &g_settings.parentallock_prompt , PARENTALLOCK_PROMPT_OPTIONS, PARENTALLOCK_PROMPT_OPTION_COUNT , !parentallocked);
	mc->setHint("", LOCALE_MENU_HINT_PARENTALLOCK_PROMPT);
	plock->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_PARENTALLOCK_LOCKAGE, &g_settings.parentallock_lockage, PARENTALLOCK_LOCKAGE_OPTIONS, PARENTALLOCK_LOCKAGE_OPTION_COUNT, !parentallocked);
	mc->setHint("", LOCALE_MENU_HINT_PARENTALLOCK_LOCKAGE);
	plock->addItem(mc);

	mc = new CMenuOptionChooser(LOCALE_PARENTALLOCK_BOUQUETMODE, &g_settings.parentallock_defaultlocked, PARENTALLOCK_DEFAULTLOCKED_OPTIONS, PARENTALLOCK_DEFAULTLOCKED_OPTION_COUNT, !parentallocked);
	plock->addItem(mc);
	plock->addItem(new CMenuOptionNumberChooser(LOCALE_PARENTALLOCK_ZAPTIME, (int *)&g_settings.parentallock_zaptime, !parentallocked, 0, 10000));

	CPINChangeWidget pinChangeWidget(LOCALE_PARENTALLOCK_CHANGEPIN, &g_settings.parentallock_pincode, 4, LOCALE_PARENTALLOCK_CHANGEPIN_HINT1);
	mf = new CMenuForwarder(LOCALE_PARENTALLOCK_CHANGEPIN, true, g_settings.parentallock_pincode, &pinChangeWidget);
	mf->setHint("", LOCALE_MENU_HINT_PARENTALLOCK_CHANGEPIN);
	plock->addItem(mf);

	int res = plock->exec(NULL, "");
	delete plock;
	return res;
}
