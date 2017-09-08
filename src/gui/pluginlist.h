/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __pluginlist__
#define __pluginlist__

#include <gui/widget/menue.h>

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

class CPluginsExec : public CMenuTarget
{
	public:
		static CPluginsExec* getInstance();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif
