/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	OPKG-Manager Class for Neutrino-GUI

	Implementation:
	Copyright (C) 2012-2015 T. Graf 'dbt'
	www.dbox2-tuning.net

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

#include <gui/opkg_manager.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>

#include <gui/widget/progresswindow.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/keyboard_input.h>
#include <driver/screen_max.h>
#include <gui/filebrowser.h>
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

#if 0
#define OPKG_CL "opkg"
#else
#define OPKG_CL "opkg-cl"
#endif

#define OPKG_TMP_DIR "/tmp/.opkg"
#define OPKG_TEST_DIR OPKG_TMP_DIR "/test"
#define OPKG_CL_CONFIG_OPTIONS " -V2 --tmp-dir=/tmp --cache=" OPKG_TMP_DIR

#define OPKG_BAD_PATTERN_LIST_FILE CONFIGDIR "/bad_package_pattern.list"
#define OPKG_CONFIG_FILE "/etc/opkg/opkg.conf"

/* script to call instead of "opkg upgrade"
 * opkg fails to gracefully self-upgrade, and additionally has some ordering issues
 */
#define SYSTEM_UPDATE "system-update"

using namespace std;

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
	OM_STATUS,
	OM_CONFIGURE,
	OM_DOWNLOAD,
	OM_MAX
};

static string pkg_types[OM_MAX] =
{
	OPKG_CL " list ",
	OPKG_CL " list-installed ",
	OPKG_CL " list-upgradable ",
	OPKG_CL " -A update ",
	OPKG_CL OPKG_CL_CONFIG_OPTIONS " upgrade ",
	OPKG_CL OPKG_CL_CONFIG_OPTIONS " remove ",
	OPKG_CL " info ",
	OPKG_CL OPKG_CL_CONFIG_OPTIONS " install ",
	OPKG_CL " status ",
	OPKG_CL " configure ",
	OPKG_CL " download "
};

COPKGManager::COPKGManager(): opkg_conf('\t')
{
	init();
}

void COPKGManager::init()
{
	if (!hasOpkgSupport())
		return;

	OM_ERRORS();
	width = 80;

	//define default dest keys
	string dest_defaults[] = {"/", OPKG_TEST_DIR, OPKG_TMP_DIR, "/mnt"};
	for(size_t i=0; i<sizeof(dest_defaults)/sizeof(dest_defaults[0]) ;i++)
		config_dest.push_back(dest_defaults[i]);

	loadConfig();
	pkg_map.clear();
	list_installed_done = false;
	list_upgradeable_done = false;
	expert_mode = false;
	local_dir = &g_settings.update_dir_opkg;
	v_bad_pattern = getBadPackagePatternList();
	CFileHelpers::createDir(OPKG_TMP_DIR);
}

COPKGManager::~COPKGManager()
{
	pkg_map.clear();
	CFileHelpers::removeDir(OPKG_TMP_DIR);
}

int COPKGManager::exec(CMenuTarget* parent, const string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (actionKey.empty()) {
		if (parent)
			parent->hide();
		int ret = showMenu();
		saveConfig();
		CFileHelpers::removeDir(OPKG_TMP_DIR);
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
			execCmd(pkg_types[OM_REMOVE] + pkg_vec[selected]->name, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT);
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
	if (actionKey == "local_package") {
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
				showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), "", pkg_name);
				/* errno is never set properly, the string is totally useless.
				showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), strerror(errno), pkg_name);
				 */

			*local_dir = fileBrowser.getCurrentDir();
		refreshMenu();
		}
		return res;
	}
	if(actionKey == pkg_types[OM_UPGRADE]) {
		if (parent)
			parent->hide();
		int r = execCmd(actionKey, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT);
		if (r) {
			/* errno is never set properly, the string is totally useless.
			showError(g_Locale->getText(LOCALE_OPKG_FAILURE_UPGRADE), strerror(errno), actionKey);
			 */
			showError(g_Locale->getText(LOCALE_OPKG_FAILURE_UPGRADE), "", actionKey);
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
		execCmd(pkg_types[OM_DOWNLOAD] + plain_pkg); //download package

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


#define COPKGManagerFooterButtonCount 3
static const struct button_label COPKGManagerFooterButtons[COPKGManagerFooterButtonCount] = {
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_OPKG_BUTTON_EXPERT_ON },
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_OPKG_BUTTON_INFO },
	{ NEUTRINO_ICON_BUTTON_OKAY,	   LOCALE_OPKG_BUTTON_INSTALL }
};
#define COPKGManagerFooterButtonCountExpert 4
static const struct button_label COPKGManagerFooterButtonsExpert[COPKGManagerFooterButtonCountExpert] = {
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_OPKG_BUTTON_EXPERT_OFF },
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL, LOCALE_OPKG_BUTTON_INFO },
	{ NEUTRINO_ICON_BUTTON_OKAY,	   LOCALE_OPKG_BUTTON_INSTALL },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_OPKG_BUTTON_UNINSTALL }
};

vector<string> COPKGManager::getBadPackagePatternList()
{
	vector<string> v_ret;

	ifstream in (OPKG_BAD_PATTERN_LIST_FILE, ios::in);
	if (!in){
		dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d] can't open %s, %s\n", __func__, __LINE__, OPKG_BAD_PATTERN_LIST_FILE, strerror(errno));
		return v_ret;
	}
	string line;

	while(getline(in, line)){
		v_ret.push_back(line);
	}
	in.close();

	return v_ret;
}

bool COPKGManager::badpackage(std::string &s)
{
	if(v_bad_pattern.empty())
		return false;

	size_t i;
	string st = "";
	for (i = 0; i < v_bad_pattern.size(); i++)
	{
		string p = v_bad_pattern[i];
		if (p.empty())
			continue;

		size_t patlen = p.length() - 1;
		bool res = false;
		/* poor man's regex :-) only supported are "^" and "$" */
		if (p.substr(patlen, 1) == "$") { /* match at end */
			size_t pos = s.rfind(p.substr(0, patlen)); /* s.len-patlen can be -1 == npos */
			if (pos != string::npos && pos == (s.length() - patlen))
				res = true;
		} else if (p.substr(0, 1) == "^") { /* match at beginning */
			if (s.find(p.substr(1)) == 0)
				res = true;
		} else { /* match everywhere */
			if (s.find(p) != string::npos)
				res = true;
		}
		if (res)
			st += p + " ";
	}

	if (!st.empty()){
		dprintf(DEBUG_INFO, "[%s] filtered '%s' pattern(s) '%s'\n", __func__, s.c_str(), st.c_str());
		return true;
	}

	return false;
}

void COPKGManager::updateMenu()
{
	bool upgradesAvailable = false;
	getPkgData(OM_LIST_INSTALLED);
	getPkgData(OM_LIST_UPGRADEABLE);
	for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it) {
		/* this should no longer trigger at all */
		if (badpackage(it->second.name))
			continue;
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

	if (expert_mode){
		menu->setFooter(COPKGManagerFooterButtonsExpert, COPKGManagerFooterButtonCountExpert);
	}
	else{
		menu->setSelected(2); //back-item
		menu->setFooter(COPKGManagerFooterButtons, COPKGManagerFooterButtonCount);
	}
}

bool COPKGManager::checkUpdates(const std::string & package_name, bool show_progress)
{
	if (!hasOpkgSupport())
		return false;

	doUpdate();

	bool ret = false;

	size_t i = 0;
	CProgressWindow status;
	status.showHeader(false);

	if (show_progress){
		status.paint();
		status.showStatusMessageUTF(g_Locale->getText(LOCALE_OPKG_UPDATE_READING_LISTS));
		status.showStatus(25); /* after do_update, we have actually done the hardest work already */
	}

	getPkgData(OM_LIST);
	if (show_progress)
		status.showStatus(50);
	getPkgData(OM_LIST_UPGRADEABLE);
	if (show_progress)
		status.showStatus(75);

	for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it){
		dprintf(DEBUG_INFO,  "[COPKGManager] [%s - %d]  Update check for...%s\n", __func__, __LINE__, it->second.name.c_str());
		if (show_progress){
			/* showing the names only makes things *much* slower...
			status.showStatusMessageUTF(it->second.name);
			 */
			status.showStatus(75 + 25*i /  pkg_map.size());
		}

		if (it->second.upgradable){
			dprintf(DEBUG_INFO,  "[COPKGManager] [%s - %d]  Update packages available for...%s\n", __func__, __LINE__, it->second.name.c_str());
			if (!package_name.empty() && package_name == it->second.name){
				ret = true;
				break;
			}else
				ret = true;
		}
		i++;
	}

	if (show_progress){
		status.showGlobalStatus(100);
		status.showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_READY)); // UTF-8
		status.hide();
	}

#if 0
	pkg_map.clear();
#endif

	return ret;
}

int COPKGManager::doUpdate()
{
	CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_OPKG_UPDATE_CHECK);
	hintBox.paint();
	int r = execCmd(pkg_types[OM_UPDATE], CShellWindow::QUIET);
	hintBox.hide();
	if (r) {
		string msg = string(g_Locale->getText(LOCALE_OPKG_FAILURE_UPDATE));
		msg += '\n' + tmp_str;
		DisplayErrorMessage(msg.c_str());
		return r;
	}
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
	if (checkUpdates())
		DisplayInfoMessage(g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_UPDATES_AVAILABLE));

#if 0
	getPkgData(OM_LIST);
	getPkgData(OM_LIST_UPGRADEABLE);
#endif

	menu = new CMenuWidget(g_Locale->getText(LOCALE_SERVICEMENU_UPDATE), NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_SOFTWAREUPDATE);
	menu->addIntroItems(LOCALE_OPKG_TITLE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_BACK, CMenuWidget::BRIEF_HINT_YES);

	//upgrade all installed packages
	upgrade_forwarder = new CMenuForwarder(LOCALE_OPKG_UPGRADE, true, NULL , this, pkg_types[OM_UPGRADE].c_str(), CRCInput::RC_red);
	upgrade_forwarder->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_OPKG_UPGRADE);
	menu->addItem(upgrade_forwarder);

	//select and install local package
	CMenuForwarder *fw;
	fw = new CMenuForwarder(LOCALE_OPKG_INSTALL_LOCAL_PACKAGE, true, NULL, this, "local_package", CRCInput::RC_green);
	fw->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_OPKG_INSTALL_LOCAL_PACKAGE);
	menu->addItem(fw);
#if ENABLE_OPKG_GUI_FEED_SETUP
	//feed setup
	CMenuWidget feeds_menu(LOCALE_OPKG_TITLE, NEUTRINO_ICON_UPDATE, w_max (100, 10));
	showMenuConfigFeed(&feeds_menu);
	fw = new CMenuForwarder(LOCALE_OPKG_FEED_ADDRESSES, true, NULL, &feeds_menu, NULL, CRCInput::RC_www);
	fw->setHint(NEUTRINO_ICON_HINT_SW_UPDATE, LOCALE_MENU_HINT_OPKG_FEED_ADDRESSES_EDIT);
	menu->addItem(fw);
#endif
	menu->addItem(GenericMenuSeparatorLine);

	menu_offset = menu->getItemsCount();

	menu->addKey(CRCInput::RC_info, this, "rc_info");
	menu->addKey(CRCInput::RC_blue, this, "rc_blue");
	menu->addKey(CRCInput::RC_yellow, this, "rc_yellow");

	pkg_vec.clear();
	for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it) {
		/* this should no longer trigger at all */
		if (badpackage(it->second.name))
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
		/*!
			Show a success message only if restart/reboot is required and user should decide what to do or not.
			NOTE: marker file should be generated by opkg package itself (eg. with preinstall scripts),
			so it's controlled by the package maintainer!
		*/
		//restart neutrino: user decision
		if(!access( "/tmp/.restart", F_OK)){
			int msg = ShowMsg(LOCALE_OPKG_TITLE, g_Locale->getText(LOCALE_OPKG_SUCCESS_INSTALL), CMsgBox::mbrNo,
			CMsgBox::mbYes | CMsgBox::mbNo,
			NEUTRINO_ICON_QUESTION,
			width);
			if (msg == CMsgBox::mbrYes)
				exit_action = "restart";
		}
		//restart neutrino: forced
		if (!access( "/tmp/.force_restart", F_OK))
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
	if (find_executable(OPKG_CL).empty()) {
		dprintf(DEBUG_NORMAL, "[COPKGManager] [%s - %d]" OPKG_CL " executable not found\n", __func__, __LINE__);
		return false;
	}

	if (! find_executable(SYSTEM_UPDATE).empty()) {
		dprintf(DEBUG_NORMAL, "[COPKGManager] [%s - %d] " SYSTEM_UPDATE " script found\n", __func__, __LINE__);
		pkg_types[OM_UPGRADE] = SYSTEM_UPDATE;
	}

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
	string dirs[] = {TARGET_PREFIX"/opt/opkg/info", TARGET_PREFIX"/var/lib/opkg/info"};
	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		if (access(dirs[i].c_str(), R_OK) == 0)
			return dirs[i];
	}
	dprintf(DEBUG_NORMAL, "[COPKGManager] [%s - %d] InfoDir not found\n", __func__, __LINE__);
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
		fseek(fd, 0, SEEK_END);
		fgetpos(fd, &fz);
		fseek(fd, 0, SEEK_SET);
		if (fz.__pos == 0)
			return pkgDesc;

		char buf[512];
		string package, version, description;
		while (fgets(buf, sizeof(buf), fd)) {
			if (buf[0] == ' ')
				continue;
			string line(buf);
			trim(line, " ");
			string tmp;
			/* When pkgDesc is empty, return description only for OM_INFO */
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
	dprintf(DEBUG_INFO, "[COPKGManager] [%s - %d] executing %s\n", __func__, __LINE__, pkg_types[pkg_content_id].c_str());

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
			for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it)
				it->second.installed = false;
			break;
		case OM_LIST_UPGRADEABLE:
			if (list_upgradeable_done)
				return;
			list_upgradeable_done = true;
			for (map<string, struct pkg>::iterator it = pkg_map.begin(); it != pkg_map.end(); ++it)
				it->second.upgradable = false;
			break;
	}

	pid_t pid = 0;
	FILE *f = my_popen(pid, pkg_types[pkg_content_id].c_str(), "r");
	if (!f) {
		showError("Internal Error", strerror(errno), pkg_types[pkg_content_id]);
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
			case OM_LIST: {
				/* do not even put "bad" packages into the list to save memory */
				if (badpackage(name))
					continue;
				pkg_map[name] = pkg(name, line, line);
				map<string, struct pkg>::iterator it = pkg_map.find(name);
				if (it != pkg_map.end())
					it->second.desc = getPkgDescription(name, line);
				break;
			}
			case OM_LIST_INSTALLED: {
				map<string, struct pkg>::iterator it = pkg_map.find(name);
				if (it != pkg_map.end())
					it->second.installed = true;
				break;
			}
			case OM_LIST_UPGRADEABLE: {
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
	execCmd(pkg_types[current_status ? OM_STATUS : OM_INFO] + pkg_name, CShellWindow::QUIET);
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

		/*duplicate option cache: option is defined in OPKG_CL_CONFIG_OPTIONS,
		 * NOTE: if found first cache option in the opkg.conf file, this will be preferred and it's not really an error!
		*/
		if (line.find("Duplicate option cache") != string::npos){
			dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  WARNING: %s\n", __func__, __LINE__,  line.c_str());
			*ok = true;
			has_err = false;
			*res = OM_SUCCESS;
			return;
		}
		/*resolve_conffiles: already existent configfiles are not installed, but renamed in the same directory,
		 * NOTE: It's not fine but not really bad. Files should be installed separate or user can change manually
		*/
		if (line.find("Existing conffile") != string::npos){
			dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  WARNING: %s\n", __func__, __LINE__, line.c_str());
			*ok = true;
			has_err = false;
			*res = OM_SUCCESS;
			return;
		}
		//download error:
		if (line.find("opkg_download:") != string::npos){
			*res = OM_DOWNLOAD_ERR;
			//*ok = false;
			return;
		}
		//not enough space
		if (line.find("No space left on device") != string::npos){
			*res = OM_OUT_OF_SPACE_ERR;
			//*ok = false;
			return;
		}
		//deps
		if (line.find("satisfy_dependencies") != string::npos){
			*res = OM_UNSATISFIED_DEPS_ERR;
			*ok = false;
			return;
		}
		/* hack */
		if (line.find("system-update: err_reset") != string::npos) {
			*res = OM_SUCCESS;
			*ok = true;
			has_err = false;
			return;
		}
		//unknown error
		if (*ok){
			dprintf(DEBUG_DEBUG, "[COPKGManager] [%s - %d]  ERROR: unhandled error %s\n", __func__, __LINE__, line.c_str());
			*res = OM_UNKNOWN_ERR;
			//*ok = false;
			return;
		}

		if (!has_err){
			*ok = true;
			*res = OM_SUCCESS;
		}
	}

	*res = _res;
}

void COPKGManager::showErr(int* res)
{
	string err = to_string(*res);
	string errtest = err_list[1].id;
	DisplayErrorMessage(errtest.c_str());
}	

void COPKGManager::showError(const char* local_msg, char* err_message, const string& additional_text)
{
	string msg = local_msg ? string(local_msg) + "\n" : "";
	if (err_message)
		msg += string(err_message) + ":\n";
	if (!additional_text.empty())
		msg += additional_text;
	DisplayErrorMessage(msg.c_str());
}

bool COPKGManager::installPackage(const string& pkg_name, string options, bool force_configure)
{
	//check package size...cancel installation if size check failed
	if (!checkSize(pkg_name)){
		DisplayErrorMessage(g_Locale->getText(LOCALE_OPKG_MESSAGEBOX_SIZE_ERROR));
	}
	else{
		string opts = " " + options + " ";

		int r = execCmd(pkg_types[OM_INSTALL] + opts + pkg_name, CShellWindow::VERBOSE | CShellWindow::ACKNOWLEDGE_EVENT | CShellWindow::ACKNOWLEDGE);
		if (r){
			switch(r){
				case OM_OUT_OF_SPACE_ERR:
					DisplayErrorMessage("Not enough space available");
					break;
				case OM_DOWNLOAD_ERR:
					DisplayErrorMessage("Can't download package. Check network!");
					break;
				case OM_UNSATISFIED_DEPS_ERR:{
					int msgRet = ShowMsg("Installation", "Unsatisfied deps while installation! Try to repeat to force dependencies!", CMsgBox::mbrCancel, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 600, -1);
					if (msgRet == CMsgBox::mbrYes)
						return installPackage(pkg_name, "--force-depends");
					break;
				}
				default:
					showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), "", pkg_types[OM_INSTALL] + opts + pkg_name);
					/* errno / strerror considered useless here
					showError(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL), strerror(errno), pkg_types[OM_INSTALL] + opts + pkg_name);
					 */
			}
		}else{
			if (force_configure)
				execCmd(pkg_types[OM_CONFIGURE] + getBlankPkgName(pkg_name), 0);
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


void COPKGManager::showMenuConfigFeed(CMenuWidget *feed_menu)
{
	feed_menu->addIntroItems(LOCALE_OPKG_FEED_ADDRESSES);

	for(size_t i=0; i<OPKG_MAX_FEEDS ;i++){
		CKeyboardInput *feedinput = new CKeyboardInput("Feed " +to_string(i+1), &config_src[i], 0, NULL, NULL, LOCALE_OPKG_ENTER_FEED_ADDRESS, LOCALE_OPKG_ENTER_FEED_ADDRESS_EXAMPLE);
		CMenuForwarder *fw = new CMenuDForwarder( string(), true , config_src[i], feedinput, NULL, CRCInput::convertDigitToKey(i));
		feed_menu->addItem( fw);
	}
}

void COPKGManager::loadConfig()
{
	opkg_conf.clear();
	bool load_defaults = false;

	if (!opkg_conf.loadConfig(OPKG_CONFIG_FILE,  '\t')){
		dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  Error: error while loading opkg config file! -> %s. Using default settings!\n", __func__, __LINE__, OPKG_CONFIG_FILE);
		load_defaults = true;
	}

	//package feeds
	for(size_t i=0; i<OPKG_MAX_FEEDS ;i++){
		string src_key = "src " + to_string(i);
		config_src[i] = opkg_conf.getString(src_key, string());
	}

	//dest dir default keys, predefined in constructor
	for(size_t j=0; j<config_dest.size() ;j++){
		string dest_key = "dest " + to_string(j);
		opkg_conf.getString(dest_key, config_dest[j]);
	}

	//load default settings and write to config file
	if (load_defaults)
		saveConfig();
}

void COPKGManager::saveConfig()
{
	//set package feeds
	for(size_t i=0; i<OPKG_MAX_FEEDS ;i++){
		string src_key = "src " + to_string(i);

		if (!config_src[i].empty())
			opkg_conf.setString(src_key, config_src[i]);
		else
			opkg_conf.deleteKey(src_key); //remove unused keys
	}

	//set dest dir default key values
	for(size_t j=0; j<config_dest.size() ;j++){
		string dest_key = "dest " + to_string(j);
		opkg_conf.setString(dest_key, config_dest[j]);
	}

	//finally save config file
	if (!opkg_conf.saveConfig(OPKG_CONFIG_FILE, '\t')){
		dprintf(DEBUG_NORMAL,  "[COPKGManager] [%s - %d]  Error: error while saving opkg config file! -> %s\n", __func__, __LINE__, OPKG_CONFIG_FILE);
		DisplayErrorMessage("Error while saving opkg config file!");
	}
}
