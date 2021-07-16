 /*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	OPKG-Manager Class for Neutrino-GUI

	Implementation:
	Copyright (C) 2012-2021 T. Graf 'dbt'

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

#include "widget/icons.h"
#include "widget/msgbox.h"

#include "widget/progresswindow.h"
#include "widget/hintbox.h"
#include "widget/keyboard_input.h"
#include <driver/screen_max.h>
#include "filebrowser.h"
#include <system/debug.h>
#include <system/helpers.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <poll.h>
#include <fcntl.h>
#include <alloca.h>
#include <errno.h>
#include <sys/wait.h>
#include <fstream>

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

COPKGManager::COPKGManager(int wizard_mode): opkg_conf('\t')
{
	init(wizard_mode);
}

void COPKGManager::init(int wizard_mode)
{
	if (!hasOpkgSupport())
		return;

	is_wizard = wizard_mode;
	OPKG_ERRORS();
	width = 80;

	pkg_map.clear();
	list_installed_done = false;
	list_upgradeable_done = false;
	expert_mode = false;
	local_dir = &g_settings.update_dir_opkg;
	initPackagePatternLists();

	silent = false;
	num_updates = 0;
}

COPKGManager::~COPKGManager()
{
	pkg_map.clear();
	execCmd(pm_cmd[CMD_CLEAN], CShellWindow::QUIET);
}

int COPKGManager::exec(CMenuTarget* parent, const string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	removeInfoBarTxt();

	if (actionKey.empty()) {
		if (parent)
			parent->hide();
		int ret = showMenu();

		return ret;
	}
	int selected = menu->getSelected() - menu_offset;

	if (expert_mode && actionKey == "rc_blue") {
		if (selected < 0 || selected >= (int) pkg_vec.size() || !pkg_vec[selected]->installed)
			return menu_return::RETURN_NONE;

		char loc[200];
		snprintf(loc, sizeof(loc), g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_REMOVE), pkg_vec[selected]->name.c_str());
		if (ShowMsg(LOCALE_OPKG_TITLE, loc, CMsgBox::mbrCancel, CMsgBox::mbYes | CMsgBox::mbCancel) != CMsgBox::mbrCancel) {
			if (parent)
				parent->hide();
			execCmd(pm_cmd[CMD_REMOVE] + pkg_vec[selected]->name, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT);
			refreshMenu();
		}
		return res;
	}
	if (actionKey == "rc_info") {
		if (selected < 0 || selected >= (int) pkg_vec.size()){
			DisplayInfoMessage("No information available! Please first select a package!");
			return menu_return::RETURN_NONE;
		}
		//show package info...
		bool is_installed = pkg_vec[selected]->installed;
		string infostr = getPkgInfo(pkg_vec[selected]->name, "", is_installed /*status or info*/);

		//if available, generate a readable string for installation time
		if (is_installed){
			string tstr = getPkgInfo(pkg_vec[selected]->name, "Installed-Time", is_installed);
			stringstream sstr(tstr);
			time_t tval; sstr >> tval;
			string newstr = asctime(localtime(&tval));
			infostr = str_replace(tstr, newstr, infostr);
		}

		DisplayInfoMessage(infostr.c_str());
		return res;
	}
	if (actionKey == "rc_yellow") {
		expert_mode = !expert_mode;
		updateMenu();
		return res;
	}
	if (actionKey == "rc_green") {
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
			string pkg_name = fileBrowser.getSelectedFile()->Name;
			if (!installPackage(pkg_name))
				showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), NULL, pkg_name);
				/* errno is never set properly, the string is totally useless.
				showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), strerror(errno), pkg_name);
				 */

			*local_dir = fileBrowser.getCurrentDir();
			refreshMenu();
		}
		return res;
	}
	if(actionKey == pm_cmd[CMD_UPGRADE]) {
		if (parent)
			parent->hide();
		int r = execCmd(actionKey, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT);
		if (r) {
			/* errno is never set properly, the string is totally useless.
			showError(g_Locale->getText(LOCALE_OPKG_FAILURE_UPGRADE), strerror(errno), actionKey);
			 */
			showError(g_Locale->getText(LOCALE_OPKG_FAILURE_UPGRADE), NULL, actionKey);
		} else
			installed = true;
		refreshMenu();
		/* I don't think ending up at the last package in the list is a good idea...
		g_RCInput->postMsg((neutrino_msg_t) CRCInput::RC_up, 0);
		 */
		return res;
	}

	map<string, struct pkg>::iterator it = pkg_map.find(actionKey);
	if (it != pkg_map.end()) {
		if (parent)
			parent->hide();
		string force = "";
		if (it->second.installed && !it->second.upgradable) {
			char l[200];
			snprintf(l, sizeof(l), g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_REINSTALL), actionKey.c_str());
			l[sizeof(l) - 1] = 0;
			if (ShowMsg(LOCALE_OPKG_TITLE, l, CMsgBox::mbrCancel, CMsgBox::mbYes | CMsgBox::mbCancel) == CMsgBox::mbrCancel)
				return res;
			force = "--force-reinstall ";
		}

		//install package with size check ...cancel installation if check failed
		installPackage(actionKey, force);

		refreshMenu();
	}
	return res;
}

bool COPKGManager::checkSize(const string& pkg_name)
{
	string pkg_file = pkg_name;
	string plain_pkg = getBaseName(pkg_file);

	//exit check size if package already installed, because of auto remove of old stuff during installation
	if (isInstalled(plain_pkg))
		return true;

	/* this is pretty broken right now for several reasons:
	   * space in /tmp is limited (/tmp being ramfs usually, but wasted
	     by unpacking the archive and then untaring it instead of using a pipe
	   * the file is downloaded for this test, then discarded and later
	     downloaded again for installation
	   so until a better solution is found, simply disable it.  */
#if 0
	//get available root fs size
	//TODO: Check writability!
	struct statfs root_fs;
	statfs("/", &root_fs);
	u_int64_t free_size = root_fs.f_bfree*root_fs.f_bsize;

	/*
	 * To calculate the required size for installation here we make a quasi-dry run,
	 * it is a bit awkward, but relatively specific, other solutions are welcome.
	 * We create a temporary test directory and fill it with downloaded or user uploaded package file.
	 * Then we unpack the package and change into temporary testing directory.
	 * The required size results from the size of generated folders and subfolders.
	 * TODO: size of dependencies are not really considered
	*/
	CFileHelpers fh;

	//create test pkg dir
	string 	tmp_dest = OPKG_TEST_DIR;
		tmp_dest += "/package";
	fh.createDir(tmp_dest);

	//change into test dir
	chdir(OPKG_TEST_DIR);

	//copy package into test dir
	string  tmp_dest_file = OPKG_TEST_DIR;
		tmp_dest_file += "/" + plain_pkg;
	if(!access( pkg_file.c_str(), F_OK)) //use local package
		fh.copyFile(pkg_file.c_str(), tmp_dest_file.c_str(), 0644);
	else
		execCmd(pm_cmd[CMD_DOWNLOAD] + plain_pkg); //download package

	//unpack package into test dir
	string ar = "ar -x " + plain_pkg + char(0x2a);
	execCmd(ar);

	//untar package into test directory
	string 	untar_tar_cmd = "tar -xf ";
		untar_tar_cmd += OPKG_TEST_DIR;
		untar_tar_cmd += "/data.tar.gz -C " + tmp_dest;
	execCmd(untar_tar_cmd);

	//get new current required minimal size from dry run test dir
	u_int64_t req_size = fh.getDirSize(tmp_dest);

	//clean up
	fh.removeDir(OPKG_TEST_DIR);

	dprintf(DEBUG_INFO,  "[COPKGManager] [%s - %d] Package: %s [required size=%" PRId64 " (free size: %" PRId64 ")]\n", __func__, __LINE__, pkg_name.c_str(), req_size, free_size);
	if (free_size < req_size){
		//exit if required size too much
		dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  WARNING: size check freesize=%" PRId64 " (recommended: %" PRId64 ")\n", __func__, __LINE__, free_size, req_size);
		return false;
	}
#endif
	return true;
}


static const struct button_label COPKGManagerFooterButtons[] = {
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_OPKG_INSTALL_LOCAL_PACKAGE },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_OPKG_BUTTON_EXPERT_ON },
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_OPKG_BUTTON_INFO },
	{ NEUTRINO_ICON_BUTTON_OKAY,	   LOCALE_OPKG_BUTTON_INSTALL }
};
size_t  COPKGManagerFooterButtonCount = sizeof(COPKGManagerFooterButtons)/sizeof(COPKGManagerFooterButtons[0]);

static const struct button_label COPKGManagerFooterButtonsExpert[] = {
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_OPKG_INSTALL_LOCAL_PACKAGE },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_OPKG_BUTTON_EXPERT_OFF },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_OPKG_BUTTON_UNINSTALL },
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_OPKG_BUTTON_INFO },
	{ NEUTRINO_ICON_BUTTON_OKAY,	   LOCALE_OPKG_BUTTON_INSTALL }
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
	if (!in){
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
			if (package_name.find(p.substr(1)) == 0)
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


void COPKGManager::updateMenu()
{
	bool upgradesAvailable = false;
	getPkgData(CMD_LIST_INSTALLED);
	getPkgData(CMD_LIST_UPGRADEABLE);

	for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it) {
		/* this should no longer trigger at all */
		if (!isPermittedPackage(it->second.name))
			continue;

		it->second.forwarder->iconName_Info_right = "";
		it->second.forwarder->setActive(true);
		if (it->second.upgradable) {
			it->second.forwarder->iconName_Info_right = NEUTRINO_ICON_MARKER_UPDATE_AVAILABLE;
			upgradesAvailable = true;
		} else if (it->second.installed) {
			it->second.forwarder->iconName_Info_right = NEUTRINO_ICON_MARKER_DIALOG_OK;
			it->second.forwarder->setActive(expert_mode);
		}
	}

	upgrade_forwarder->setActive(upgradesAvailable);

	if (expert_mode){
		menu->setFooter(COPKGManagerFooterButtonsExpert, COPKGManagerFooterButtonCountExpert);
	}
	else if (is_wizard) {
		menu->setSelected(2); //next-item
	}else{
		menu->setSelected(2); //back-item
		menu->setFooter(COPKGManagerFooterButtons, COPKGManagerFooterButtonCount);
	}
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

	checkUpdates(std::string(), enable_message);
	if (num_updates)
		initUpdateMessage(enable_message);
	else
		removeInfoBarTxt();
}


bool COPKGManager::checkUpdates(const std::string & package_name, bool show_progress)
{
	if (!hasOpkgSupport() || file_exists("/run/opkg.lock"))
		return false;

	silent = !show_progress;
	num_updates = 0;
	doUpdate();

	bool ret = false;

	size_t i = 0;
	CProgressWindow *status = NULL;

	if (show_progress){
		status = new CProgressWindow();
		status->showHeader(false);
		status->paint();
		status->showStatusMessageUTF(g_Locale->getText(LOCALE_OPKG_UPDATE_READING_LISTS));
		status->showStatus(25); /* after do_update, we have actually done the hardest work already */
	}

	getPkgData(CMD_LIST);
	if (show_progress)
		status->showStatus(50);
	getPkgData(CMD_LIST_UPGRADEABLE);
	if (show_progress)
		status->showStatus(75);

	for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it){
		dprintf(DEBUG_INFO,  "[COPKGManager] [%s - %d]  Update check for...%s\n", __func__, __LINE__, it->second.name.c_str());
		if (show_progress){
			/* showing the names only makes things *much* slower...
			status->showStatusMessageUTF(it->second.name);
			 */
			status->showStatus(75 + 25*i /  pkg_map.size());
		}

		if (it->second.upgradable){
			dprintf(DEBUG_INFO,  "[COPKGManager] [%s - %d]  Update packages available for...%s\n", __func__, __LINE__, it->second.name.c_str());
			if (!package_name.empty() && package_name == it->second.name)
				num_updates = 1;
			else
				num_updates++;
			ret = true;
		}
		i++;
	}

	if (show_progress){
		status->showGlobalStatus(100);
		status->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_READY)); // UTF-8
		status->hide();
	}

	if (status) {
		delete status; status = NULL;
	}
#if 0
	pkg_map.clear();
#endif
	return ret;
}

int COPKGManager::doUpdate()
{
	CHintBox *hintBox = NULL;

	if (!silent ) {
		hintBox = new CHintBox (LOCALE_MESSAGEBOX_INFO, LOCALE_OPKG_UPDATE_CHECK);
		hintBox->paint();
	}

	int r = execCmd(pm_cmd[CMD_UPDATE], CShellWindow::QUIET);

	if (hintBox){
		hintBox->hide();
		delete hintBox; hintBox = NULL;
	}

	if (r) {
		string msg = string(g_Locale->getText(LOCALE_OPKG_FAILURE_UPDATE));
		msg += '\n' + tmp_str;
		if (!silent)
			DisplayErrorMessage(msg.c_str());
		return r;
	}
	initPackagePatternLists();
	return 0;
}

void COPKGManager::refreshMenu() {
	list_installed_done = false,
	list_upgradeable_done = false;
	updateMenu();
}

int COPKGManager::showMenu()
{
	installed = false;
	setUpdateCheckResult(true);
#if 0
	getPkgData(CMD_LIST);
	getPkgData(CMD_LIST_UPGRADEABLE);
#endif

	menu = new CMenuWidget(g_Locale->getText(LOCALE_SERVICEMENU_UPDATE), NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE);
	menu->addIntroItems(LOCALE_OPKG_TITLE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_BACK, CMenuWidget::BRIEF_HINT_YES);
	menu->setWizardMode(is_wizard);

	//upgrade all installed packages
	std::string upd_info = to_string(num_updates) + " " + g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_UPDATES_AVAILABLE);
	upgrade_forwarder = new CMenuForwarder(LOCALE_OPKG_UPGRADE, true, upd_info.c_str() , this, pm_cmd[CMD_UPGRADE].c_str(), CRCInput::RC_red);
	upgrade_forwarder->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_OPKG_UPGRADE);
	menu->addItem(upgrade_forwarder);

	if (!is_wizard)
	{
#if 0
		CMenuForwarder *fw = NULL;

		//select and install local package
		fw = new CMenuForwarder(LOCALE_OPKG_INSTALL_LOCAL_PACKAGE, true, NULL, this, "local_package", CRCInput::RC_green);
		fw->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_OPKG_INSTALL_LOCAL_PACKAGE);
		menu->addItem(fw);
#endif
#if ENABLE_OPKG_GUI_FEED_SETUP
		//feed setup
		CMenuWidget feeds_menu(LOCALE_OPKG_TITLE, NEUTRINO_ICON_UPDATE, w_max (100, 10));
		showMenuConfigFeed(&feeds_menu);
		fw = new CMenuForwarder(LOCALE_OPKG_FEED_ADDRESSES, true, NULL, &feeds_menu, NULL, CRCInput::RC_www);
		fw->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_OPKG_FEED_ADDRESSES_EDIT);
		menu->addItem(fw);
#endif
	}

	menu->addItem(GenericMenuSeparatorLine);

	menu_offset = menu->getItemsCount();

	menu->addKey(CRCInput::RC_help, this, "rc_info");
	menu->addKey(CRCInput::RC_info, this, "rc_info");
	menu->addKey(CRCInput::RC_blue, this, "rc_blue");
	menu->addKey(CRCInput::RC_yellow, this, "rc_yellow");
	menu->addKey(CRCInput::RC_green, this, "rc_green");

	// package list
	pkg_vec.clear();
	for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it) {
		/* this should no longer trigger at all */
		if (!isPermittedPackage(it->second.name))
			continue;

		it->second.forwarder = new CMenuForwarder(it->second.desc, true, NULL , this, it->second.name.c_str());
		it->second.forwarder->setHint("", it->second.desc);
		menu->addItem(it->second.forwarder);
		pkg_vec.push_back(&it->second);
	}

	updateMenu();

	int res = menu->exec(NULL, "");

	menu->hide ();

	//handling after successful installation
	string exit_action = "";
	if (!has_err && installed){
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

string COPKGManager::getInfoDir()
{
	/* /opt/opkg/... is path in patched opkg, /var/lib/opkg/... is original path */
	string dirs[] = {TARGET_PREFIX"/opt/opkg/info", TARGET_PREFIX"/var/lib/opkg/info", "/var/lib/opkg/info", "/opt/opkg/info"};
	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		if (access(dirs[i].c_str(), R_OK) == 0)
			return dirs[i];
		dprintf(DEBUG_NORMAL, "[COPKGManager] [%s - %d] InfoDir [%s] not found\n", __func__, __LINE__, dirs[i].c_str());
	}
	return "";
}

string COPKGManager::getPkgDescription(std::string pkgName, std::string pkgDesc)
{
	static string infoPath;
	if (infoPath.empty())
		infoPath = getInfoDir();
	if (infoPath.empty())
		return pkgDesc;

	string infoFile = infoPath + "/" + pkgName + ".control";
	if (file_exists(infoFile.c_str())) {
		FILE* fd = fopen(infoFile.c_str(), "r");
		if (fd == NULL)
			return pkgDesc;

		fpos_t fz;
		fz.__pos = 0;
		fseek(fd, 0, SEEK_END);
		fgetpos(fd, &fz);
		fseek(fd, 0, SEEK_SET);
		if (fz.__pos == 0){
			fclose(fd);
			return pkgDesc;
		}
		char buf[512];
		string package, version, description;
		while (fgets(buf, sizeof(buf), fd)) {
			if (buf[0] == ' ')
				continue;
			string line(buf);
			trim(line, " ");
			string tmp;
			/* When pkgDesc is empty, return description only for CMD_INFO */
			if (!pkgDesc.empty()) {
				tmp = getKeyInfo(line, "Package:", " ");
				if (!tmp.empty())
					package = tmp;
				tmp = getKeyInfo(line, "Version:", " ");
				if (!tmp.empty())
					version = tmp;
			}
			tmp = getKeyInfo(line, "Description:", " ");
			if (!tmp.empty())
				description = tmp;
		}
		fclose(fd);

		if (pkgDesc.empty())
			return description;

		string desc = package + " - " + version;
		if (!description.empty())
			desc += " - " + description;

		return desc;
	}
	return pkgDesc;
}

void COPKGManager::getPkgData(const int pkg_content_id)
{
	dprintf(DEBUG_INFO, "[COPKGManager] [%s - %d] executing %s\n", __func__, __LINE__, pm_cmd[pkg_content_id].c_str());

	switch (pkg_content_id) {
		case CMD_LIST:
			pkg_map.clear();
			list_installed_done = false;
			list_upgradeable_done = false;
			break;
		case CMD_LIST_INSTALLED:
			if (list_installed_done)
				return;
			list_installed_done = true;
			for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it)
				it->second.installed = false;
			break;
		case CMD_LIST_UPGRADEABLE:
			if (list_upgradeable_done)
				return;
			list_upgradeable_done = true;
			for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it)
				it->second.upgradable = false;
			break;
	}

	pid_t pid = 0;
	FILE *f = my_popen(pid, pm_cmd[pkg_content_id].c_str(), "r");
	if (!f) {
		showError("Internal Error", strerror(errno), pm_cmd[pkg_content_id]);
		return;
	}

	char buf[256];

	while (fgets(buf, sizeof(buf), f))
	{
		if (buf[0] == ' ')
			continue; /* second, third, ... line of description will not be shown anyway */
		std::string line(buf);
		trim(line);

		string name = getBlankPkgName(line);
		if (name.empty())
			continue;

		switch (pkg_content_id) {
			case CMD_LIST: {
				/* do not even put "bad" packages into the list to save memory */
				if (!isPermittedPackage(name))
					continue;

				pkg_map[name] = pkg(name, line, line);
				map<string, struct pkg>::iterator it = pkg_map.find(name);
				if (it != pkg_map.end())
					it->second.desc = getPkgDescription(name, line);
				break;
			}
			case CMD_LIST_INSTALLED: {
				map<string, struct pkg>::iterator it = pkg_map.find(name);
				if (it != pkg_map.end())
					it->second.installed = true;
				break;
			}
			case CMD_LIST_UPGRADEABLE: {
				map<string, struct pkg>::iterator it = pkg_map.find(name);
				if (it != pkg_map.end())
					it->second.upgradable = true;
				break;
			}
			default:
				fprintf(stderr, "%s %s %d: unrecognized content id %d\n", __file__, __func__, __LINE__, pkg_content_id);
				break;
		}
	}

	waitpid(pid, NULL, 0); /* beware of the zombie apocalypse! */

	fclose(f);
}

string COPKGManager::getBlankPkgName(const string& line)
{
	dprintf(DEBUG_INFO,  "[COPKGManager] [%s - %d]  line: %s\n", __func__, __LINE__, line.c_str());

	//check for error relevant contents and return an empty string if found
	size_t pos0 = line.find("Collected errors:");
	size_t pos01 = line.find(" * ");
	if (pos0 != string::npos || pos01 != string::npos)
		return "";

	//split line and use name as return value
	size_t pos1 = line.find(" ");
	if (pos1 != string::npos)
		return line.substr(0, pos1);

	return "";
}

string COPKGManager::getPkgInfo(const string& pkg_name, const string& pkg_key, bool current_status)
{
	execCmd(pm_cmd[current_status ? CMD_STATUS : CMD_INFO] + pkg_name, CShellWindow::QUIET);
	dprintf(DEBUG_INFO,  "[COPKGManager] [%s - %d]  [data: %s]\n", __func__, __LINE__, tmp_str.c_str());

	if (pkg_key.empty()) {
		/* When description is empty, read data from InfoDir */
		string tmp = getKeyInfo(tmp_str, "Description:", " ");
		if (tmp.empty()) {
			tmp = getPkgDescription(pkg_name);
			if (!tmp.empty()) {
				tmp_str += (string)"\nDescription: " + tmp + "\n";
			}
		}
		return tmp_str;
	}

	return getKeyInfo(tmp_str, pkg_key, ":");
}

string COPKGManager::getKeyInfo(const string& input, const std::string& key, const string& delimiters)
{
	string s = input;
	size_t pos1 = s.find(key);
	if (pos1 != string::npos){
		size_t pos2 = s.find(delimiters, pos1)+ delimiters.length();
		if (pos2 != string::npos){
			size_t pos3 = s.find("\n", pos2);
			if (pos3 != string::npos){
				string ret = s.substr(pos2, pos3-pos2);
				return trim(ret, " ");
			}
			else
				dprintf(DEBUG_INFO, "[COPKGManager] [%s - %d]  Error: [key: %s] missing end of line...\n", __func__, __LINE__, key.c_str());
		}
	}
	return "";
}

int COPKGManager::execCmd(const char *cmdstr, int verbose_mode)
{
	fprintf(stderr, "execCmd(%s)\n", cmdstr);
	string cmd = string(cmdstr);
	int res = 0;
	has_err = false;
	tmp_str.clear();
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

	//tmp_str contains all output lines and is available in the object scope of this
	tmp_str += line + '\n';

	//dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  come into shell handler with res: %d, line = %s\n", __func__, __LINE__, _res, line.c_str());

	//detect any collected error
	size_t pos2 = line.find("Collected errors:");
	if (pos2 != string::npos)
		has_err = true;

	dprintf(DEBUG_NORMAL, "[COPKGManager:%d] %s\n", __LINE__, line.c_str());
	//check for collected errors and set res value
	if (has_err){
		/* all lines printed already
		dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  result: %s\n", __func__, __LINE__, line.c_str());
		 */

		/*duplicate option cache: option is defined in OPKG_CONFIG_OPTIONS,
		 * NOTE: if found first cache option in the opkg.conf file, this will be preferred and it's not really an error!
		*/
		if (line.find("Duplicate option cache") != string::npos){
			dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  WARNING: %s\n", __func__, __LINE__,  line.c_str());
			*ok = true;
			has_err = false;
			*res = OPKG_SUCCESS;
			return;
		}
		/*resolve_conffiles: already existent configfiles are not installed, but renamed in the same directory,
		 * NOTE: It's not fine but not really bad. Files should be installed separate or user can change manually
		*/
		if (line.find("Existing conffile") != string::npos){
			dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  WARNING: %s\n", __func__, __LINE__, line.c_str());
			*ok = true;
			has_err = false;
			*res = OPKG_SUCCESS;
			return;
		}
		//download error:
		if (line.find("opkg_download:") != string::npos){
			*res = OPKG_DOWNLOAD_ERR;
			//*ok = false;
			return;
		}
		//not enough space
		if (line.find("No space left on device") != string::npos){
			*res = OPKG_OUT_OF_SPACE_ERR;
			//*ok = false;
			return;
		}
		//deps
		if (line.find("satisfy_dependencies") != string::npos){
			*res = OPKG_UNSATISFIED_DEPS_ERR;
			*ok = false;
			return;
		}
		/* hack */
		if (line.find("system-update: err_reset") != string::npos) {
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

void COPKGManager::showError(const char* local_msg, char* err_message, const string& additional_text)
{
	string msg = local_msg ? string(local_msg) + "\n" : "";
	if (err_message)
		msg += string(err_message) + ":\n";
	if (!additional_text.empty())
		msg += additional_text;
	if (!silent)
		DisplayErrorMessage(msg.c_str());
}

bool COPKGManager::installPackage(const string& pkg_name, string options, bool force_configure)
{
	//check package size...cancel installation if size check failed
	if (!checkSize(pkg_name)){
		if (!silent)
			DisplayErrorMessage(g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_SIZE_ERROR));
	}
	else{
		string opts = " " + options + " ";

		int r = execCmd(pm_cmd[CMD_INSTALL] + opts + pkg_name, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT | CShellWindow::ACKNOWLEDGE);
		if (r){
			switch(r){
				case OPKG_OUT_OF_SPACE_ERR:
					DisplayErrorMessage("Not enough space available");
					break;
				case OPKG_DOWNLOAD_ERR:
					DisplayErrorMessage("Can't download package. Check network!");
					break;
				case OPKG_UNSATISFIED_DEPS_ERR:{
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
		}else{
			if (force_configure)
				execCmd(pm_cmd[CMD_CONFIGURE] + getBlankPkgName(pkg_name), 0);
			installed = true; //TODO: catch real result
		}
	}

	return true;
}

bool COPKGManager::isInstalled(const string& pkg_name)
{
	string package = pkg_name;
	package = getBaseName(package);

	map<string, struct pkg>::iterator it = pkg_map.find(package);
	if (it != pkg_map.end())
		if (it->second.installed)
			return true;
	return false;
}

bool COPKGManager::isUpgradable(const string& pkg_name)
{
	string package = pkg_name;
	package = getBaseName(package);

	map<string, struct pkg>::iterator it = pkg_map.find(package);
	if (it != pkg_map.end())
		if (it->second.upgradable)
			return true;
	return false;
}
