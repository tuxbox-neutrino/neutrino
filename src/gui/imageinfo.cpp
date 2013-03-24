/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Implementation of component classes
	Copyright (C) 2013, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/imageinfo.h>

#include <global.h>
#include <neutrino.h>

#include <driver/rcinput.h>

#include <sys/utsname.h>
#include <string>
#include <fstream>
#include <errno.h>
#include <daemonc/remotecontrol.h>
#include <system/flashtool.h>

#include "git_version.h"
#define GIT_DESC "GIT Desc.:"
#define GIT_REV "GIT Build:"
#define LICENSEDIR DATADIR "/neutrino/license/"

using namespace std;

extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

CImageInfo::CImageInfo(): config ('\t')
{
	Init();
}

//init all var members
void CImageInfo::Init(void)
{
	cc_win 		= NULL;
	cc_lic 		= NULL;
	item_offset 	= 20;
	item_top 	= item_offset;
	license_txt 	= "";
	v_info.clear();
	v_info_supp.clear();
	config.loadConfig("/.version");
}

CImageInfo::~CImageInfo()
{
	//deallocate window object, deletes also added cc_items
	delete cc_win;
}

int CImageInfo::exec(CMenuTarget* parent, const std::string &)
{
	int res = menu_return::RETURN_REPAINT;
	if (parent)
		parent->hide();

	//init window object and add cc_items
	ShowWindow();

	neutrino_msg_t msg;
	while (1)
	{
		neutrino_msg_data_t data;
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS(100);
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if(msg == CRCInput::RC_setup) {
			res = menu_return::RETURN_EXIT_ALL;
			break;
		}
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
			g_RCInput->postMsg (msg, 0);
			res = menu_return::RETURN_EXIT_ALL;
			break;
		}
		else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_page_up)) {
			ScrollLic(false);
		}
		else if ((msg == CRCInput::RC_down) || (msg == CRCInput::RC_page_down)) {
			ScrollLic(true);
		}
		else if (msg <= CRCInput::RC_MaxRC){
			break;
		}

		if ( msg >  CRCInput::RC_MaxRC && msg != CRCInput::RC_timeout){
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
		}
	}

	//hide window
	cc_win->hide();

	//deallocate of the window object causes also deallocate added items,
	delete cc_win;
	//it's important to set here a null pointer
	cc_win = NULL;

	return res;
}

//contains all actions to init and add the window object and items
void CImageInfo::ShowWindow()
{
	if (cc_win == NULL){
		cc_win = new CComponentsWindow(LOCALE_IMAGEINFO_HEAD, NEUTRINO_ICON_INFO);
		item_top = cc_win->getStartY() + item_offset;
		cc_win->setWindowHeaderButtons(CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT);
	}

	//prepare minitv: important! init the minitv object as first
	InitMinitv();

	//prepare infos
	InitInfos();

	//add section space
	item_top += 5;

	//prepare suppoprt infos
	InitSupportInfos();

	//prepare license text
	InitLicenseText();

	//paint window
	cc_win->paint();
}

//prepare minitv
void CImageInfo::InitMinitv()
{
	//init the minitv object
	CComponentsPIP *cc_tv = new CComponentsPIP (0, item_top, 33/*%*/);

	//init x pos and use as parameter for setXPos
	int cc_tv_x = (cc_win->getWidth() - cc_tv->getWidth()) - item_offset;
	cc_tv->setXPos(cc_tv_x);

	//add minitv to container
	cc_win->addCCItem(cc_tv);
}

//prepare distribution infos
void CImageInfo::InitInfos()
{
	v_info.clear();

#ifdef GITVERSION
	const char * builddate     = GITVERSION;
#else
	const char * builddate     = config.getString("builddate",     BUILT_DATE).c_str();
#endif

	const char * version   = config.getString("version",   "no version").c_str();
	config.getString("version",   "no version");
	static CFlashVersionInfo versionInfo(version);
	const char * releaseCycle = versionInfo.getReleaseCycle();

	struct utsname uts_info;
	std::string Version_Kernel;
	if( uname(&uts_info) < 0 ) {
		Version_Kernel = releaseCycle;
		Version_Kernel += " ";
		Version_Kernel += versionInfo.getType();
	}else{
		Version_Kernel  = releaseCycle;
		Version_Kernel += " ";
		Version_Kernel += versionInfo.getType();
		Version_Kernel += " - Kernel: ";
		Version_Kernel += uts_info.release;
	}

	image_info_t imagename 	= {LOCALE_IMAGEINFO_IMAGE, 	config.getString("imagename", "Neutrino-HD")};
	v_info.push_back(imagename);
	image_info_t date	= {LOCALE_IMAGEINFO_DATE,	builddate};
	v_info.push_back(date);
	image_info_t kversion	= {LOCALE_IMAGEINFO_VERSION,	Version_Kernel};
	v_info.push_back(kversion);
	image_info_t creator	= {LOCALE_IMAGEINFO_CREATOR,	config.getString("creator",   "n/a")};
	v_info.push_back(creator);

	//create label and text items
	for (size_t i = 0; i < v_info.size(); i++) {
		CComponentsLabel *cc_txt = new CComponentsLabel();
		cc_txt->setDimensionsAll(item_offset, item_top, 200, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());
		cc_txt->setText(v_info[i].caption, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]);

		//add label to container
		cc_win->addCCItem(cc_txt);

		CComponentsText *cc_info = new CComponentsText();
		cc_info->setDimensionsAll(item_offset+cc_txt->getWidth(), item_top, 450, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());
		cc_info->setText(v_info[i].info_text.c_str(), CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]);

		//add text to container
		cc_win->addCCItem(cc_info);

		item_top += item_offset*2-5;
	}
}

//prepare support infos
void CImageInfo::InitSupportInfos()
{
	v_info_supp.clear();

	image_info_t www	= {LOCALE_IMAGEINFO_HOMEPAGE,	config.getString("homepage",  "n/a")};
	v_info_supp.push_back(www);
	image_info_t doc	= {LOCALE_IMAGEINFO_DOKUMENTATION, config.getString("docs",      "http://wiki.neutrino-hd.de")};
	v_info_supp.push_back(doc);
	image_info_t forum	= {LOCALE_IMAGEINFO_FORUM,	config.getString("forum",     "http://forum.tuxbox.org")};
	v_info_supp.push_back(forum);
	image_info_t license	= {LOCALE_IMAGEINFO_LICENSE,	"GPL"};
	v_info_supp.push_back(license);

	//create text an label items
	for (size_t i = 0; i < v_info_supp.size(); i++) {
		CComponentsLabel *cc_txt = new CComponentsLabel();
		cc_txt->setDimensionsAll(item_offset, item_top, 200, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());
		cc_txt->setText(v_info_supp[i].caption, CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]);
		
		cc_win->addCCItem(cc_txt);

		CComponentsText *cc_info = new CComponentsText();
		cc_info->setDimensionsAll(item_offset+cc_txt->getWidth(), item_top, 450, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());
		cc_info->setText(v_info_supp[i].info_text.c_str(), CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]);
		
		cc_win->addCCItem(cc_info);

		item_top += item_offset*2-5;
	}
}

//prepare license infos
void CImageInfo::InitLicenseText()
{
	license_txt = "";
	char line[1024];
	string file = LICENSEDIR;
	file += g_settings.language;
	file += ".license";
	ifstream in (file.c_str(), ios::in);

	if (!in){
		printf("[CImageInfo]    [%s - %d] error while open %s -> %s\n", __FUNCTION__, __LINE__, file.c_str(), strerror(errno));
		return;
	}

	while (in.getline (line, sizeof(line)-1)){
		string lline = (string)line;
		license_txt += lline + '\n';
	}
	in.close();

	cc_lic = new CComponentsInfoBox(item_offset, item_top, cc_win->getWidth()-2*item_offset, cc_win->getHeight()-item_top-item_offset);
	cc_lic->setText(license_txt, CTextBox::AUTO_WIDTH | CTextBox::SCROLL, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT]);

	//add text to container
	cc_win->addCCItem(cc_lic);
}

//scroll licens text
void CImageInfo::ScrollLic(bool scrollDown)
{
	if (cc_lic && (cc_lic->cctext)) {
		//get the textbox instance from infobox object and use CTexBbox scroll methods
		CTextBox* ctb = cc_lic->cctext->getCTextBoxObject();
		if (ctb) {
			ctb->enableBackgroundPaint(true);
			if (scrollDown)
				ctb->scrollPageDown(1);
			else
				ctb->scrollPageUp(1);
			ctb->enableBackgroundPaint(false);
		}
	}
}



void CImageInfo::hide()
{
  	cc_win->hide();
}
