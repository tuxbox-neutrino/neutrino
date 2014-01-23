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
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <driver/screen_max.h>

#include <fstream>

#include <global.h>

#include <errno.h>
#include <pthread.h>
#include <sys/mount.h>
#include <unistd.h>
#include <neutrino.h>
#include <zapit/client/zapittools.h>

CNFSMountGui::CNFSMountGui()
{
	// FIXME #warning move probing from exec() to fsmounter
	m_nfs_sup = CFSMounter::FS_UNPROBED;
	m_cifs_sup = CFSMounter::FS_UNPROBED;
	m_lufs_sup = CFSMounter::FS_UNPROBED;
	
	width = w_max (50, 10); //%
}


const char * nfs_entry_printf_string[3] =
{
	"NFS %s:%s -> %s auto: %4s",
	"CIFS //%s/%s -> %s auto: %4s",
	"FTPFS %s/%s -> %s auto: %4s"
};

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
		for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
		{
			m_entry[i] = getEntryString(i);
		}
		returnval = menu();
	}
	else if(actionKey == "rc_spkr")
	{
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
			m_entry[i] = getEntryString(i);
			ISO_8859_1_entry[i] = ZapitTools::UTF8_to_Latin1(m_entry[i].c_str());
		}
	}
	else if(actionKey.substr(0,10)=="mountentry")
	{
		parent->hide();
		returnval = menuEntry(actionKey[10]-'0');
		for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
		{
			m_entry[i] = getEntryString(i);
			ISO_8859_1_entry[i] = ZapitTools::UTF8_to_Latin1(m_entry[i].c_str());
		}
	}
	else if(actionKey.substr(0,7)=="domount")
	{
		int nr=atoi(actionKey.substr(7,1).c_str());
		CFSMounter::MountRes mres = CFSMounter::mount(
				g_settings.network_nfs[nr].ip, g_settings.network_nfs[nr].dir,
				g_settings.network_nfs[nr].local_dir, (CFSMounter::FSType) g_settings.network_nfs[nr].type,
				  g_settings.network_nfs[nr].username, g_settings.network_nfs[nr].password,
				  g_settings.network_nfs[nr].mount_options1, g_settings.network_nfs[nr].mount_options2);

		if (mres == CFSMounter::MRES_OK || mres == CFSMounter::MRES_FS_ALREADY_MOUNTED)
			mountMenuEntry[nr]->iconName = NEUTRINO_ICON_MOUNTED;
		else
			mountMenuEntry[nr]->iconName = NEUTRINO_ICON_NOT_MOUNTED;

		// TODO show msg in case of error
		returnval = menu_return::RETURN_EXIT;
	}
	else if(actionKey.substr(0,3)=="dir")
	{
		parent->hide();
		int nr=atoi(actionKey.substr(3,1).c_str());
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

	for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++)
	{
		sprintf(s2,"mountentry%d",i);
		ISO_8859_1_entry[i] = ZapitTools::UTF8_to_Latin1(m_entry[i].c_str());
		mountMenuEntry[i] = new CMenuForwarder("", true, ISO_8859_1_entry[i], this, s2);
		if (!i)
			menu_offset = mountMenuW.getItemsCount();
		
		if (CFSMounter::isMounted(g_settings.network_nfs[i].local_dir))
		{
			mountMenuEntry[i]->iconName = NEUTRINO_ICON_MOUNTED;
		} else
		{
			mountMenuEntry[i]->iconName = NEUTRINO_ICON_NOT_MOUNTED;
		}
		mountMenuW.addItem(mountMenuEntry[i]);
	}
	int ret=mountMenuW.exec(this,"");
	return ret;
}

	// FIXME #warning MESSAGEBOX_NO_YES_XXX is defined in neutrino.cpp, too!
#define MESSAGEBOX_NO_YES_OPTION_COUNT 2
const CMenuOptionChooser::keyval MESSAGEBOX_NO_YES_OPTIONS[MESSAGEBOX_NO_YES_OPTION_COUNT] =
{
	{ 0, LOCALE_MESSAGEBOX_NO },
	{ 1, LOCALE_MESSAGEBOX_YES }
};

#define NFS_TYPE_OPTION_COUNT 3
const CMenuOptionChooser::keyval NFS_TYPE_OPTIONS[NFS_TYPE_OPTION_COUNT] =
{
	{ CFSMounter::NFS , LOCALE_NFS_TYPE_NFS },
	{ CFSMounter::CIFS, LOCALE_NFS_TYPE_CIFS } /*,
	{ CFSMounter::LUFS, LOCALE_NFS_TYPE_LUFS } */
};

int CNFSMountGui::menuEntry(int nr)
{
	int type;

	char cmd[9];
	char cmd2[9];

	snprintf(cmd, sizeof(cmd), "domount%d",nr);
	snprintf(cmd2, sizeof(cmd2), "dir%d",nr);

   /* rewrite fstype in new entries */
   if(g_settings.network_nfs[nr].local_dir.empty())
   {
	   if(m_cifs_sup != CFSMounter::FS_UNSUPPORTED && m_nfs_sup == CFSMounter::FS_UNSUPPORTED && m_lufs_sup == CFSMounter::FS_UNSUPPORTED)
		   type = (int) CFSMounter::CIFS;

	   else if(m_lufs_sup != CFSMounter::FS_UNSUPPORTED && m_cifs_sup == CFSMounter::FS_UNSUPPORTED && m_nfs_sup == CFSMounter::FS_UNSUPPORTED)
		   type = (int) CFSMounter::LUFS;
   }
   bool typeEnabled = (m_cifs_sup != CFSMounter::FS_UNSUPPORTED && m_nfs_sup != CFSMounter::FS_UNSUPPORTED && m_lufs_sup != CFSMounter::FS_UNSUPPORTED) ||
	   (m_cifs_sup != CFSMounter::FS_UNSUPPORTED && type != (int)CFSMounter::CIFS) ||
	   (m_nfs_sup != CFSMounter::FS_UNSUPPORTED && type != (int)CFSMounter::NFS) ||
	   (m_lufs_sup != CFSMounter::FS_UNSUPPORTED && type != (int)CFSMounter::LUFS);

	CMenuWidget mountMenuEntryW(LOCALE_NFS_MOUNT, NEUTRINO_ICON_NETWORK, width);
	mountMenuEntryW.addIntroItems();

	CIPInput ipInput(LOCALE_NFS_IP, &g_settings.network_nfs[nr].ip, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CStringInputSMS dirInput(LOCALE_NFS_DIR, &g_settings.network_nfs[nr].dir, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE,"abcdefghijklmnopqrstuvwxyz0123456789-_.,:|!?/ ");

	CMenuOptionChooser *automountInput= new CMenuOptionChooser(LOCALE_NFS_AUTOMOUNT, &g_settings.network_nfs[nr].automount, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

	CStringInputSMS options1Input(LOCALE_NFS_MOUNT_OPTIONS, &g_settings.network_nfs[nr].mount_options1, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789-_=.,:|!?/ ");
	CMenuForwarder *options1_fwd = new CMenuForwarder(LOCALE_NFS_MOUNT_OPTIONS, true, g_settings.network_nfs[nr].mount_options1, &options1Input);

	CStringInputSMS options2Input(LOCALE_NFS_MOUNT_OPTIONS, &g_settings.network_nfs[nr].mount_options2, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789-_=.,:|!?/ ");
	CMenuForwarder *options2_fwd = new CMenuForwarder(LOCALE_NFS_MOUNT_OPTIONS, true, g_settings.network_nfs[nr].mount_options2, &options2Input);

	CStringInputSMS userInput(LOCALE_NFS_USERNAME, &g_settings.network_nfs[nr].username, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789-_.,:|!?/ ");
	CMenuForwarder *username_fwd = new CMenuForwarder(LOCALE_NFS_USERNAME, (type != (int)CFSMounter::NFS), g_settings.network_nfs[nr].username, &userInput);

	CStringInputSMS passInput(LOCALE_NFS_PASSWORD, &g_settings.network_nfs[nr].password, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789-_.,:|!?/ ");
	CMenuForwarder *password_fwd = new CMenuForwarder(LOCALE_NFS_PASSWORD, (type != (int)CFSMounter::NFS), NULL, &passInput);

	CMACInput macInput(LOCALE_RECORDINGMENU_SERVER_MAC, &g_settings.network_nfs[nr].mac, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder * macInput_fwd = new CMenuForwarder(LOCALE_RECORDINGMENU_SERVER_MAC, true, g_settings.network_nfs[nr].mac, &macInput);

	CMenuForwarder *mountnow_fwd = new CMenuForwarder(LOCALE_NFS_MOUNTNOW, !(CFSMounter::isMounted(g_settings.network_nfs[nr].local_dir)), NULL, this, cmd);

	mountnow_fwd->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true);
	COnOffNotifier notifier(CFSMounter::NFS);
	notifier.addItem(username_fwd);
	notifier.addItem(password_fwd);

	mountMenuEntryW.addItem(new CMenuOptionChooser(LOCALE_NFS_TYPE, &type, NFS_TYPE_OPTIONS, NFS_TYPE_OPTION_COUNT, typeEnabled, &notifier));
	mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_IP      , true, g_settings.network_nfs[nr].ip,        &ipInput       ));
	mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_DIR     , true, g_settings.network_nfs[nr].dir,       &dirInput      ));
	mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_LOCALDIR, true, g_settings.network_nfs[nr].local_dir, this     , cmd2));
	mountMenuEntryW.addItem(automountInput);
	mountMenuEntryW.addItem(options1_fwd);
	mountMenuEntryW.addItem(options2_fwd);
	mountMenuEntryW.addItem(username_fwd);
	mountMenuEntryW.addItem(password_fwd);
	mountMenuEntryW.addItem(macInput_fwd);
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
		returnval = menu();
	}
	else if(actionKey.substr(0,8)=="doumount")
	{
		CFSMounter::umount((actionKey.substr(9)).c_str());
		returnval = menu_return::RETURN_EXIT;
	}
	else
		returnval = menu_return::RETURN_REPAINT;

	return returnval;
}
int CNFSUmountGui::menu()
{
	int count = 0;
	CFSMounter::MountInfos infos;
	CMenuWidget umountMenu(LOCALE_NFS_UMOUNT, NEUTRINO_ICON_NETWORK, width);
	umountMenu.addIntroItems();
	CFSMounter::getMountedFS(infos);
	for (CFSMounter::MountInfos::const_iterator it = infos.begin();
	     it != infos.end();++it)
	{
		if(it->type == "nfs" || it->type == "cifs" || it->type == "lufs")
		{
			count++;
			std::string s1 = it->device;
			s1 += " -> ";
			s1 += it->mountPoint;
			std::string s2 = "doumount ";
			s2 += it->mountPoint;
			CMenuForwarder *forwarder = new CMenuForwarder(s1.c_str(), true, NULL, this, s2.c_str());
			forwarder->iconName = NEUTRINO_ICON_MOUNTED;
			umountMenu.addItem(forwarder);
		}
	}
	if( !infos.empty() )
		return umountMenu.exec(this,"");
	else
		return menu_return::RETURN_REPAINT;
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
		return sm_menu.exec(parent, actionKey);
	}
	else if(actionKey.substr(0,7) == "remount")
	{
		//umount automount dirs
		for(int i = 0; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
		{
			if(g_settings.network_nfs[i].automount)
				umount2(g_settings.network_nfs[i].local_dir.c_str(),MNT_FORCE);
		}
		CFSMounter::automount();
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
