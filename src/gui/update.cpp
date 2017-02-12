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
	along with this program; if not, write to the 
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/update.h>
#include <gui/update_ext.h>

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <mymenu.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>

#include <gui/color.h>
#include <gui/filebrowser.h>
#include <gui/opkg_manager.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/hintbox.h>

#include <system/flashtool.h>
#include <system/httptool.h>
#include <system/helpers.h>
#include <system/debug.h>

#include <lib/libnet/libnet.h>

#define SQUASHFS

#include <curl/curl.h>
#include <curl/easy.h>

#if LIBCURL_VERSION_NUM < 0x071507
#include <curl/types.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dirent.h>

#include <fstream>

#include <cs_api.h>

extern int allow_flash;

//#define gTmpPath "/var/update/"
#define gTmpPath "/tmp/"
#define gUserAgent "neutrino/softupdater 1.0"

#define LIST_OF_UPDATES_LOCAL_FILENAME "coolstream.list"
#define UPDATE_LOCAL_FILENAME          "update.img"
#define RELEASE_CYCLE                  "2.0"
#define FILEBROWSER_UPDATE_FILTER      "img"

#define MTD_OF_WHOLE_IMAGE             0
#ifdef BOXMODEL_CS_HD2
#define MTD_DEVICE_OF_UPDATE_PART      "/dev/mtd0"
#else
#define MTD_DEVICE_OF_UPDATE_PART      "/dev/mtd3"
#endif
int pinghost  (const std::string &hostname, std::string *ip = NULL);

CFlashUpdate::CFlashUpdate()
	:CProgressWindow()
{
	width = 40;
	setTitle(LOCALE_FLASHUPDATE_HEAD);
	sysfs = CMTDInfo::getInstance()->findMTDsystem();
	if (sysfs.empty())
		sysfs = MTD_DEVICE_OF_UPDATE_PART;
	printf("Mtd partition to update: %s\n", sysfs.c_str());
	notify = true;
}



class CUpdateMenuTarget : public CMenuTarget
{
	int    myID;
	int *  myselectedID;

public:
	CUpdateMenuTarget(const int id, int * const selectedID)
		{
			myID = id;
			myselectedID = selectedID;
		}

	virtual int exec(CMenuTarget *, const std::string &)
		{
			*myselectedID = myID;
			return menu_return::RETURN_EXIT_ALL;
		}
};

bool CFlashUpdate::checkOnlineVersion()
{
	CHTTPTool httpTool;
	std::string url;
	std::string name;
	std::string version;
	std::string md5;
	std::vector<std::string> updates_lists, urls, names, versions, descriptions, md5s;
	int curVer, newVer;
	bool newfound = false;

	std::vector<CUpdateMenuTarget*> update_t_list;

	CConfigFile _configfile('\t');
	const char * versionString = (_configfile.loadConfig(TARGET_PREFIX "/.version")) ? (_configfile.getString( "version", "????????????????").c_str()) : "????????????????";
#ifdef DEBUG
	printf("[update] file %s\n", g_settings.softupdate_url_file.c_str());
#endif
	CFlashVersionInfo curInfo(versionString);
	curVer = curInfo.getVersion();
	printf("current flash-version: %s (%d) date %s (%ld)\n", versionString, curInfo.getVersion(), curInfo.getDate(), curInfo.getDateTime());

	std::ifstream urlFile(g_settings.softupdate_url_file.c_str());
	if (urlFile >> url) {
		/* extract domain name */
		std::string::size_type startpos, endpos;
		std::string host;
		startpos = url.find("//");
		if (startpos != std::string::npos) {
			startpos += 2;
			endpos    = url.find('/', startpos);
			host = url.substr(startpos, endpos - startpos);
		}
		printf("[update] host %s\n", host.c_str());
		if (host.empty() || (pinghost(host) != 1))
			return false;
		if (httpTool.downloadFile(url, gTmpPath LIST_OF_UPDATES_LOCAL_FILENAME, 20)) {
			std::ifstream in(gTmpPath LIST_OF_UPDATES_LOCAL_FILENAME);
			while (in >> url >> version >> md5 >> std::ws) {
				std::getline(in, name);
				CFlashVersionInfo versionInfo(version);
				newVer = versionInfo.getVersion();
#ifdef DEBUG
				printf("[update] url %s version %s (%d) timestamp %s (%ld) md5 %s name %s\n", url.c_str(), version.c_str(), newVer, versionInfo.getDate(), versionInfo.getDateTime(), md5.c_str(), name.c_str());
#endif
				if(versionInfo.snapshot < '3' && (newVer > curVer || versionInfo.getDateTime() > curInfo.getDateTime())) {
					newfound = true;
					printf("[update] found new image\n");
					break;
				}
			}
		}
	}
	return newfound;
}

//#define DEBUG
bool CFlashUpdate::selectHttpImage(void)
{
	CHTTPTool httpTool;
	std::string url;
	std::string name;
	std::string version;
	std::string md5;
	std::vector<std::string> updates_lists, urls, names, versions, descriptions, md5s;
	char fileTypes[128];
	int selected = -1, listWidth = 80;
	int curVer, newVer, newfound = 0;

	CConfigFile _configfile('\t');
	const char * versionString = (_configfile.loadConfig(TARGET_PREFIX "/.version")) ? (_configfile.getString( "version", "????????????????").c_str()) : "????????????????";

	CFlashVersionInfo curInfo(versionString);
	printf("current flash-version: %s (%d) date %s (%ld)\n", versionString, curInfo.getVersion(), curInfo.getDate(), curInfo.getDateTime());
	curVer = curInfo.getVersion();

	httpTool.setStatusViewer(this);
	showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_GETINFOFILE)); // UTF-8

	char current[200];
	snprintf(current, 200, "%s: %s %s %s %s %s", g_Locale->getText(LOCALE_FLASHUPDATE_CURRENTVERSION_SEP), curInfo.getReleaseCycle(), 
		g_Locale->getText(LOCALE_FLASHUPDATE_CURRENTVERSIONDATE), curInfo.getDate(), 
		g_Locale->getText(LOCALE_FLASHUPDATE_CURRENTVERSIONTIME), curInfo.getTime());

	CMenuWidget SelectionWidget(LOCALE_FLASHUPDATE_SELECTIMAGE, NEUTRINO_ICON_UPDATE, listWidth, MN_WIDGET_ID_IMAGESELECTOR);

	SelectionWidget.addItem(GenericMenuSeparator);
	SelectionWidget.addItem(GenericMenuBack);
	SelectionWidget.addItem(new CMenuSeparator(CMenuSeparator::LINE));

	SelectionWidget.addItem(new CMenuForwarder(current, false));
	std::ifstream urlFile(g_settings.softupdate_url_file.c_str());
#ifdef DEBUG
	printf("[update] file %s\n", g_settings.softupdate_url_file.c_str());
#endif

	unsigned int i = 0;
	while (urlFile >> url)
	{
		std::string::size_type startpos, endpos;
#ifdef DEBUG
		printf("[update] url %s\n", url.c_str());
#endif

		/* extract domain name */
		startpos = url.find("//");
		if (startpos == std::string::npos)
		{
			startpos = 0;
			endpos   = std::string::npos;
			updates_lists.push_back(url.substr(startpos, endpos - startpos));
		}
		else
		{
			//startpos += 2;
			//endpos    = url.find('/', startpos);
			startpos = url.find('/', startpos+2)+1;
			endpos   = std::string::npos;
			updates_lists.push_back(url.substr(startpos, endpos - startpos));
		}
		//updates_lists.push_back(url.substr(startpos, endpos - startpos));

		SelectionWidget.addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE, updates_lists.rbegin()->c_str()));
		if (httpTool.downloadFile(url, gTmpPath LIST_OF_UPDATES_LOCAL_FILENAME, 20))
		{
			std::ifstream in(gTmpPath LIST_OF_UPDATES_LOCAL_FILENAME);
			bool enabled;
			while (in >> url >> version >> md5 >> std::ws)
			{
				urls.push_back(url);
				versions.push_back(version);
				std::getline(in, name);
				names.push_back(name);
				//std::getline(in, md5);
				md5s.push_back(md5);
				enabled = true;

				CFlashVersionInfo versionInfo(versions[i]);
				newVer = versionInfo.getVersion();
#ifdef DEBUG
				printf("[update] url %s version %s (%d) timestamp %s (%ld) md5 %s name %s\n", url.c_str(), version.c_str(), newVer, versionInfo.getDate(), versionInfo.getDateTime(), md5.c_str(), name.c_str());
#endif
				if(versionInfo.snapshot < '3' && (newVer > curVer || versionInfo.getDateTime() > curInfo.getDateTime()))
					newfound = 1;
				if(!allow_flash && (versionInfo.snapshot < '3'))
					enabled = false;
				fileTypes[i] = versionInfo.snapshot;
				//std::string description = versionInfo.getType();
				std::string description = versionInfo.getReleaseCycle();
				description += ' ';
				description += versionInfo.getType();
				description += ' ';
				description += versionInfo.getDate();
				description += ' ';
				description += versionInfo.getTime();

				descriptions.push_back(description); /* workaround since CMenuForwarder does not store the Option String itself */

				//SelectionWidget.addItem(new CMenuForwarder(names[i].c_str(), enabled, descriptions[i].c_str(), new CUpdateMenuTarget(i, &selected)));
				CUpdateMenuTarget * up = new CUpdateMenuTarget(i, &selected);
				SelectionWidget.addItem(new CMenuDForwarder(descriptions[i].c_str(), enabled, names[i].c_str(), up));
				i++;
			}
		}
	}

	hide();

	if (urls.empty())
	{
		ShowMsg(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_GETINFOFILEERROR), CMsgBox::mbrOk, CMsgBox::mbOk); // UTF-8
		return false;
	}
	if (notify) {
		if(newfound)
			ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_FLASHUPDATE_NEW_FOUND, CMsgBox::mbrOk, CMsgBox::mbOk, NEUTRINO_ICON_INFO, MSGBOX_MIN_WIDTH, 6 );
#if 0
		else
			ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_FLASHUPDATE_NEW_NOTFOUND, CMsgBox::mbrOk, CMsgBox::mbOk, NEUTRINO_ICON_INFO, MSGBOX_MIN_WIDTH, 6);
#endif
	}

	menu_ret = SelectionWidget.exec(NULL, "");

	if (selected == -1)
		return false;

	filename = urls[selected];
	newVersion = versions[selected];
	file_md5 = md5s[selected];
	fileType = fileTypes[selected];
#ifdef BOXMODEL_CS_HD2
	if(fileType < '3') {
		int esize = CMTDInfo::getInstance()->getMTDEraseSize(sysfs);
		printf("[update] erase size is %x\n", esize);
		if (esize == 0x40000) {
			filename += ".256k";
		}
	}
#endif
#ifdef DEBUG
	printf("[update] filename %s type %c newVersion %s md5 %s\n", filename.c_str(), fileType, newVersion.c_str(), file_md5.c_str());
#endif

	return true;
}

bool CFlashUpdate::getUpdateImage(const std::string & version)
{
	CHTTPTool httpTool;
	char const * fname;
	char dest_name[100];
	httpTool.setStatusViewer(this);

	fname = rindex(filename.c_str(), '/');
	if(fname != NULL) fname++;
	else return false;

	sprintf(dest_name, "%s/%s", g_settings.update_dir.c_str(), fname);
	showStatusMessageUTF(std::string(g_Locale->getText(LOCALE_FLASHUPDATE_GETUPDATEFILE)) + ' ' + version); // UTF-8

	printf("get update (url): %s - %s\n", filename.c_str(), dest_name);
	return httpTool.downloadFile(filename, dest_name, 40 );
	//return httpTool.downloadFile(filename, gTmpPath UPDATE_LOCAL_FILENAME, 40 );
}

bool CFlashUpdate::checkVersion4Update()
{
	char msg[400];
	CFlashVersionInfo * versionInfo;
	neutrino_locale_t msg_body;
#ifdef DEBUG
printf("[update] mode is %d\n", softupdate_mode);
#endif
	if(softupdate_mode==1) //internet-update
	{
		if(!selectHttpImage())
			return false;

		showLocalStatus(100);
		showGlobalStatus(20);
		showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_VERSIONCHECK)); // UTF-8

		printf("internet version: %s\n", newVersion.c_str());

		showLocalStatus(100);
		showGlobalStatus(20);
		hide();

		msg_body = LOCALE_FLASHUPDATE_MSGBOX;
#ifdef SQUASHFS
		versionInfo = new CFlashVersionInfo(newVersion);//Memory leak: versionInfo
		sprintf(msg, g_Locale->getText(msg_body), versionInfo->getDate(), versionInfo->getTime(), versionInfo->getReleaseCycle(), versionInfo->getType());

		if(fileType < '3')
		{
			if ((strncmp(RELEASE_CYCLE, versionInfo->getReleaseCycle(), 2) != 0) &&
		    (ShowMsg(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_WRONGBASE), CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE) != CMsgBox::mbrYes))
			{
				delete versionInfo;
				//ShowHint(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_WRONGBASE)); // UTF-8
				return false;
			}

			if ((strcmp("Release", versionInfo->getType()) != 0) &&
			    //(ShowMsg(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_EXPERIMENTALIMAGE), CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE) != CMsgBox::mbrYes)) // UTF-8
			    (ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_FLASHUPDATE_EXPERIMENTALIMAGE, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE) != CMsgBox::mbrYes))
			{
				delete versionInfo;
				return false;
			}
		}

		delete versionInfo;
#endif
	}
	else
	{
		CFileBrowser UpdatesBrowser;

		CFileFilter UpdatesFilter;
		if(allow_flash) UpdatesFilter.addFilter(FILEBROWSER_UPDATE_FILTER);

		string filters[] = {"bin", "txt", "opk", "ipk"};
		for(size_t i=0; i<sizeof(filters)/sizeof(filters[0]) ;i++)
			UpdatesFilter.addFilter(filters[i]);

		UpdatesBrowser.Filter = &UpdatesFilter;

		CFile * file_selected = NULL;
		if (!(UpdatesBrowser.exec(g_settings.update_dir.c_str()))) {
			menu_ret = UpdatesBrowser.getMenuRet();
			return false;
		}

		file_selected = UpdatesBrowser.getSelectedFile();

		if (file_selected == NULL)
			return false;

		filename = file_selected->Name;

		FILE* fd = fopen(filename.c_str(), "r");
		if(fd)
			fclose(fd);
		else {
			hide();
			printf("flash/package-file not found: %s\n", filename.c_str());
			DisplayErrorMessage(g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENFILE));
			return false;
		}
		hide();

		//package install:
		if (file_selected->getType() == CFile::FILE_PKG_PACKAGE){
			COPKGManager opkg;
			if (opkg.hasOpkgSupport()){
				int msgres = ShowMsg(LOCALE_MESSAGEBOX_INFO, LOCALE_OPKG_WARNING_3RDPARTY_PACKAGES, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE, 700); // UTF-8
				if (msgres == CMsgBox::mbrYes){
					if (!opkg.installPackage(UpdatesBrowser.getSelectedFile()->Name))
						DisplayErrorMessage(g_Locale->getText(LOCALE_OPKG_FAILURE_INSTALL));
				}
			}
			else
				DisplayInfoMessage(g_Locale->getText(LOCALE_MESSAGEBOX_FEATURE_NOT_SUPPORTED));
			//!always leave here!
			return false;
		}
		//set internal filetype
		char const * ptr = rindex(filename.c_str(), '.');
		if(ptr) {
			ptr++;
			if(!strcmp(ptr, "bin"))
				fileType = 'A';
			else if(!strcmp(ptr, "txt"))
				fileType = 'T';
			else if(!allow_flash)
				return false;
			else
				fileType = 0;
#ifdef DEBUG
			printf("[update] manual file type: %s %c\n", ptr, fileType);
#endif
		}

		strcpy(msg, g_Locale->getText(LOCALE_FLASHUPDATE_NOVERSION));
//never read		msg_body = LOCALE_FLASHUPDATE_MSGBOX_MANUAL;
	}
	return (ShowMsg(LOCALE_MESSAGEBOX_INFO, msg, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE) == CMsgBox::mbrYes); // UTF-8
}

int CFlashUpdate::exec(CMenuTarget* parent, const std::string &actionKey)
{
	printf("CFlashUpdate::exec: [%s]\n", actionKey.c_str());
	if (actionKey == "local")
		softupdate_mode = 0;
	else
		softupdate_mode = 1;

	if(parent)
		parent->hide();

	menu_ret = menu_return::RETURN_REPAINT;
	paint();

	if(sysfs.size() < 8) {
		ShowHint(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENMTD));
		hide();
		return menu_return::RETURN_REPAINT;
	}
	if(!checkVersion4Update()) {
		hide();
		return menu_ret; //menu_return::RETURN_REPAINT;
	}

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(0,"checking",0,"Update Neutrino");
	CVFD::getInstance()->setMode(CLCD::MODE_PROGRESSBAR2);
#endif // VFD_UPDATE


	paint();
	showGlobalStatus(20);

	if(softupdate_mode==1) //internet-update
	{
		char const * fname = rindex(filename.c_str(), '/') +1;
		char fullname[255];

		if(!getUpdateImage(newVersion)) {
			hide();
			ShowHint(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_GETUPDATEFILEERROR)); // UTF-8
			return menu_return::RETURN_REPAINT;
		}
		sprintf(fullname, "%s/%s", g_settings.update_dir.c_str(), fname);
		filename = std::string(fullname);
	}

	showGlobalStatus(40);

	CFlashTool ft;
	//ft.setMTDDevice(MTD_DEVICE_OF_UPDATE_PART);
	ft.setMTDDevice(sysfs);
	ft.setStatusViewer(this);

	showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_MD5CHECK)); // UTF-8
	if((softupdate_mode==1) && !ft.check_md5(filename, file_md5)) {
		hide();
		ShowHint(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_MD5SUMERROR)); // UTF-8
		return menu_return::RETURN_REPAINT;
	}
	if(softupdate_mode==1) { //internet-update
		if ( ShowMsg(LOCALE_MESSAGEBOX_INFO, (fileType < '3') ? LOCALE_FLASHUPDATE_INSTALL_IMAGE : LOCALE_FLASHUPDATE_INSTALL_PACKAGE, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE) != CMsgBox::mbrYes) // UTF-8
		{
			hide();
			return menu_return::RETURN_REPAINT;
		}
	}

	showGlobalStatus(60);

#ifdef DEBUG
	printf("[update] flash/install filename %s type %c\n", filename.c_str(), fileType);
#endif
	if(fileType < '3') {
		//flash it...
#if ENABLE_EXTUPDATE
#ifndef BOXMODEL_CS_HD2
		if (g_settings.apply_settings) {
			if (ShowMsg(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_APPLY_SETTINGS), CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE) == CMsgBox::mbrYes)
				if (!CExtUpdate::getInstance()->applySettings(filename, CExtUpdate::MODE_SOFTUPDATE)) {
					hide();
					return menu_return::RETURN_REPAINT;
				}
		}
#endif
#endif

#ifdef DEBUG1
		if(1) {
#else
		if(!ft.program(filename, 80, 100)) {
#endif
			hide();
			ShowHint(LOCALE_MESSAGEBOX_ERROR, ft.getErrorMessage().c_str()); // UTF-8
			return menu_return::RETURN_REPAINT;
		}

		//status anzeigen
		showGlobalStatus(100);
		showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_READY)); // UTF-8
		hide();
		ShowHint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_FLASHREADYREBOOT)); // UTF-8
		sleep(2);
		ft.reboot();
	}
	else if(fileType == 'T') // display file contents
	{
		FILE* fd = fopen(filename.c_str(), "r");
		if(fd) {
			char * buffer;
			off_t filesize = lseek(fileno(fd), 0, SEEK_END);
			lseek(fileno(fd), 0, SEEK_SET);
			buffer =(char *) malloc((uint32_t)filesize+1);
			fread(buffer, (uint32_t)filesize, 1, fd);
			fclose(fd);
			buffer[filesize] = 0;
			ShowMsg(LOCALE_MESSAGEBOX_INFO, buffer, CMsgBox::mbrBack, CMsgBox::mbBack); // UTF-8
			free(buffer);
		}
	}
	else // not image, install
	{
		const char install_sh[] = "/bin/install.sh";
#ifdef DEBUG1
		printf("[update] calling %s %s %s\n",install_sh, g_settings.update_dir.c_str(), filename.c_str() );
#else
		printf("[update] calling %s %s %s\n",install_sh, g_settings.update_dir.c_str(), filename.c_str() );
		my_system(3, install_sh, g_settings.update_dir.c_str(), filename.c_str());
#endif
		showGlobalStatus(100);
		ShowHint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_READY)); // UTF-8
	}
	hide();
	return menu_return::RETURN_REPAINT;
}


//--------------------------------------------------------------------------------------------------------------


CFlashExpert::CFlashExpert()
	:CProgressWindow()
{
	selectedMTD = -1;
	width = 40;
#ifdef BOXMODEL_CS_HD2
	forceOtherFilename = false;
	otherFilename = "";
	createimage_other = 0;
#endif
}

CFlashExpert* CFlashExpert::getInstance()
{
	static CFlashExpert* FlashExpert = NULL;
	if(!FlashExpert)
		FlashExpert = new CFlashExpert();
	return FlashExpert;
}

bool CFlashExpert::checkSize(int mtd, std::string &backupFile)
{
#ifndef BOXMODEL_CS_HD2
	if (mtd < 0) return false;
#endif
	char errMsg[1024] = {0};
	std::string path = getPathName(backupFile);
	if (!file_exists(path.c_str()))  {
		snprintf(errMsg, sizeof(errMsg)-1, g_Locale->getText(LOCALE_FLASHUPDATE_READ_DIRECTORY_NOT_EXIST), path.c_str());
		ShowHint(LOCALE_MESSAGEBOX_ERROR, errMsg);
		return false;
	}

	uint64_t btotal = 0, bused = 0;
	long bsize = 0;
	uint64_t backupRequiredSize = 0;
#ifdef BOXMODEL_CS_HD2
	if (mtd == -1) { // check disk space for image creation
		if (!get_fs_usage("/", btotal, bused, &bsize)) {
			snprintf(errMsg, sizeof(errMsg)-1, g_Locale->getText(LOCALE_FLASHUPDATE_READ_VOLUME_ERROR), "root0");
			ShowHint(LOCALE_MESSAGEBOX_ERROR, errMsg);
			return false;
		}
		backupRequiredSize = ((bused * bsize) / 1024ULL) * 2ULL; // twice disk space for summarized image
		dprintf(DEBUG_INFO, "##### [%s] backupRequiredSize: %" PRIu64 ", btotal: %" PRIu64 ", bused: %" PRIu64 ", bsize: %ld\n",
				__func__, backupRequiredSize, btotal, bused, bsize);
	}
	else
#endif
		backupRequiredSize = CMTDInfo::getInstance()->getMTDSize(mtd) / 1024ULL;

	btotal = 0; bused = 0; bsize = 0;
	if (!get_fs_usage(path.c_str(), btotal, bused, &bsize)) {
		snprintf(errMsg, sizeof(errMsg)-1, g_Locale->getText(LOCALE_FLASHUPDATE_READ_VOLUME_ERROR), path.c_str());
		ShowHint(LOCALE_MESSAGEBOX_ERROR, errMsg);
		return false;
	}
	uint64_t backupMaxSize = (btotal - bused) * (uint64_t)bsize;
	uint64_t res = 10; // Reserved 10% of available space
	backupMaxSize = (backupMaxSize - ((backupMaxSize * res) / 100ULL)) / 1024ULL;
	dprintf(DEBUG_INFO, "##### [%s] backupMaxSize: %" PRIu64 ", btotal: %" PRIu64 ", bused: %" PRIu64 ", bsize: %ld\n",
			__func__, backupMaxSize, btotal, bused, bsize);

	if (backupMaxSize < backupRequiredSize) {
		snprintf(errMsg, sizeof(errMsg)-1, g_Locale->getText(LOCALE_FLASHUPDATE_READ_NO_AVAILABLE_SPACE), path.c_str(), to_string(backupMaxSize).c_str(), to_string(backupRequiredSize).c_str());
		ShowHint(LOCALE_MESSAGEBOX_ERROR, errMsg);
		return false;
	}

	return true;
}

#ifdef BOXMODEL_CS_HD2
bool CFlashExpert::readDevtableFile(std::string &devtableFile, CMkfsJFFS2::v_devtable_t &v_devtable)
{
	FILE *fd = fopen(devtableFile.c_str(), "r");
	if (!fd) return false;
	char lineRead[1024];
	memset(lineRead, 0, sizeof(lineRead));
	bool status = false;
	while (fgets(lineRead, sizeof(lineRead)-1, fd)) {
		std::string line = lineRead;
		line = trim(line);
		// ignore comments
		if (line.find_first_of("#") == 0) {
			continue;
		}
		// ignore comments after the entry
		size_t pos = line.find_first_of("#");
		if (pos != std::string::npos) {
			line = line.substr(0, pos);
			line = trim(line);
		}
		// minimal entry: "/dev/x x 0000"
		// length = 13
		if (line.length() > 12) {
			v_devtable.push_back(line);
			status = true;
		}
		memset(lineRead, 0, sizeof(lineRead));
	}
	fclose(fd);
	if (!status) return false;
	return true;
}

void CFlashExpert::readmtdJFFS2(std::string &filename, std::string title/*=""*/, std::string path/*="/"*/, bool makeDevTable/*=true*/)
{
	if (!checkSize(-1, filename))
		return;
	CProgressWindow progress;
	if (title == "")
		progress.setTitle(LOCALE_FLASHUPDATE_TITLEREADFLASH);
	else
		progress.setTitle(g_Locale->getText(LOCALE_FLASHUPDATE_TITLEREADFLASH) + title);
	progress.paint();

	int eSize = CMTDInfo::getInstance()->getMTDEraseSize(CMTDInfo::getInstance()->findMTDsystem());
	if (createimage_other == 1) {
		if (eSize == 0x40000) eSize = 0x20000;
		else if (eSize == 0x20000) eSize = 0x40000;
	}
	CMkfsJFFS2 mkfs;
	if (makeDevTable) {
		CMkfsJFFS2::v_devtable_t v_devtable;
		bool devtableFileIO = false;
		std::string devtableFile = (std::string)CONFIGDIR + "/devtable.txt";
		if (file_exists(devtableFile.c_str())) {
			if (readDevtableFile(devtableFile, v_devtable))
				devtableFileIO = true;
		}
		if (!devtableFileIO || v_devtable.empty()) {
			v_devtable.push_back("/dev/console c 0600 0 0 5 1 0 0 0");
			v_devtable.push_back("/dev/null c 0666 0 0 1 3 0 0 0");
		}
		mkfs.makeJffs2Image(path, filename, eSize, 0, 0, __LITTLE_ENDIAN, true, true, &progress, &v_devtable);
	}
	else
		mkfs.makeJffs2Image(path, filename, eSize, 0, 0, __LITTLE_ENDIAN, true, true, &progress);

	progress.hide();

	char message[500];
	snprintf(message, sizeof(message)-1, g_Locale->getText(LOCALE_FLASHUPDATE_SAVESUCCESS), filename.c_str());
	ShowHint(LOCALE_MESSAGEBOX_INFO, message);
}
#endif

void CFlashExpert::readmtd(int preadmtd)
{
	std::string filename;
	CMTDInfo* mtdInfo    = CMTDInfo::getInstance();
	std::string hostName ="";
	netGetHostname(hostName);
	std::string timeStr  = getNowTimeStr("_%Y%m%d_%H%M");
	std::string tankStr  = "";

#if ENABLE_EXTUPDATE
#ifdef BOXMODEL_CS_HD2
	int eSize = CMTDInfo::getInstance()->getMTDEraseSize(CMTDInfo::getInstance()->findMTDsystem());
	if (preadmtd == 0) {
		if (createimage_other == 0) {
			if (eSize == 0x40000) tankStr = ".256k";
			if (eSize == 0x20000) tankStr = "";
		}
		else if (createimage_other == 1) {
			if (eSize == 0x40000) tankStr = "";
			if (eSize == 0x20000) tankStr = ".256k";
		}
	}
#endif
	if (g_settings.softupdate_name_mode_backup == CExtUpdate::SOFTUPDATE_NAME_HOSTNAME_TIME)
		filename = (std::string)g_settings.update_dir + "/" + mtdInfo->getMTDName(preadmtd) + timeStr + "_" + hostName + tankStr + ".img";
	else
#endif
		filename = (std::string)g_settings.update_dir + "/" + mtdInfo->getMTDName(preadmtd) + timeStr + tankStr + ".img";

#ifdef BOXMODEL_CS_HD2
	std::string title  = " (" + CMTDInfo::getInstance()->getMTDName(preadmtd) + ")";
	std::string mountp = getJFFS2MountPoint(preadmtd);
	if (preadmtd == 0) {
		readmtdJFFS2(filename, title);
		return;
	}
	if (preadmtd == 1) {
		if (mountp != "")
			readmtdJFFS2(filename, title, mountp.c_str(), false);
		return;
	}
	if (preadmtd == 2) {
		if (mountp != "")
			readmtdJFFS2(filename, title, mountp.c_str(), false);
		return;
	}
#endif
	if (preadmtd == -1) {
		filename = (std::string)g_settings.update_dir + "/flashimage.img"; // US-ASCII (subset of UTF-8 and ISO8859-1)
		preadmtd = MTD_OF_WHOLE_IMAGE;
	}

	bool skipCheck = false;
#ifndef BOXMODEL_CS_HD2
	if ((std::string)g_settings.update_dir == "/tmp")
		skipCheck = true;
#else
	if (forceOtherFilename)
		filename = otherFilename;
#endif
	if ((!skipCheck) && (!checkSize(preadmtd, filename)))
		return;

	setTitle(LOCALE_FLASHUPDATE_TITLEREADFLASH);
	paint();
	showGlobalStatus(0);
	showStatusMessageUTF((std::string(g_Locale->getText(LOCALE_FLASHUPDATE_ACTIONREADFLASH)) + " (" + mtdInfo->getMTDName(preadmtd) + ')')); // UTF-8
	CFlashTool ft;
	ft.setStatusViewer( this );
	ft.setMTDDevice(mtdInfo->getMTDFileName(preadmtd));

	if(!ft.readFromMTD(filename, 100)) {
		showStatusMessageUTF(ft.getErrorMessage()); // UTF-8
		sleep(10);
	} else {
		showGlobalStatus(100);
		showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_READY)); // UTF-8
		char message[500];
		sprintf(message, g_Locale->getText(LOCALE_FLASHUPDATE_SAVESUCCESS), filename.c_str());
		sleep(1);
		hide();
#ifdef BOXMODEL_CS_HD2
		if (!forceOtherFilename)
			ShowHint(LOCALE_MESSAGEBOX_INFO, message);
#else
		ShowHint(LOCALE_MESSAGEBOX_INFO, message);
#endif
	}
}

void CFlashExpert::writemtd(const std::string & filename, int mtdNumber)
{
	char message[500];

	snprintf(message, sizeof(message),
		g_Locale->getText(LOCALE_FLASHUPDATE_REALLYFLASHMTD),
		FILESYSTEM_ENCODING_TO_UTF8_STRING(filename).c_str(),
		CMTDInfo::getInstance()->getMTDName(mtdNumber).c_str());

	if (ShowMsg(LOCALE_MESSAGEBOX_INFO, message, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE) != CMsgBox::mbrYes) // UTF-8
		return;
#ifdef VFD_UPDATE
        CVFD::getInstance()->showProgressBar2(0,"checking",0,"Update Neutrino");
        CVFD::getInstance()->setMode(CLCD::MODE_PROGRESSBAR2);
#endif // VFD_UPDATE

	setTitle(LOCALE_FLASHUPDATE_TITLEWRITEFLASH);
	paint();
	showGlobalStatus(0);
	CFlashTool ft;
	ft.setStatusViewer( this );
	ft.setMTDDevice( CMTDInfo::getInstance()->getMTDFileName(mtdNumber) );
	if(!ft.program( (std::string)g_settings.update_dir + "/" + filename, 50, 100)) {
		showStatusMessageUTF(ft.getErrorMessage()); // UTF-8
		sleep(10);
	} else {
		showGlobalStatus(100);
		showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_READY)); // UTF-8
		sleep(2);
		hide();
		ShowHint(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_FLASHREADYREBOOT)); // UTF-8
		ft.reboot();
	}
}

int CFlashExpert::showMTDSelector(const std::string & actionkey)
{
	int shortcut = 0;

	mn_widget_id_t widget_id = NO_WIDGET_ID;
	if (actionkey == "readmtd")
		widget_id = MN_WIDGET_ID_MTDREAD_SELECTOR;
	else if (actionkey == "writemtd")
		widget_id = MN_WIDGET_ID_MTDWRITE_SELECTOR;

	//generate mtd-selector
	CMenuWidget* mtdselector = new CMenuWidget(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE, width, widget_id);
	mtdselector->addIntroItems(LOCALE_FLASHUPDATE_MTDSELECTOR, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	CMTDInfo* mtdInfo =CMTDInfo::getInstance();
	for(int lx=0;lx<mtdInfo->getMTDCount();lx++) {
		char sActionKey[20];
		bool enabled = true;
#ifdef BOXMODEL_CS_HD2
		// disable write uboot / uldr / env
		if ((actionkey == "writemtd") && (lx == mtdInfo->findMTDNumberFromName("u-boot") || 
			                          lx == mtdInfo->findMTDNumberFromName("uldr") ||
			                          lx == mtdInfo->findMTDNumberFromName("env")))
			enabled = false;
		if (actionkey == "readmtd") {
			// Enabled when file system is mounted
			if (lx == mtdInfo->findMTDNumberFromName("var"))
				enabled = (getJFFS2MountPoint(lx) == "") ? false : true;
			else if (lx == mtdInfo->findMTDNumberFromName("root1"))
				enabled = (getJFFS2MountPoint(lx) == "") ? false : true;
		}
		if (lx == 3)
			mtdselector->addItem(GenericMenuSeparatorLine);
#else
		// disable write uboot
		if ((actionkey == "writemtd") && (lx == mtdInfo->findMTDNumberFromName("U-Boot")))
			enabled = false;
#endif
		sprintf(sActionKey, "%s%d", actionkey.c_str(), lx);
		mtdselector->addItem(new CMenuForwarder(mtdInfo->getMTDName(lx).c_str(), enabled, NULL, this, sActionKey, CRCInput::convertDigitToKey(shortcut++)));
	}
#if ENABLE_EXTUPDATE
#ifndef BOXMODEL_CS_HD2
	if (actionkey == "writemtd")
		mtdselector->addItem(new CMenuForwarder("systemFS with settings", true, NULL, this, "writemtd10", CRCInput::convertDigitToKey(shortcut++)));
#endif
#endif
	int res = mtdselector->exec(NULL,"");
	delete mtdselector;
	return res;
}

int CFlashExpert::showFileSelector(const std::string & actionkey)
{
	CMenuWidget* fileselector = new CMenuWidget(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_FILESELECTOR);
	fileselector->addIntroItems(LOCALE_FLASHUPDATE_FILESELECTOR, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	struct dirent **namelist;
	int n = scandir(g_settings.update_dir.c_str(), &namelist, 0, alphasort);
	if (n < 0)
	{
		perror("no flashimages available");
		//should be available...
	}
	else
	{
		for(int count=0;count<n;count++)
		{
			std::string filen = namelist[count]->d_name;
			int pos = filen.find(".img");
			if(pos!=-1)
			{
				fileselector->addItem(new CMenuForwarder(filen.c_str(), true, NULL, this, (actionkey + filen).c_str()));
				//TODO  make sure filen is UTF-8 encoded
			}
			free(namelist[count]);
		}
		free(namelist);
	}
	int res = fileselector->exec(NULL,"");
	delete fileselector;
	return res;
}

int CFlashExpert::exec(CMenuTarget* parent, const std::string & actionKey)
{
	int res =  menu_return::RETURN_REPAINT;
	if(parent)
		parent->hide();

	if(actionKey=="readflash") {
		readmtd(-1);
	}
	else if(actionKey=="writeflash") {
		res = showFileSelector("");
	}
	else if(actionKey=="readflashmtd") {
		res = showMTDSelector("readmtd");
	}
	else if(actionKey=="writeflashmtd") {
		res = showMTDSelector("writemtd");
	}
	else {
		int iReadmtd = -1;
		int iWritemtd = -1;
		sscanf(actionKey.c_str(), "readmtd%d", &iReadmtd);
		sscanf(actionKey.c_str(), "writemtd%d", &iWritemtd);
		if(iReadmtd!=-1) {
			readmtd(iReadmtd);
		}
		else if(iWritemtd!=-1) {
			printf("mtd-write\n\n");
			selectedMTD = iWritemtd;
			showFileSelector("");
		} else {
#if ENABLE_EXTUPDATE
			if(selectedMTD == 10) {
				std::string aK = actionKey;
				CExtUpdate::getInstance()->applySettings(aK, CExtUpdate::MODE_EXPERT);
			}
			else
#endif
			if (selectedMTD == -1) {
				writemtd(actionKey, MTD_OF_WHOLE_IMAGE);
			} else {
				writemtd(actionKey, selectedMTD);
				selectedMTD=-1;
			}
		}
		res = menu_return::RETURN_REPAINT;
	}
	hide();
	return res;
}

#ifdef BOXMODEL_CS_HD2
CFlashExpertSetup::CFlashExpertSetup()
{
	width = 40;
}

void CFlashExpertSetup::readMTDPart(int mtd, const std::string &fileName)
{
	CFlashExpert *cfe = CFlashExpert::getInstance();
	if (file_exists(fileName.c_str()))
		unlink(fileName.c_str());
	cfe->otherFilename = fileName;
	cfe->readmtd(mtd);
	sync();
}

#define UBOOT_BIN
//#define SPARE_BIN

int CFlashExpertSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
#define UPDATEDIR "/var/update"
	if (parent)
		parent->hide();

	if (actionKey == "readmtd0") {
		CFlashExpert *cfe = CFlashExpert::getInstance();
		CMTDInfo *mtdInfo = CMTDInfo::getInstance();
		bool skipImage = false;
		if (cfe->createimage_other == 1) {
			char message[512] = {0};
			// create image warning
			const char *box = (mtdInfo->getMTDEraseSize(mtdInfo->findMTDsystem()) == 0x40000) ? "Trinity" : "Tank";
			snprintf(message, sizeof(message)-1, g_Locale->getText(LOCALE_FLASHUPDATE_CREATEIMAGE_WARNING), box, box);
			if (ShowMsg(LOCALE_MESSAGEBOX_INFO, message, CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_UPDATE, 600) != CMsgBox::mbrYes)
				skipImage = true;
		}
		if (!skipImage) {
			std::string uldrName   = (std::string)UPDATEDIR + "/uldr.bin";
			cfe->forceOtherFilename = true;
			if (g_settings.flashupdate_createimage_add_uldr == 1)
				readMTDPart(mtdInfo->findMTDNumberFromName("uldr"), uldrName);
#ifdef UBOOT_BIN
			std::string ubootName   = (std::string)UPDATEDIR + "/u-boot.bin";
			if (g_settings.flashupdate_createimage_add_u_boot == 1)
				readMTDPart(mtdInfo->findMTDNumberFromName("u-boot"), ubootName);
			std::string envName   = (std::string)UPDATEDIR + "/env.bin";
			if (g_settings.flashupdate_createimage_add_env == 1)
				readMTDPart(mtdInfo->findMTDNumberFromName("env"), envName);
#endif
#ifdef SPARE_BIN
			std::string spareName   = (std::string)UPDATEDIR + "/spare.bin";
			if (g_settings.flashupdate_createimage_add_spare == 1)
				readMTDPart(mtdInfo->findMTDNumberFromName("spare"), spareName);
#endif
			std::string kernelName = (std::string)UPDATEDIR + "/vmlinux.ub.gz";
			if (g_settings.flashupdate_createimage_add_kernel == 1)
				readMTDPart(mtdInfo->findMTDNumberFromName("kernel"), kernelName);
			cfe->forceOtherFilename = false;
			cfe->otherFilename = "";

			if (g_settings.flashupdate_createimage_add_var == 1)
				cfe->readmtd(1);

			if (g_settings.flashupdate_createimage_add_root1 == 1)
				cfe->readmtd(2);

			cfe->readmtd(0);

			if (g_settings.flashupdate_createimage_add_uldr == 1)
				unlink(uldrName.c_str());
#ifdef UBOOT_BIN
			if (g_settings.flashupdate_createimage_add_u_boot == 1)
				unlink(ubootName.c_str());
			if (g_settings.flashupdate_createimage_add_env == 1)
				unlink(envName.c_str());
#endif
#ifdef SPARE_BIN
			if (g_settings.flashupdate_createimage_add_spare == 1)
				unlink(spareName.c_str());
#endif
			if (g_settings.flashupdate_createimage_add_kernel == 1)
				unlink(kernelName.c_str());
			sync();
		}

		cfe->createimage_other = 0;
		return menu_return::RETURN_REPAINT;
	}
	return showMenu();
}

int CFlashExpertSetup::showMenu()
{
	CFlashExpert *cfe = CFlashExpert::getInstance();
	CMenuWidget *rootfsSetup = new CMenuWidget(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_MTDREAD_ROOT0);
	rootfsSetup->addIntroItems(LOCALE_FLASHUPDATE_CREATEIMAGE_MENU);

	CMenuSeparator     *s1 = new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FLASHUPDATE_CREATEIMAGE_OPTIONS);
	CMenuForwarder     *m1 = new CMenuForwarder(LOCALE_FLASHUPDATE_CREATEIMAGE, true, NULL, this, "readmtd0", CRCInput::convertDigitToKey(0));
	bool isMountVar = (getJFFS2MountPoint(1) == "") ? false : true;
	CMenuOptionChooser *m8=NULL;
	if (isMountVar)
		m8 = new CMenuOptionChooser(LOCALE_FLASHUPDATE_CREATEIMAGE_ADD_VAR,   &g_settings.flashupdate_createimage_add_var,
								MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

	bool isMountRoot1 = (getJFFS2MountPoint(2) == "") ? false : true;
	CMenuOptionChooser *m9=NULL;
	if (isMountRoot1)
		m9 = new CMenuOptionChooser(LOCALE_FLASHUPDATE_CREATEIMAGE_ADD_ROOT1,   &g_settings.flashupdate_createimage_add_root1,
								MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

	CMenuOptionChooser *m2 = new CMenuOptionChooser(LOCALE_FLASHUPDATE_CREATEIMAGE_ADD_ULDR,   &g_settings.flashupdate_createimage_add_uldr,
								MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);
#ifndef UBOOT_BIN
	g_settings.flashupdate_createimage_add_u_boot = 0;
g_settings.flashupdate_createimage_add_env = 0;
#endif
#ifdef UBOOT_BIN
	CMenuOptionChooser *m3 = new CMenuOptionChooser(LOCALE_FLASHUPDATE_CREATEIMAGE_ADD_U_BOOT, &g_settings.flashupdate_createimage_add_u_boot,
								MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);
	CMenuOptionChooser *m4 = new CMenuOptionChooser(LOCALE_FLASHUPDATE_CREATEIMAGE_ADD_ENV,    &g_settings.flashupdate_createimage_add_env,
								MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);
#endif
#ifndef SPARE_BIN
g_settings.flashupdate_createimage_add_spare = 0;
#endif
#ifdef SPARE_BIN
	CMenuOptionChooser *m5 = new CMenuOptionChooser(LOCALE_FLASHUPDATE_CREATEIMAGE_ADD_SPARE,  &g_settings.flashupdate_createimage_add_spare,
								MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);
#endif
	CMenuOptionChooser *m6 = new CMenuOptionChooser(LOCALE_FLASHUPDATE_CREATEIMAGE_ADD_KERNEL, &g_settings.flashupdate_createimage_add_kernel,
								MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

	rootfsSetup->addItem(m1); // create image
	rootfsSetup->addItem(s1);
	if (isMountVar)
		rootfsSetup->addItem(m8); // include var
	if (isMountRoot1)
		rootfsSetup->addItem(m9); // include root1
	rootfsSetup->addItem(m2); // include uldr
#ifdef UBOOT_BIN
	rootfsSetup->addItem(m3); // include u-boot
	rootfsSetup->addItem(m4); // include env
#endif
#ifdef SPARE_BIN
	rootfsSetup->addItem(m5); // include spare
#endif
	rootfsSetup->addItem(m6); // include kernel

	if (cs_get_revision() != 12) { // not kronos
		CMTDInfo *mtdInfo = CMTDInfo::getInstance();
		const char *box = (mtdInfo->getMTDEraseSize(mtdInfo->findMTDsystem()) == 0x40000) ? "Trinity" : "Tank";
		char mText[512] = {0};
		snprintf(mText, sizeof(mText)-1, g_Locale->getText(LOCALE_FLASHUPDATE_CREATEIMAGE_OTHER), box);
		CMenuOptionChooser *m7 = new CMenuOptionChooser(mText, &(cfe->createimage_other), MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

		rootfsSetup->addItem(GenericMenuSeparatorLine);
		rootfsSetup->addItem(m7); // create image for other STB
	} else
		cfe->createimage_other = 0;

	int res = rootfsSetup->exec (NULL, "");
	delete rootfsSetup;

	cfe->createimage_other = 0;
	return res;
}
#endif // BOXMODEL_CS_HD2
