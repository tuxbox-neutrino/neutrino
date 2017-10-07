/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2010-2015 Stefan Seyfried
	Copyright (C) 2013-2014 martii
	Copyright (C) 2009-2014 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

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

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/mount.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include "hdd_menu.h"

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/progresswindow.h>

#include <system/helpers.h>
#include <system/settings.h>
#include <system/debug.h>

#include <mymenu.h>
#include <driver/screen_max.h>
#include <driver/record.h>

#define BLKID_BIN    "/sbin/blkid"
#define EJECT_BIN    "/bin/eject"

#define MDEV_MOUNT	"/lib/mdev/fs/mount"
#define MOUNT_BASE	"/media/"

#define HDD_NOISE_OPTION_COUNT 4
const CMenuOptionChooser::keyval HDD_NOISE_OPTIONS[HDD_NOISE_OPTION_COUNT] =
{
	{ 0,   LOCALE_OPTIONS_OFF },
	{ 128, LOCALE_HDD_SLOW },
	{ 190, LOCALE_HDD_MIDDLE },
	{ 254, LOCALE_HDD_FAST }
};

#define HDD_SLEEP_OPTION_COUNT 6
const CMenuOptionChooser::keyval HDD_SLEEP_OPTIONS[HDD_SLEEP_OPTION_COUNT] =
{
	{ 0,   LOCALE_OPTIONS_OFF },
	//{ 12,  LOCALE_HDD_1MIN },
	{ 60,  LOCALE_HDD_5MIN },
	{ 120, LOCALE_HDD_10MIN },
	{ 240, LOCALE_HDD_20MIN },
	{ 241, LOCALE_HDD_30MIN },
	{ 242, LOCALE_HDD_60MIN }
};

devtool_s CHDDMenuHandler::devtools[] = {
	{ "ext4",  "/sbin/fsck.ext4",  "-C 1 -f -y", "/sbin/mkfs.ext4",  "-m 0", false, false },
	{ "ext3",  "/sbin/fsck.ext3",  "-C 1 -f -y", "/sbin/mkfs.ext3",  "-m 0", false, false },
	{ "ext2",  "/sbin/fsck.ext2",  "-C 1 -f -y", "/sbin/mkfs.ext2",  "-m 0", false, false },
	{ "vfat",  "/sbin/fsck.vfat",  "-a",         "/sbin/mkfs.vfat",  "",    false, false },
	{ "exfat", "/sbin/fsck.exfat", "",           "/sbin/mkfs.exfat", "",    false, false },
	{ "xfs",   "/sbin/xfs_repair", "",           "/sbin/mkfs.xfs",   "-f",  false, false },
};
#define FS_MAX (sizeof(CHDDMenuHandler::devtools)/sizeof(devtool_s))

static int my_filter(const struct dirent * dent)
{
	return CHDDMenuHandler::getInstance()->filterDevName(dent->d_name);
}

CHDDMenuHandler::CHDDMenuHandler()
{
	width = 58;
	show_menu = false;
	in_menu = false;
	lock_refresh = false;
}

CHDDMenuHandler::~CHDDMenuHandler()
{
}

CHDDMenuHandler* CHDDMenuHandler::getInstance()
{
	static CHDDMenuHandler* me = NULL;

	if(!me)
		me = new CHDDMenuHandler();

	return me;
}

int CHDDMenuHandler::filterDevName(const char * name)
{
	if (((name[0] == 's' || name[0] == 'h') && (name[1] == 'd' || name[1] == 'r'))
#if !HAVE_ARM_HARDWARE
		|| !strncmp(name, "mmcblk", 6)
#endif
	)
		return 1;
	return 0;
}

static std::string readlink(const char *path)
{
	char link[PATH_MAX + 1];
	if (realpath(path, link))
		return std::string(link);
	return "";
}

bool CHDDMenuHandler::is_mounted(const char *dev)
{
	bool res = false;
	char devpath[40];
	if (!strncmp(dev, "/dev/", 5))
		snprintf(devpath, sizeof(devpath), "%s", dev);
	else
		snprintf(devpath, sizeof(devpath), "/dev/%s", dev);

	char buffer[255];
	std::string realdev = readlink(devpath);
	realdev = trim(realdev);
	FILE *f = fopen("/proc/mounts", "r");
	if(f) {
		while (!res && fgets(buffer, sizeof(buffer), f)) {
			if (buffer[0] != '/')
				continue; /* only "real" devices are interesting */
			char *p = strchr(buffer, ' ');
			if (p)
				*p = 0; /* terminate at first space, kernel-user ABI is fixed */
			if (!strcmp(buffer, devpath)) /* default '/dev/sda1' mount */
				res = true;
			else {	/* now the case of '/dev/disk/by-label/myharddrive' mounts */
				std::string realmount = readlink(buffer);
				if (realdev == trim(realmount))
					res = true;
			}
		}
		fclose(f);
	}
	printf("CHDDMenuHandler::is_mounted: dev [%s] is %s\n", devpath, res ? "mounted" : "not mounted");
	return res;
}

void CHDDMenuHandler::getBlkIds()
{
	pid_t pid;
	std::string blkid = find_executable("blkid");
	if (blkid.empty())
		return;
	std::string pcmd = blkid + (std::string)" -s TYPE";

	FILE* f = my_popen(pid, pcmd.c_str(), "r");
	if (!f) {
		printf("getBlkIds: cmd [%s] failed\n", pcmd.c_str());
		return;
	}

	hdd_list.clear();
	char buff[512];
	while (fgets(buff, sizeof(buff), f)) {
		std::string ret = buff;
		std::string search = "TYPE=\"";
		size_t pos = ret.find(search);
		if (pos == std::string::npos)
			continue;

		ret = ret.substr(pos + search.length());
		pos = ret.find("\"");
		if (pos != std::string::npos)
			ret = ret.substr(0, pos);

		char *e = strstr(buff + 7, ":");
		if (!e)
			continue;
		*e = 0;

		hdd_s hdd;
		hdd.devname = std::string(buff + 5);
#if HAVE_ARM_HARDWARE
		if (strncmp(hdd.devname.c_str(), "mmcblk", 6) == 0)
			continue;
#endif
		hdd.mounted = is_mounted(buff + 5);
		hdd.fmt = ret;
		hdd.desc = hdd.devname + " (" + hdd.fmt + ")";
		printf("device: %s filesystem %s (%s)\n", hdd.devname.c_str(), hdd.fmt.c_str(), hdd.mounted ? "mounted" : "not mounted" );
		hdd_list.push_back(hdd);
	}
	fclose(f);
	waitpid(pid, NULL, 0); /* beware of the zombie apocalypse! */
}

std::string CHDDMenuHandler::getDefaultPart(std::string dev)
{
	std::string part = "1";
	if (dev == "mmcblk0")
		part = "p1";
	return part;
}

std::string CHDDMenuHandler::getFmtType(std::string name, std::string part)
{
	std::string ret = "";
	std::string dev = name + part;
	for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
		if (it->devname == dev) {
			ret = it->fmt;
			break;
		}
	}
	printf("getFmtType: dev [%s] fmt [%s]\n", dev.c_str(), ret.c_str());
	return ret;
}

void CHDDMenuHandler::check_kernel_fs()
{
	char line[128]; /* /proc/filesystems lines are shorter */
	kernel_fs_list.clear();
	FILE *f = fopen("/proc/filesystems", "r");
	if (! f) {
		fprintf(stderr, "CHDDMenuHandler::%s: opening /proc/filesystems failed: %m\n", __func__);
		return;
	}
	while (fgets(line, sizeof(line), f)) {
		size_t l = strlen(line);
		if (l > 0)
			line[l - 1] = 0; /* remove \n */
		char *tab = strchr(line, '\t');
		if (! tab)	/* should not happen in any kernel I have seen */
			continue;
		tab++;
		kernel_fs_list.insert(std::string(tab));
	}
	fclose(f);
}

void CHDDMenuHandler::check_dev_tools()
{
	for (unsigned i = 0; i < FS_MAX; i++) {
		if (kernel_fs_list.find(devtools[i].fmt) == kernel_fs_list.end()) {
			printf("%s: filesystem '%s' not supported by kernel\n",
				__func__, devtools[i].fmt.c_str());
			continue;
		}
		if (!access(devtools[i].fsck.c_str(), X_OK))
			devtools[i].fsck_supported = true;
		if (!access(devtools[i].mkfs.c_str(), X_OK))
			devtools[i].mkfs_supported = true;
		printf("check_dev_tools: %s: fsck %d mkfs %d\n", devtools[i].fmt.c_str(), devtools[i].fsck_supported, devtools[i].mkfs_supported);
	}
}

devtool_s * CHDDMenuHandler::get_dev_tool(std::string fmt)
{
	for (unsigned i = 0; i < FS_MAX; i++) {
		if (fmt == devtools[i].fmt)
			return &devtools[i];
	}
	return NULL;
}

bool CHDDMenuHandler::mount_dev(std::string name)
{
	std::string dev = name.substr(0, 2);
	if (dev == "sr" && !access(EJECT_BIN, X_OK)) {
		std::string eject = std::string(EJECT_BIN) + " -t /dev/" + name;
		system(eject.c_str());
		sleep(3);
	}
#ifdef ASSUME_MDEV
	std::string cmd = std::string("ACTION=add") + " MDEV=" + name + " " + MDEV_MOUNT;
#else
	std::string dst = MOUNT_BASE + name;
	safe_mkdir(dst.c_str());
	std::string cmd = std::string("mount ") + "/dev/" + name + " " + dst;
#endif
	printf("CHDDMenuHandler::mount_dev: mount cmd [%s]\n", cmd.c_str());
	system(cmd.c_str());
	lock_refresh = true;
	return is_mounted(name.c_str());
}

bool CHDDMenuHandler::umount_dev(std::string name)
{
#ifdef ASSUME_MDEV
	std::string cmd = std::string("ACTION=remove") + " MDEV=" + name + " " + MDEV_MOUNT;
	printf("CHDDMenuHandler::umount_dev: umount cmd [%s]\n", cmd.c_str());
	system(cmd.c_str());
#else
	std::string path = MOUNT_BASE + name;
	if (::umount(path.c_str()))
		return false;
#endif
	std::string dev = name.substr(0, 2);
	if (dev == "sr" && !access(EJECT_BIN, X_OK)) {
		std::string eject = std::string(EJECT_BIN) + " /dev/" + name;
		system(eject.c_str());
	}
	lock_refresh = true;
	return !is_mounted(name.c_str());
}

bool CHDDMenuHandler::umount_all(std::string dev)
{
	bool ret = true;
	for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
		std::string mdev = it->devname.substr(0, dev.size());
		if (mdev == dev) {
			if (is_mounted(it->devname.c_str()))
				ret &= umount_dev(it->devname);
		}
	}
	return ret;
}

#ifdef ASSUME_MDEV
bool CHDDMenuHandler::add_dev(std::string dev, std::string part)
{
	std::string filename = "/sys/block/" + dev + "/" + dev + part + "/uevent";
	if (!access(filename.c_str(), W_OK)) {
		FILE *f = fopen(filename.c_str(), "w");
		if (!f)
			printf("HDD: %s could not open %s: %m\n", __func__, filename.c_str());
		else {
			printf("-> triggering add uevent in %s\n", filename.c_str());
			fprintf(f, "add\n");
			fclose(f);
			return true;
		}
	}
	return false;
}

bool CHDDMenuHandler::waitfordev(std::string dev, int maxwait)
{
	int ret = true;
	int waitcount = 0;
	/* wait for the device to show up... */
	while (access(dev.c_str(), W_OK)) {
		if (!waitcount)
			printf("CHDDFmtExec: waiting for %s", dev.c_str());
		else
			printf(".");
		fflush(stdout);
		waitcount++;
		if (waitcount > maxwait) {
			fprintf(stderr, "CHDDFmtExec: device %s did not appear!\n", dev.c_str());
			ret = false;
			break;
		}
		sleep(1);
	}
	if (waitcount && waitcount <= maxwait)
		printf("\n");
	return ret;
}
#endif

void CHDDMenuHandler::showHint(std::string &message)
{
	CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, message.c_str());
	hintBox.paint();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(3);
        neutrino_msg_t      msg;
        neutrino_msg_data_t data;

	while(true) {
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ((msg == CRCInput::RC_timeout) || (msg < CRCInput::RC_MaxRC))
			break;
		else if (msg == NeutrinoMessages::EVT_HOTPLUG) {
			g_RCInput->postMsg(msg, data);
			break;
		}
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			break;
	}
	hintBox.hide();
}

void CHDDMenuHandler::setRecordPath(std::string &dev)
{
	std::string newpath = std::string(MOUNT_BASE) + dev + "/movies";
	if (g_settings.network_nfs_recordingdir == newpath) {
		printf("CHDDMenuHandler::setRecordPath: recordingdir already set to %s\n", newpath.c_str());
		return;
	}
	/* don't annoy if the recordingdir is a symlink pointing to the 'right' location */
	std::string readl = readlink(g_settings.network_nfs_recordingdir.c_str());
	readl = trim(readl);
	if (newpath.compare(readl) == 0) {
		printf("CHDDMenuHandler::%s: recordingdir is a symlink to %s\n",
					__func__, newpath.c_str());
		return;
	}
	bool old_menu = in_menu;
	in_menu = false;
	int res = ShowMsg(LOCALE_RECORDINGMENU_DEFDIR, LOCALE_HDD_SET_RECDIR, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo);
	if(res == CMsgBox::mbrYes) {
		g_settings.network_nfs_recordingdir = newpath;
		CRecordManager::getInstance()->SetDirectory(g_settings.network_nfs_recordingdir);
		if(g_settings.timeshiftdir.empty())
		{
			std::string timeshiftDir = g_settings.network_nfs_recordingdir + "/.timeshift";
			safe_mkdir(timeshiftDir.c_str());
			printf("New timeshift dir: %s\n", timeshiftDir.c_str());
			CRecordManager::getInstance()->SetTimeshiftDirectory(timeshiftDir);
		}
	}
	in_menu = old_menu;
}

int CHDDMenuHandler::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if (msg == NeutrinoMessages::EVT_HOTPLUG) {
		std::string str((char *) data);
		std::map<std::string,std::string> smap;

		if (!split_config_string(str, smap))
			return messages_return::handled;

		std::string dev;
		std::map<std::string,std::string>::iterator it = smap.find("MDEV");
		if (it != smap.end())
			dev = it->second;
		else {
			it = smap.find("DEVNAME");
			if (it == smap.end())
				return messages_return::handled;
			dev = it->second;
			if (dev.length() > 5)
				dev = dev.substr(5); /* strip off /dev/ */
		}
		printf("CHDDMenuHandler::handleMsg: MDEV=%s\n", dev.c_str());
		if (!filterDevName(dev.c_str()))
			return messages_return::handled;

		it = smap.find("ACTION");
		if (it == smap.end())
			return messages_return::handled;

		bool added = it->second == "add";
		bool mounted = is_mounted(dev.c_str());
		std::string tmp = dev.substr(0, 2);

		if (added && !mounted && tmp != "sr") {
			std::string message = dev + ": " + g_Locale->getText(LOCALE_HDD_MOUNT_FAILED);
			message +=  std::string(" ") + g_Locale->getText(LOCALE_HDD_FORMAT) + std::string(" ?");
			int res = ShowMsg(LOCALE_MESSAGEBOX_INFO, message, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo);
			if(res == CMsgBox::mbrYes) {
				unsigned char * p = new unsigned char[dev.size() + 1];
				if (p) {
					sprintf((char *)p, "%s", dev.c_str());
					g_RCInput->postMsg(NeutrinoMessages::EVT_FORMAT_DRIVE , (neutrino_msg_data_t)p);
					return messages_return::handled | messages_return::cancel_all;
				}
			}
		} else {
			std::string message = dev + ": " + (added ?
					g_Locale->getText(mounted ? LOCALE_HDD_MOUNT_OK : LOCALE_HDD_MOUNT_FAILED)
					: g_Locale->getText(LOCALE_HDD_UMOUNTED));
			showHint(message);
			if (added && tmp != "sr")
				setRecordPath(dev);
		}
		if (in_menu && !lock_refresh) {
			show_menu = true;
			return messages_return::handled | messages_return::cancel_all;
		}
		lock_refresh = false;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_FORMAT_DRIVE) {
		std::string dev((char *) data);
		printf("NeutrinoMessages::EVT_FORMAT_DRIVE: [%s]\n", dev.c_str());
		check_dev_tools();
		getBlkIds();
		scanDevices();
		for (std::map<std::string, std::string>::iterator it = devtitle.begin(); it != devtitle.end(); ++it) {
			if (dev.substr(0, it->first.size()) == it->first) {
				showDeviceMenu(it->first);
				break;
			}
		}
		hdd_list.clear();
		devtitle.clear();
		return messages_return::handled;
	}
	return messages_return::unhandled;
}

int CHDDMenuHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	if (parent)
		parent->hide();

	if (actionkey.empty())
		return doMenu();

	std::string dev = actionkey.substr(1);
	printf("CHDDMenuHandler::exec actionkey %s dev %s\n", actionkey.c_str(), dev.c_str());
	if (actionkey[0] == 'm') {
		for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
			if (it->devname == dev) {
				CHintBox hintbox(it->mounted ? LOCALE_HDD_UMOUNT : LOCALE_HDD_MOUNT, it->devname.c_str());
				hintbox.paint();
				if  (it->mounted)
					umount_dev(it->devname);
				else
					mount_dev(it->devname);

				it->mounted = is_mounted(it->devname.c_str());
				it->cmf->setOption(it->mounted ? umount : mount);
				hintbox.hide();
				return menu_return::RETURN_REPAINT;
			}
		}
	}
	else if (actionkey[0] == 'd') {
		return showDeviceMenu(dev);
	}
	else if (actionkey[0] == 'c') {
		return checkDevice(dev);
	}
	else if (actionkey[0] == 'f') {
		int ret = formatDevice(dev);
#if 0
		std::string devname = "/dev/" + dev + getDefaultPart(dev);
		if (show_menu && is_mounted(devname.c_str())) {
			devname = dev + getDefaultPart(dev);
			setRecordPath(devname);
		}
#endif
		return ret;
	}
	return menu_return::RETURN_REPAINT;
}

int CHDDMenuHandler::showDeviceMenu(std::string dev)
{
	printf("CHDDMenuHandler::showDeviceMenu: dev %s\n", dev.c_str());
	CMenuWidget* hddmenu = new CMenuWidget(devtitle[dev].c_str(), NEUTRINO_ICON_SETTINGS);
	hddmenu->addIntroItems();

	CMenuForwarder * mf;

	std::string fmt_type = getFmtType(dev, getDefaultPart(dev));
	bool fsck_enabled = false;
	bool mkfs_enabled = false;
	struct CMenuOptionChooser::keyval_ext fsoptions[FS_MAX];
	int opcount = 0;
	for (unsigned i = 0; i < FS_MAX; i++) {
		if (devtools[i].mkfs_supported) {
			fsoptions[opcount].key = i;
			fsoptions[opcount].value = NONEXISTANT_LOCALE;
			fsoptions[opcount].valname = devtools[i].fmt.c_str();
			mkfs_enabled = true;
			opcount++;
		}
		if (fmt_type == devtools[i].fmt)
			g_settings.hdd_fs = i;
	}
	if (!opcount) {
		fsoptions[0].key = 0;
		fsoptions[0].valname = devtools[0].fmt.c_str();
		opcount++;
	}
	int cnt = 0;
	bool found = false;
	for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
		std::string mdev = it->devname.substr(0, dev.size());
		if (mdev == dev) {
			printf("found %s partition %s\n", dev.c_str(), it->devname.c_str());
			fsck_enabled = false;
			devtool_s * devtool = get_dev_tool(it->fmt);
			if (devtool) {
				fsck_enabled = devtool->fsck_supported;
			}

			std::string key = "c" + it->devname;
			mf = new CMenuForwarder(LOCALE_HDD_CHECK, fsck_enabled, it->desc, this, key.c_str());
			mf->setHint("", LOCALE_MENU_HINT_HDD_CHECK);
			hddmenu->addItem(mf);
			found = true;
			cnt++;
		}
	}

	if (found)
		hddmenu->addItem(new CMenuSeparator(CMenuSeparator::LINE));

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_HDD_FS, &g_settings.hdd_fs, fsoptions, opcount, mkfs_enabled);
	mc->setHint("", LOCALE_MENU_HINT_HDD_FMT);
	hddmenu->addItem(mc);

	std::string key = "f" + dev;
	mf = new CMenuForwarder(LOCALE_HDD_FORMAT, true, "", this, key.c_str());
	mf->setHint("", LOCALE_MENU_HINT_HDD_FORMAT);
	hddmenu->addItem(mf);

	int res = hddmenu->exec(NULL, "");
	delete hddmenu;
	return res;
}

bool CHDDMenuHandler::scanDevices()
{
	struct dirent **namelist;
	struct stat s;
	int root_dev = -1;

	int n = scandir("/sys/block", &namelist, my_filter, alphasort);
	if (n < 0) {
		perror("CHDDMenuHandler::scanDevices: scandir(\"/sys/block\") failed");
		return false;
	}

	int drive_mask = 0xfff0;
	if (stat("/", &s) != -1)
	{
		if ((s.st_dev & drive_mask) == 0x0300) /* hda, hdb,... has max 63 partitions */
			drive_mask = 0xffc0; /* hda: 0x0300, hdb: 0x0340, sda: 0x0800, sdb: 0x0810 */
		root_dev = (s.st_dev & drive_mask);
	}
	printf("HDD: root_dev: 0x%04x\n", root_dev);

	for(int i = 0; i < n;i++) {
		char str[256];
		char vendor[128] = { 0 };
		char model[128] = { 0 };
		int64_t bytes = 0;
		int64_t megabytes;
		bool oldkernel = false;
		bool isroot = false;

		printf("HDD: checking /sys/block/%s\n", namelist[i]->d_name);
		snprintf(str, sizeof(str), "/dev/%s", namelist[i]->d_name);
		int fd = open(str, O_RDONLY);
		if (fd >= 0) {
			if (ioctl(fd, BLKGETSIZE64, &bytes))
				perror("BLKGETSIZE64");

			int ret = fstat(fd, &s);
			if (ret != -1) {
				if ((int)(s.st_rdev & drive_mask) == root_dev) {
					isroot = true;
					/* dev_t is different sized on different architectures :-( */
					printf("-> root device is on this disk 0x%04x, skipping\n", (int)s.st_rdev);
				}
			}
			close(fd);
		} else {
			printf("Cant open %s\n", str);
		}
		if (isroot)
			continue;

		megabytes = bytes/1000000;

		snprintf(str, sizeof(str), "/sys/block/%s/device/vendor", namelist[i]->d_name);
		FILE * f = fopen(str, "r");
		if(!f) {
			printf("Cant open %s\n", str);
			snprintf(str, sizeof(str), "/sys/block/%s/device/type", namelist[i]->d_name);
			f = fopen(str, "r");
		}
		if (f) {
			fscanf(f, "%s", vendor);
			fclose(f);
		} else {
			oldkernel = true;
			strcpy(vendor, "");
		}

		/* the Tripledragon only has kernel 2.6.12 available.... :-( */
		if (oldkernel)
			snprintf(str, sizeof(str), "/proc/ide/%s/model", namelist[i]->d_name);
		else
			snprintf(str, sizeof(str), "/sys/block/%s/device/model", namelist[i]->d_name);
		f = fopen(str, "r");
		if(!f) {
			printf("Cant open %s\n", str);
			snprintf(str, sizeof(str), "/sys/block/%s/device/name", namelist[i]->d_name);
			f = fopen(str, "r");
		}
		if (f) {
			fscanf(f, "%s", model);
			fclose(f);
		}
#if 0
		int removable = 0;
		snprintf(str, sizeof(str), "/sys/block/%s/removable", namelist[i]->d_name);
		f = fopen(str, "r");
		if(!f) {
			printf("Cant open %s\n", str);
			continue;
		}
		fscanf(f, "%d", &removable);
		fclose(f);
#endif
		std::string dev = std::string(namelist[i]->d_name).substr(0, 2);
		std::string fmt = getFmtType(namelist[i]->d_name);
		/* epmty cdrom do not appear in blkid output */
		if (fmt.empty() && dev == "sr") {
			hdd_s hdd;
			hdd.devname = namelist[i]->d_name;
			hdd.mounted = false;
			hdd.fmt = "";
			hdd.desc = hdd.devname;
			hdd_list.push_back(hdd);
		}

		snprintf(str, sizeof(str), "%s %s %ld %s", vendor, model, (long)(megabytes < 10000 ? megabytes : megabytes/1000), megabytes < 10000 ? "MB" : "GB");
		printf("HDD: %s\n", str);
		devtitle[namelist[i]->d_name] = str;

		free(namelist[i]);
	}
	if (n >= 0)
		free(namelist);
	return !devtitle.empty();
}

int CHDDMenuHandler::doMenu()
{
	show_menu = false;
	in_menu = true;

	check_kernel_fs();
	check_dev_tools();

_show_menu:
	getBlkIds();
	scanDevices();

	CMenuWidget* hddmenu = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_DRIVESETUP);

	hddmenu->addIntroItems(LOCALE_HDD_SETTINGS, LOCALE_HDD_EXTENDED_SETTINGS);

	CHDDDestExec hddexec;
	CMenuForwarder * mf = new CMenuForwarder(LOCALE_HDD_ACTIVATE, true, "", &hddexec, NULL, CRCInput::RC_red);
	mf->setHint("", LOCALE_MENU_HINT_HDD_APPLY);
	hddmenu->addItem(mf);

	CMenuOptionChooser * mc = new CMenuOptionChooser(LOCALE_HDD_SLEEP, &g_settings.hdd_sleep, HDD_SLEEP_OPTIONS, HDD_SLEEP_OPTION_COUNT, true);
	mc->setHint("", LOCALE_MENU_HINT_HDD_SLEEP);
	hddmenu->addItem(mc);

	const char hdparm[] = "/sbin/hdparm";
	struct stat stat_buf;
	bool have_nonbb_hdparm = !::lstat(hdparm, &stat_buf) && !S_ISLNK(stat_buf.st_mode);
	if (have_nonbb_hdparm) {
		mc = new CMenuOptionChooser(LOCALE_HDD_NOISE, &g_settings.hdd_noise, HDD_NOISE_OPTIONS, HDD_NOISE_OPTION_COUNT, true);
		mc->setHint("", LOCALE_MENU_HINT_HDD_NOISE);
		hddmenu->addItem(mc);
	}

	hddmenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_HDD_MANAGE));

	for (std::map<std::string, std::string>::iterator it = devtitle.begin(); it != devtitle.end(); ++it) {
		std::string dev = it->first.substr(0, 2);
		bool enabled = !CNeutrinoApp::getInstance()->recordingstatus && dev != "sr";
		std::string key = "d" + it->first;
		mf = new CMenuForwarder(it->first, enabled, it->second, this, key.c_str());
		mf->setHint("", LOCALE_MENU_HINT_HDD_TOOLS);
		hddmenu->addItem(mf);
	}

	if(devtitle.empty()) {
		//if no drives found, select 'back'
		if (hddmenu->getSelected() != -1)
			hddmenu->setSelected(2);
		hddmenu->addItem(new CMenuForwarder(LOCALE_HDD_NOT_FOUND, false));
	}

	if (!hdd_list.empty()) {
		struct stat rec_st, root_st, dev_st;
		memset(&rec_st, 0, sizeof(rec_st));
		memset(&root_st, 0, sizeof(root_st));
		stat(g_settings.network_nfs_recordingdir.c_str(), &rec_st);
		stat("/", &root_st);

		sort(hdd_list.begin(), hdd_list.end(), cmp_hdd_by_name());
		mount = g_Locale->getText(LOCALE_HDD_MOUNT);
		umount = g_Locale->getText(LOCALE_HDD_UMOUNT);
		int shortcut = 1;
		hddmenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_HDD_MOUNT_UMOUNT));
		for (std::vector<hdd_s>::iterator it = hdd_list.begin(); it != hdd_list.end(); ++it) {
			const char * rec_icon = NULL;
			if (it->mounted) {
				std::string dst = MOUNT_BASE + it->devname;
				if (!stat(dst.c_str(), &stat_buf) && rec_st.st_dev == stat_buf.st_dev)
					rec_icon = CNeutrinoApp::getInstance()->recordingstatus ? NEUTRINO_ICON_REC : NEUTRINO_ICON_REC_GRAY;
			}
			std::string key = "m" + it->devname;
			bool enabled = !rec_icon || !CNeutrinoApp::getInstance()->recordingstatus;
			/* do not allow to unmount the rootfs, and skip filesystems without kernel support */
			memset(&dev_st, 0, sizeof(dev_st));
			if (stat(("/dev/" + it->devname).c_str(), &dev_st) != -1
			    && dev_st.st_rdev == root_st.st_dev)
				enabled = false;
			else if (kernel_fs_list.find(it->fmt) == kernel_fs_list.end())
				enabled = false;
			it->cmf = new CMenuForwarder(it->desc, enabled, it->mounted ? umount : mount , this,
					key.c_str(), CRCInput::convertDigitToKey(shortcut++), NULL, rec_icon);
			hddmenu->addItem(it->cmf);
		}
	}

	int ret = hddmenu->exec(NULL, "");

	delete hddmenu;
	hdd_list.clear();
	devtitle.clear();
	if (show_menu) {
		show_menu = false;
		goto _show_menu;
	}
	in_menu = false;
	return ret;
}

#if 0
static int dev_umount(char *dev)
{
	char buffer[255];
	FILE *f = fopen("/proc/mounts", "r");
	if(f == NULL)
		return -1;
	while (fgets (buffer, 255, f) != NULL) {
		char *p = buffer + strlen(dev);
		if (strstr(buffer, dev) == buffer && *p == ' ') {
			p++;
			char *q = strchr(p, ' ');
			if (q == NULL)
				continue;
			*q = 0x0;
			fclose(f);
			printf("dev_umount %s: umounting %s\n", dev, p);
			return umount(p);
		}
	}
#ifndef ASSUME_MDEV
	/* with mdev, we hopefully don't have to umount anything here... */
	printf("dev_umount %s: not found\n", dev);
#endif
	errno = ENOENT;
	fclose(f);
	return -1;
}

/* unmounts all partitions of a given block device, dev can be /dev/sda, sda or sda4 */
static int umount_all(const char *dev)
{
	char buffer[255];
	int i;
	char *d = strdupa(dev);
	char *p = d + strlen(d) - 1;
	while (isdigit(*p))
		p--;
	*++p = 0x0;
	if (strstr(d, "/dev/") == d)
		d += strlen("/dev/");
	printf("HDD: %s dev = '%s' d = '%s'\n", __func__, dev, d);
	for (i = 1; i < 16; i++)
	{
		sprintf(buffer, "/dev/%s%d", d, i);
		// printf("checking for '%s'\n", buffer);
		if (access(buffer, R_OK))
			continue;	/* device does not exist? */
#ifdef ASSUME_MDEV
		/* we can't use a 'remove' uevent, as that would also remove the device node
		 * which we certainly need for formatting :-) */
		if (! access("/etc/mdev/mdev-mount.sh", X_OK)) {
			sprintf(buffer, "MDEV=%s%d ACTION=remove /etc/mdev/mdev-mount.sh block", d, i);
			printf("-> running '%s'\n", buffer);
			my_system(3, "/bin/sh", "-c", buffer);
		}
#endif
		sprintf(buffer, "/dev/%s%d", d, i);
		/* just to make sure */
		swapoff(buffer);
		if (dev_umount(buffer) && errno != ENOENT)
			fprintf(stderr, "could not umount %s: %m\n", buffer);
	}
	return 0;
}

/* triggers a uevent for all partitions of a given blockdev, dev can be /dev/sda, sda or sda4 */
static int mount_all(const char *dev)
{
	char buffer[255];
	int i, ret = -1;
	char *d = strdupa(dev);
	char *p = d + strlen(d) - 1;
	while (isdigit(*p))
		p--;
	if (strstr(d, "/dev/") == d)
		d += strlen("/dev/");
	*++p = 0x0;
	printf("HDD: %s dev = '%s' d = '%s'\n", __func__, dev, d);
	for (i = 1; i < 16; i++)
	{
#ifdef ASSUME_MDEV
		sprintf(buffer, "/sys/block/%s/%s%d/uevent", d, d, i);
		if (!access(buffer, W_OK)) {
			FILE *f = fopen(buffer, "w");
			if (!f)
				fprintf(stderr, "HDD: %s could not open %s: %m\n", __func__, buffer);
			else {
				printf("-> triggering add uevent in %s\n", buffer);
				fprintf(f, "add\n");
				fclose(f);
				ret = 0;
			}
		}
#endif
	}
	return ret;
}

#ifdef ASSUME_MDEV
static void waitfordev(const char *src, int maxwait)
{
	int waitcount = 0;
	/* wait for the device to show up... */
	while (access(src, W_OK)) {
		if (!waitcount)
			printf("CHDDFmtExec: waiting for %s", src);
		else
			printf(".");
		fflush(stdout);
		waitcount++;
		if (waitcount > maxwait) {
			fprintf(stderr, "CHDDFmtExec: device %s did not appear!\n", src);
			break;
		}
		sleep(1);
	}
	if (waitcount && waitcount <= maxwait)
		printf("\n");
}
#else
static void waitfordev(const char *, int)
{
}
#endif
#endif

void CHDDMenuHandler::showError(neutrino_locale_t err)
{
	ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(err), CMsgBox::mbrOk, CMsgBox::mbOk);
}

int CHDDMenuHandler::formatDevice(std::string dev)
{
	char cmd[100];
	char cmd2[100];
	int res;
	FILE * f;
	CProgressWindow * progress;
	std::string fdisk, sfdisk, tune2fs;

	printf("CHDDMenuHandler::formatDevice: dev %s hdd_fs %d\n", dev.c_str(), g_settings.hdd_fs);

	if (g_settings.hdd_fs < 0 || g_settings.hdd_fs >= (int) FS_MAX)
		return menu_return::RETURN_REPAINT;

	devtool_s * devtool = &devtools[g_settings.hdd_fs];
	if (!devtool || !devtool->mkfs_supported) {
		printf("CHDDMenuHandler::formatDevice: mkfs.%s is not supported\n", devtool ? devtool->fmt.c_str() : "unknown");
		return menu_return::RETURN_REPAINT;
	}

	std::string devname = "/dev/" + dev;
	std::string part = getDefaultPart(dev);

	std::string devpart = dev + part;
	std::string partname = devname + part;

	std::string mkfscmd = devtool->mkfs + " " + devtool->mkfs_options + " " + partname;
	printf("mkfs cmd: [%s]\n", mkfscmd.c_str());

	res = ShowMsg(LOCALE_HDD_FORMAT, g_Locale->getText(LOCALE_HDD_FORMAT_WARN), CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo );
	if(res != CMsgBox::mbrYes)
		return menu_return::RETURN_REPAINT;

	bool srun = my_system(3, "killall", "-9", "smbd");

	res = umount_all(dev);
	printf("CHDDMenuHandler::formatDevice: umount res %d\n", res);

	if(!res) {
		showError(LOCALE_HDD_UMOUNT_WARN);
		goto _return;
	}

#ifndef ASSUME_MDEV
	f = fopen("/proc/sys/kernel/hotplug", "w");
	if(f) {
		fprintf(f, "none\n");
		fclose(f);
	}
#endif
	creat("/tmp/.nomdevmount", 00660);

	progress = new CProgressWindow();
	progress->setTitle(LOCALE_HDD_FORMAT);
	progress->exec(NULL,"");
	progress->showGlobalStatus(0);

	fdisk   = find_executable("fdisk");
	sfdisk  = find_executable("sfdisk");
	tune2fs = find_executable("tune2fs");
	if (! sfdisk.empty()) {
		snprintf(cmd, sizeof(cmd), "%s -f -uM %s", sfdisk.c_str(), devname.c_str());
		strcpy(cmd2, "0,\n;\n;\n;\ny\n");
	} else if (! fdisk.empty()) {
		snprintf(cmd, sizeof(cmd), "%s -u %s", fdisk.c_str(), devname.c_str());
		strcpy(cmd2, "o\nn\np\n1\n2048\n\nw\n");
	} else {
		/* cannot do anything */
		fprintf(stderr, "CHDDFmtExec: neither fdisk nor sfdisk found in $PATH :-(\n");
		showError(LOCALE_HDD_FORMAT_FAILED);
		goto _remount;
	}
	progress->showStatusMessageUTF(cmd);

#ifdef ASSUME_MDEV
	/* mdev will create it and waitfordev will wait for it... */
	unlink(partname.c_str());
#endif
	printf("CHDDMenuHandler::formatDevice: executing %s\n", cmd);
	f = popen(cmd, "w");
	if (!f) {
		showError(LOCALE_HDD_FORMAT_FAILED);
		res = -1;
		goto _remount;
	}
	show_menu = true;

	fprintf(f, "%s", cmd2);
	res = pclose(f);
	printf("CHDDMenuHandler::formatDevice: (s)fdisk res: %d\n", res);
	if (res) {
		showError(LOCALE_HDD_FORMAT_FAILED);
		goto _remount;
	}
	sleep(2);
#ifdef ASSUME_MDEV
	add_dev(dev, part);
	waitfordev(devname + part, 30);
#endif

	progress->showStatusMessageUTF(mkfscmd.c_str());
	f = popen(mkfscmd.c_str(), "r");
	if (!f) {
		showError(LOCALE_HDD_FORMAT_FAILED);
		res = -1;
		goto _remount;
	}

	char buf[256];
	setbuf(f, NULL);
	int n, t, in, pos, stage;
	pos = 0;
	stage = 0;
	while (true)
	{
		in = fgetc(f);
		if (in == EOF)
			break;

		buf[pos] = (char)in;
		pos++;
		buf[pos] = 0;
		if (in == '\b' || in == '\n')
			pos = 0; /* start a new line */
		//printf("%s", buf);
		switch (stage) {
			case 0:
				if (strcmp(buf, "Writing inode tables:") == 0) {
					stage++;
					progress->showGlobalStatus(20);
					progress->showStatusMessageUTF(buf);
				}
				break;
			case 1:
				if (in == '\b' && sscanf(buf, "%d/%d\b", &n, &t) == 2) {
					if (t == 0)
						t = 1;
					int percent = 100 * n / t;
					progress->showLocalStatus(percent);
					progress->showGlobalStatus(20 + percent / 5);
				}
				if (strstr(buf, "done")) {
					stage++;
					pos = 0;
				}
				break;
			case 2:
				if (strstr(buf, "blocks):") && sscanf(buf, "Creating journal (%d blocks):", &n) == 1) {
					progress->showLocalStatus(0);
					progress->showGlobalStatus(60);
					progress->showStatusMessageUTF(buf);
					pos = 0;
				}
				if (strstr(buf, "done")) {
					stage++;
					pos = 0;
				}
				break;
			case 3:
				if (strcmp(buf, "Writing superblocks and filesystem accounting information:") == 0) {
					progress->showGlobalStatus(80);
					progress->showStatusMessageUTF(buf);
					pos = 0;
				}
				break;
			default:
				// printf("unknown stage! %d \n\t", stage);
				break;
		}
	}
	progress->showLocalStatus(100);
	res = pclose(f);
	printf("CHDDMenuHandler::formatDevice: mkfs res: %d\n", res);
	progress->showGlobalStatus(100);
	if (res) {
		showError(LOCALE_HDD_FORMAT_FAILED);
		goto _remount;
	}
	sleep(2);

	if (devtool->fmt.substr(0, 3) == "ext" && ! tune2fs.empty()) {
		std::string d = "/dev/" + devpart;
		printf("CHDDMenuHandler::formatDevice: executing %s %s %s\n", tune2fs.c_str(), "-r 0 -c 0 -i 0", d.c_str());
		my_system(8, tune2fs.c_str(), "-r", "0", "-c", "0", "-i", "0", d.c_str());
	}
	show_menu = true;

_remount:
	unlink("/tmp/.nomdevmount");
	progress->hide();
	delete progress;

#ifndef ASSUME_MDEV
	f = fopen("/proc/sys/kernel/hotplug", "w");
	if(f) {
		fprintf(f, "/sbin/hotplug\n");
		fclose(f);
	}
#endif
	if (!res) {
		res = mount_dev(devpart);

		if(res) {
			std::string dst = MOUNT_BASE + devpart;
			snprintf(cmd, sizeof(cmd), "%s/movies", dst.c_str());
			safe_mkdir(cmd);
			snprintf(cmd, sizeof(cmd), "%s/pictures", dst.c_str());
			safe_mkdir(cmd);
			snprintf(cmd, sizeof(cmd), "%s/epg", dst.c_str());
			safe_mkdir(cmd);
			snprintf(cmd, sizeof(cmd), "%s/music", dst.c_str());
			safe_mkdir(cmd);
			snprintf(cmd, sizeof(cmd), "%s/logos", dst.c_str());
			safe_mkdir(cmd);
			snprintf(cmd, sizeof(cmd), "%s/plugins", dst.c_str());
			safe_mkdir(cmd);
			// sync();
#if HAVE_TRIPLEDRAGON
		/* on the tripledragon, we mount via fstab, so we need to add an
		   fstab entry for dst */
		FILE *g;
		char *line = NULL;
		unlink("/etc/fstab.new");
		g = fopen("/etc/fstab.new", "w");
		f = fopen("/etc/fstab", "r");
		if (!g)
			perror("open /etc/fstab.new");
		else {
			if (f) {
				int ret;
				while (true) {
					size_t dummy;
					ret = getline(&line, &dummy, f);
					if (ret < 0)
						break;
					/* remove lines that start with the same disk we formatted
					   devname is /dev/xda" */
					if (strncmp(line, devname.c_str(), devname.length()) != 0)
						fprintf(g, "%s", line);
				}
				free(line);
				fclose(f);
			}
			/* now add our new entry */
			fprintf(g, "%s %s auto defaults 0 0\n", partname.c_str(), dst.c_str());
			fclose(g);
			rename("/etc/fstab", "/etc/fstab.old");
			rename("/etc/fstab.new", "/etc/fstab");
		}
#endif
		}
	}
_return:
	if (!srun) my_system(1, "smbd");
	if (show_menu)
		return menu_return::RETURN_EXIT_ALL;

	return menu_return::RETURN_REPAINT;
}

int CHDDMenuHandler::checkDevice(std::string dev)
{
	int res;
	FILE * f;
	CProgressWindow * progress;
	int oldpass = 0, pass, step, total;
	int percent = 0, opercent = 0;
	char buf[256] = { 0 };

	bool loop;
	uint64_t timeoutEnd;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	std::string devname = "/dev/" + dev;

	printf("CHDDMenuHandler::checkDevice: dev %s\n", dev.c_str());

	std::string fmt = getFmtType(dev);
	devtool_s * devtool = get_dev_tool(fmt);
	if (!devtool || !devtool->fsck_supported)
		return menu_return::RETURN_REPAINT;

	std::string cmd = devtool->fsck + " " + devtool->fsck_options + " " + devname;
	printf("fsck cmd: [%s]\n", cmd.c_str());

	bool srun = my_system(3, "killall", "-9", "smbd");

	res = true;
	if (is_mounted(dev.c_str()))
		res = umount_dev(dev);

	printf("CHDDMenuHandler::checkDevice: umount res %d\n", res);
	if(!res) {
		showError(LOCALE_HDD_UMOUNT_WARN);
		return menu_return::RETURN_REPAINT;
	}

	printf("CHDDMenuHandler::checkDevice: Executing %s\n", cmd.c_str());
	f=popen(cmd.c_str(), "r");
	if(!f) {
		showError(LOCALE_HDD_CHECK_FAILED);
		goto ret1;
	}

	progress = new CProgressWindow();
	progress->setTitle(LOCALE_HDD_CHECK);
	progress->exec(NULL,"");
	progress->showStatusMessageUTF(cmd.c_str());

	while(fgets(buf, 255, f) != NULL)
	{
		if(isdigit(buf[0])) {
			sscanf(buf, "%d %d %d\n", &pass, &step, &total);
			if(total == 0)
				total = 1;
			if(oldpass != pass) {
				oldpass = pass;
				progress->showGlobalStatus(pass > 0 ? (pass-1)*20: 0);
			}
			percent = (step * 100) / total;
			if(opercent != percent) {
				opercent = percent;
//printf("CHDDChkExec: pass %d : %d\n", pass, percent);
				progress->showLocalStatus(percent);
			}
		}
		else {
			char *t = strrchr(buf, '\n');
			if (t)
				*t = 0;
			if(!strncmp(buf, "Pass", 4)) {
				progress->showStatusMessageUTF(buf);
			}
		}
	}
//printf("CHDDChkExec: %s\n", buf);
	res = pclose(f);
	if(res)
		showError(LOCALE_HDD_CHECK_FAILED);

	progress->showGlobalStatus(100);
	progress->showStatusMessageUTF(buf);

	timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
	loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
		if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_ok || msg == CRCInput::RC_home)
			loop = false;
	}

	progress->hide();
	delete progress;

ret1:
	res = mount_dev(dev);
	printf("CHDDMenuHandler::checkDevice: mount res %d\n", res);

	if (!srun) my_system(1, "smbd");
	return menu_return::RETURN_REPAINT;
}

int CHDDDestExec::exec(CMenuTarget* /*parent*/, const std::string&)
{
	struct dirent **namelist;
	int n = scandir("/sys/block", &namelist, my_filter, alphasort);

	if (n < 0)
		return menu_return::RETURN_NONE;

	if (g_settings.hdd_sleep > 0 && g_settings.hdd_sleep < 60)
		g_settings.hdd_sleep = 60;

	const char hdidle[] = "/sbin/hd-idle";
	bool have_hdidle = !access(hdidle, X_OK);

	if (have_hdidle) {
		system("kill $(pidof hd-idle)");
		int sleep_seconds = g_settings.hdd_sleep;
		switch (sleep_seconds) {
			case 241:
					sleep_seconds = 30 * 60;
					break;
			case 242:
					sleep_seconds = 60 * 60;
					break;
			default:
					sleep_seconds *= 5;
		}
		if (sleep_seconds)
			my_system(3, hdidle, "-i", to_string(sleep_seconds).c_str());

		while (n--)
			free(namelist[n]);
		free(namelist);
		return menu_return::RETURN_NONE;
	}

	const char hdparm[] = "/sbin/hdparm";
	bool have_hdparm = !access(hdparm, X_OK);
	if (!have_hdparm)
	{
		while (n--)
			free(namelist[n]);
		free(namelist);
		return menu_return::RETURN_NONE;
	}

	struct stat stat_buf;
	bool have_nonbb_hdparm = !::lstat(hdparm, &stat_buf) && !S_ISLNK(stat_buf.st_mode);

	for (int i = 0; i < n; i++) {
		printf("CHDDDestExec: noise %d sleep %d /dev/%s\n",
			 g_settings.hdd_noise, g_settings.hdd_sleep, namelist[i]->d_name);

		char M_opt[50],S_opt[50], opt[100];
		snprintf(S_opt, sizeof(S_opt), "-S%d", g_settings.hdd_sleep);
		snprintf(M_opt, sizeof(M_opt), "-M%d", g_settings.hdd_noise);
		snprintf(opt, sizeof(opt), "/dev/%s",namelist[i]->d_name);

		if (have_nonbb_hdparm)
			my_system(4, hdparm, M_opt, S_opt, opt);
		else // busybox hdparm doesn't support "-M"
			my_system(3, hdparm, S_opt, opt);

		free(namelist[i]);
	}
	free(namelist);
	return menu_return::RETURN_NONE;
}
