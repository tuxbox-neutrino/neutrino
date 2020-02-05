/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Copyright (C) 2013, M. Liebmann 'micha-bbg'
	Copyright (C) 2013-2017, 2020, Thilo Graf 'dbt'

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


#include <global.h>
#include <neutrino.h>

#include <string>
#include <driver/neutrinofonts.h>
#include "gui/buildinfo.h"
#include "gui/widget/msgbox.h"
#include <system/helpers.h>

#include <local_build_config.h>

using namespace std;

CBuildInfo::CBuildInfo(bool show) : CComponentsWindow(0, 0, CCW_PERCENT 85, CCW_PERCENT 85, LOCALE_BUILDINFO_MENU, NEUTRINO_ICON_INFO)
{
	initVarBuildInfo();
// 	setBodyBGImage(DATADIR "/neutrino/icons/start.jpg");
	if (show)
		exec(NULL, "");
	else
		GetData();
}

//init all var members
void CBuildInfo::initVarBuildInfo()
{
	setCenterPos();

	font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT];
	setWindowHeaderButtons(CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT);

	shadow = CC_SHADOW_ON;
}


int CBuildInfo::exec(CMenuTarget* parent, const string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	//exit if no informations available
	if (!GetData()){
		return res;
	}

	InitInfoItems();

	//paint window
	if (!is_painted)
		paint();


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
		else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			g_RCInput->postMsg (msg, 0);
			res = menu_return::RETURN_EXIT_ALL;
			break;
		}
		else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_page_up)) {
			Scroll(false);
		}
		else if ((msg == CRCInput::RC_down) || (msg == CRCInput::RC_page_down)) {
			Scroll(true);
		}
		else if (msg <= CRCInput::RC_MaxRC){
			break;
		}

		if ( msg >  CRCInput::RC_MaxRC && msg != CRCInput::RC_timeout){
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
		}
		

	}

	//hide window
	hide();

	return res;
}

void CBuildInfo::Scroll(bool scrollDown)
{
	CTextBox* ctb = static_cast<CComponentsExtTextForm*>(ccw_body->getCCItem(3))->getTextObject()->getCTextBoxObject();
	ctb->enableBackgroundPaint(true); //FIXME: behavior of CTextBox scroll is broken with disabled background paint
	if (ctb) {
		if (scrollDown)
			ctb->scrollPageDown(1);
		else
			ctb->scrollPageUp(1);
	}
}

void CBuildInfo::setFontType(Font* font_text)
{
	if (font_text == NULL)
		return;
	font = font_text;
	InitInfoItems();
}

bool CBuildInfo::GetData()
{
	v_info.clear();

#ifdef USED_COMPILER
	build_info_t compiler = {BI_TYPE_ID_USED_COMPILER, LOCALE_BUILDINFO_COMPILED_WITH, USED_COMPILER};
	v_info.push_back(compiler);
#endif

#ifdef USED_BUILD
	build_info_t build = {BI_TYPE_ID_USED_BUILD , LOCALE_BUILDINFO_COMPILED_ON, USED_BUILD};
	v_info.push_back(build);
#endif

	CComponentsText utext;
	build_info_t kernel = {BI_TYPE_ID_USED_KERNEL, LOCALE_BUILDINFO_KERNEL, utext.getTextFromFile("/proc/version")};
	v_info.push_back(kernel);

#ifdef USED_CXXFLAGS
	string cxxflags = USED_CXXFLAGS;
	cxxflags = trim(cxxflags);
	// Remove double spaces
	size_t pos = cxxflags.find("  ");
	while (pos != string::npos) {
		cxxflags.erase(pos, 1);
		pos = cxxflags.find("  ", pos);
	}
	build_info_t flags = {BI_TYPE_ID_USED_CXXFLAGS, LOCALE_BUILDINFO_COMPILER_FLAGS, cxxflags};
	v_info.push_back(flags);
#endif

#if 0
	CConfigFile data ('\t');
	data.loadConfig(TARGET_PREFIX "/.version");
	build_info_t creator	= {BI_TYPE_ID_CREATOR, LOCALE_BUILDINFO_CREATOR, data.getString("creator", "n/a")};
	v_info.push_back(creator);
#endif

	if (v_info.empty()){
		DisplayInfoMessage("No Informations available. Please report!");
		return false;
	}

	return true;
}

void CBuildInfo::InitInfoItems()
{
	//get and checkup required informations
	if (!GetData())
		return;

	//ensure a clean body
	ccw_body->clear();

	//define size and position
	int x_info = OFFSET_INNER_MID;
	int h_info = 0; //default height
	int w_info = width-2*x_info;
	int y_info = OFFSET_INNER_MID;

	//init info texts
	for(size_t i=0; i<v_info.size(); i++){
		h_info = v_info[i].type_id != BI_TYPE_ID_USED_CXXFLAGS ? font->getHeight() * 2 + OFFSET_INNER_MID : ccw_body->getHeight() - y_info;
		CComponentsExtTextForm *info = new CComponentsExtTextForm(OFFSET_INNER_MID, y_info, w_info, h_info, g_Locale->getText(v_info[i].caption), v_info[i].info_text, NULL, ccw_body);
		info->setLabelAndTextFont(font);
		if (v_info[i].type_id == BI_TYPE_ID_USED_CXXFLAGS)
			info->setTextModes(CTextBox::TOP , CTextBox::TOP | CTextBox::AUTO_WIDTH | CTextBox::SCROLL);
		else
			info->setTextModes(CTextBox::TOP , CTextBox::TOP | CTextBox::AUTO_LINEBREAK_NO_BREAKCHARS);
		info->doPaintBg(false);
		y_info += h_info;
	}
}

// This allows to retrieve information about build infos.
// Use parameter 'type_info' to get specific information.
build_info_t CBuildInfo::getInfo(const info_type_id_t& type_id)
{
	for(size_t i=0; i<v_info.size(); i++){
		if (v_info[i].type_id == type_id)
			return v_info[i];
	}

	build_info_t res;
	res.type_id = type_id;
	res.caption = NONEXISTANT_LOCALE;
	res.info_text = "Info not available!";

	return res;
}

void CBuildInfo::hide()
{
	CComponentsWindow::hide();
}
