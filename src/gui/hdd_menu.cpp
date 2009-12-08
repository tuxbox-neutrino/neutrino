/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/mount.h>

#include <global.h>
#include <neutrino.h>
#include <gui/widget/icons.h>
#include "gui/widget/menue.h"
#include "gui/widget/stringinput.h"
#include "gui/widget/messagebox.h"
#include "gui/widget/hintbox.h"
#include "gui/widget/progresswindow.h"

#include "system/setting_helpers.h"
#include "system/settings.h"
#include "system/debug.h"

#include <gui/hdd_menu.h>
#include <mymenu.h>

int safe_mkdir(char * path);

static int my_filter(const struct dirent * dent)
{
	if(dent->d_name[0] == 's' && dent->d_name[1] == 'd')
		return 1;
	return 0;
}

int CHDDMenuHandler::exec(CMenuTarget* parent, const std::string &actionkey)
{
	if (parent)
		parent->hide();

	return doMenu ();
}

int CHDDMenuHandler::doMenu ()
{
	FILE * f;
	int fd;
	struct dirent **namelist;
	int ret;

	bool hdd_found = 0;
	int n = scandir("/sys/block", &namelist, my_filter, alphasort);
#if 0
	if(n <= 0) {
		//FIXME no hdd found message
		return 0;
	}
#endif

	CMenuWidget* hddmenu = new CMenuWidget(LOCALE_HDD_SETTINGS, NEUTRINO_ICON_SETTINGS);
	hddmenu->addItem( GenericMenuBack );
	hddmenu->addItem( GenericMenuSeparatorLine );

	hddmenu->addItem( new CMenuOptionChooser(LOCALE_HDD_SLEEP, &g_settings.hdd_sleep, HDD_SLEEP_OPTIONS, HDD_SLEEP_OPTION_COUNT, true));
	hddmenu->addItem( new CMenuOptionChooser(LOCALE_HDD_NOISE, &g_settings.hdd_noise, HDD_NOISE_OPTIONS, HDD_NOISE_OPTION_COUNT, true));

	hddmenu->addItem(new CMenuForwarder(LOCALE_HDD_ACTIVATE, true, "", new CHDDDestExec()));

	//if(n > 0)
	hddmenu->addItem( GenericMenuSeparatorLine );

	for(int i = 0; i < n;i++) {
		char str[256];
		char vendor[128], model[128];
		int64_t bytes;
		int64_t megabytes;
		int removable = 0;

		printf("HDD: checking /sys/block/%s\n", namelist[i]->d_name);
		sprintf(str, "/dev/%s", namelist[i]->d_name);
		fd = open(str, O_RDONLY);
		if(fd < 0) {
			printf("Cant open %s\n", str);
			continue;
		}
		if (ioctl(fd, BLKGETSIZE64, &bytes))
			perror("BLKGETSIZE64");
		close(fd);

		megabytes = bytes/1000000;

		sprintf(str, "/sys/block/%s/device/vendor", namelist[i]->d_name);
		f = fopen(str, "r");
		if(!f) {
			printf("Cant open %s\n", str);
			continue;
		}
		fscanf(f, "%s", vendor);
		fclose(f);

		sprintf(str, "/sys/block/%s/device/model", namelist[i]->d_name);
		f = fopen(str, "r");
		if(!f) {
			printf("Cant open %s\n", str);
			continue;
		}
		fscanf(f, "%s", model);
		fclose(f);

		sprintf(str, "/sys/block/%s/removable", namelist[i]->d_name);
		f = fopen(str, "r");
		if(!f) {
			printf("Cant open %s\n", str);
			continue;
		}
		fscanf(f, "%d", &removable);
		fclose(f);

		sprintf(str, "%s %s (%s-%s %lld %s)\n", g_Locale->getText(LOCALE_HDD_MANAGE), namelist[i]->d_name, vendor, model, megabytes < 10000 ? megabytes : megabytes/1000, megabytes < 10000 ? "MB" : "GB");
		printf("HDD: %s\n", str);
		CMenuWidget * tempMenu = new CMenuWidget(str, NEUTRINO_ICON_SETTINGS);
		tempMenu->addItem( GenericMenuBack );
		tempMenu->addItem( GenericMenuSeparatorLine );
		//tempMenu->addItem( new CMenuOptionChooser(LOCALE_HDD_FS, &g_settings.hdd_fs, HDD_FILESYS_OPTIONS, HDD_FILESYS_OPTION_COUNT, true));
		tempMenu->addItem(new CMenuForwarder(LOCALE_HDD_FORMAT, true, "", new CHDDFmtExec, namelist[i]->d_name));
		tempMenu->addItem(new CMenuForwarder(LOCALE_HDD_CHECK, true, "", new CHDDChkExec, namelist[i]->d_name));
		hddmenu->addItem(new CMenuForwarderNonLocalized(str, removable ? false : true, NULL, tempMenu));
		hdd_found = 1;
	}
	if(!hdd_found)
		hddmenu->addItem(new CMenuForwarder(LOCALE_HDD_NOT_FOUND, false));

	ret = hddmenu->exec(NULL, "");
	delete hddmenu;
	return ret;
}

int CHDDDestExec::exec(CMenuTarget* parent, const std::string&)
{
	char cmd[100];

	printf("CHDDDestExec: noise %d sleep %d\n", g_settings.hdd_noise, g_settings.hdd_sleep);
	//FIXME: atm we can have 2 hdd, sata and usb
	sprintf(cmd, "hdparm -S%d /dev/sda >/dev/null 2>/dev/null", g_settings.hdd_sleep);
	system(cmd);
	sprintf(cmd, "hdparm -M%d /dev/sda >/dev/null 2>/dev/null", g_settings.hdd_noise);
	system(cmd);
	sprintf(cmd, "hdparm -S%d /dev/sdb >/dev/null 2>/dev/null", g_settings.hdd_sleep);
	system(cmd);
	sprintf(cmd, "hdparm -M%d /dev/sdb >/dev/null 2>/dev/null", g_settings.hdd_noise);
	system(cmd);
	sprintf(cmd, "hdparm -K /dev/sda >/dev/null 2>/dev/null");
	system(cmd);
	sprintf(cmd, "hdparm -K /dev/sdb >/dev/null 2>/dev/null");
	system(cmd);
	return 1;
}

static int check_and_umount(char * dev, char * path)
{
	char buffer[255];
	FILE *f = fopen("/proc/mounts", "r");
	if(f == NULL)
		return -1;
	while (fgets (buffer, 255, f) != NULL) {
		if(strstr(buffer, dev)) {
			printf("HDD: %s mounted\n", path);
			fclose(f);
			return umount(path);
		}
	}
	fclose(f);
	printf("HDD: %s not mounted\n", path);
	return 0;
}

int CHDDFmtExec::exec(CMenuTarget* parent, const std::string& key)
{
	char cmd[100];
	CHintBox * hintbox;
	int res;
	FILE * f;
	char src[128], dst[128];
	CProgressWindow * progress;
	bool idone;

	sprintf(src, "/dev/%s1", key.c_str());
	sprintf(dst, "/media/%s1", key.c_str());

	printf("CHDDFmtExec: key %s\n", key.c_str());

	res = ShowMsgUTF ( LOCALE_HDD_FORMAT, g_Locale->getText(LOCALE_HDD_FORMAT_WARN), CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo );
	if(res != CMessageBox::mbrYes)
		return 0;

	bool srun = system("killall -9 smbd");

	//res = check_and_umount(dst);
	res = check_and_umount(src, dst);
	printf("CHDDFmtExec: umount res %d\n", res);

	if(res) {
		hintbox = new CHintBox(LOCALE_HDD_FORMAT, g_Locale->getText(LOCALE_HDD_UMOUNT_WARN));
		hintbox->paint();
		sleep(2);
		delete hintbox;
		goto _return;
	}

	f = fopen("/proc/sys/kernel/hotplug", "w");
	if(f) {
		fprintf(f, "none\n");
		fclose(f);
	}

	progress = new CProgressWindow();
	progress->setTitle(LOCALE_HDD_FORMAT);
	progress->exec(NULL,"");
	progress->showStatusMessageUTF("Executing fdisk");
	progress->showGlobalStatus(0);

	sprintf(cmd, "/sbin/sfdisk -f -uM /dev/%s", key.c_str());
	printf("CHDDFmtExec: executing %s\n", cmd);
	f=popen(cmd, "w");
	if (!f) {
		hintbox = new CHintBox(LOCALE_HDD_FORMAT, g_Locale->getText(LOCALE_HDD_FORMAT_FAILED));
		hintbox->paint();
		sleep(2);
		delete hintbox;
		goto _remount;
	}

	fprintf(f, "0,\n;\n;\n;\ny\n");
	pclose(f);
	//sleep(1);

	switch(g_settings.hdd_fs) {
		case 0:
			sprintf(cmd, "/sbin/mkfs.ext3 -T largefile -m0 %s", src);
			break;
		case 1:
			sprintf(cmd, "/sbin/mkreiserfs -f -f %s", src);
			break;
		default:
			return 0;
	}

	printf("CHDDFmtExec: executing %s\n", cmd);

	f=popen(cmd, "r");
	if (!f) {
		hintbox = new CHintBox(LOCALE_HDD_FORMAT, g_Locale->getText(LOCALE_HDD_FORMAT_FAILED));
		hintbox->paint();
		sleep(2);
		delete hintbox;
		goto _remount;
	}

	char buf[256];
	idone = false;
	while(fgets(buf, 255, f) != NULL)
	{
		printf("%s", buf);
                if(!idone && strncmp(buf, "Writing inode", 13)) {
			idone = true;
			buf[21] = 0;
			progress->showGlobalStatus(20);
                        progress->showStatusMessageUTF(buf);
                } 
		else if(strncmp(buf, "Creating", 8)) {
			progress->showGlobalStatus(40);
                        progress->showStatusMessageUTF(buf);
		}
		else if(strncmp(buf, "Writing superblocks", 19)) {
			progress->showGlobalStatus(60);
                        progress->showStatusMessageUTF(buf);
		}
	}
	pclose(f);
	progress->showGlobalStatus(100);
	sleep(2);

	sprintf(cmd, "/sbin/tune2fs -r 0 -c 0 -i 0 %s", src);
	printf("CHDDFmtExec: executing %s\n", cmd);
	system(cmd);

_remount:
	progress->hide();
	delete progress;

        switch(g_settings.hdd_fs) {
                case 0:
			res = mount(src, dst, "ext3", 0, NULL);
                        break;
                case 1:
			res = mount(src, dst, "reiserfs", 0, NULL);
                        break;
		default:
                        break;
        }
	f = fopen("/proc/sys/kernel/hotplug", "w");
	if(f) {
		fprintf(f, "/sbin/hotplug\n");
		fclose(f);
	}

	if(!res) {
		sprintf(cmd, "%s/movies", dst);
		safe_mkdir((char *) cmd);
		sprintf(cmd, "%s/pictures", dst);
		safe_mkdir((char *) cmd);
		sprintf(cmd, "%s/epg", dst);
		safe_mkdir((char *) cmd);
		sprintf(cmd, "%s/music", dst);
		safe_mkdir((char *) cmd);
		sync();
	}
_return:
	if(!srun) system("smbd");
	return menu_return::RETURN_REPAINT;
}

int CHDDChkExec::exec(CMenuTarget* parent, const std::string& key)
{
	char cmd[100];
	CHintBox * hintbox;
	int res;
	char src[128], dst[128];
	FILE * f;
	CProgressWindow * progress;
	int oldpass = 0, pass, step, total;
	int percent = 0, opercent = 0;

	sprintf(src, "/dev/%s1", key.c_str());
	sprintf(dst, "/media/%s1", key.c_str());

printf("CHDDChkExec: key %s\n", key.c_str());

	bool srun = system("killall -9 smbd");

	//res = check_and_umount(dst);
	res = check_and_umount(src, dst);
	printf("CHDDChkExec: umount res %d\n", res);
	if(res) {
		hintbox = new CHintBox(LOCALE_HDD_CHECK, g_Locale->getText(LOCALE_HDD_UMOUNT_WARN));
		hintbox->paint();
		sleep(2);
		delete hintbox;
		return menu_return::RETURN_REPAINT;
	}

	switch(g_settings.hdd_fs) {
		case 0:
			sprintf(cmd, "/sbin/fsck.ext3 -C 1 -f -y %s", src);
			break;
		case 1:
			sprintf(cmd, "/sbin/reiserfsck --fix-fixable %s", src);
			break;
		default:
			return 0;
	}

	printf("CHDDChkExec: Executing %s\n", cmd);
	f=popen(cmd, "r");
	if(!f) {
		hintbox = new CHintBox(LOCALE_HDD_CHECK, g_Locale->getText(LOCALE_HDD_CHECK_FAILED));
		hintbox->paint();
		sleep(2);
		delete hintbox;
		goto ret1;
	}

	progress = new CProgressWindow();
	progress->setTitle(LOCALE_HDD_CHECK);
	progress->exec(NULL,"");

	char buf[256];
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
		else if(!strncmp(buf, "Pass", 4))
			progress->showStatusMessageUTF(buf);
	}
//printf("CHDDChkExec: %s\n", buf);
	pclose(f);
	progress->showGlobalStatus(100);
	progress->showStatusMessageUTF(buf);
	sleep(2);
	progress->hide();
	delete progress;

ret1:
        switch(g_settings.hdd_fs) {
                case 0:
			res = mount(src, dst, "ext3", 0, NULL);
                        break;
                case 1:
			res = mount(src, dst, "reiserfs", 0, NULL);
                        break;
		default:
                        break;
        }
	printf("CHDDChkExec: mount res %d\n", res);
	
	if(!srun) system("smbd");
	return menu_return::RETURN_REPAINT;
}
