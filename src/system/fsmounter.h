/*
	Neutrino-GUI  -   DBoxII-Project

	FS Mount/Umount by Zwen
	
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

#ifndef __neutrino_fs_mounter__
#define __neutrino_fs_mounter__

#include <system/settings.h>

#include <vector>

class CFSMounter
{
// protected
 public:
	
	enum FS_Support
	{
		FS_UNSUPPORTED   = 0,
		FS_READY         = 1,
		FS_NEEDS_MODULES = 2,
		FS_UNPROBED      = 3
	};
	
 public:

	enum FSType
	{
		NFS  = 0,
		CIFS = 1,
		LUFS = 2
	};
	
	enum MountRes
	{
		MRES_FS_NOT_SUPPORTED     = 0,
		MRES_FS_ALREADY_MOUNTED   = 1,
		MRES_TIMEOUT              = 2,
		MRES_UNKNOWN              = 3,
		MRES_OK                   = 4
	};

	enum UMountRes
	{
		UMRES_ERR = 0,
		UMRES_OK  = 1
	};

	struct MountInfo
	{
		std::string device;
		std::string mountPoint;
		std::string type;
	};

	typedef std::vector<CFSMounter::MountInfo> MountInfos;

 private:
/*
	FS_Support m_nfs_sup;
	FS_Support m_cifs_sup;
	FS_Support m_lufs_sup;
*/
 public:
	CFSMounter();
	static bool isMounted(const std::string &local_dir);
	static CFSMounter::MountRes mount(const std::string &ip, const std::string &dir, const std::string &local_dir,
				       const FSType fstype, const std::string &username, const std::string &password,
				       std::string options1, std::string options2);
	static bool automount();
	static CFSMounter::UMountRes umount(const char * const dir = NULL);
	static void getMountedFS(MountInfos& fs); 
	static FS_Support fsSupported(const FSType fs, const bool keep_modules = false);
};

bool in_proc_filesystems(const char * const fsname);
bool insert_modules(const CFSMounter::FSType fstype);
bool remove_modules(const CFSMounter::FSType fstype);

extern bool nfs_mounted_once; /* needed by update.cpp to prevent removal of modules after flashing a new cramfs, since rmmod (busybox) might no longer be available */

#endif
