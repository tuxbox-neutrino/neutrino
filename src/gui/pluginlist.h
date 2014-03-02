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

#ifndef __pluginlist__
#define __pluginlist__

#include <gui/widget/menue.h>

#include <driver/framebuffer.h>
#include <system/localize.h>

#include <string>
#include <vector>

class CPluginList : public CMenuTarget
{

	private:
		neutrino_locale_t title;
		uint32_t pluginlisttype;
		int width;

	protected:
		int selected;
		int number;

	public:
		CPluginList(const neutrino_locale_t Title, const uint32_t listtype);
		int exec(CMenuTarget* parent, const std::string & actionKey);
		virtual int run ();
};

class CPluginChooser : public CPluginList
{
	private:
		std::string *selectedFilePtr;
	public:
		CPluginChooser(const neutrino_locale_t Name, const uint32_t listtype, std::string &selectedFile);
		int run ();
};

#endif
