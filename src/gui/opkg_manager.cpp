 /*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	OPKG-Manager Class for Neutrino-GUI

	Implementation:
	Copyright (C) 2012-2023 T. Graf 'dbt'

	Adaptions:
	Copyright (C) 2013 martii
	gitorious.org/neutrino-mp/martiis-neutrino-mp
	Copyright (C) 2015-2017 Stefan Seyfried

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

#include "opkg_manager.h"
#include "widget/termwindow.h"
/* hack, so we don't have to change all code below */
#define CShellWindow CTermWindow

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include "filebrowser.h"

#include "widget/icons.h"
#include "widget/keyboard_input.h"
#include "widget/msgbox.h"

#include <system/debug.h>
#include <system/helpers.h>

#include <driver/screen_max.h>

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <unistd.h>

#if 1
#define OPKG "opkg"
#else
#define OPKG "opkg-cl"
#endif

#if 0
#define OPKG_CONFIG_OPTIONS " -V2 --tmp-dir=/tmp --cache=" OPKG_TMP_DIR
#else
#define OPKG_CONFIG_OPTIONS ""
#endif

#define OPKG_BAD_PATTERN_LIST_FILE CONFIGDIR "/bad_package_pattern.list"
#define OPKG_GOOD_PATTERN_LIST_FILE CONFIGDIR "/good_package_pattern.list"

/* script to call instead of "opkg upgrade"
 * opkg fails to gracefully self-upgrade, and additionally has some ordering issues
 */
#define SYSTEM_UPDATE "system-update"

#define INFOBAR_TXT_FILE "/tmp/infobar.txt"

using namespace std;

enum
{
	CMD_LIST,
	CMD_LIST_INSTALLED,
	CMD_LIST_UPGRADEABLE,
	CMD_UPDATE,
	CMD_UPGRADE,
	CMD_REMOVE,
	CMD_INFO,
	CMD_INSTALL,
	CMD_STATUS,
	CMD_CONFIGURE,
	CMD_DOWNLOAD,
	CMD_CLEAN,
	CMD_MAX
};

static string pm_cmd[CMD_MAX] =
{
	OPKG " list ",
	OPKG " list-installed ",
	OPKG " list-upgradable ",
	OPKG " -A update ",
	OPKG OPKG_CONFIG_OPTIONS " upgrade ",
	OPKG OPKG_CONFIG_OPTIONS " remove ",
	OPKG " info ",
	OPKG OPKG_CONFIG_OPTIONS " install ",
	OPKG " status ",
	OPKG " configure ",
	OPKG " download ",
	OPKG " clean "
};

COPKGManager::COPKGManager(int wizard_mode)
{
	init(wizard_mode);
}

void COPKGManager::init(int wizard_mode)
{
	if (!hasOpkgSupport())
		return;

	is_wizard = wizard_mode;
	OPKG_ERRORS();
	width = 100;
	menu = NULL;
	pkg_map.clear();
	expert_mode = false;
	local_dir = &g_settings.update_dir_opkg;
	initPackagePatternLists();

	hintBox = new CLoaderHint(LOCALE_OPKG_UPDATE_CHECK);
	silent = false;
	menu_used = false;
	num_updates = 0;
}

COPKGManager::~COPKGManager()
{
	if (menu_used)
	{
		// TODO: Show message only if the waiting time is too long
		hintBox->setMsgText(LOCALE_OPKG_MESSAGEBOX_PLEASE_WAIT);
		hintBox->paint();
		if (menu)
			delete menu;
	}
	pkg_map.clear();
	execCmd(pm_cmd[CMD_CLEAN], CShellWindow::QUIET);
	hintBox->hide();
	delete hintBox;
}

int COPKGManager::exec(CMenuTarget* parent, const string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	removeInfoBarTxt();

	if (actionKey.empty())
	{
		if (parent)
			parent->hide();
		int ret = initMenu();
		menu_used = true;
		return ret;
	}

	int selected = menu->getSelected();
	std::string pkg_name = std::string(menu->getItem(selected)->getName());

	// Toggle expert mode
	if (actionKey == "rc_yellow")
	{
		expert_mode = !expert_mode;

		// Show message while reloading menu
		hintBox->setMsgText(LOCALE_OPKG_UPDATE_READING_LISTS);
		hintBox->paint();

		updateMenu();
		menu->setSelectedByName(pkg_name);

		// Close message
		hintBox->hide();
		return res;
	}

	// Install local/3rd party package
	if (actionKey == "rc_green")
	{
		if (parent)
			parent->hide();

		CFileFilter fileFilter;
		string filters[] = {"opk", "ipk"};
		for(size_t i=0; i<sizeof(filters)/sizeof(filters[0]) ;i++)
			fileFilter.addFilter(filters[i]);

		CFileBrowser fileBrowser;
		fileBrowser.Filter = &fileFilter;

		if (fileBrowser.exec((*local_dir).c_str()))
		{
			string package = fileBrowser.getSelectedFile()->Name;
			if (!installPackage(package))
				showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), NULL, package);
				/* errno is never set properly, the string is totally useless.
				showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), strerror(errno), package);
				 */

			*local_dir = fileBrowser.getCurrentDir();

			// Show message while reloading package list
			hintBox->setMsgText(LOCALE_OPKG_UPDATE_READING_LISTS);
			hintBox->paint();

			pullPkgData();
			updateMenu();

			// Close message
			hintBox->hide();
		}
		return res;
	}

	// Upgrade system
	if(actionKey == "rc_red")
	{
		if (parent)
			parent->hide();

		int r = execCmd(pm_cmd[CMD_UPGRADE], CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT);
		if (r)
		{
			/* errno is never set properly, the string is totally useless.
			showError(g_Locale->getText(LOCALE_OPKG_FAILURE_UPGRADE), strerror(errno), actionKey);
			 */
			showError(g_Locale->getText(LOCALE_OPKG_FAILURE_UPGRADE), NULL, pm_cmd[CMD_UPGRADE]);
		}
		else
		{
			installed = true;
		}

		// Show message while reloading package list
		hintBox->setMsgText(LOCALE_OPKG_UPDATE_READING_LISTS);
		hintBox->paint();

		// Reloading package lists
		pullPkgData();
		updateMenu();

		// Close message
		hintBox->hide();

		return res;
	}

	// Get package name for further processing
	std::string pkg_info = getPkgInfo(pkg_name);

	// Package install
	if (actionKey == pkg_name) // actionKey = package name
	{
		// Initialize message to choose required action.
		CMsgBox msgBox(pkg_info.c_str(), pkg_name.c_str());
		msgBox.setDefaultResult(CMsgBox::mbrBack);

		// We use properties of buttons and
		// create custom alias text for buttons. This saves us unnecessary message text.
		if (!pkg_map[pkg_name].installed)
		{
			// allow install
			msgBox.setShowedButtons(CMsgBox::mbOk | CMsgBox::mbBack);
			msgBox.setButtonText(CMsgBox::mbOk, LOCALE_OPKG_BUTTON_INSTALL);
		}
		else
		{
			// allow uninstall...
			std::string section = pkg_map[pkg_name].section;
			if (!expert_mode)
			{
				// Allow uninstall of optional packages independent of expert mode
				if (section == "neutrino-plugin" || section == "optional")
				{	msgBox.setShowedButtons(CMsgBox::mbYes | CMsgBox::mbNo | CMsgBox::mbBack);
					msgBox.setButtonText(CMsgBox::mbNo, LOCALE_OPKG_BUTTON_UNINSTALL);
				}
				else
					msgBox.setShowedButtons(CMsgBox::mbBack | CMsgBox::mbYes);
			}
			else
			{
				// Modify uninstall function with expert mode, some system packages should not be touched!
				if (section.find(" user ") != std::string::npos)
				{
					// Allow uninstall with expert mode
					msgBox.setShowedButtons(CMsgBox::mbYes | CMsgBox::mbNo | CMsgBox::mbBack);
					msgBox.setButtonText(CMsgBox::mbNo, LOCALE_OPKG_BUTTON_UNINSTALL);
				}
				else
				{
					// Disallow uninstall with expert mode
					msgBox.setShowedButtons(CMsgBox::mbBack | CMsgBox::mbYes);
				}
			}
			msgBox.setButtonText(CMsgBox::mbYes, LOCALE_OPKG_BUTTON_REINSTALL);
		}

		// Show and execute message handler.
		msgBox.paint();
		res = msgBox.exec();
		msgBox.hide();

		// Deposit message result.
		int msg_result = msgBox.getResult();

		// Evaluate return value of the message.
		if (msg_result == CMsgBox::mbrBack)
			// User has chosen cancel, so we must exit here.
			return res;

		// User has chosen 'yes' (alias: install)
		if (msg_result == CMsgBox::mbrOk)
		{
			map<string, struct pkg>::iterator it = pkg_map.find(actionKey);
			if (it != pkg_map.end())
			{
				if (parent)
					parent->hide();

				// Install package
				installPackage(actionKey);
			}
		}
		else if (msg_result == CMsgBox::mbrYes || msg_result == CMsgBox::mbrNo)
		{
			// User has chosen 'no' (alias: uninstall)
			if (msg_result == CMsgBox::mbrNo)
			{
				std::string info = g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_REMOVE);
				size_t max_size = info.size() + pkg_name.size() + 1;
				char loc[max_size];
				snprintf(loc, max_size, info.c_str(), pkg_name.c_str());

				if (ShowMsg(LOCALE_OPKG_TITLE, loc, CMsgBox::mbrCancel, CMsgBox::mbYes | CMsgBox::mbCancel) != CMsgBox::mbrCancel)
				{
					if (parent)
						parent->hide();

					execCmd(pm_cmd[CMD_REMOVE] + pkg_name, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT);
				}
			}

			// If user wants to reinstall, set force parameter
			if (msg_result == CMsgBox::mbrYes)
			{
				// Init warn message
				char l[200];
				snprintf(l, sizeof(l), g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_REINSTALL), actionKey.c_str());
				l[sizeof(l) - 1] = 0;
				if (ShowMsg(LOCALE_OPKG_TITLE, l, CMsgBox::mbrCancel, CMsgBox::mbYes | CMsgBox::mbCancel) == CMsgBox::mbrCancel)
					return res;

				// Re-install package
				installPackage(actionKey, " --force-reinstall ");
			}

		}

		// Show message while reloading package list
		hintBox->setMsgText(LOCALE_OPKG_UPDATE_READING_LISTS);
		hintBox->paint();

		pullPkgData();
		updateMenu();
		menu->setSelectedByName(pkg_name);

		// Close message
		hintBox->hide();
		return res;
	}

	if (actionKey == "rc_blue")
	{
		// Show message while update
		hintBox->setMsgText(LOCALE_OPKG_UPDATE_CHECK);
		hintBox->paint();

		doUpdate();
		pullPkgData();

		hintBox->setMsgText(LOCALE_OPKG_UPDATE_READING_LISTS);
		updateMenu();

		menu->setSelected(selected);

		// Close message
		hintBox->hide();

		return res;
	}

	return res;
}

static const struct button_label COPKGManagerFooterButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_OPKG_UPGRADE },
	{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_OPKG_INSTALL_LOCAL_PACKAGE },
	{ NEUTRINO_ICON_BUTTON_YELLOW,	LOCALE_OPKG_BUTTON_EXPERT_ON },
	{ NEUTRINO_ICON_BUTTON_BLUE,	LOCALE_OPKG_BUTTON_UPDATE_CHECK }
};
size_t  COPKGManagerFooterButtonCount = sizeof(COPKGManagerFooterButtons)/sizeof(COPKGManagerFooterButtons[0]);

static const struct button_label COPKGManagerFooterButtonsExpert[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_OPKG_UPGRADE },
	{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_OPKG_INSTALL_LOCAL_PACKAGE },
	{ NEUTRINO_ICON_BUTTON_YELLOW,	LOCALE_OPKG_BUTTON_EXPERT_OFF },
	{ NEUTRINO_ICON_BUTTON_BLUE,	LOCALE_OPKG_BUTTON_UPDATE_CHECK }
};
size_t  COPKGManagerFooterButtonCountExpert = sizeof(COPKGManagerFooterButtonsExpert)/sizeof(COPKGManagerFooterButtonsExpert[0]);

void COPKGManager::initPackagePatternLists()
{
	v_bad_pattern.clear();
	v_bad_pattern = getBadPackagePatternList();
	v_good_pattern.clear();
	v_good_pattern = getGoodPackagePatternList();
}

vector<string> COPKGManager::getBadPackagePatternList()
{
	return COPKGManager::getPackagePatternList(OPKG_BAD_LIST);
}

vector<string> COPKGManager::getGoodPackagePatternList()
{
	return COPKGManager::getPackagePatternList(OPKG_GOOD_LIST);
}

vector<string> COPKGManager::getPackagePatternList(opkg_pattern_list_t type)
{
	vector<string> v_ret;
	std::string list_file = OPKG_BAD_PATTERN_LIST_FILE;

	if (type == OPKG_GOOD_LIST)
		list_file = OPKG_GOOD_PATTERN_LIST_FILE;

	ifstream in (list_file, ios::in);
	if (!in)
	{
		dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d] can't open %s, %s\n", __func__, __LINE__, OPKG_BAD_PATTERN_LIST_FILE, strerror(errno));
		return v_ret;
	}
	string line;

	while(getline(in, line))
		v_ret.push_back(line);

	in.close();

	return v_ret;
}

bool COPKGManager::isFilteredPackage(std::string &package_name, opkg_pattern_list_t type)
{
	std::vector<std::string> v_patterns;

	if (type == OPKG_BAD_LIST)
		v_patterns = v_bad_pattern;
	else if (type == OPKG_GOOD_LIST)
		v_patterns = v_good_pattern;

	if(v_patterns.empty())
		return false;

	size_t i;
	string st = "";
	for (i = 0; i < v_patterns.size(); i++)
	{
		string p = v_patterns[i];
		if (p.empty())
			continue;

		size_t patlen = p.length() - 1;
		bool res = false;

		/* poor man's regex :-) only supported are "^" and "$" */
		if (p.substr(patlen, 1) == "$")
		{ /* match at end */
			size_t pos = package_name.rfind(p.substr(0, patlen)); /* package_name.len-patlen can be -1 == npos */
			if (pos != string::npos && pos == (package_name.length() - patlen))
				res = true;
		}
		else if (p.substr(0, 1) == "^")
		{ /* match at beginning */
			if (package_name.find(p.substr(1)) != std::string::npos)
				res = true;
		}
		else
		{ /* match everywhere */
			if (package_name.find(p) != string::npos)
				res = true;
		}
		if (res)
			st += p + " ";
	}

	if (!st.empty()){
		dprintf(DEBUG_DEBUG, "[%s] filtered '%s' pattern(s) '%s'\n", __func__, package_name.c_str(), st.c_str());
		return true;
	}

	return false;
}

bool COPKGManager::isBadPackage(std::string &package_name)
{
	return isFilteredPackage(package_name, OPKG_BAD_LIST);
}

bool COPKGManager::isGoodPackage(std::string &package_name)
{
	return isFilteredPackage(package_name, OPKG_GOOD_LIST);
}

bool COPKGManager::isPermittedPackage(std::string &package_name)
{
	bool ret = (!isBadPackage(package_name));

	if (isBadPackage(package_name) && !isGoodPackage(package_name))
		ret = false;

// 	if (isGoodPackage(package_name))
// 		ret = true;

	if (isBadPackage(package_name) && isGoodPackage(package_name))
		ret = true;


	return ret;
}

bool COPKGManager::removeInfoBarTxt()
{
	if (file_exists(INFOBAR_TXT_FILE))
	{
		//ensure remove infobar.txt with relevant content.
		std::string txt = readFile(INFOBAR_TXT_FILE);
		std::string update_text = g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_UPDATES_AVAILABLE);
		std::size_t found = txt.find(update_text);
		if (found != std::string::npos)
		{
			unlink(INFOBAR_TXT_FILE);
			return true;
		}
	}
	return false;
}

void COPKGManager::initUpdateMessage(bool enable_message)
{
	std::string update_count = to_string(num_updates) + " ";
	std::string update_text = g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_UPDATES_AVAILABLE);
	std::string update_msg = update_count + update_text;

	dprintf(DEBUG_NORMAL,"\033[32m[COPKGManager] [%s - %d] %s...\033[0m\n", __func__, __LINE__, update_msg.c_str());

	if (enable_message && !removeInfoBarTxt())
		DisplayInfoMessage(update_msg.c_str());

	fstream f;
	f.open(INFOBAR_TXT_FILE, ios::out);
	f << update_msg << endl;
	f.close();
}


void COPKGManager::setUpdateCheckResult(bool enable_message)
{
	std::lock_guard<std::mutex> g(opk_mutex);

	checkUpdates(enable_message);
	handleUpdateFlagFile();

	if (num_updates)
		initUpdateMessage(enable_message);
	else
		removeInfoBarTxt();
}


void COPKGManager::handleUpdateFlagFile()
{
	if (file_exists(HAS_PKG_UPDATE_FLAGFILE))
		unlink(HAS_PKG_UPDATE_FLAGFILE);

	if (num_updates)
	{
		fstream f;
		f.open(HAS_PKG_UPDATE_FLAGFILE, ios::out);
		f << num_updates << endl;
		f.close();
	}
}

int COPKGManager::getNumUpdates()
{
	return num_updates;
}

bool COPKGManager::checkUpdates(bool show_progress)
{
	if (!hasOpkgSupport() || file_exists("/run/opkg.lock"))
		return false;

	silent = !show_progress;

	// Updating available package lists from configured feeds
	doUpdate();

	// Grab package list data into pkg_map
	pullPkgData();

	return num_updates > 0;
}

int COPKGManager::doUpdate()
{
	int r = execCmd(pm_cmd[CMD_UPDATE], CShellWindow::QUIET);

	if (r)
	{
		string msg = string(g_Locale->getText(LOCALE_OPKG_FAILURE_UPDATE));
		msg += '\n' + terminal_str;
		if (!silent)
		{
			DisplayErrorMessage(msg.c_str());
		}
		return r;
	}

	initPackagePatternLists();

	return 0;
}

void COPKGManager::updateMenu()
{
	// Reset widget, we need a clean list
	if (!menu->getItems().empty())
		menu->resetWidget(true);

	// Set subhead title with short update information
	std::string subhead_txt = !expert_mode ? g_Locale->getText(LOCALE_OPKG_TITLE) : g_Locale->getText(LOCALE_OPKG_TITLE_EXPERT_MODE);
	if (num_updates)
	{
		subhead_txt += " - ";
		subhead_txt += to_string(num_updates) + " ";
		subhead_txt += g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_UPDATES_AVAILABLE);
	}
	menu->setSubheadText(subhead_txt);

	// Set wizard mode
	menu->setWizardMode(is_wizard);

	// We need some vectors which contains intro items and forwarders with different package modes
	std::vector<CMenuItem*> v_intro_items, v_upgradable, v_plugins_installed, v_plugins_not_installed, v_not_installed, v_installed;

	// Create forwarders for intro items. addIntroItems() is unsuited here for this plan
	// to assemble this widget, because we must work with each element individually.
	// This makes it easier to assemble the items after any menu content update.
	v_intro_items.push_back(GenericMenuSeparator);
	v_intro_items.push_back(GenericMenuBack);
// 	v_intro_items.push_back(GenericMenuSeparatorLine); // TODO: check if we need separator if there are no packages below, but most likely not

	// Add marker for descriptive separators,
	// only one separator is required for each package section.
	bool not_installed_marked = false;
	bool install_marked = false;
	bool upgrades_marked = false;
	bool install_marked_plugin = false;
	bool not_install_marked_plugin = false;

	// Creating forwarders with package contents
	for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it)
	{
		/* this should no longer trigger at all */
		if (!isPermittedPackage(it->second.name) && !it->second.upgradable)
			continue;

		// Create raw forwarders
		it->second.forwarder = new CMenuForwarder(it->second.name, true, it->second.version.c_str() , this, it->second.name.c_str());

		// Modify forwarder content, we fill vectors with packages in dependence of install mode and plugin status
		if (it->second.upgradable)
		{
			// Here we fill vector v_upgradable with upgradable and permitted packages
			if (!upgrades_marked)
			{
				// Add descriptive separator
				v_upgradable.push_back(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_OPKG_SEPARATOR_UPGRADES_AVAILABLE));
				upgrades_marked = true;
			}
			// Add upgradeable packages
			it->second.forwarder->iconName_Info_right = NEUTRINO_ICON_MARKER_UPDATE_AVAILABLE;
			v_upgradable.push_back(it->second.forwarder);
		}
		else if (it->second.installed)
		{
			// Here we fill vectors  with installed and permitted packages
			if (it->second.section == "neutrino-plugin")
			{
				// Add descriptive section separator for installed plugins
				if (!install_marked_plugin)
				{
					v_plugins_installed.push_back(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_OPKG_SEPARATOR_PACKAGES_INSTALLED_PLUGINS));
					install_marked_plugin = true;
				}
				// Add installed plugin package
				v_plugins_installed.push_back(it->second.forwarder);
			}
			else
			{
				// Add descriptive section separator for other installed packages
				if (!install_marked)
				{
					v_installed.push_back(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_OPKG_SEPARATOR_PACKAGES_INSTALLED));
					install_marked = true;
				}
				// Add installed package
				v_installed.push_back(it->second.forwarder);
			}
			it->second.forwarder->iconName_Info_right = NEUTRINO_ICON_MARKER_DIALOG_OK;
		}
		else
		{
			if (it->second.section == "neutrino-plugin")
			{
				// Add descriptive section separator for available plugins
				if (!not_install_marked_plugin)
				{
					v_plugins_not_installed.push_back(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_OPKG_SEPARATOR_PACKAGES_AVAILABLE_PLUGINS));
					not_install_marked_plugin = true;
				}
				// Add available plugin package
				v_plugins_not_installed.push_back(it->second.forwarder);
			}
			else
			{
				// Add descriptive section separator for other available packages
				if (!not_installed_marked)
				{
					v_not_installed.push_back(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_OPKG_SEPARATOR_PACKAGES_AVAILABLE));
					not_installed_marked = true;
				}
				// Add available packages
				v_not_installed.push_back(it->second.forwarder);
			}
			it->second.forwarder->iconName_Info_right = NEUTRINO_ICON_MARKER_DOWNLOAD_LATER;
		}

		// Add hints with descriptions for packages
		it->second.forwarder->setHint("", it->second.desc);

		// Add package hint to forwarder description line.
		if (!it->second.hint.empty())
		{
			it->second.forwarder->setDescription(it->second.hint);
			it->second.forwarder->iconName_Info_right = NEUTRINO_ICON_MARKER_USER_BUSY;
		}

		// Here we fill the vector 'v_all_items' with all contents, which we created above.
		std::vector<CMenuItem*> v_all_items;
		v_all_items.reserve(v_intro_items.size() + v_upgradable.size() + v_installed.size() + v_not_installed.size()); // HINT: size of plugin vectors not relevant here

		// At 1st we add intro items...
		v_all_items.insert(v_all_items.end(), v_intro_items.begin(), v_intro_items.end());

		// ... section upgradeable packages
		v_all_items.insert(v_all_items.end(), v_upgradable.begin(), v_upgradable.end());

		// ... section installed and available plugins
		v_all_items.insert(v_all_items.end(), v_plugins_installed.begin(), v_plugins_installed.end());
		v_all_items.insert(v_all_items.end(), v_plugins_not_installed.begin(), v_plugins_not_installed.end());

		// ... all other installed and available packages
		v_all_items.insert(v_all_items.end(), v_installed.begin(), v_installed.end());
		v_all_items.insert(v_all_items.end(), v_not_installed.begin(), v_not_installed.end());

		// Finally we move all forwarders to current menu widget
		menu->getItems() = std::move(v_all_items);
	}

	// add footer with some buttons
	menu->addKey(CRCInput::RC_blue, this, "rc_blue");
	menu->addKey(CRCInput::RC_yellow, this, "rc_yellow");
	menu->addKey(CRCInput::RC_green, this, "rc_green");
	menu->addKey(CRCInput::RC_red, this, "rc_red");

	if (expert_mode)
		menu->setFooter(COPKGManagerFooterButtonsExpert, COPKGManagerFooterButtonCountExpert);
	else
		menu->setFooter(COPKGManagerFooterButtons, COPKGManagerFooterButtonCount);
}

int COPKGManager::initMenu()
{
	installed = false;

	hintBox->paint();
	setUpdateCheckResult(false); // without message

	if (menu == NULL)
	{
		menu = new CMenuWidget(g_Locale->getText(LOCALE_SERVICEMENU_UPDATE), NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE);
		updateMenu();
		menu->setSelected(1); //back-item in wizard mode next-item
	}

	hintBox->hide();

	int res = menu->exec(NULL, "");

	menu->hide();

	//handling after successful installation
	string exit_action = "";
	if (!has_err && installed)
	{
		/*
			Show a success message only if restart/reboot is required and user should decide what to do or not.
			NOTE: marker file should be generated by opkg package itself (eg. with preinstall scripts),
			so it's controlled by the package maintainer!
		*/
		//restart neutrino: user decision
		if(!access( "/tmp/.restart_neutrino", F_OK)){
			int msg = ShowMsg(LOCALE_OPKG_TITLE, g_Locale->getText(LOCALE_OPKG_SUCCESS_INSTALL), CMsgBox::mbrNo,
			CMsgBox::mbYes | CMsgBox::mbNo,
			NEUTRINO_ICON_QUESTION,
			width);
			if (msg == CMsgBox::mbrYes)
				exit_action = "restart";
		}
		//restart neutrino: forced
		if (!access( "/tmp/.force_restart_neutrino", F_OK))
			exit_action = "restart";
		//reboot stb: forced
		if (!access( "/tmp/.reboot", F_OK)){
			//ShowHint("", "Reboot ...", 300, 3); //TODO
			g_RCInput->postMsg( NeutrinoMessages::REBOOT, 0);
			res = menu_return::RETURN_EXIT_ALL;
		}
	}
	/* remove the package-generated files... */
	unlink("/tmp/.restart");
	unlink("/tmp/.force_restart");
	unlink("/tmp/.reboot");

	delete menu;
	menu = NULL;

	if (!exit_action.empty())
		CNeutrinoApp::getInstance()->exec(NULL, exit_action);

	return res;
}

bool COPKGManager::hasOpkgSupport()
{
	if (find_executable(OPKG).empty()) {
		dprintf(DEBUG_NORMAL, "[COPKGManager] [%s - %d]" OPKG " executable not found\n", __func__, __LINE__);
		return false;
	}

	if (find_executable(SYSTEM_UPDATE).empty())
		dprintf(DEBUG_NORMAL, "[COPKGManager] [%s - %d] NOTE: " SYSTEM_UPDATE " script not found\n", __func__, __LINE__);
	else
		pm_cmd[CMD_UPGRADE] = SYSTEM_UPDATE;

#if 0
	/* If directory /var/lib/opkg resp. /opt/opkg
	   does not exist, it is created by opkg itself */
	string deps[] = {"/var/lib/opkg", /*"/bin/opkg-check-config", "/bin/update-alternatives", "/share/opkg/intercept"*/};
	for(size_t i=0; i<sizeof(deps)/sizeof(deps[0]) ;i++){
		if(access(deps[i].c_str(), R_OK) !=0) {
			dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d] %s not found\n", __func__, __LINE__, deps[i].c_str());
			return false;
		}
	}
#endif
	return true;
}

void COPKGManager::pullPkgData()
{
	dprintf(DEBUG_NORMAL, "[COPKGManager] [%s - %d] current package entries = [%zu]\n", __func__, __LINE__, pkg_map.size());

	// for better readability, we "mask" std::string::npos here
	static const size_t npos = -1;

	// STEP 1: Create a pkg map with all permitted package names
	execCmd(pm_cmd[CMD_INFO], CShellWindow::QUIET);
	std::istringstream ss1(terminal_str);
	std::vector<std::string> v_lines;
	std::string l;

	// Temporarily read lines of terminal_str into vector
	while (std::getline(ss1, l))
		v_lines.push_back(l);

	// Iterate lines and generate package map with required details
	std::string name;
	for (const auto &line : v_lines)
	{
		if (line.empty())
			continue;

		// If the line begins with "Package: ", save the package name
		if (line.find("Package:") != npos)
		{
			name = line.substr(9);
			// Add a new package to the map
			pkg_map[name] = pkg(name);
			pkg_map[name].installed = false;
			pkg_map[name].upgradable = false;
		}
		// Add details to pkg_map
		else if (line.find("Version: ") != npos)
		{
			pkg_map[name].version = line.substr(9);
			// Save version info for neutrino into file, for usage in other classes
			if (name == "neutrino-mp")
			{
				std::ofstream outfile("/tmp/.neutrino.version");
				outfile << pkg_map[name].version;
				outfile.close();
			}
		}
		else if (line.find("Description: ") != npos)
			pkg_map[name].desc = line.substr(13);
		else if (line.find("Section: ") != npos)
			pkg_map[name].section = line.substr(9);
		else if (line.find("Priority: ") != npos)
			pkg_map[name].priority = line.substr(10);
		else if (line.find("Recommends: ") != npos)
			pkg_map[name].recommends = line.substr(12);
		else if (line.find("Status: ") != npos)
			pkg_map[name].status = line.substr(8);
		else if (line.find("Architecture: ") != npos)
			pkg_map[name].architecture = line.substr(14);
		else if (line.find("Maintainer: ") != npos)
			pkg_map[name].maintainer = line.substr(12);
		else if (line.find("MD5Sum: ") != npos)
			pkg_map[name].md5sum = line.substr(9);
		else if (line.find("Size: ") != npos)
			pkg_map[name].size = line.substr(6);
		else if (line.find("Filename: ") != npos)
			pkg_map[name].filename = line.substr(10);
		else if (line.find("Source: ") != npos)
			pkg_map[name].source = line.substr(8);
		else if (line.find("Homepage: ") != npos)
			pkg_map[name].homepage = line.substr(10);
		else if (line.find("License: ") != npos)
			pkg_map[name].license = line.substr(9);
		else if (line.find("Installed-Size:") != npos || line.find("Installed-Time:") != npos)
		{
			pkg_map[name].installed = true;
			if (line.find("Installed-Size: ") != npos)
				pkg_map[name].installed_size = line.substr(16);
			if (line.find("Installed-Time: ") != npos)
				pkg_map[name].installed_time = line.substr(16);
		}

	}
	v_lines.clear();

	// STEP 2: Create vector with all upgradable packages
	execCmd(pm_cmd[CMD_LIST_UPGRADEABLE], CShellWindow::QUIET);
	std::istringstream ss2(terminal_str);
	std::vector<std::string> v_upgradeables;

	// read lines with upgradeable packages from terminal_str into vector
	l.clear();
	while (std::getline(ss2, l))
		v_upgradeables.push_back(l);

	// reset num_updates
	num_updates = 0;

	// set upgrade mode for packages
	for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it)
	{
		// Parse package name and possible hint and set upgrade flag with possibly deviating version information.
		for (const auto &package : v_upgradeables)
		{
			if (!package.empty())
			{
				string s(package);

				// Extract package name, we must ensure to get a clean package name from possible text around it.
				s = str_replace("Not selecting", "", s);
				s = str_replace("as installing it would break existing dependencies.", "", s);
				s = trim(s, " ");

				// Cache package name with included version text
				string s1 = s;

				// Parse pure package name
				size_t pos1 = s.find(" ");
				if (pos1 != npos)
					s = s.substr(0, pos1);

				// Set upgrade flag and version
				if (s == it->second.name)
				{
					// Set upgrade tag
					it->second.upgradable = true;

					// Set version
					// First we must normalize version string ...
					s1 = trim(s1, " ");
					s1 = str_replace(" - ", " to ", s1, s1.find(" - ")+3);
					s1 = str_replace(" - ", " ", s1);

					// ... then set version string to map
					size_t pos2 = s1.find(' ');
					if (pos2 != npos)
						it->second.version = s1.substr(pos2 + 1);

					// Catch possible hint(s).
					size_t pos0 = package.find("installing it would break existing dependencies");
					if (pos0 != npos)
						it->second.hint = package;

					// Count up num_updates
					num_updates++;
				}
			}
		}
	}
	v_upgradeables.clear();
}

string COPKGManager::getBlankPkgName(const string &line)
{
	dprintf(DEBUG_INFO,  "[COPKGManager] [%s - %d]  line: %s\n", __func__, __LINE__, line.c_str());
	std::string res = "";

	//check for error relevant contents and return an empty string if found
	size_t pos0 = line.find("Collected errors:");
	size_t pos01 = line.find(" * ");
	if (pos0 != string::npos || pos01 != string::npos)
		return "";

	//split line and use name as return value
	size_t pos1 = line.find(" ");
	if (pos1 != string::npos)
		res = line.substr(0, pos1);
	else
		return "";

	return trim(res, " ");
}

string COPKGManager::getPkgInfo(const string &pkg_name, const std::string &pkg_key)
{
	auto it = pkg_map.find(pkg_name);

	if (it == pkg_map.end())
		return "";

	const pkg &p = it->second;
	string ret;

	if (!p.version.empty())
		if (pkg_key.empty() || pkg_key == "Version")
			ret += "Version: " + p.version + '\n';

	if (!p.desc.empty())
		if (pkg_key.empty() || pkg_key == "Description")
			ret += "Description: " + p.desc + '\n';

	if (!p.section.empty())
		if (pkg_key.empty() || pkg_key == "Section")
			ret += "Section: " + p.section + '\n';

	if (!p.priority.empty())
		if (pkg_key.empty() || pkg_key == "Priority")
			ret += "Priority: " + p.priority + '\n';

	if (!p.recommends.empty())
		if (pkg_key.empty() || pkg_key == "Recommends")
			ret += "Recommends: " + p.recommends + '\n';

	if (!p.status.empty())
		if (pkg_key.empty() || pkg_key == "Status")
			ret += "Status: " + p.status + '\n';

	if (!p.architecture.empty())
		if (pkg_key.empty() || pkg_key == "Architecture")
			ret += "Architecture: " + p.architecture + '\n';

	if (!p.maintainer.empty())
		if (pkg_key.empty() || pkg_key == "Maintainer")
			ret += "Maintainer: " + p.maintainer + '\n';

	if (!p.md5sum.empty())
		if (pkg_key.empty() || pkg_key == "MD5Sum")
			ret += "Checksum (md5): " + p.md5sum + '\n';

	if (!p.size.empty())
		if (pkg_key.empty() || pkg_key == "Installed-Size")
			ret += "Size: " + p.size + '\n';

	if (!p.filename.empty())
		if (pkg_key.empty() || pkg_key == "Filename")
			ret += "Packagefile: " + p.filename + '\n';

	if (!p.source.empty())
		if (pkg_key.empty() || pkg_key == "Source")
			ret += "Sourcefile: " + p.source + '\n';

	if (!p.homepage.empty())
		if (pkg_key.empty() || pkg_key == "Hompage")
			ret += "Hompage: " + p.homepage + '\n';

	if (!p.license.empty())
		if (pkg_key.empty() || pkg_key == "License")
			ret += "License: " + p.license + '\n';

	if (!p.hint.empty())
		if (pkg_key.empty() || pkg_key == "Hint")
			ret += "\nNote: \n" + p.hint + '\n';

	return ret;
}

int COPKGManager::execCmd(const char *cmdstr, int verbose_mode)
{
	fprintf(stderr, "execCmd(%s)\n", cmdstr);
	string cmd = string(cmdstr);
	int res = 0;
	has_err = false;
	terminal_str.clear();
	//bool ok = true;

	//create CShellWindow object
	CShellWindow shell(cmd, verbose_mode, &res, false);

	//init slot for shell output handler with 3 args, no return value, and connect with loop handler inside of CShellWindow object
	sigc::slot3<void, string*, int*, bool*> sl_shell;
	sl_shell = sigc::mem_fun(*this, &COPKGManager::handleShellOutput);
	shell.OnShellOutputLoop.connect(sl_shell);
#if 0
	//demo for custom error message inside shell window loop
	sigc::slot1<void, int*> sl1;
	sl1 = sigc::mem_fun(*this, &COPKGManager::showErr);
	shell.OnResultError.connect(sl1);
#endif
	shell.exec();

	return res;
}

void COPKGManager::handleShellOutput(string* cur_line, int* res, bool* ok)
{
	//hold current res value
	int _res = *res;

	//use current line
	string line = *cur_line;

	//terminal_str contains all output lines and is available in the object scope of this
	terminal_str += line + '\n';

	//dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  come into shell handler with res: %d, line = %s\n", __func__, __LINE__, _res, line.c_str());

	//detect any collected error
	size_t pos2 = line.find("Collected errors:");
	if (pos2 != string::npos)
		has_err = true;

	dprintf(DEBUG_INFO, "[COPKGManager:%d] %s\n", __LINE__, line.c_str());
	//check for collected errors and set res value
	if (has_err)
	{
		/* all lines printed already
		dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  result: %s\n", __func__, __LINE__, line.c_str());
		 */

		/*duplicate option cache: option is defined in OPKG_CONFIG_OPTIONS,
		 * NOTE: if found first cache option in the opkg.conf file, this will be preferred and it's not really an error!
		*/
		if (line.find("Duplicate option cache") != string::npos)
		{
			dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  WARNING: %s\n", __func__, __LINE__,  line.c_str());
			*ok = true;
			has_err = false;
			*res = OPKG_SUCCESS;
			return;
		}
		/*resolve_conffiles: already existent configfiles are not installed, but renamed in the same directory,
		 * NOTE: It's not fine but not really bad. Files should be installed separate or user can change manually
		*/
		if (line.find("Existing conffile") != string::npos)
		{
			dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  WARNING: %s\n", __func__, __LINE__, line.c_str());
			*ok = true;
			has_err = false;
			*res = OPKG_SUCCESS;
			return;
		}
		//download error:
		if (line.find("opkg_download:") != string::npos)
		{
			*res = OPKG_DOWNLOAD_ERR;
			//*ok = false;
			return;
		}
		//not enough space
		if (line.find("No space left on device") != string::npos)
		{
			*res = OPKG_OUT_OF_SPACE_ERR;
			//*ok = false;
			return;
		}
		//deps
		if (line.find("satisfy_dependencies") != string::npos)
		{
			*res = OPKG_UNSATISFIED_DEPS_ERR;
			*ok = false;
			return;
		}
		/* hack */
		if (line.find("system-update: err_reset") != string::npos)
		{
			*res = OPKG_SUCCESS;
			*ok = true;
			has_err = false;
			return;
		}
		//unknown error
		if (*ok){
			dprintf(DEBUG_DEBUG, "[COPKGManager] [%s - %d]  ERROR: unhandled error %s\n", __func__, __LINE__, line.c_str());
			*res = OPKG_UNKNOWN_ERR;
			//*ok = false;
			return;
		}

#if 0
		/* never reached */
		if (!has_err){
			*ok = true;
			*res = OPKG_SUCCESS;
		}
#endif
	}

	*res = _res;
}

void COPKGManager::showErr(int* res)
{
	string err = to_string(*res);
	string errtest = err_list[1].id;
	if (!silent)
		DisplayErrorMessage(errtest.c_str());
}	

void COPKGManager::showError(const char* local_msg, char* err_message, const string &additional_text)
{
	string msg = local_msg ? string(local_msg) + "\n" : "";
	if (err_message)
		msg += string(err_message) + ":\n";
	if (!additional_text.empty())
		msg += additional_text;
	if (!silent)
		DisplayErrorMessage(msg.c_str());
}

bool COPKGManager::installPackage(const string &pkg_name, string options, bool force_configure)
{
	string opts = " " + options + " ";

	int r = execCmd(pm_cmd[CMD_INSTALL] + opts + pkg_name, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT | CShellWindow::ACKNOWLEDGE);
	if (r)
	{
		switch(r)
		{
			case OPKG_OUT_OF_SPACE_ERR:
				DisplayErrorMessage("Not enough space available");
				break;
			case OPKG_DOWNLOAD_ERR:
				DisplayErrorMessage("Can't download package. Check network!");
				break;
			case OPKG_UNSATISFIED_DEPS_ERR:
			{
				int msgRet = ShowMsg("Installation", "Unsatisfied deps while installation! Try to repeat to force dependencies!", CMsgBox::mbrCancel, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 600, -1);
				if (msgRet == CMsgBox::mbrYes)
					return installPackage(pkg_name, "--force-depends");
				break;
			}
			default:
				showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), NULL, pm_cmd[CMD_INSTALL] + opts + pkg_name);
				/* errno / strerror considered useless here
				showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), strerror(errno), pm_cmd[CMD_INSTALL] + opts + pkg_name);
					*/
		}
	}
	else
	{
		if (force_configure)
			execCmd(pm_cmd[CMD_CONFIGURE] + getBlankPkgName(pkg_name), 0);
		installed = pkg_map[getBlankPkgName(pkg_name)].installed; //TODO: check this

	}

	return true;
}

void COPKGManagerExtra::setUpdateStateIcon2Item(CMenuItem *item)
{
	if (!item)
		return;

	if (file_exists(HAS_PKG_UPDATE_FLAGFILE))
		item->setInfoIconRight(NEUTRINO_ICON_MARKER_UPDATE_AVAILABLE);
	else
		item->setInfoIconRight(NEUTRINO_ICON_MARKER_DIALOG_OK);
}


