/*
	$port: proxyserver_setup.cpp,v 1.2 2010/03/02 20:11:42 tuxbox-cvs Exp $

	proxyserver_setup menue - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2010 T. Graf 'dbt'
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

#include "proxyserver_setup.h"

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/components/cc_input_dialog.h>

#include <driver/screen_max.h>
#include <system/debug.h>

namespace
{
std::string makeDialogHint(const neutrino_locale_t hint1,
	const neutrino_locale_t hint2)
{
	std::string hint = g_Locale->getText(hint1);
	const std::string hint_extra = g_Locale->getText(hint2);

	if (!hint_extra.empty())
	{
		if (!hint.empty())
			hint += "\n";
		hint += hint_extra;
	}

	return hint;
}

void initProxyTextDialog(CCTextInputDialog &dialog,
	const neutrino_locale_t title,
	const neutrino_locale_t hint1,
	const neutrino_locale_t hint2,
	const bool password_mode = false)
{
	dialog.setHintText(makeDialogHint(hint1, hint2));
	dialog.setPlaceholder(g_Locale->getText(title));
	if (title == LOCALE_FLASHUPDATE_PROXYSERVER)
		dialog.setFieldFontReference("server.example.org:8080");
	if (password_mode)
		dialog.enablePasswordMode(true);
}
}


CProxySetup::CProxySetup(const neutrino_locale_t title, const char * const IconName )
{
	menue_title = title;
	menue_icon = IconName;

	width = 40;
}

CProxySetup::~CProxySetup()
{
}

int CProxySetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_DEBUG, "init proxy menu\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	res = showProxySetup();
	
	return res;
}

/* shows entries for proxy settings */
int CProxySetup::showProxySetup()
{
	//init
	CMenuWidget * mn = new CMenuWidget(menue_title, menue_icon, width, MN_WIDGET_ID_PROXYSETUP);
	
	neutrino_locale_t subtitle = (menue_title == LOCALE_FLASHUPDATE_PROXYSERVER_SEP ? NONEXISTANT_LOCALE : LOCALE_FLASHUPDATE_PROXYSERVER_SEP);
	mn->addIntroItems(subtitle);

	CCTextInputDialog softUpdate_proxy(
		g_Locale->getText(LOCALE_FLASHUPDATE_PROXYSERVER),
		&g_settings.softupdate_proxyserver);
	initProxyTextDialog(softUpdate_proxy,
		LOCALE_FLASHUPDATE_PROXYSERVER,
		LOCALE_FLASHUPDATE_PROXYSERVER_HINT1,
		LOCALE_FLASHUPDATE_PROXYSERVER_HINT2);
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_FLASHUPDATE_PROXYSERVER,
		true,
		NULL,
		&softUpdate_proxy,
		NULL,
		CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_NET_PROXYSERVER);
	mn->addItem(mf);

	CCTextInputDialog softUpdate_proxyuser(
		g_Locale->getText(LOCALE_FLASHUPDATE_PROXYUSERNAME),
		&g_settings.softupdate_proxyusername);
	initProxyTextDialog(softUpdate_proxyuser,
		LOCALE_FLASHUPDATE_PROXYUSERNAME,
		LOCALE_FLASHUPDATE_PROXYUSERNAME_HINT1,
		LOCALE_FLASHUPDATE_PROXYUSERNAME_HINT2);
	mf = new CMenuForwarder(LOCALE_FLASHUPDATE_PROXYUSERNAME,
		true,
		NULL,
		&softUpdate_proxyuser,
		NULL,
		CRCInput::RC_green);
	mf->setHint("", LOCALE_MENU_HINT_NET_PROXYUSER);
	mn->addItem(mf);

	CCTextInputDialog softUpdate_proxypass(
		g_Locale->getText(LOCALE_FLASHUPDATE_PROXYPASSWORD),
		&g_settings.softupdate_proxypassword);
	initProxyTextDialog(softUpdate_proxypass,
		LOCALE_FLASHUPDATE_PROXYPASSWORD,
		LOCALE_FLASHUPDATE_PROXYPASSWORD_HINT1,
		LOCALE_FLASHUPDATE_PROXYPASSWORD_HINT2,
		true);
	mf = new CMenuForwarder(LOCALE_FLASHUPDATE_PROXYPASSWORD,
		true,
		NULL,
		&softUpdate_proxypass,
		NULL,
		CRCInput::RC_yellow);
	mf->setHint("", LOCALE_MENU_HINT_NET_PROXYPASS);
	mn->addItem(mf);

	int res = mn->exec(NULL, "");
	delete mn;

	if (g_settings.softupdate_proxyserver.empty())
		unsetenv("http_proxy");
	else {
		std::string proxy = "http://";
		if (!g_settings.softupdate_proxyusername.empty())
			proxy += g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword + "@";
		proxy += g_settings.softupdate_proxyserver;
		setenv("http_proxy", proxy.c_str(), 1);
	}

	return res;
}
