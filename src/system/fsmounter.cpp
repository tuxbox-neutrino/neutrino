/*
	Neutrino-GUI  -   DBoxII-Project

	FSMount/Umount by Zwen

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

#include <system/fsmounter.h>
#include <system/helpers.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>

#include <global.h>

#include <errno.h>
#include <pthread.h>
#include <atomic>

#include <sys/mount.h>
#include <unistd.h>

pthread_mutex_t g_mut;
pthread_cond_t g_cond;
pthread_t g_mnt;
int g_mntstatus;

/* One mount or unmount at a time: the four above are shared by every mount,
   the modules loaded, the counter of changes and the check of what is mounted
   before an unmount are shared by all of them, and the screen at the box and
   the web interface's routes both mount and unmount. Recursive, as a mount
   asks fsSupported(), which takes it as well. */
static pthread_mutex_t g_mounting = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

namespace
{
struct Mounting
{
	Mounting() { pthread_mutex_lock(&g_mounting); }
	~Mounting() { pthread_mutex_unlock(&g_mounting); }
};
}

/* A value handed to the shell as one word, whatever it holds: in single
   quotes, a quote in it closed, escaped and opened again. The settings hold
   what somebody typed, a password among it. */
static std::string shellWord(const std::string &v)
{
	std::string out = "'";
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (v[i] == '\'')
			out += "'\\''";
		else
			out += v[i];
	}
	return out + "'";
}

void *mount_thread(void *cmd)
{
	int ret;
	ret = system((const char *) cmd);
	pthread_mutex_lock(&g_mut);
	g_mntstatus = ret;
	pthread_cond_broadcast(&g_cond);
	pthread_mutex_unlock(&g_mut);
	pthread_exit(NULL);
}

CFSMounter::CFSMounter()
{
}

bool in_proc_filesystems(const char *const fsname)
{
	std::string s;
	std::string t;
	std::ifstream in("/proc/filesystems", std::ifstream::in);

	t = fsname;

	while (in >> s)
	{
		if (s == t)
		{
			in.close();
			return true;
		}
	}
	in.close();
	return false;
}

bool insert_modules(const CFSMounter::FSType fstype)
{
	if (fstype == CFSMounter::NFS)
	{
#ifdef HAVE_MODPROBE
		return (system("modprobe nfs") == 0);
#else
		return ((system("insmod sunrpc") == 0) && (system("insmod lockd") == 0) && (system("insmod nfs") == 0));
#endif
	}
	else if (fstype == CFSMounter::CIFS)
		return (system("insmod cifs") == 0);
	else if (fstype == CFSMounter::LUFS)
		return (system("insmod lufs") == 0);
	return false;
}

bool nfs_mounted_once = false;

/* see CFSMounter::getMountGeneration() */
/* Atomic and read without the lock: the screen of the network shares reads it
   as it opens and closes, and must not wait out a mount of a server that does
   not answer. */
static std::atomic<unsigned int> g_mount_generation(0);

unsigned int CFSMounter::getMountGeneration()
{
	return g_mount_generation;
}

bool remove_modules(const CFSMounter::FSType fstype)
{
	if (fstype == CFSMounter::NFS)
	{
		return ((system("rmmod nfs") == 0) && (system("rmmod lockd") == 0) && (system("rmmod sunrpc") == 0));
	}
	else if (fstype == CFSMounter::CIFS)
		return (system("rmmod cifs") == 0);
	else if (fstype == CFSMounter::LUFS)
		return (system("rmmod lufs") == 0);
	return false;
}

CFSMounter::FS_Support CFSMounter::fsSupported(const CFSMounter::FSType fstype, const bool keep_modules)
{
	const char *fsname = NULL;

	if (fstype == CFSMounter::NFS)
		fsname = "nfs";
	else if (fstype == CFSMounter::CIFS)
		fsname = "cifs";
	else if (fstype == CFSMounter::LUFS)
		fsname = "lufs";

	if (in_proc_filesystems(fsname))
		return CFSMounter::FS_READY;

	// loading and unloading modules, and the note of the NFS one, under the lock
	Mounting one_at_a_time;
	if (insert_modules(fstype))
	{
		if (in_proc_filesystems(fsname))
		{
			if (keep_modules)
			{
				if (fstype == CFSMounter::NFS)
					nfs_mounted_once = true;
			}
			else
			{
				remove_modules(fstype);
			}

			return CFSMounter::FS_NEEDS_MODULES;
		}
	}
	remove_modules(fstype);
	return CFSMounter::FS_UNSUPPORTED;
}

std::string CFSMounter::getDeviceString(const FSType fstype, const std::string &ip, const std::string &dir)
{
	switch (fstype)
	{
		case NFS:
			return ip + ":" + dir;
		case CIFS:
			return "//" + ip + "/" + dir;
		case LUFS:
			/* lufsd has no remote device, it always registers as "none" */
			return "none";
	}
	return "";
}

/*
	The settings hold whatever the user typed, /proc/mounts holds a resolved
	path. Returns the resolved form, or an empty string when the directory does
	not exist, in which case nothing can be mounted on it either. isMounted()
	judges the same way, and the two must agree: otherwise an entry could end up
	with a "not mounted" icon and an "in use" marker at the same time.
*/
static std::string resolve_mount_point(const std::string &local_dir)
{
	if (local_dir.empty())
		return "";

#ifdef PATH_MAX
	char buf[PATH_MAX];
#else
	char buf[4096];
#endif
	if (realpath(local_dir.c_str(), buf) == NULL)
		return "";
	return std::string(buf);
}

/*
	/proc/mounts writes space, tab, newline and backslash as an octal escape, so
	a CIFS share like //nas/My Documents arrives as //nas/My\040Documents. Left
	as it is, that string matches neither the device we would build for it nor
	the directory we would hand to umount2().
*/
static std::string unescape_mount_field(const std::string &field)
{
	std::string out;
	out.reserve(field.size());

	for (size_t i = 0; i < field.size(); i++)
	{
		if (field[i] == '\\' && i + 3 < field.size()
			&& field[i + 1] >= '0' && field[i + 1] <= '3'
			&& field[i + 2] >= '0' && field[i + 2] <= '7'
			&& field[i + 3] >= '0' && field[i + 3] <= '7')
		{
			out += (char)(((field[i + 1] - '0') << 6)
					| ((field[i + 2] - '0') << 3)
					|  (field[i + 3] - '0'));
			i += 3;
		}
		else
			out += field[i];
	}
	return out;
}

bool CFSMounter::isMounted(const std::string &local_dir)
{
	std::ifstream in;
	if (local_dir.empty())
		return false;

#ifdef PATH_MAX
	char mount_point[PATH_MAX];
#else
	char mount_point[4096];
#endif
	if (realpath(local_dir.c_str(), mount_point) == NULL)
	{
		printf("[CFSMounter] could not resolve dir: %s: %s\n", local_dir.c_str(), strerror(errno));
		return false;
	}
	in.open("/proc/mounts", std::ifstream::in);
	while (in.good())
	{
		MountInfo mi;
		in >> mi.device >> mi.mountPoint >> mi.type;
		/* discard the remaining fields before any early exit, otherwise the
		   next iteration reads them as if they were a new record */
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		if (mi.type == "tmpfs")
			continue;

		if (strcmp(mi.mountPoint.c_str(), mount_point) == 0)
		{
			return true;
		}
	}
	return false;
}

CFSMounter::MountRes CFSMounter::mount(const std::string &ip, const std::string &dir, const std::string &local_dir,
				       const FSType fstype, const std::string &username, const std::string &password,
				       std::string options1, std::string options2)
{
	std::string cmd;
	Mounting one_at_a_time;
	pthread_mutex_init(&g_mut, NULL);
	pthread_cond_init(&g_cond, NULL);
	g_mntstatus = -1;

	FS_Support sup = fsSupported(fstype, true); /* keep modules if necessary */

	if (sup == CFSMounter::FS_UNSUPPORTED)
	{
		printf("[CFSMounter] FS type %d not supported\n", (int) fstype);
		return MRES_FS_NOT_SUPPORTED;
	}

	printf("[CFSMounter] Mount(%d) %s:%s -> %s\n", (int) fstype, ip.c_str(), dir.c_str(), local_dir.c_str());

	CFileHelpers fh;
	fh.createDir(local_dir.c_str(), 0755);

	if (isMounted(local_dir))
	{
		printf("[CFSMounter] FS mount error %s already mounted\n", local_dir.c_str());
		return MRES_FS_ALREADY_MOUNTED;
	}

	if (options1.empty())
	{
		options1 = options2;
		options2 = "";
	}

	if (options1.empty() && options2.empty())
	{
		if (fstype == NFS)
		{
			options1 = "soft";
			options2 = "nolock";
		}
		else if (fstype == CIFS)
		{
			options1 = "ro";
			options2 = "";
		}
		else if (fstype == LUFS)
		{
			options1 = "";
			options2 = "";
		}
	}

	/* Every value one word for the shell, so nothing typed into a setting is
	   read as a command. The options go to the mount as one list. */
	std::string options;
	if (fstype == CIFS || fstype == LUFS)
	{
		if (fstype == LUFS)
			options = "fs=ftpfs,";
		options += "username=" + username + ",password=" + password;
		if (fstype == LUFS)
			options += ",host=" + ip + ",root=/" + dir;
	}
	if (!options1.empty())
		options += (options.empty() ? "" : ",") + options1;
	if (!options2.empty())
		options += (options.empty() ? "" : ",") + options2;

	if (fstype == NFS)
		cmd = "mount -t nfs " + shellWord(getDeviceString(NFS, ip, dir)) + ' ' + shellWord(local_dir);
	else if (fstype == CIFS)
		cmd = "mount -t cifs " + shellWord(getDeviceString(CIFS, ip, dir)) + ' ' + shellWord(local_dir);
	else
		cmd = "lufsd none " + shellWord(local_dir);
	if (!options.empty())
		cmd += " -o " + shellWord(options);

	pthread_create(&g_mnt, 0, mount_thread, (void *) cmd.c_str());

	struct timespec timeout;
	int retcode;

	pthread_mutex_lock(&g_mut);
	timeout.tv_sec = time(NULL) + 5;
	timeout.tv_nsec = 0;
	retcode = pthread_cond_timedwait(&g_cond, &g_mut, &timeout);
	if (retcode == ETIMEDOUT)
	{
		// timeout occurred
		pthread_cancel(g_mnt);
	}
	pthread_mutex_unlock(&g_mut);
	pthread_join(g_mnt, NULL);
	if (g_mntstatus != 0)
	{
		// what was mounted where, and never the command, which holds the password
		printf("[CFSMounter] FS mount error (%d) %s:%s -> %s\n", (int) fstype, ip.c_str(), dir.c_str(),
		       local_dir.c_str());
		return (retcode == ETIMEDOUT) ? MRES_TIMEOUT : MRES_UNKNOWN;
	}
	g_mount_generation++;
	return MRES_OK;

}

bool CFSMounter::automount()
{
	bool res = true;
	for (int i = 0; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
	{
		if (g_settings.network_nfs[i].automount)
		{
			res = (MRES_OK == mount(g_settings.network_nfs[i].ip, g_settings.network_nfs[i].dir, g_settings.network_nfs[i].local_dir,
						(FSType) g_settings.network_nfs[i].type, g_settings.network_nfs[i].username,
						g_settings.network_nfs[i].password, g_settings.network_nfs[i].mount_options1,
						g_settings.network_nfs[i].mount_options2)) && res;
		}
	}
	return res;
}

CFSMounter::UMountRes CFSMounter::umount(const char *const dir)
{
	Mounting one_at_a_time;
	UMountRes res = UMRES_OK;
	if (dir != NULL)
	{
		if (umount2(dir, MNT_FORCE) != 0)
		{
			return UMRES_ERR;
		}
		g_mount_generation++;
	}
	else
	{
		MountInfo mi;
		std::ifstream in("/proc/mounts", std::ifstream::in);
		while (in.good())
		{
			in >> mi.device >> mi.mountPoint >> mi.type;
			in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

			if (strcmp(mi.type.c_str(), "nfs") == 0 && strcmp(mi.mountPoint.c_str(), "/") == 0)
			{
				if (umount2(mi.mountPoint.c_str(), MNT_FORCE) != 0)
				{
					printf("[CFSMounter] Error umounting %s\n", mi.device.c_str());
					res = UMRES_ERR;
				}
			}
		}
	}
	if (nfs_mounted_once)
		remove_modules(CFSMounter::NFS);
	return res;
}

/* /proc/mounts names the filesystem type the kernel actually used, and nfs4 and
   smb3 are types of their own, not aliases of nfs and cifs. Both are the common
   case on current kernels, so a filter that only knows the old names hides them
   from every caller. */
static bool is_network_fs(const std::string &type)
{
	return type == "nfs" || type == "nfs4"
		|| type == "cifs" || type == "smb3"
		|| type == "lufs";
}

/* What is mounted on top at local_dir: the last line of /proc/mounts naming that
   directory, tmpfs included, as an unmount takes away what was mounted last.
   False when nothing is mounted there or the directory does not resolve. */
bool CFSMounter::topMount(const std::string &local_dir, MountInfo &top)
{
	const std::string mount_point = resolve_mount_point(local_dir);
	if (mount_point.empty())
		return false;
	bool found = false;
	std::ifstream in("/proc/mounts", std::ifstream::in);
	while (in.good())
	{
		MountInfo mi;
		in >> mi.device >> mi.mountPoint >> mi.type;
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		if (mi.mountPoint.empty())
			continue;
		mi.device = unescape_mount_field(mi.device);
		mi.mountPoint = unescape_mount_field(mi.mountPoint);
		if (mi.mountPoint == mount_point)
		{
			top = mi;
			found = true;
		}
	}
	return found;
}

/* Whether a mount is the share of that kind from that server and directory:
   its device as the mount writes it, and a type of that kind. A share of the
   userspace FTP filesystem registers as "none", whatever it is, so none is
   ever told apart from another and none counts. */
bool CFSMounter::isShare(const MountInfo &mi, const FSType fstype, const std::string &ip, const std::string &dir)
{
	if (fstype == NFS)
		return (mi.type == "nfs" || mi.type == "nfs4") && mi.device == getDeviceString(NFS, ip, dir);
	if (fstype == CIFS)
		return (mi.type == "cifs" || mi.type == "smb3") && mi.device == getDeviceString(CIFS, ip, dir);
	return false;
}

/* Takes the share away from local_dir only where it is what is mounted on top
   there, the look and the unmount one step for every caller of this mounter.
   The resolved directory is unmounted and not a link on the way to it. */
CFSMounter::UMountRes CFSMounter::umountShare(const std::string &local_dir, const FSType fstype,
					      const std::string &ip, const std::string &dir)
{
	Mounting one_at_a_time;
	MountInfo top;
	if (!topMount(local_dir, top) || !isShare(top, fstype, ip, dir))
		return UMRES_NOT_OURS;
	if (umount2(top.mountPoint.c_str(), MNT_FORCE | UMOUNT_NOFOLLOW) != 0)
		return UMRES_ERR;
	g_mount_generation++;
	return UMRES_OK;
}

/*
	An active mount is ours when it is exactly what one of the configured
	entries describes: same remote device and same mount point. Matching the
	device alone would claim a share that somebody else mounted somewhere else,
	and the entry's own icon would then contradict the claim, because that icon
	only ever looks at the mount point.

	LUFS needs no exception: it registers as "none", which is exactly what
	getDeviceString() builds for it, so the plain comparison holds. Skipping the
	comparison for LUFS would credit any foreign mount on that directory to the
	entry and swallow its "in use" warning.
*/
int CFSMounter::getMountEntry(const MountInfo &mi)
{
	for (int i = 0; i < NETWORK_NFS_NR_OF_ENTRIES; i++)
	{
		if (g_settings.network_nfs[i].local_dir.empty())
			continue;

		if (mi.mountPoint != resolve_mount_point(g_settings.network_nfs[i].local_dir))
			continue;

		if (mi.device == getDeviceString((FSType) g_settings.network_nfs[i].type,
						 g_settings.network_nfs[i].ip,
						 g_settings.network_nfs[i].dir))
			return i;
	}
	return MOUNT_ENTRY_NONE;
}

const CFSMounter::MountInfo *CFSMounter::findMountPoint(const MountInfos &infos, const std::string &local_dir)
{
	const std::string mount_point = resolve_mount_point(local_dir);
	if (mount_point.empty())
		return NULL;

	for (MountInfos::const_iterator it = infos.begin(); it != infos.end(); ++it)
	{
		if (it->mountPoint == mount_point)
			return &(*it);
	}
	return NULL;
}

void CFSMounter::getMounts(MountInfos &info)
{
	std::ifstream in("/proc/mounts", std::ifstream::in);

	while (in.good())
	{
		MountInfo mi;
		in >> mi.device >> mi.mountPoint >> mi.type;
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		/* the last read fails at end of file and leaves the fields empty */
		if (mi.mountPoint.empty() || mi.type == "tmpfs")
			continue;

		mi.device = unescape_mount_field(mi.device);
		mi.mountPoint = unescape_mount_field(mi.mountPoint);
		info.push_back(mi);
	}
}

void CFSMounter::getMountedFS(MountInfos &info)
{
	MountInfos mounts;
	getMounts(mounts);

	for (MountInfos::const_iterator it = mounts.begin(); it != mounts.end(); ++it)
	{
		if (is_network_fs(it->type))
		{
			info.push_back(*it);
			printf("[CFSMounter] mounted fs: dev: %s, mp: %s, type: %s\n",
			       it->device.c_str(), it->mountPoint.c_str(), it->type.c_str());
		}
	}
}
