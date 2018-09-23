/*
	XMLTV setup menue

	Copyright (C) 2018 'vanhofen'
	Homepage: http://www.neutrino-images.de/

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

#include "xmltv_setup.h"

#include <neutrino.h>
#include <gui/filebrowser.h>
#include <gui/widget/keyboard_input.h>

CXMLTVSetup::CXMLTVSetup()
{
	width = 55;
	selected = -1;
	item_offset = 0;
	changed = false;
}

CXMLTVSetup::~CXMLTVSetup()
{
}

static const struct button_label CXMLTVSetupFooterButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_XMLTV_XML_DEL },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_XMLTV_XML_ADD },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_XMLTV_XML_ENTER },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_XMLTV_XML_RELOAD }
};
#define CXMLTVSetupFooterButtonCount (sizeof(CXMLTVSetupFooterButtons)/sizeof(button_label))

int CXMLTVSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (actionKey == "d" /* delete */)
	{
		selected = m->getSelected();
		if (selected >= item_offset)
		{
			m->removeItem(selected);
			m->hide();
			selected = m->getSelected();
			changed = true;
		}
		return res;
	}
	if (actionKey == "c" /* change */)
	{
		selected = m->getSelected();
		CMenuItem *item = m->getItem(selected);
		CMenuForwarder *f = static_cast<CMenuForwarder *>(item);
		std::string dirname(f->getName());
		if (strstr(dirname.c_str(), "://"))
		{
			std::string entry = dirname;

			CKeyboardInput *e = new CKeyboardInput(LOCALE_XMLTV_XML_ENTER, &entry, 50);
			e->exec(this, "");
			delete e;

			if (entry.compare(dirname) != 0)
			{
				f->setName(entry);
				changed = true;
			}
		}
		else
		{
			CFileBrowser fileBrowser;
			CFileFilter fileFilter;
			fileFilter.addFilter("xml");
			fileBrowser.Filter = &fileFilter;

			dirname = dirname.substr(0, dirname.rfind('/'));
			if (fileBrowser.exec(dirname.c_str()))
			{
				f->setName(fileBrowser.getSelectedFile()->Name);
				changed = true;
			}
		}
		return res;
	}
	if (actionKey == "a" /* add */)
	{
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("xml");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec("/tmp")) // /tmp ?
		{
			std::string s = fileBrowser.getSelectedFile()->Name;
			m->addItem(new CMenuForwarder(s, true, NULL, this, "c"));
			changed = true;
		}
		return res;
	}
	if (actionKey == "e" /* enter */)
	{
		std::string tpl = "http://xxx.xxx.xxx.xxx/control/xmltv.xml";
		std::string entry = tpl;

		CKeyboardInput *e = new CKeyboardInput(LOCALE_XMLTV_XML_ENTER, &entry, 50);
		e->exec(this, "");
		delete e;

		if (entry.compare(tpl) != 0)
		{
			m->addItem(new CMenuForwarder(entry, true, NULL, this, "c"));
			changed = true;
		}
		return res;
	}
	if (actionKey == "r" /* reload */)
	{
		changed = true;
		return menu_return::RETURN_EXIT_ALL;
	}

	if (parent)
		parent->hide();

	res = Show();

	return res;
}

int CXMLTVSetup::Show()
{
	item_offset = 0;

	m = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_STREAMING, width, MN_WIDGET_ID_XMLTVSETUP);
	m->addKey(CRCInput::RC_red, this, "d");
	m->addKey(CRCInput::RC_green, this, "a");
	m->addKey(CRCInput::RC_yellow, this, "e");
	m->addKey(CRCInput::RC_blue, this, "r");

	m->addIntroItems(LOCALE_XMLTV_HEAD, LOCALE_XMLTV_XML);

	item_offset = m->getItemsCount();
	// show users xmltv files
	for (std::list<std::string>::iterator it = g_settings.xmltv_xml.begin(); it != g_settings.xmltv_xml.end(); ++it)
	{
		m->addItem(new CMenuForwarder(*it, true, NULL, this, "c"));
	}

	m->setFooter(CXMLTVSetupFooterButtons, CXMLTVSetupFooterButtonCount);

	int res = m->exec(NULL, "");
	m->hide();

	if (changed)
	{
		g_settings.xmltv_xml.clear();
		for (int i = item_offset; i < m->getItemsCount(); i++)
		{
			CMenuItem *item = m->getItem(i);
			CMenuForwarder *f = static_cast<CMenuForwarder *>(item);
			g_settings.xmltv_xml.push_back(f->getName());
		}

		for (std::list<std::string>::iterator it = g_settings.xmltv_xml.begin(); it != g_settings.xmltv_xml.end(); ++it)
		{
			printf("Reading xmltv epg from %s ...\n", (*it).c_str());
			g_Sectionsd->readSIfromXMLTV((*it).c_str());
		}
	}

	delete m;
	return res;
}
