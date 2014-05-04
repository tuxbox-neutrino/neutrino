/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	OPKG-Manager Class for Neutrino-GUI

	Implementation:
	Copyright (C) 2012 T. Graf 'dbt'
	www.dbox2-tuning.net

	Adaptions:
	Copyright (C) 2013 martii
	gitorious.org/neutrino-mp/martiis-neutrino-mp

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

#include <gui/opkg_manager.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/shellwindow.h>
#include <driver/screen_max.h>

#include <system/debug.h>
#include <system/helpers.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <alloca.h>

#define OPKG_CL find_executable("opkg-cl")
#define OPKG_KEY find_executable("opkg-key")

enum
{
	OM_LIST,
	OM_LIST_INSTALLED,
	OM_LIST_UPGRADEABLE,
	OM_UPDATE,
	OM_UPGRADE,
	OM_REMOVE,
	OM_INFO,
	OM_INSTALL,
	OM_MAX
};

static const std::string pkg_types[OM_MAX] =
{
	OPKG_CL + " list",
	OPKG_CL + " list-installed",
	OPKG_CL + " list-upgradable",
	OPKG_CL + " update",
	OPKG_CL + " upgrade ",
	OPKG_CL + " remove ",
	OPKG_CL + " info ",
	OPKG_CL + " install "
};

COPKGManager::COPKGManager()
{
	width = w_max (80, 10); //%
	pkg_map.clear();
	list_installed_done = false;
	list_upgradeable_done = false;
	expert_mode = false;
}

COPKGManager::~COPKGManager()
{
}

int COPKGManager::exec(CMenuTarget* parent, const std::string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (actionKey == "") {
		if (parent)
			parent->hide();
		return showMenu();
	}
	int selected = menu->getSelected() - menu_offset;

	if (expert_mode && actionKey == "rc_blue") {
		if (selected < 0 || selected >= (int) pkg_vec.size() || !pkg_vec[selected]->installed)
			return menu_return::RETURN_NONE;

		char loc[200];
		snprintf(loc, sizeof(loc), g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_REMOVE), pkg_vec[selected]->name.c_str());
		if (ShowMsg(LOCALE_OPKG_TITLE, loc, CMessageBox::mbrCancel, CMessageBox::mbYes | CMessageBox::mbCancel) != CMessageBox::mbrCancel) {
			if (parent)
				parent->hide();
			execCmd(pkg_types[OM_REMOVE] + pkg_vec[selected]->name, true, true);
			refreshMenu();
		}
		return res;
	}
	if (actionKey == "rc_info") {
		if (selected < 0 || selected >= (int) pkg_vec.size())
			return menu_return::RETURN_NONE;
		if (parent)
			parent->hide();
		execCmd(pkg_types[OM_INFO] + pkg_vec[selected]->name, true, true);
		return res;
	}
	if (actionKey == "rc_red") {
		expert_mode = !expert_mode;
		updateMenu();
		return res;
	}
	if(actionKey == pkg_types[OM_UPGRADE]) {
		if (parent)
			parent->hide();
		int r = execCmd(actionKey, true, true);
		if (r) {
			std::string loc = g_Locale->getText(LOCALE_OPKG_FAILURE_UPGRADE);
			char rs[strlen(loc.c_str()) + 20];
			snprintf(rs, sizeof(rs), loc.c_str(), r);
			DisplayInfoMessage(rs);
		} else
			installed = true;
		refreshMenu();
		g_RCInput->postMsg((neutrino_msg_t) CRCInput::RC_up, 0);
		return res;
	}

	std::map<string, struct pkg>::iterator it = pkg_map.find(actionKey);
	if (it != pkg_map.end()) {
		if (parent)
			parent->hide();
		std::string force = "";
		if (it->second.installed && !it->second.upgradable) {
			char l[200];
			snprintf(l, sizeof(l), g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_REINSTALL), actionKey.c_str());
			l[sizeof(l) - 1] = 0;
			if (ShowMsg(LOCALE_OPKG_TITLE, l, CMessageBox::mbrCancel, CMessageBox::mbYes | CMessageBox::mbCancel) == CMessageBox::mbrCancel)
				return res;
			force = "--force-reinstall ";
		}
		int r = execCmd(pkg_types[OM_INSTALL] + force + actionKey, true, true);
		if (r) {
			std::string err = g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL);
			char rs[strlen(err.c_str()) + 20];
			snprintf(rs, sizeof(rs), err.c_str(), r);
			DisplayInfoMessage(rs);
		} else
				installed = true;
		refreshMenu();
	}
	return res;
}

#define COPKGManagerFooterButtonCount 3
static const struct button_label COPKGManagerFooterButtons[COPKGManagerFooterButtonCount] = {
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_OPKG_BUTTON_EXPERT_ON },
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_OPKG_BUTTON_INFO },
	{ NEUTRINO_ICON_BUTTON_OKAY,	   LOCALE_OPKG_BUTTON_INSTALL }
};
#define COPKGManagerFooterButtonCountExpert 4
static const struct button_label COPKGManagerFooterButtonsExpert[COPKGManagerFooterButtonCountExpert] = {
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_OPKG_BUTTON_EXPERT_OFF },
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_OPKG_BUTTON_INFO },
	{ NEUTRINO_ICON_BUTTON_OKAY,	   LOCALE_OPKG_BUTTON_INSTALL },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_OPKG_BUTTON_UNINSTALL }
};

void COPKGManager::updateMenu()
{
	bool upgradesAvailable = false;
	getPkgData(OM_LIST_INSTALLED);
	getPkgData(OM_LIST_UPGRADEABLE);
	for (std::map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); it++) {
		it->second.forwarder->iconName_Info_right = "";
		it->second.forwarder->setActive(true);
		if (it->second.upgradable) {
			it->second.forwarder->iconName_Info_right = NEUTRINO_ICON_WARNING;
			upgradesAvailable = true;
		} else if (it->second.installed) {
			it->second.forwarder->iconName_Info_right = NEUTRINO_ICON_CHECKMARK;
			it->second.forwarder->setActive(expert_mode);
		}
	}

	upgrade_forwarder->setActive(upgradesAvailable);

	if (expert_mode)
		menu->setFooter(COPKGManagerFooterButtonsExpert, COPKGManagerFooterButtonCountExpert);
	else
		menu->setFooter(COPKGManagerFooterButtons, COPKGManagerFooterButtonCount);
}

void COPKGManager::refreshMenu() {
	list_installed_done = false,
	list_upgradeable_done = false;
	updateMenu();
}

int COPKGManager::showMenu()
{
	installed = false;

	int r = execCmd(pkg_types[OM_UPDATE]);
	if (r) {
		std::string loc = g_Locale->getText(LOCALE_OPKG_FAILURE_UPDATE);
		char rs[strlen(loc.c_str()) + 20];
		snprintf(rs, sizeof(rs), loc.c_str(), r);
		DisplayInfoMessage(rs);
	}

	getPkgData(OM_LIST);
	getPkgData(OM_LIST_UPGRADEABLE);

	menu = new CMenuWidget(g_Locale->getText(LOCALE_OPKG_TITLE), NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE);
	CMenuForwarder menuBack(LOCALE_MENU_BACK, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_LEFT, NULL, true);
	menuBack.setItemButton(!g_settings.menu_left_exit ? NEUTRINO_ICON_BUTTON_HOME : NEUTRINO_ICON_BUTTON_LEFT);
	menu->addItem(&menuBack);
	menuBack.setHint(NEUTRINO_ICON_HINT_BACK, LOCALE_MENU_HINT_BACK_BRIEF);
	menu->addItem(GenericMenuSeparatorLine);

	upgrade_forwarder = new CMenuForwarder(LOCALE_OPKG_UPGRADE, true, NULL , this, pkg_types[OM_UPGRADE].c_str(), CRCInput::RC_red);
	upgrade_forwarder->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_OPKG_UPGRADE);
	menu->addItem(upgrade_forwarder);
	menu->addItem(GenericMenuSeparatorLine);

	menu_offset = menu->getItemsCount();

	menu->addKey(CRCInput::RC_info, this, "rc_info");
	menu->addKey(CRCInput::RC_blue, this, "rc_blue");
	menu->addKey(CRCInput::RC_red, this, "rc_red");

	pkg_vec.clear();
	for (std::map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); it++) {
		it->second.forwarder = new CMenuForwarder(it->second.desc, true, NULL , this, it->second.name.c_str());
		it->second.forwarder->setHint("", it->second.desc);
		menu->addItem(it->second.forwarder);
		pkg_vec.push_back(&it->second);
	}

	updateMenu();

	int res = menu->exec(NULL, "");

	menu->hide ();

	if (installed)
		DisplayInfoMessage(g_Locale->getText(LOCALE_OPKG_SUCCESS_INSTALL));
	delete menu;
	return res;
}

bool COPKGManager::hasOpkgSupport()
{
	string deps[] = {OPKG_CL, OPKG_KEY, "/etc/opkg/opkg.conf", "/var/lib/opkg"};

	for(size_t i=0; i<sizeof(deps)/sizeof(deps[0]) ;i++){
		dprintf(DEBUG_INFO,  "[neutrino opkg] check if %s is available...\n", deps[i].c_str());
		if(access(deps[i].c_str(), R_OK) !=0) {
			dprintf(DEBUG_INFO,  "[neutrino opkg] %s not found\n", deps[i].c_str());
			return false;
		}
	}

	return true;
}

void COPKGManager::getPkgData(const int pkg_content_id)
{
	dprintf(DEBUG_INFO, "COPKGManager: executing %s\n", pkg_types[pkg_content_id].c_str());

	switch (pkg_content_id) {
		case OM_LIST:
			pkg_map.clear();
			list_installed_done = false;
			list_upgradeable_done = false;
			break;
		case OM_LIST_INSTALLED:
			if (list_installed_done)
				return;
			list_installed_done = true;
			for (std::map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); it++)
				it->second.installed = false;
			break;
		case OM_LIST_UPGRADEABLE:
			if (list_upgradeable_done)
				return;
			list_upgradeable_done = true;
			for (std::map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); it++)
				it->second.upgradable = false;
			break;
	}

	FILE *f = popen(pkg_types[pkg_content_id].c_str(), "r");
	if (!f) {
		DisplayInfoMessage("Command failed");
		return;
	}

	char buf[256];

	while (fgets(buf, sizeof(buf), f))
	{
		std::string line(buf);
		trim(line);

		std::string name = getBlankPkgName(line);

		switch (pkg_content_id) {
			case OM_LIST: {
				pkg_map[name] = pkg(name, line);
				break;
			}
			case OM_LIST_INSTALLED: {
				std::map<string, struct pkg>::iterator it = pkg_map.find(name);
				if (it != pkg_map.end())
					it->second.installed = true;
				break;
			}
			case OM_LIST_UPGRADEABLE: {
				std::map<string, struct pkg>::iterator it = pkg_map.find(name);
				if (it != pkg_map.end())
					it->second.upgradable = true;
				break;
			}
			default:
				fprintf(stderr, "%s %s %d: unrecognized content id %d\n", __FILE__, __func__, __LINE__, pkg_content_id);
				break;
		}
	}

	pclose(f);
}

std::string COPKGManager::getBlankPkgName(const std::string& line)
{
	size_t l_pos = line.find(" ");
	if (l_pos != string::npos)
		return line.substr(0, l_pos);
	return line;
}

int COPKGManager::execCmd(const char *cmdstr, bool verbose, bool acknowledge)
{
fprintf(stderr, "execCmd(%s)\n", cmdstr);
	std::string cmd(cmdstr);
	if (verbose) {
		cmd += " 2>&1";
		int res;
		CShellWindow(cmd, (verbose ? CShellWindow::VERBOSE : 0) | (acknowledge ? CShellWindow::ACKNOWLEDGE : 0), &res);
		return res;
	} else {
		cmd += " 2>/dev/null >&2";
		int r = system(cmd.c_str());
		if (r == -1)
			return r;
		return WEXITSTATUS(r);
	}
}
