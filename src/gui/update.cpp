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

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>

#include <gui/color.h>
#include <gui/filebrowser.h>

#include <gui/widget/messagebox.h>
#include <gui/widget/hintbox.h>

#include <system/flashtool.h>
#include <system/httptool.h>
#include <system/helpers.h>

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

extern int allow_flash;

//#define gTmpPath "/var/update/"
#define gTmpPath "/tmp/"
#define gUserAgent "neutrino/softupdater 1.0"

#define LIST_OF_UPDATES_LOCAL_FILENAME "coolstream.list"
#define UPDATE_LOCAL_FILENAME          "update.img"
#define RELEASE_CYCLE                  "2.0"
#define FILEBROWSER_UPDATE_FILTER      "img"

#define MTD_OF_WHOLE_IMAGE             0
#ifdef BOXMODEL_APOLLO
#define MTD_DEVICE_OF_UPDATE_PART      "/dev/mtd0"
#else
#define MTD_DEVICE_OF_UPDATE_PART      "/dev/mtd3"
#endif

CFlashUpdate::CFlashUpdate()
	:CProgressWindow()
{
	width = w_max (40, 10); 
	setTitle(LOCALE_FLASHUPDATE_HEAD);
	sysfs = CMTDInfo::getInstance()->findMTDsystem();
	if (sysfs.empty())
		sysfs = MTD_DEVICE_OF_UPDATE_PART;
	printf("Mtd partition to update: %s\n", sysfs.c_str());
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
	int selected = -1, listWidth = w_max (80, 10);
	int curVer, newVer, newfound = 0;

	std::vector<CUpdateMenuTarget*> update_t_list;

	CConfigFile _configfile('\t');
	const char * versionString = (_configfile.loadConfig("/.version")) ? (_configfile.getString( "version", "????????????????").c_str()) : "????????????????";
	installedVersion = versionString;

	CFlashVersionInfo curInfo(versionString);
	printf("current flash-version: %s (%d)\n", installedVersion.c_str(), curInfo.getVersion());
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

	SelectionWidget.addItem(new CMenuForwarderNonLocalized(current, false));
	std::ifstream urlFile(g_settings.softupdate_url_file);
#ifdef DEBUG
	printf("[update] file %s\n", g_settings.softupdate_url_file);
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

		SelectionWidget.addItem(new CNonLocalizedMenuSeparator(updates_lists.rbegin()->c_str(), LOCALE_FLASHUPDATE_SELECTIMAGE));
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
				printf("[update] url %s version %s (%d) md5 %s name %s\n", url.c_str(), version.c_str(), newVer, md5.c_str(), name.c_str());
#endif
				if(newVer > curVer)
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

				//SelectionWidget.addItem(new CMenuForwarderNonLocalized(names[i].c_str(), enabled, descriptions[i].c_str(), new CUpdateMenuTarget(i, &selected)));
				CUpdateMenuTarget * up = new CUpdateMenuTarget(i, &selected);
				update_t_list.push_back(up);
				SelectionWidget.addItem(new CMenuForwarderNonLocalized(descriptions[i].c_str(), enabled, names[i].c_str(), up));
				i++;
			}
		}
	}

	hide();

	if (urls.empty())
	{
		ShowMsgUTF(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_GETINFOFILEERROR), CMessageBox::mbrOk, CMessageBox::mbOk); // UTF-8
		return false;
	}
	if(newfound)
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_NEW_FOUND), CMessageBox::mbrOk, CMessageBox::mbOk, NEUTRINO_ICON_INFO);
	else
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_NEW_NOTFOUND), CMessageBox::mbrOk, CMessageBox::mbOk, NEUTRINO_ICON_INFO);

	menu_ret = SelectionWidget.exec(NULL, "");

	for (std::vector<CUpdateMenuTarget*>::iterator it = update_t_list.begin(); it != update_t_list.end(); ++it)
		delete (*it);

	if (selected == -1)
		return false;

	filename = urls[selected];
	newVersion = versions[selected];
	file_md5 = md5s[selected];
	fileType = fileTypes[selected];
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

	sprintf(dest_name, "%s/%s", g_settings.update_dir, fname);
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
		    (ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_WRONGBASE), CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_UPDATE) != CMessageBox::mbrYes))
			{
				delete versionInfo;
				//ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_WRONGBASE)); // UTF-8
				return false;
			}

			if ((strcmp("Release", versionInfo->getType()) != 0) &&
			    //(ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_EXPERIMENTALIMAGE), CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_UPDATE) != CMessageBox::mbrYes)) // UTF-8
		    	    (ShowLocalizedMessage(LOCALE_MESSAGEBOX_INFO, LOCALE_FLASHUPDATE_EXPERIMENTALIMAGE, CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_UPDATE) != CMessageBox::mbrYes))
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
		UpdatesFilter.addFilter("bin");
		UpdatesFilter.addFilter("txt");

		UpdatesBrowser.Filter = &UpdatesFilter;

		CFile * CFileSelected = NULL;
		if (!(UpdatesBrowser.exec(g_settings.update_dir))) {
			menu_ret = UpdatesBrowser.getMenuRet();
			return false;
		}

		CFileSelected = UpdatesBrowser.getSelectedFile();

		if (CFileSelected == NULL)
			return false;

		filename = CFileSelected->Name;

		FILE* fd = fopen(filename.c_str(), "r");
		if(fd)
			fclose(fd);
		else {
			hide();
			printf("flash-file not found: %s\n", filename.c_str());
			ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENFILE)); // UTF-8
			return false;
		}
		hide();
		char const * ptr = rindex(filename.c_str(), '.');
		if(ptr) {
			ptr++;
			if(!strcmp(ptr, "bin")) fileType = 'A';
			else if(!strcmp(ptr, "txt")) fileType = 'T';
			else if(!allow_flash) return false;
			else fileType = 0;
#ifdef DEBUG
			printf("[update] manual file type: %s %c\n", ptr, fileType);
#endif
		}

		strcpy(msg, g_Locale->getText(LOCALE_FLASHUPDATE_SQUASHFS_NOVERSION));
		msg_body = LOCALE_FLASHUPDATE_MSGBOX_MANUAL;
	}
	return (ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, msg, CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_UPDATE) == CMessageBox::mbrYes); // UTF-8
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
		ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENMTD));
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

	showGlobalStatus(19);
	paint();
	showGlobalStatus(20);

	if(softupdate_mode==1) //internet-update
	{
		char const * fname = rindex(filename.c_str(), '/') +1;
		char fullname[255];

		if(!getUpdateImage(newVersion)) {
			hide();
			ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_GETUPDATEFILEERROR)); // UTF-8
			return menu_return::RETURN_REPAINT;
		}
		sprintf(fullname, "%s/%s", g_settings.update_dir, fname);
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
		ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText(LOCALE_FLASHUPDATE_MD5SUMERROR)); // UTF-8
		return menu_return::RETURN_REPAINT;
	}
	if(softupdate_mode==1) { //internet-update
		if ( ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, (fileType < '3') ? "Flash downloaded image ?" : "Install downloaded pack ?", CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_UPDATE) != CMessageBox::mbrYes) // UTF-8
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
		if (g_settings.apply_settings) {
			if (ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_APPLY_SETTINGS), CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_UPDATE) == CMessageBox::mbrYes)
				if (!CExtUpdate::getInstance()->applySettings(filename, CExtUpdate::MODE_SOFTUPDATE))
					return menu_return::RETURN_REPAINT;
		}
#endif

#ifdef DEBUG1
		if(1) {
#else
		if(!ft.program(filename, 80, 100)) {
#endif
			hide();
			ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, ft.getErrorMessage().c_str()); // UTF-8
			return menu_return::RETURN_REPAINT;
		}

		//status anzeigen
		showGlobalStatus(100);
		showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_READY)); // UTF-8
		hide();
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_FLASHREADYREBOOT)); // UTF-8
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
			buffer =(char *) malloc(filesize+1);
			fread(buffer, filesize, 1, fd);
			fclose(fd);
			buffer[filesize] = 0;
			ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, buffer, CMessageBox::mbrBack, CMessageBox::mbBack); // UTF-8
			free(buffer);
		}
	}
	else // not image, install
	{
		const char install_sh[] = "/bin/install.sh";
#ifdef DEBUG1
		printf("[update] calling %s %s %s\n",install_sh, g_settings.update_dir, filename.c_str() );
#else
		printf("[update] calling %s %s %s\n",install_sh, g_settings.update_dir, filename.c_str() );
		my_system(3, install_sh, g_settings.update_dir, filename.c_str());
#endif
		showGlobalStatus(100);
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_READY)); // UTF-8
	}
	hide();
	return menu_return::RETURN_REPAINT;
}


//--------------------------------------------------------------------------------------------------------------


CFlashExpert::CFlashExpert()
	:CProgressWindow()
{
	selectedMTD = -1;
	width = w_max (40, 10);
}

CFlashExpert* CFlashExpert::getInstance()
{
	static CFlashExpert* FlashExpert = NULL;
	if(!FlashExpert)
		FlashExpert = new CFlashExpert();
	return FlashExpert;
}

void CFlashExpert::readmtd(int preadmtd)
{
	char tmpStr[256];
	struct timeval tv;
	gettimeofday(&tv, NULL);	
	strftime(tmpStr, sizeof(tmpStr), "_%Y%m%d_%H%M.img", localtime(&tv.tv_sec));
	CMTDInfo* mtdInfo = CMTDInfo::getInstance();
	std::string filename = (std::string)g_settings.update_dir + "/" + mtdInfo->getMTDName(preadmtd);
	filename += tmpStr;

	if (preadmtd == -1) {
		filename = (std::string)g_settings.update_dir + "/flashimage.img"; // US-ASCII (subset of UTF-8 and ISO8859-1)
		preadmtd = MTD_OF_WHOLE_IMAGE;
	}
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
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, message);
	}
}

void CFlashExpert::writemtd(const std::string & filename, int mtdNumber)
{
	char message[500];

	snprintf(message, sizeof(message),
		g_Locale->getText(LOCALE_FLASHUPDATE_REALLYFLASHMTD),
		FILESYSTEM_ENCODING_TO_UTF8_STRING(filename).c_str(),
		CMTDInfo::getInstance()->getMTDName(mtdNumber).c_str());

	if (ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, message, CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_UPDATE) != CMessageBox::mbrYes) // UTF-8
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
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_FLASHUPDATE_FLASHREADYREBOOT)); // UTF-8
		ft.reboot();
	}
}

void CFlashExpert::showMTDSelector(const std::string & actionkey)
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
#ifdef BOXMODEL_APOLLO
		// disable write uboot / uldr, FIXME correct numbers
		if ((actionkey == "writemtd") && (lx == 5 || lx == 6))
			enabled = false;
#else
		// disable write uboot
		if ((actionkey == "writemtd") && (lx == 0))
			enabled = false;
#endif
		sprintf(sActionKey, "%s%d", actionkey.c_str(), lx);
		mtdselector->addItem(new CMenuForwarderNonLocalized(mtdInfo->getMTDName(lx).c_str(), enabled, NULL, this, sActionKey, CRCInput::convertDigitToKey(shortcut++)));
	}
#if ENABLE_EXTUPDATE
	if (actionkey == "writemtd")
		mtdselector->addItem(new CMenuForwarderNonLocalized("systemFS with settings", true, NULL, this, "writemtd10", CRCInput::convertDigitToKey(shortcut++)));
#endif
	mtdselector->exec(NULL,"");
	delete mtdselector;
}

void CFlashExpert::showFileSelector(const std::string & actionkey)
{
	CMenuWidget* fileselector = new CMenuWidget(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE, width, MN_WIDGET_ID_FILESELECTOR);
	fileselector->addIntroItems(LOCALE_FLASHUPDATE_FILESELECTOR, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);

	struct dirent **namelist;
	int n = scandir(g_settings.update_dir, &namelist, 0, alphasort);
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
				fileselector->addItem(new CMenuForwarderNonLocalized(filen.c_str(), true, NULL, this, (actionkey + filen).c_str()));
				//TODO  make sure filen is UTF-8 encoded
			}
			free(namelist[count]);
		}
		free(namelist);
	}
	fileselector->exec(NULL,"");
	delete fileselector;
}

int CFlashExpert::exec(CMenuTarget* parent, const std::string & actionKey)
{
	if(parent)
		parent->hide();

	if(actionKey=="readflash") {
		readmtd(-1);
	}
	else if(actionKey=="writeflash") {
		showFileSelector("");
	}
	else if(actionKey=="readflashmtd") {
		showMTDSelector("readmtd");
	}
	else if(actionKey=="writeflashmtd") {
		showMTDSelector("writemtd");
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
				CExtUpdate::getInstance()->applySettings(actionKey, CExtUpdate::MODE_EXPERT);
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
		hide();
		return menu_return::RETURN_EXIT_ALL;
	}

	hide();
	return menu_return::RETURN_REPAINT;
}
