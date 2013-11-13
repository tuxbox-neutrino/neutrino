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
#include <daemonc/remotecontrol.h>
#include <system/flashtool.h>
#include "version.h"
#include <gui/buildinfo.h>
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
	cc_win		= NULL;
	cc_info 	= NULL;
	cc_tv		= NULL;
	cc_lic		= NULL;
	cc_sub_caption	= NULL;
	b_info 		= NULL;
	btn_red		= NULL;
	item_offset	= 10;
	item_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	item_height 	= item_font->getHeight();
	
	license_txt	= "";
	v_info.clear();
	config.loadConfig("/.version");
}

CImageInfo::~CImageInfo()
{
	hide();
	//deallocate window object, deletes also added cc_items
	delete cc_win;
}

void CImageInfo::Clean()
{
	//deallocate of the window object causes also deallocate added items,
	if (cc_win){
		delete cc_win;
		//it's important to set here a null pointer
		cc_win 	= NULL;
		cc_info = NULL;
		cc_tv	= NULL;
		cc_lic	= NULL;
		cc_sub_caption = NULL;
		b_info 	= NULL;
		btn_red	= NULL;
	}
}

int CImageInfo::exec(CMenuTarget* parent, const std::string &)
{
	int res = menu_return::RETURN_REPAINT;
	if (parent)
		parent->hide();

	//clean up before, because we could have a current instance with already initialized contents
	Clean();

	//init window object, add cc-items and paint all
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
		else if (msg == CRCInput::RC_red){
			// init temporarly vars
			neutrino_locale_t info_cap , new_btn_cap; 
			info_cap = new_btn_cap = NONEXISTANT_LOCALE;
			string info_txt = "";
			neutrino_locale_t btn_cap = btn_red->getCaptionLocale();

			//toggle caption and info contents
			if (btn_cap == LOCALE_BUILDINFO_MENU){
				info_cap = LOCALE_BUILDINFO_MENU;
				for (uint i=0; i<CBuildInfo::BI_TYPE_IDS; i++){
					info_txt += g_Locale->getText(b_info->getInfo(i).caption);
					info_txt += "\n";
					info_txt += b_info->getInfo(i).info_text  + "\n\n";
				}
				new_btn_cap = LOCALE_IMAGEINFO_LICENSE;
			}
			if (btn_cap == LOCALE_IMAGEINFO_LICENSE){
				info_cap = LOCALE_IMAGEINFO_LICENSE;
				info_txt = getLicenseText();
				new_btn_cap = LOCALE_BUILDINFO_MENU;
			}
			
			//assign new caption and info contents
			cc_sub_caption->setText(info_cap, CTextBox::AUTO_WIDTH, item_font);
			InitInfoText(info_txt);
			btn_red->setCaption(new_btn_cap);
			
			//paint items
			cc_sub_caption->paint(false);
			cc_lic->paint(false);
			btn_red->paint(false);
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

	hide();
	
	return res;
}

//contains all actions to init and add the window object and items
void CImageInfo::ShowWindow()
{
	CComponentsFooter *footer = NULL;
	if (cc_win == NULL){
		cc_win = new CComponentsWindow(LOCALE_IMAGEINFO_HEAD, NEUTRINO_ICON_INFO);
		cc_win->setWindowHeaderButtons(CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT);
		footer = cc_win->getFooterObject();
		footer->setColorBody(COL_INFOBAR_SHADOW_PLUS_1);
		btn_red = new CComponentsButtonRed(10, CC_CENTERED, 250, footer->getHeight(), LOCALE_BUILDINFO_MENU, false , true, false, footer->getColorBody(), footer->getColorBody());
		footer->addCCItem(btn_red);
	}

	//prepare minitv
	InitMinitv();

	//prepare infos
	InitInfos();

	//prepare build infos
	InitBuildInfos();

	//prepare info text
	InitInfoText(getLicenseText());

	//paint window
	cc_win->paint();
}

//prepare minitv
void CImageInfo::InitMinitv()
{
	//init the minitv object
	if (cc_tv == NULL)
		cc_tv = new CComponentsPIP (0, item_offset);
	
#if 0 //static assign of dimensions are distorting ratio of mini tv picture
	//init width and height
	cc_tv->setWidth(cc_win->getWidth()/3);
	cc_tv->setHeight(cc_win->getHeight()/3);
#endif

	//init x pos and use as parameter for setXPos
	int cc_tv_x = (cc_win->getWidth() - cc_tv->getWidth()) - item_offset;
	cc_tv->setXPos(cc_tv_x);

	//add minitv to container
	if (!cc_tv->isAdded())
		cc_win->addWindowItem(cc_tv);
}

//prepare distribution infos
void CImageInfo::InitBuildInfos()
{
	if (b_info == NULL)
		b_info = new CBuildInfo();
}

//collect required data from environment
void CImageInfo::InitInfoData()
{
	v_info.clear();

#ifdef BUILT_DATE
	const char * builddate = BUILT_DATE;
#else
	const char * builddate = config.getString("builddate", "n/a").c_str();
#endif

	std::string version_string;
#ifdef IMAGE_VERSION
	version_string = IMAGE_VERSION;
#else
	const char * _version = config.getString("version", "U000000000000000").c_str();
	static CFlashVersionInfo versionInfo(_version);

	version_string = versionInfo.getReleaseCycle();
	version_string += " ";
	version_string += versionInfo.getType();
	version_string += " (";
	version_string += versionInfo.getDate();
	version_string += ")";
#endif

	struct utsname uts_info;

	image_info_t imagename 	= {LOCALE_IMAGEINFO_IMAGE,	config.getString("imagename", "Neutrino-HD")};
	v_info.push_back(imagename);
	image_info_t version	= {LOCALE_IMAGEINFO_VERSION,	version_string};
	v_info.push_back(version);
#ifdef VCS
	image_info_t vcs	= {LOCALE_IMAGEINFO_VCS,	VCS};
	v_info.push_back(vcs);
#endif
	image_info_t date	= {LOCALE_IMAGEINFO_DATE,	builddate};
	v_info.push_back(date);
	if (uname(&uts_info) == 0) {
		image_info_t kernel	= {LOCALE_IMAGEINFO_KERNEL,	uts_info.release};
		v_info.push_back(kernel);
	}
	image_info_t creator	= {LOCALE_IMAGEINFO_CREATOR,	config.getString("creator", "n/a")};
	v_info.push_back(creator);
	image_info_t www	= {LOCALE_IMAGEINFO_HOMEPAGE,	config.getString("homepage", "n/a")};
	v_info.push_back(www);
	image_info_t doc	= {LOCALE_IMAGEINFO_DOKUMENTATION, config.getString("docs", "http://wiki.neutrino-hd.de")};
	v_info.push_back(doc);
	image_info_t forum	= {LOCALE_IMAGEINFO_FORUM,	config.getString("forum", "http://forum.tuxbox.org")};
	v_info.push_back(forum);
}


//prepare distribution infos
void CImageInfo::InitInfos()
{
	InitInfoData();

	//initialize container for infos
	if (cc_info == NULL)
		cc_info = new CComponentsForm();
	if (!cc_info->isAdded())
		cc_win->addWindowItem(cc_info);
	
	cc_info->setPos(item_offset, item_offset);
	
	//set width, use size between left border and minitv
	cc_info->setWidth(cc_win->getWidth() - cc_tv->getWidth() - 2*item_offset);
	
	//calculate initial height for info form
	cc_info->setHeight(v_info.size()*item_height);
	
	//create label and text items
	for (size_t i=0; i<v_info.size(); i++) {
		CComponentsExtTextForm *item = new CComponentsExtTextForm(1, CC_APPEND, cc_info->getWidth(), item_height, g_Locale->getText(v_info[i].caption), v_info[i].info_text);
		item->setLabelAndTextFont(item_font);
		item->setLabelWidthPercent(20);

		if ((i == 0) && (item->getYPos() == CC_APPEND))
			item->setYPos(1);

		//add ext-text object to window body
		if (!item->isAdded())
			cc_info->addCCItem(item);

		//add an offset before homepage and license and at the end
		if (v_info[i].caption == LOCALE_IMAGEINFO_CREATOR || v_info[i].caption == LOCALE_IMAGEINFO_FORUM){
			CComponentsShapeSquare *spacer = new CComponentsShapeSquare(1, CC_APPEND, 1, item_offset);
			//spacer ist not visible!
			spacer->allowPaint(false);
			cc_info->addCCItem(spacer);
			//increase height of cc_info object with offset
			int tmp_h = cc_info->getHeight();
			cc_info->setHeight(tmp_h + item_offset);
		}
	}
}

//get license
string CImageInfo::getLicenseText()
{
	string file = LICENSEDIR;
	file += g_settings.language;
	file += ".license";

	CComponentsText txt;
	string res = txt.getTextFromFile(file);

	return res;
}

//prepare info text
void CImageInfo::InitInfoText(const std::string& text)
{
	//get window body object
	CComponentsForm *winbody = cc_win->getBodyObject();
	int h_body = winbody->getHeight();
	int w_body = winbody->getWidth();

	//add a caption for info contents
	Font * caption_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	int caption_height = caption_font->getHeight();
	if (cc_sub_caption == NULL)
		cc_sub_caption = new CComponentsLabel(cc_info->getXPos(), CC_APPEND, cc_info->getWidth(), caption_height, 
						     g_Locale->getText(LOCALE_IMAGEINFO_LICENSE), CTextBox::AUTO_WIDTH, item_font);
	if (!cc_sub_caption->isAdded())
		cc_win->addWindowItem(cc_sub_caption);

	//add info text box
	int h_txt = h_body - item_offset - cc_info->getHeight() - cc_sub_caption->getHeight() - item_offset;

	if (cc_lic == NULL)
		cc_lic = new CComponentsInfoBox(CC_CENTERED, CC_APPEND, w_body-2*item_offset, h_txt);
	cc_lic->setSpaceOffset(1);
	cc_lic->setText(text, CTextBox::TOP | CTextBox::AUTO_WIDTH | CTextBox::SCROLL, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT]);

	//add text to container
	if (!cc_lic->isAdded())
		cc_win->addWindowItem(cc_lic);
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
	printf("[CImageInfo]   [%s - %d] hide...\n", __FUNCTION__, __LINE__);
	if (cc_win){
		cc_win->hide();
		Clean();
	}
}
