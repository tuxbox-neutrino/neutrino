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
#include <system/helpers.h>
#include "version.h"
#include <gui/buildinfo.h>
#define LICENSEDIR DATADIR "/neutrino/license/"
#ifdef ENABLE_LUA
#include <gui/lua/lua_api_version.h>
#endif
#include <nhttpd/yconfig.h>

#define VERSION_FILE TARGET_PREFIX "/.version"
#define Y_VERSION_FILE DATADIR "/neutrino/httpd/Y_Version.txt"

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
	item_font 	= NULL;
	item_height 	= 0;
	
	license_txt	= "";
	v_info.clear();
	config.loadConfig(VERSION_FILE);
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
		delete b_info;
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
	bool fadeout = false;
	neutrino_msg_t postmsg = 0;
	neutrino_msg_t msg;
	while (1)
	{
		neutrino_msg_data_t data;
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS(100);
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if ((msg == NeutrinoMessages::EVT_TIMER) && (data ==cc_win->GetFadeTimer())){
			if (cc_win->FadeDone())
				break;
			continue;
		}
		if (fadeout && msg == CRCInput::RC_timeout){
			if (cc_win->StartFadeOut()){
				msg = menu_return::RETURN_EXIT_ALL;
				continue;
			}
			else
				break;
		}

		if(msg == CRCInput::RC_setup) {
			res = menu_return::RETURN_EXIT_ALL;
			fadeout = true;
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
			btn_red->kill();
			btn_red->paint(false);
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			postmsg = msg;
			res = menu_return::RETURN_EXIT_ALL;
			fadeout = true;
		}
		else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_page_up)) {
			ScrollLic(false);
		}
		else if ((msg == CRCInput::RC_down) || (msg == CRCInput::RC_page_down)) {
			ScrollLic(true);
		}
		else if (msg <= CRCInput::RC_MaxRC){
			fadeout = true;
		}

		if ( msg >  CRCInput::RC_MaxRC && msg != CRCInput::RC_timeout){
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
		}

	}

	if (postmsg)
		g_RCInput->postMsg(postmsg, 0);

	hide();
	return res;
}

//contains all actions to init and add the window object and items
void CImageInfo::ShowWindow()
{
	CComponentsFooter *footer = NULL;
	if (cc_win == NULL){
		cc_win = new CComponentsWindowMax(LOCALE_IMAGEINFO_HEAD, NEUTRINO_ICON_INFO, 0, CC_SHADOW_ON);
		cc_win->setWindowHeaderButtons(CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT);
		footer = cc_win->getFooterObject();
		int h_footer = footer->getHeight();
		fb_pixel_t btn_col = /*g_settings.theme.Button_gradient ?  COL_BUTTON_BODY :*/ footer->getColorBody(); //TODO: Button_gradient option
		btn_red = new CComponentsButtonRed(10, CC_CENTERED, 250, h_footer-h_footer/4, LOCALE_BUILDINFO_MENU, footer, false , true, false, footer->getColorBody(), btn_col);
		btn_red->doPaintBg(false);
		btn_red->setButtonTextColor(COL_MENUFOOT_TEXT);
		btn_red->setColBodyGradient(CC_COLGRAD_OFF);
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
	cc_win->StartFadeIn();
	cc_win->paint(CC_SAVE_SCREEN_NO);
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
	std::string _version = config.getString("version", "U000000000000000").c_str();
	static CFlashVersionInfo versionInfo(_version.c_str());

	version_string = versionInfo.getReleaseCycle();
	version_string += " ";
	version_string += versionInfo.getType();
	version_string += " (";
	version_string += versionInfo.getDate();
	version_string += ")";
#endif

	struct utsname uts_info;

	image_info_t imagename 	= {LOCALE_IMAGEINFO_IMAGE,	config.getString("imagename", PACKAGE_NAME)};
	v_info.push_back(imagename);
	image_info_t version	= {LOCALE_IMAGEINFO_VERSION,	version_string};
	v_info.push_back(version);
#ifdef VCS
	image_info_t vcs	= {LOCALE_IMAGEINFO_VCS,	VCS};
	v_info.push_back(vcs);
#endif
	image_info_t date	= {LOCALE_IMAGEINFO_DATE,	builddate};
	v_info.push_back(date);
	string s_api;
#ifdef ENABLE_LUA
	s_api	+= "LUA " + to_string(LUA_API_VERSION_MAJOR) + "." + to_string(LUA_API_VERSION_MINOR);
	s_api	+= ", ";
#endif
	s_api	+= "yWeb ";
	s_api	+= getYApi();
	s_api	+= ", ";
	s_api	+= HTTPD_NAME;
	s_api	+= + " ";
	s_api	+= HTTPD_VERSION;
	s_api	+= + ", ";
	s_api	+= YHTTPD_NAME;
	s_api	+= + " ";
	s_api	+= YHTTPD_VERSION;
	image_info_t api	= {LOCALE_IMAGEINFO_API,	s_api};
	v_info.push_back(api);
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
	
	//create label and text items
	for (size_t i=0; i<v_info.size(); i++) {
		CComponentsExtTextForm *item = new CComponentsExtTextForm(1, CC_APPEND, cc_info->getWidth(), 0, g_Locale->getText(v_info[i].caption), v_info[i].info_text);
		item->setLabelWidthPercent(20);

		if (!item_font){
			item_font = item->getFont();
			//calculate initial height for info form
			item_height = item_font->getHeight();
		}
		item->setHeight(item_height);
		cc_info->setHeight(v_info.size()*item_height);

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

	string res = CComponentsText::getTextFromFile(file);

	//assign default text, if language file is not available
	if(res.empty()){
		file = LICENSEDIR;
		file += "english.license";
		res = CComponentsText::getTextFromFile(file);
	}

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
	cc_lic->doPaintTextBoxBg(true);

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
			//ctb->enableBackgroundPaint(true);
			if (scrollDown)
				ctb->scrollPageDown(1);
			else
				ctb->scrollPageUp(1);
			//ctb->enableBackgroundPaint(false);
		}
	}
}

void CImageInfo::hide()
{
	printf("[CImageInfo]   [%s - %d] hide...\n", __FUNCTION__, __LINE__);
	if (cc_win){
		cc_win->kill();
		cc_win->StopFade();
		Clean();
	}
}

string CImageInfo::getYApi()
{
	string ret;
	config.loadConfig(Y_VERSION_FILE);
	ret = config.getString("version", "n/a");
	config.loadConfig(VERSION_FILE);
	return ret;
}

