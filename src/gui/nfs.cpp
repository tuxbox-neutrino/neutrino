/*
	Neutrino-GUI  -   DBoxII-Project

	NFSMount/Umount GUI by Zwen

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/nfs.h>

#include <gui/filebrowser.h>
#include <gui/widget/menue.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <gui/widget/keyboard_input.h>
#include <driver/screen_max.h>

#include <fstream>

#include <system/helpers.h>
#include <global.h>

#include <errno.h>
#include <pthread.h>
#include <sys/mount.h>
#include <unistd.h>
#include <neutrino.h>
#include <zapit/client/zapittools.h>

extern int pinghost (const std::string &hostname, std::string *ip = NULL);

CNFSMountGui::CNFSMountGui()
{
	// FIXME #warning move probing from exec() to fsmounter
	m_nfs_sup = CFSMounter::FS_UNPROBED;
	m_cifs_sup = CFSMounter::FS_UNPROBED;
	m_lufs_sup = CFSMounter::FS_UNPROBED;

	mountMenuWPtr = NULL;
	menu_offset = 0;
	for (int i = 0; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
		mountMenuEntry[i] = NULL;

	width = 50;
}

std::string CNFSMountGui::getEntryString(int i)
{
	std::string res;
	switch(g_settings.network_nfs[i].type) {
		case CFSMounter::NFS: res = "NFS "     + g_settings.network_nfs[i].ip + ":"; break;
		case CFSMounter::CIFS: res = "CIFS //" + g_settings.network_nfs[i].ip + "/"; break;
		case CFSMounter::LUFS: res = "FTPS "   + g_settings.network_nfs[i].ip + "/"; break;
	}
	if (g_settings.network_nfs[i].dir.empty() || g_settings.network_nfs[i].local_dir.empty() || g_settings.network_nfs[i].ip.empty())
		return "";
	return res
		+ FILESYSTEM_ENCODING_TO_UTF8(g_settings.network_nfs[i].dir)
		+ " -> "
		+ FILESYSTEM_ENCODING_TO_UTF8(g_settings.network_nfs[i].local_dir)
		+ " (auto: "
		+ g_Locale->getText(g_settings.network_nfs[i].automount ? LOCALE_MESSAGEBOX_YES : LOCALE_MESSAGEBOX_NO)
		+ ")";
}

/*
	Rebuild the visible state of list item i from the settings.

	The text goes through setOption() on purpose. CMenuForwarder::init() keeps a
	pointer to the caller's string when it is not empty, but copies it when it
	is. An item that was built from an empty string therefore never looks at
	that string again, so exactly the entry the user has just filled in stays
	blank until the list is rebuilt. setOption() always copies and behaves the
	same in both cases.

	The items themselves belong to the CMenuWidget in menu(), which lives on the
	stack and deletes them when it returns. mountMenuEntry[] is therefore only
	valid while that menu runs; menu() clears it afterwards and the NULL check
	below keeps that contract enforced rather than merely assumed.
*/
void CNFSMountGui::updateMountEntry(int i, const CFSMounter::MountInfos &mounts)
{
	if (i < 0 || i >= NETWORK_NFS_NR_OF_ENTRIES || mountMenuEntry[i] == NULL)
		return;

	std::string text = getEntryString(i);
	std::string hint;

	/*
		Icon and warning come from the same lookup on purpose. The icon answers
		"is anything mounted there", so the list must cover every filesystem
		type, not just network ones. Otherwise a local disk on the target
		directory would disable "mount now" while the row explained nothing.
	*/
	const CFSMounter::MountInfo *holder = CFSMounter::findMountPoint(mounts, g_settings.network_nfs[i].local_dir);

	/*
		Warn before the fact when that mount is not this entry's own.
		CFSMounter::mount() refuses such a mount anyway, but only after the
		attempt and without naming who holds the path. This adds no second lock,
		just the missing piece of information.
	*/
	if (!text.empty() && holder != NULL && CFSMounter::getMountEntry(*holder) != i)
	{
		text += "  ";
		text += g_Locale->getText(LOCALE_NFS_LOCALDIR_IN_USE);
		hint = std::string(g_Locale->getText(LOCALE_NFS_LOCALDIR_IN_USE_BY)) + " " + holder->device;
	}

	mountMenuEntry[i]->setOption(ZapitTools::UTF8_to_Latin1(text));
	mountMenuEntry[i]->setHint("", hint);
	mountMenuEntry[i]->iconName = (holder != NULL)
		? NEUTRINO_ICON_MOUNTED
		: NEUTRINO_ICON_NOT_MOUNTED;
}

void CNFSMountGui::updateMountEntry(int i)
{
	CFSMounter::MountInfos mounts;
	CFSMounter::getMounts(mounts);
	updateMountEntry(i, mounts);
}

int CNFSMountGui::exec( CMenuTarget* parent, const std::string & actionKey )
{
	//printf("exec: %s\n", actionKey.c_str());
	int returnval = menu_return::RETURN_REPAINT;

	if (m_nfs_sup == CFSMounter::FS_UNPROBED)
		m_nfs_sup = CFSMounter::fsSupported(CFSMounter::NFS);

	if (m_cifs_sup == CFSMounter::FS_UNPROBED)
		m_cifs_sup = CFSMounter::fsSupported(CFSMounter::CIFS);

	if (m_lufs_sup == CFSMounter::FS_UNPROBED)
		m_lufs_sup = CFSMounter::fsSupported(CFSMounter::LUFS);

	printf("SUPPORT: NFS: %d, CIFS: %d, LUFS: %d\n", m_nfs_sup, m_cifs_sup, m_lufs_sup);

	if (actionKey.empty())
	{
		parent->hide();
		const unsigned int generation = CFSMounter::getMountGeneration();
		returnval = menu();
		/*
			Our caller shows a list of the active shares that it built once, on
			entering. Mounting something here makes that list wrong, and it is
			only repainted, not rebuilt. Leaving it as well is the honest
			answer: the next visit reads /proc/mounts again.
		*/
		if (generation != CFSMounter::getMountGeneration())
			returnval = menu_return::RETURN_EXIT;
	}
	else if(actionKey == "rc_spkr")
	{
		if (mountMenuWPtr == NULL)
			return returnval;
		int i = mountMenuWPtr->getSelected() - menu_offset;
		if (i > -1 && i < NETWORK_NFS_NR_OF_ENTRIES) {
			g_settings.network_nfs[i].ip = "";
			g_settings.network_nfs[i].dir = "";
			g_settings.network_nfs[i].local_dir = "";
			g_settings.network_nfs[i].automount = 0;
			g_settings.network_nfs[i].type = 0;
			g_settings.network_nfs[i].username = "";
			g_settings.network_nfs[i].password = "";
			g_settings.network_nfs[i].mount_options1 = "ro,soft,udp";
			g_settings.network_nfs[i].mount_options2 = "nolock,rsize=8192,wsize=8192";
			g_settings.network_nfs[i].mac = "11:22:33:44:55:66";
			updateMountEntry(i);
		}
	}
	else if(actionKey.substr(0,10)=="refreshMAC")
	{
		int nr=atoi(actionKey.substr(10,1));
		std::string h;
		pinghost(g_settings.network_nfs[nr].ip, &h);
		if (!h.empty()) {
			FILE *arptable = fopen("/proc/net/arp", "r");
			if (arptable) {
				char line[120], ip[120], mac[120];
				while (fgets(line, sizeof(line), arptable)) {
					if (sscanf(line, "%s %*s %*s %s %*[^\n]", ip, mac) == 2) {
						if (!strcmp(ip, h.c_str())) {
							g_settings.network_nfs[nr].mac = std::string(mac);
							break;
						}
					}
				}
				fclose(arptable);
			}
		}
	}
	else if(actionKey.substr(0,10)=="mountentry")
	{
		parent->hide();
		int nr = actionKey[10]-'0';
		returnval = menuEntry(nr);
		updateMountEntry(nr);
	}
	else if(actionKey.substr(0,7)=="domount")
	{
		int nr=atoi(actionKey.substr(7,1));
		CFSMounter::MountRes mres = CFSMounter::mount(
				g_settings.network_nfs[nr].ip, g_settings.network_nfs[nr].dir,
				g_settings.network_nfs[nr].local_dir, (CFSMounter::FSType) g_settings.network_nfs[nr].type,
				  g_settings.network_nfs[nr].username, g_settings.network_nfs[nr].password,
				  g_settings.network_nfs[nr].mount_options1, g_settings.network_nfs[nr].mount_options2);

		updateMountEntry(nr);
		if (mres != CFSMounter::MRES_OK && mres != CFSMounter::MRES_FS_ALREADY_MOUNTED)
			DisplayErrorMessage(mntRes2Str(mres));

		returnval = (mres == CFSMounter::MRES_OK || mres == CFSMounter::MRES_FS_ALREADY_MOUNTED)
			? menu_return::RETURN_EXIT
			: menu_return::RETURN_REPAINT;
	}
	else if(actionKey.substr(0,3)=="dir")
	{
		parent->hide();
		int nr=atoi(actionKey.substr(3,1));
		chooserDir(g_settings.network_nfs[nr].local_dir, false, NULL);
		returnval = menu_return::RETURN_REPAINT;
	}
	return returnval;
}

int CNFSMountGui::menu()
{
	CMenuWidget mountMenuW(LOCALE_NFS_MOUNT, NEUTRINO_ICON_NETWORK, width);
	mountMenuWPtr = &mountMenuW;
	mountMenuW.addIntroItems();
	mountMenuW.addKey(CRCInput::RC_spkr, this, "rc_spkr");
	char s2[12];

	/* one look at /proc/mounts for all eight entries */
	CFSMounter::MountInfos mounts;
	CFSMounter::getMounts(mounts);

	for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++)
	{
		sprintf(s2,"mountentry%d",i);
		mountMenuEntry[i] = new CMenuForwarder("", true, NULL, this, s2);
		if (!i)
			menu_offset = mountMenuW.getItemsCount();

		updateMountEntry(i, mounts);
		mountMenuW.addItem(mountMenuEntry[i]);
	}
	int ret=mountMenuW.exec(this,"");

	/* mountMenuW is about to go out of scope and takes its items with it */
	for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++)
		mountMenuEntry[i] = NULL;
	mountMenuWPtr = NULL;

	return ret;
}

	// FIXME #warning MESSAGEBOX_NO_YES_XXX is defined in neutrino.cpp, too!
#define MESSAGEBOX_NO_YES_OPTION_COUNT 2
const CMenuOptionChooser::keyval MESSAGEBOX_NO_YES_OPTIONS[MESSAGEBOX_NO_YES_OPTION_COUNT] =
{
	{ 0, LOCALE_MESSAGEBOX_NO },
	{ 1, LOCALE_MESSAGEBOX_YES }
};

#define NFS_TYPE_OPTION_COUNT 2
const CMenuOptionChooser::keyval NFS_TYPE_OPTIONS[NFS_TYPE_OPTION_COUNT] =
{
	{ CFSMounter::NFS , LOCALE_NFS_TYPE_NFS },
	{ CFSMounter::CIFS, LOCALE_NFS_TYPE_CIFS } /*,
	{ CFSMounter::LUFS, LOCALE_NFS_TYPE_LUFS } */
};

int CNFSMountGui::menuEntry(int nr)
{
	/* rewrite fstype in new entries */
	if(g_settings.network_nfs[nr].local_dir.empty()) {
		if(m_cifs_sup != CFSMounter::FS_UNSUPPORTED && m_nfs_sup == CFSMounter::FS_UNSUPPORTED && m_lufs_sup == CFSMounter::FS_UNSUPPORTED)
			g_settings.network_nfs[nr].type = (int) CFSMounter::CIFS;
   		else if(m_lufs_sup != CFSMounter::FS_UNSUPPORTED && m_cifs_sup == CFSMounter::FS_UNSUPPORTED && m_nfs_sup == CFSMounter::FS_UNSUPPORTED)
			g_settings.network_nfs[nr].type = (int) CFSMounter::LUFS;
	}
	bool typeEnabled = (m_cifs_sup != CFSMounter::FS_UNSUPPORTED && m_nfs_sup != CFSMounter::FS_UNSUPPORTED && m_lufs_sup != CFSMounter::FS_UNSUPPORTED) ||
			   (m_cifs_sup != CFSMounter::FS_UNSUPPORTED && g_settings.network_nfs[nr].type != (int)CFSMounter::CIFS) ||
			   (m_nfs_sup  != CFSMounter::FS_UNSUPPORTED && g_settings.network_nfs[nr].type != (int)CFSMounter::NFS) ||
			   (m_lufs_sup != CFSMounter::FS_UNSUPPORTED && g_settings.network_nfs[nr].type != (int)CFSMounter::LUFS);

	CMenuWidget mountMenuEntryW(LOCALE_NFS_MOUNT, NEUTRINO_ICON_NETWORK, width);
	mountMenuEntryW.addIntroItems();

	CIPInput ipInput(LOCALE_NFS_IP, &g_settings.network_nfs[nr].ip, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CKeyboardInput dirInput(LOCALE_NFS_DIR, &g_settings.network_nfs[nr].dir);

	CMenuOptionChooser *automountInput= new CMenuOptionChooser(LOCALE_NFS_AUTOMOUNT, &g_settings.network_nfs[nr].automount, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

	CKeyboardInput options1Input(LOCALE_NFS_MOUNT_OPTIONS, &g_settings.network_nfs[nr].mount_options1);
	CMenuForwarder *options1_fwd = new CMenuForwarder(LOCALE_NFS_MOUNT_OPTIONS, true, NULL, &options1Input);

	CKeyboardInput options2Input(LOCALE_NFS_MOUNT_OPTIONS, &g_settings.network_nfs[nr].mount_options2);
	CMenuForwarder *options2_fwd = new CMenuForwarder(LOCALE_NFS_MOUNT_OPTIONS, true, NULL, &options2Input);

	CKeyboardInput userInput(LOCALE_NFS_USERNAME, &g_settings.network_nfs[nr].username);
	CMenuForwarder *username_fwd = new CMenuForwarder(LOCALE_NFS_USERNAME, (g_settings.network_nfs[nr].type != (int)CFSMounter::NFS), NULL, &userInput);

	CKeyboardInput passInput(LOCALE_NFS_PASSWORD, &g_settings.network_nfs[nr].password);
	CMenuForwarder *password_fwd = new CMenuForwarder(LOCALE_NFS_PASSWORD, (g_settings.network_nfs[nr].type != (int)CFSMounter::NFS), NULL, &passInput);

	CMACInput macInput(LOCALE_NFS_MAC, &g_settings.network_nfs[nr].mac, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder *macInput_fwd = new CMenuForwarder(LOCALE_NFS_MAC, true, NULL, &macInput);

	CMenuForwarder *refreshMAC_fwd = new CMenuForwarder(LOCALE_NFS_MAC_REFRESH, true, NULL, this, ("refreshMAC" + to_string(nr)).c_str(), CRCInput::RC_yellow);

	CMenuForwarder *mountnow_fwd = new CMenuForwarder(LOCALE_NFS_MOUNTNOW, !(CFSMounter::isMounted(g_settings.network_nfs[nr].local_dir)), NULL, this, ("domount" + to_string(nr)).c_str(), CRCInput::RC_red);

	mountnow_fwd->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
	COnOffNotifier notifier(CFSMounter::NFS);
	notifier.addItem(username_fwd);
	notifier.addItem(password_fwd);

	mountMenuEntryW.addItem(new CMenuOptionChooser(LOCALE_NFS_TYPE, &g_settings.network_nfs[nr].type, NFS_TYPE_OPTIONS, NFS_TYPE_OPTION_COUNT, typeEnabled, &notifier));
	mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_IP      , true, g_settings.network_nfs[nr].ip,        &ipInput ));
	mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_DIR     , true, g_settings.network_nfs[nr].dir,       &dirInput));
	mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_LOCALDIR, true, g_settings.network_nfs[nr].local_dir, this, ("dir" + to_string(nr)).c_str()));
	mountMenuEntryW.addItem(automountInput);
	mountMenuEntryW.addItem(options1_fwd);
	mountMenuEntryW.addItem(options2_fwd);
	mountMenuEntryW.addItem(username_fwd);
	mountMenuEntryW.addItem(password_fwd);
	mountMenuEntryW.addItem(macInput_fwd);
	mountMenuEntryW.addItem(refreshMAC_fwd);
	mountMenuEntryW.addItem(GenericMenuSeparatorLine);
	mountMenuEntryW.addItem(mountnow_fwd);

	int ret = mountMenuEntryW.exec(this,"");

	return ret;
}

int CNFSUmountGui::exec( CMenuTarget* parent, const std::string & actionKey )
{
	//	printf("ac: %s\n", actionKey.c_str());
	int returnval;

	if (actionKey.empty())
	{
		parent->hide();
		const unsigned int generation = CFSMounter::getMountGeneration();
		returnval = menu();
		/* see CNFSMountGui::exec(): an unmount invalidates the caller's list */
		if (generation != CFSMounter::getMountGeneration())
			returnval = menu_return::RETURN_EXIT;
	}
	else if(actionKey.substr(0,8)=="doumount")
	{
		CFSMounter::UMountRes res = CFSMounter::umount((actionKey.substr(9)).c_str());
		if (res != CFSMounter::UMRES_OK) {
			DisplayErrorMessage(mntRes2Str(res));
			returnval = menu_return::RETURN_REPAINT;
		} else {
			returnval = menu_return::RETURN_EXIT;
		}
	}
	else
		returnval = menu_return::RETURN_REPAINT;

	return returnval;
}
int CNFSUmountGui::menu()
{
	CFSMounter::MountInfos infos;
	CMenuWidget umountMenu(LOCALE_NFS_UMOUNT, NEUTRINO_ICON_NETWORK, width);
	umountMenu.addIntroItems();
	/* getMountedFS() already reports network mounts only; filtering by type a
	   second time here just adds a place that gets forgotten when a new type
	   shows up. */
	CFSMounter::getMountedFS(infos);
	for (CFSMounter::MountInfos::const_iterator it = infos.begin();
	     it != infos.end();++it)
	{
		std::string s1 = it->device;
		s1 += " -> ";
		s1 += it->mountPoint;
		std::string s2 = "doumount ";
		s2 += it->mountPoint;
		CMenuForwarder *forwarder = new CMenuForwarder(s1, true, NULL, this, s2.c_str());
		forwarder->iconName = NEUTRINO_ICON_MOUNTED;
		umountMenu.addItem(forwarder);
	}
	if( !infos.empty() )
		return umountMenu.exec(this,"");
	else
		return menu_return::RETURN_REPAINT;
}



void showActiveNetworkShares(CMenuWidget *menu)
{
	CFSMounter::MountInfos mounts;
	CFSMounter::getMountedFS(mounts);
	if (mounts.empty())
		return;

	menu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_NFS_ACTIVE_SHARES));

	for (CFSMounter::MountInfos::const_iterator it = mounts.begin(); it != mounts.end(); ++it)
	{
		const int entry = CFSMounter::getMountEntry(*it);
		std::string origin;
		if (entry == CFSMounter::MOUNT_ENTRY_NONE)
			origin = g_Locale->getText(LOCALE_NFS_MOUNT_EXTERN);
		else
			origin = std::string(g_Locale->getText(LOCALE_NFS_MOUNT_ENTRY)) + " " + to_string(entry + 1);

		/* Inactive: the mount and umount entries above are where one acts.
		   The origin goes into the option column rather than into a hint,
		   because an inactive item never takes the focus a hint needs. */
		CMenuForwarder *share = new CMenuForwarder(it->type + "  " + it->device, false);
		share->setOption(origin);
		share->setDescription("-> " + it->mountPoint);
		share->iconName = NEUTRINO_ICON_MOUNTED;
		menu->addItem(share);
	}
}

int CNFSSmallMenu::exec( CMenuTarget* parent, const std::string & actionKey )
{
	if (actionKey.empty())
	{
		CMenuWidget sm_menu(LOCALE_NFSMENU_HEAD, NEUTRINO_ICON_NETWORK, width);
		CNFSMountGui mountGui;
		CNFSUmountGui umountGui;
		CMenuForwarder *remount_fwd = new CMenuForwarder(LOCALE_NFS_REMOUNT, true, NULL, this, "remount");
		remount_fwd->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
		sm_menu.addIntroItems();
		sm_menu.addItem(remount_fwd);
		sm_menu.addItem(new CMenuForwarder(LOCALE_NFS_MOUNT , true, NULL, & mountGui));
		sm_menu.addItem(new CMenuForwarder(LOCALE_NFS_UMOUNT, true, NULL, &umountGui));
		showActiveNetworkShares(&sm_menu);
		return sm_menu.exec(parent, actionKey);
	}
	else if(actionKey.substr(0,7) == "remount")
	{
		bool changed = false;
		const unsigned int generation = CFSMounter::getMountGeneration();

		//umount automount dirs
		for(int i = 0; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
		{
			if(g_settings.network_nfs[i].automount)
				changed |= (umount2(g_settings.network_nfs[i].local_dir.c_str(),MNT_FORCE) == 0);
		}
		CFSMounter::automount();

		/* like the two submenus above: the share list we came from is stale now */
		if (changed || generation != CFSMounter::getMountGeneration())
			return menu_return::RETURN_EXIT;
		return menu_return::RETURN_REPAINT;
	}
	return menu_return::RETURN_REPAINT;
}

const char * mntRes2Str(CFSMounter::MountRes res)
{
	switch(res)
	{
		case CFSMounter::MRES_FS_NOT_SUPPORTED:
			return g_Locale->getText(LOCALE_NFS_MOUNTERROR_NOTSUP);
			break;
		case CFSMounter::MRES_FS_ALREADY_MOUNTED:
			return g_Locale->getText(LOCALE_NFS_ALREADYMOUNTED);
			break;
		case CFSMounter::MRES_TIMEOUT:
			return g_Locale->getText(LOCALE_NFS_MOUNTTIMEOUT);
			break;
		case CFSMounter::MRES_UNKNOWN:
			return g_Locale->getText(LOCALE_NFS_MOUNTERROR);
			break;
		case CFSMounter::MRES_OK:
			return g_Locale->getText(LOCALE_NFS_MOUNTOK);
			break;
		default:
			return g_Locale->getText(NONEXISTANT_LOCALE);
			break;
	}
}

const char * mntRes2Str(CFSMounter::UMountRes res)
{
	switch(res)
	{
		case CFSMounter::UMRES_ERR:
			return g_Locale->getText(LOCALE_NFS_UMOUNTERROR);
			break;
		case CFSMounter::UMRES_OK:
			return g_Locale->getText(NONEXISTANT_LOCALE);
			break;
		default:
			return g_Locale->getText(NONEXISTANT_LOCALE);
			break;
	}
}
