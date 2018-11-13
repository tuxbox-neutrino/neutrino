/*
	WebTV/WebRadio Setup

	Copyright (C) 2012-2013 martii
	Copyright (C)      2018 by vanhofen

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __webtv_setup_h__
#define __webtv_setup_h__

#include <sys/types.h>
#include <string.h>
#include <gui/widget/menue.h>

#define WEBTV_XML	CONFIGDIR "/webtv_usr.xml"
#define WEBRADIO_XML	WEBRADIODIR_VAR "/webradio_usr.xml"

class CWebTVSetup : public CMenuTarget, CChangeObserver
{
	private:
		bool webradio;
		int width;
		int selected;
		int item_offset;
		bool changed;
		CMenuWidget *m;
	public:
		CWebTVSetup();
		int exec(CMenuTarget *parent, const std::string &actionKey);
		int Show();
		bool changeNotify(const neutrino_locale_t OptionName, void *data);

		void webradio_xml_auto();
		bool webradio_xml_autodir(std::string directory);

		void webtv_xml_auto();
		bool webtv_xml_autodir(std::string directory);

		void webchannels_auto();
		bool webchannels_autodir(std::string directory);
};

class CWebTVResolution : public CMenuTarget
{
	private:
		int width;
		CMenuWidget *m;
	public:
		CWebTVResolution();
		const char *getResolutionValue();
		int exec(CMenuTarget *parent, const std::string &actionKey);
		int Show();
};

#endif
