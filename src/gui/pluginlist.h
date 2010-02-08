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

	public:
		enum result_
		{
			close  = 0,
			resume = 1
		} result;

	private:

		CFrameBuffer	*frameBuffer;

		struct pluginitem
		{
			int         number;
			std::string name;   // UTF-8 encoded
			std::string desc;   // UTF-8 encoded
		};

		unsigned int	    liststart;
		unsigned int	    listmaxshow;
		int		    key;
		neutrino_locale_t   name;
		uint32_t pluginlisttype;

		int		fheight; // Fonthoehe Channellist-Inhalt
		int		theight; // Fonthoehe Channellist-Titel

		int		fheight1,fheight2;

		int 		width;
		int 		height;
		int 		x;
		int 		y;

		void paintItem(int pos);
		void paintItems();
		void paint();
		void paintHead();

	protected:

		unsigned int selected;
		std::vector<pluginitem *> pluginlist;

		virtual CPluginList::result_ pluginSelected();

	public:

		CPluginList(const neutrino_locale_t Name, const uint32_t listtype);
		virtual ~CPluginList();

		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CPluginChooser : public CPluginList
{
	private:
	char* selected_plugin;
	protected:

	CPluginList::result_ pluginSelected();

	public:
	CPluginChooser(const neutrino_locale_t Name, const uint32_t listtype, char* pluginname);
};

#endif
