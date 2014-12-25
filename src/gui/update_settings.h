/*
	$Id: port of software_update.h,v 1.6 2011/04/03 21:56:13 tuxbox-cvs Exp $

	Neutrino-GUI  -   DBoxII-Project

	Update settings implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2012 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.

*/

#ifndef __update_settings__
#define __update_settings__

#include <gui/widget/menue.h>

#include "update.h"

// #define USE_SMS_INPUT

#ifdef USE_SMS_INPUT
#include "gui/widget/stringinput.h"
#endif

#include <string>

//helper class to enable/disable update config url item
class CUrlConfigSetupNotifier : public CChangeObserver
{
	private:
		CMenuItem* toDisable[2];
		CMenuForwarder * updateItem;
	public:
		CUrlConfigSetupNotifier( CMenuItem*, CMenuItem*, CMenuForwarder *);
		bool changeNotify(const neutrino_locale_t = NONEXISTANT_LOCALE, void *data = NULL);
};

class CUpdateSettings : public CMenuTarget
{
	private:
		int width;
		int initMenu();
		
		CFlashExpert *fe;
#ifdef USE_SMS_INPUT
		CStringInputSMS *input_url_file;
#endif
	
	public:	
		CUpdateSettings();
		~CUpdateSettings();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif
