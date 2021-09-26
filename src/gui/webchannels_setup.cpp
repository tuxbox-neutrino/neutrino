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

#define __USE_FILE_OFFSET64 1
#include "filebrowser.h"
#include <stdio.h>
#include <global.h>
#include <libgen.h>
#include <neutrino.h>
#include <driver/screen_max.h>
#include <driver/framebuffer.h>
#include <gui/movieplayer.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/keyboard_input.h>
#include <zapit/zapit.h>
#include <neutrino_menue.h>
#include "webchannels_setup.h"


#include <dirent.h>
#include <mymenu.h>
#include <system/helpers.h>

CWebChannelsSetup::CWebChannelsSetup()
{
	webradio = false;
	width = 75;
	selected = -1;
	item_offset = 0;
	changed = false;
}

static const struct button_label CWebChannelsSetupFooterButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,  LOCALE_WEBCHANNELS_XML_DEL },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_WEBCHANNELS_XML_ADD },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_WEBCHANNELS_XML_ENTER },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_WEBCHANNELS_XML_RELOAD }
};
#define CWebChannelsSetupFooterButtonCount (sizeof(CWebChannelsSetupFooterButtons)/sizeof(CWebChannelsSetupFooterButtons[0]))

int CWebChannelsSetup::exec(CMenuTarget *parent, const std::string &actionKey)
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

			CKeyboardInput *e = new CKeyboardInput(LOCALE_WEBCHANNELS_XML_ENTER, &entry, 50);
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
			fileFilter.addFilter("tv");
			fileFilter.addFilter("m3u");
			fileBrowser.Filter = &fileFilter;

			dirname = dirname.substr(0, dirname.rfind('/'));
			if (fileBrowser.exec(dirname.c_str()))
			{
				f->setName(fileBrowser.getSelectedFile()->Name);
				if (webradio)
					g_settings.last_webradio_dir = dirname;
				else
					g_settings.last_webtv_dir = dirname;
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
		fileFilter.addFilter("tv");
		fileFilter.addFilter("m3u");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(webradio ? g_settings.last_webradio_dir.c_str() : g_settings.last_webtv_dir.c_str()) == true)
		{
			std::string s = fileBrowser.getSelectedFile()->Name;
			m->addItem(new CMenuForwarder(s, true, NULL, this, "c"));
			if (webradio)
				g_settings.last_webradio_dir = s.substr(0, s.rfind('/')).c_str();
			else
				g_settings.last_webtv_dir = s.substr(0, s.rfind('/')).c_str();
			changed = true;
		}
		return res;
	}
	if (actionKey == "e" /* enter */)
	{
		std::string tpl = "http://xxx.xxx.xxx.xxx/control/xmltv.m3u";
		if (webradio)
			tpl += "?mode=radio";
		else
			tpl += "?mode=tv";
		std::string entry = tpl;

		CKeyboardInput *e = new CKeyboardInput(LOCALE_WEBCHANNELS_XML_ENTER, &entry, 52);
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
	if (actionKey == "script_path")
	{
		const char *action_str = "ScriptPath";
		chooserDir(g_settings.livestreamScriptPath, false, action_str);
		return res;
	}
	if (actionKey == "webtv_menu")
	{
		webradio = false;
	}
	if (actionKey == "webradio_menu")
	{
		webradio = true;
	}

	if (parent)
		parent->hide();

	res = Show();

	return res;
}

int CWebChannelsSetup::Show()
{
	item_offset = 0;
	std::list<std::string> webchannels = (webradio ? g_settings.webradio_xml : g_settings.webtv_xml);

	m = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_STREAMING, width, webradio ? MN_WIDGET_ID_WEBRADIOSETUP : MN_WIDGET_ID_WEBTVSETUP);
	m->addKey(CRCInput::RC_red, this, "d");
	m->addKey(CRCInput::RC_green, this, "a");
	m->addKey(CRCInput::RC_yellow, this, "e");
	m->addKey(CRCInput::RC_blue, this, "r");

	m->addIntroItems(webradio ? LOCALE_WEBRADIO_HEAD : LOCALE_WEBTV_HEAD);

	int shortcut = 1;

	CMenuForwarder *mf;

#if 0
	bool _mode_webtv = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv) &&
			   (!CZapit::getInstance()->GetCurrentChannel()->getScriptName().empty());

	bool _mode_webradio = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio) &&
			   (!CZapit::getInstance()->GetCurrentChannel()->getScriptName().empty());

	mf = new CMenuForwarder(LOCALE_LIVESTREAM_SCRIPTPATH, !_mode_webtv || !_mode_webradio, g_settings.livestreamScriptPath, this, "script_path", CRCInput::convertDigitToKey(shortcut++));
	m->addItem(mf);
#endif

	if (!webradio)
	{
		mf = new CMenuForwarder(LOCALE_LIVESTREAM_RESOLUTION, true, NULL, new CWebTVResolution(), NULL, CRCInput::convertDigitToKey(shortcut++));
		m->addItem(mf);

		m->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, webradio ? LOCALE_WEBRADIO_XML : LOCALE_WEBTV_XML));
	}

	// TODO: show/hide autoloaded content when switching g_settings.webradio/webtv_xml_auto
	CMenuOptionChooser *oc;
	char hint_text[1024];
	if (webradio)
	{
		snprintf(hint_text, sizeof(hint_text) - 1, g_Locale->getText(LOCALE_MENU_HINT_WEBRADIO_XML_AUTO), WEBRADIODIR, WEBRADIODIR_VAR);
		oc = new CMenuOptionChooser(LOCALE_WEBRADIO_XML_AUTO, &g_settings.webradio_xml_auto, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::convertDigitToKey(shortcut++));
	}
	else
	{
		snprintf(hint_text, sizeof(hint_text) - 1, g_Locale->getText(LOCALE_MENU_HINT_WEBTV_XML_AUTO), WEBTVDIR, WEBTVDIR_VAR);
		oc = new CMenuOptionChooser(LOCALE_WEBTV_XML_AUTO, &g_settings.webtv_xml_auto, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this, CRCInput::convertDigitToKey(shortcut++));
	}
	oc->setHint("", hint_text);
	m->addItem(oc);
	m->addItem(GenericMenuSeparator);

	item_offset = m->getItemsCount();
	// show autoloaded webradio/webtvtv files
	for (std::list<std::string>::iterator it = webchannels.begin(); it != webchannels.end(); ++it)
	{
		if (webchannels_autodir((*it)))
			m->addItem(new CMenuForwarder(*it, false, "auto"));
	}
	if (item_offset < m->getItemsCount())
		m->addItem(GenericMenuSeparator);

	item_offset = m->getItemsCount();
	// show users webradio/webtv files
	for (std::list<std::string>::iterator it = webchannels.begin(); it != webchannels.end(); ++it)
	{
		if (!webchannels_autodir((*it)))
			m->addItem(new CMenuForwarder(*it, true, NULL, this, "c"));
	}

	m->setFooter(CWebChannelsSetupFooterButtons, CWebChannelsSetupFooterButtonCount);

	int res = m->exec(NULL, "");
	m->hide();

	if (changed)
	{
		CHintBox hint(LOCALE_MESSAGEBOX_INFO, LOCALE_SERVICEMENU_RELOAD_HINT);
		hint.paint();
		webchannels.clear();
		for (int i = item_offset; i < m->getItemsCount(); i++)
		{
			CMenuItem *item = m->getItem(i);
			CMenuForwarder *f = static_cast<CMenuForwarder *>(item);
			webchannels.push_back(f->getName());
		}
		if (webradio)
		{
			g_settings.webradio_xml.clear();
			g_settings.webradio_xml = webchannels;
		}
		else
		{
			g_settings.webtv_xml.clear();
			g_settings.webtv_xml = webchannels;
		}
		webchannels_auto();
		g_Zapit->reinitChannels();
		CNeutrinoApp::getInstance()->xmltv_xml_auto_readepg();
		changed = false;
		hint.hide();
	}

	delete m;
	return res;
}

bool CWebChannelsSetup::changeNotify(const neutrino_locale_t OptionName, void */*data*/)
{
	int ret = menu_return::RETURN_NONE;

	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_WEBTV_XML_AUTO) || ARE_LOCALES_EQUAL(OptionName, LOCALE_WEBRADIO_XML_AUTO))
	{
		changed = true;
		ret = menu_return::RETURN_REPAINT;
	}

	return ret;
}


int filefilter(const struct dirent *entry)
{
	std::string f = entry->d_name;

	int len = strlen(entry->d_name);
	if (len > 3 && (
		   (entry->d_name[len - 3] == 'x' && entry->d_name[len - 2] == 'm' && entry->d_name[len - 1] == 'l')
		|| (entry->d_name[len - 3] == 'm' && entry->d_name[len - 2] == '3' && entry->d_name[len - 1] == 'u')
		|| (                                 entry->d_name[len - 2] == 't' && entry->d_name[len - 1] == 'v')
		)
	)
		return 1;
	return 0;
}

// webradio wrapper for webchannels_auto()
void CWebChannelsSetup::webradio_xml_auto()
{
	webradio = true;
	webchannels_auto();
}

// webtv wrapper for webchannels_auto()
void CWebChannelsSetup::webtv_xml_auto()
{
	webradio = false;
	webchannels_auto();
}

void CWebChannelsSetup::webchannels_auto()
{
	std::list<std::string> webchannels;
	const char *dirs[2];

	if (webradio)
	{
		if (!g_settings.webradio_xml_auto)
			return;

		webchannels = g_settings.webradio_xml;
		dirs[0] = WEBRADIODIR_VAR;
		dirs[1] = WEBRADIODIR;
	}
	else
	{
		if (!g_settings.webtv_xml_auto)
			return;

		webchannels = g_settings.webtv_xml;
		dirs[0] = WEBTVDIR_VAR;
		dirs[1] = WEBTVDIR;
	}

	struct dirent **filelist;
	char webchannel_file[1024] = {0};
	for (int i = 0; i < 2; i++)
	{
		int file_count = scandir(dirs[i], &filelist, filefilter, alphasort);
		if (file_count > -1)
		{
			for (int count = 0; count < file_count; count++)
			{
				snprintf(webchannel_file, sizeof(webchannel_file), "%s/%s", dirs[i], filelist[count]->d_name);
				if (file_size(webchannel_file))
				{
					bool found = false;
					for (std::list<std::string>::iterator it = webchannels.begin(); it != webchannels.end(); it++)
						found |= ((*it).find(filelist[count]->d_name) != std::string::npos);

					if (!found)
					{
						printf("[CWebChannelsSetup] loading: %s\n", webchannel_file);
						if (webradio)
							g_settings.webradio_xml.push_back(webchannel_file);
						else
							g_settings.webtv_xml.push_back(webchannel_file);
					}
					else
					{
						printf("[CWebChannelsSetup] skipping: %s\n", webchannel_file);
					}
				}
				free(filelist[count]);
			}
			free(filelist);
		}
	}
}

// webradio wrapper for webchannels_autodir()
bool CWebChannelsSetup::webradio_xml_autodir(std::string directory)
{
	webradio = true;
	return webchannels_autodir(directory);
}

// webtv wrapper for webchannels_autodir()
bool CWebChannelsSetup::webtv_xml_autodir(std::string directory)
{
	webradio = false;
	return webchannels_autodir(directory);
}

bool CWebChannelsSetup::webchannels_autodir(std::string directory)
{
	if (webradio)
	{
		if (
			   (directory.find(WEBRADIODIR) != std::string::npos)
			|| (directory.find(WEBRADIODIR_VAR) != std::string::npos)
		)
			return true;
	}
	else
	{
		if (
			   (directory.find(WEBTVDIR) != std::string::npos)
			|| (directory.find(WEBTVDIR_VAR) != std::string::npos)
		)
			return true;
	}
	return false;
}

int xml_filter(const struct dirent *entry)
{
	int len = strlen(entry->d_name);
	if (len > 3 && entry->d_name[len-3] == 'x' && entry->d_name[len-2] == 'm' && entry->d_name[len-1] == 'l')
		return 1;
	return 0;
}


/* ## CWebTVResolution ############################################# */

CWebTVResolution::CWebTVResolution()
{
	width = 40;
}

const CMenuOptionChooser::keyval_ext LIVESTREAM_RESOLUTION_OPTIONS[] =
{
#if !HAVE_CST_HARDWARE
	{ 3840, NONEXISTANT_LOCALE, "3840x2160" },
	{ 2560, NONEXISTANT_LOCALE, "2560x1440" },
#endif
	{ 1920, NONEXISTANT_LOCALE, "1920x1080" },
	{ 1280, NONEXISTANT_LOCALE, "1280x720"  },
	{ 854,  NONEXISTANT_LOCALE, "854x480"   },
	{ 640,  NONEXISTANT_LOCALE, "640x360"   },
	{ 480,  NONEXISTANT_LOCALE, "480x270"   }
};
#define LIVESTREAM_RESOLUTION_OPTION_COUNT (sizeof(LIVESTREAM_RESOLUTION_OPTIONS)/sizeof(CMenuOptionChooser::keyval_ext))

int CWebTVResolution::exec(CMenuTarget *parent, const std::string & /*actionKey*/)
{
	if (parent)
		parent->hide();

	return Show();
}

int CWebTVResolution::Show()
{
	m = new CMenuWidget(LOCALE_WEBTV_HEAD, NEUTRINO_ICON_STREAMING, width, MN_WIDGET_ID_LIVESTREAM_RESOLUTION);
	m->addIntroItems(LOCALE_LIVESTREAM_HEAD);

	CMenuOptionChooser *mc;
	mc = new CMenuOptionChooser(LOCALE_LIVESTREAM_RESOLUTION, &g_settings.livestreamResolution,
				    LIVESTREAM_RESOLUTION_OPTIONS, LIVESTREAM_RESOLUTION_OPTION_COUNT,
				    true, NULL, CRCInput::RC_nokey, NULL, true);
	m->addItem(mc);

	int oldRes = g_settings.livestreamResolution;
	int res = m->exec(NULL, "");
	m->hide();
	delete m;

	bool _mode_webtv = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv) &&
			   (!CZapit::getInstance()->GetCurrentChannel()->getScriptName().empty());
	if (oldRes != g_settings.livestreamResolution && _mode_webtv)
	{
		CZapitChannel *cc = CZapit::getInstance()->GetCurrentChannel();
		if (cc && IS_WEBCHAN(cc->getChannelID()))
		{
			CMoviePlayerGui::getInstance().stopPlayBack();
			CMoviePlayerGui::getInstance().PlayBackgroundStart(cc->getUrl(), cc->getName(), cc->getChannelID(), cc->getScriptName());
		}
	}

	return res;
}

const char *CWebTVResolution::getResolutionValue()
{
	for (unsigned int i = 0; i < LIVESTREAM_RESOLUTION_OPTION_COUNT; ++i)
	{
		if (g_settings.livestreamResolution == LIVESTREAM_RESOLUTION_OPTIONS[i].key)
			return LIVESTREAM_RESOLUTION_OPTIONS[i].valname;
	}
	return "";
}
