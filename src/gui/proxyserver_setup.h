/*
	$port: proxyserver_setup.h,v 1.1 2010/02/22 10:52:34 tuxbox-cvs Exp $

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

#ifndef __proxyserver_setup__
#define __proxyserver_setup__

#include <gui/widget/menue.h>
#include <gui/widget/icons.h>

#include <string>

class CProxySetup : public CMenuTarget
{
	private:
		int width;

		neutrino_locale_t menue_title;
		std::string menue_icon;

		int showProxySetup();

	public:	
		CProxySetup(const neutrino_locale_t title = LOCALE_FLASHUPDATE_PROXYSERVER_SEP, const char * const IconName = NEUTRINO_ICON_SETTINGS);
		~CProxySetup();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
