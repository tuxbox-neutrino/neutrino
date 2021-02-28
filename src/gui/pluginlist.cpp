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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/pluginlist.h>
#include <gui/plugins.h>
#include <gui/components/cc.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/icons.h>

#include <sstream>
#include <fstream>
#include <iostream>

#include <dirent.h>
#include <dlfcn.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <global.h>
#include <neutrino.h>
#include <driver/screen_max.h>
#include <driver/fade.h>

#include <zapit/client/zapittools.h>
#include <system/helpers.h>

extern CPlugins       * g_Plugins;    /* neutrino.cpp */

CPluginList::CPluginList(const neutrino_locale_t Title, const uint32_t listtype)
{
	title = Title;
	pluginlisttype = listtype;
	width = 40;
	selected = -1;
	number = -1;
}

int CPluginList::run()
{
	int ret = g_Plugins->startPlugin(number);
	if (!g_Plugins->getScriptOutput().empty()) {
		hide();
		ShowMsg(LOCALE_PLUGINS_RESULT, g_Plugins->getScriptOutput(), CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_SHELL, 320, g_settings.timing[SNeutrinoSettings::TIMING_STATIC_MESSAGES]);
	}
	return ret;
}

int CPluginList::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if (parent)
		parent->hide();

	CColorKeyHelper keyhelper;
	neutrino_msg_t key = CRCInput::RC_nokey;
	const char * dummy = NULL;

	number = -1;
	if (!actionKey.empty())
		number = atoi(actionKey.c_str());

	if (number > -1)
		return run();

	const char *icon = "";
	if (pluginlisttype == CPlugins::P_TYPE_GAME)
		icon = NEUTRINO_ICON_GAMES;
	else
		icon = NEUTRINO_ICON_SHELL;

	CMenuWidget m(title, icon, width);
	m.setSelected(selected);
	m.addIntroItems();

	int nop = g_Plugins->getNumberOfPlugins();

	for(int count = 0; count < nop; count++) {
		if ((g_Plugins->getType(count) & pluginlisttype) && !g_Plugins->isHidden(count) && (g_Plugins->getIntegration(count) == PLUGIN_INTEGRATION_DISABLED)) {
			neutrino_msg_t d_key = g_Plugins->getKey(count);
			keyhelper.get(&key, &dummy, d_key);
			CMenuForwarder *f = new CMenuForwarder(std::string(g_Plugins->getName(count)), true, NULL, this, to_string(count).c_str(), key);
			f->setHint(g_Plugins->getHintIcon(count), g_Plugins->getDescription(count));
			m.addItem(f);
		}
	}
	int res = m.exec(NULL, "");
	m.hide();
	selected = m.getSelected();

	return res;
}

CPluginChooser::CPluginChooser(const neutrino_locale_t Name, const uint32_t listtype, std::string &selectedFile) : CPluginList(Name, listtype)
{
	selectedFilePtr = &selectedFile;
}

int CPluginChooser::run()
{
	if (number > -1)
		*selectedFilePtr = g_Plugins->getFileName(number);
	return menu_return::RETURN_EXIT;
}

CPluginsExec* CPluginsExec::getInstance()
{
	static CPluginsExec* pluginsExec = NULL;

	if (!pluginsExec)
		pluginsExec = new CPluginsExec();

	return pluginsExec;
}

int CPluginsExec::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int ret = menu_return::RETURN_REPAINT;

	if (actionKey.empty())
		return menu_return::RETURN_NONE;

	//printf("CPluginsExec exec: %s\n", actionKey.c_str());
	int sel = atoi(actionKey.c_str());

	if (parent != NULL)
		parent->hide();

	if (actionKey == "teletext") {
		g_RCInput->postMsg(CRCInput::RC_timeout, 0);
		g_RCInput->postMsg(CRCInput::RC_text, 0);
		return menu_return::RETURN_EXIT;
	}
	else if (sel >= 0)
		ret = g_Plugins->startPlugin(sel);

	if (!g_Plugins->getScriptOutput().empty())
		ShowMsg(LOCALE_PLUGINS_RESULT, g_Plugins->getScriptOutput(), CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_SHELL);

	if (g_Plugins->getIntegration(sel) == PLUGIN_INTEGRATION_DISABLED)
		return menu_return::RETURN_EXIT;

	return ret;
}
