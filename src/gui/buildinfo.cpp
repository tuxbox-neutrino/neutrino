/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Copyright (C) 2013, M. Liebmann 'micha-bbg'
	Copyright (C) 2013-2014, Thilo Graf 'dbt'

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
#include <gui/buildinfo.h>
#include <gui/widget/msgbox.h>
#include <system/helpers.h>

#include <local_build_config.h>

using namespace std;

CBuildInfo::CBuildInfo() : CComponentsWindow(0, 0, 700, 500, LOCALE_BUILDINFO_MENU, NEUTRINO_ICON_INFO)
{
	initVarBuildInfo();
}

//init all var members
void CBuildInfo::initVarBuildInfo()
{
	setCenterPos();

	font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT];
	setWindowHeaderButtons(CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT);
	InitInfoItems();


	shadow = true;
}


int CBuildInfo::exec(CMenuTarget* parent, const string & /*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();
		
	//exit if no informations available
	if (!HasData()){
		return res;
	}

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

void CBuildInfo::setFontType(Font* font_text)
{
	if (font_text == NULL)
		return;
	font = font_text;
	InitInfoItems();
}

bool CBuildInfo::HasData()
{
	v_info.clear();

#ifdef USED_COMPILER
	build_info_t compiler = {BI_TYPE_ID_USED_COMPILER, LOCALE_BUILDINFO_COMPILED_WITH, USED_COMPILER};
	v_info.push_back(compiler);
#endif

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

#ifdef USED_BUILD
	build_info_t build = {BI_TYPE_ID_USED_BUILD , LOCALE_BUILDINFO_COMPILED_ON, USED_BUILD};
	v_info.push_back(build);
#endif

	CComponentsText utext;
	build_info_t kernel = {BI_TYPE_ID_USED_KERNEL, LOCALE_BUILDINFO_KERNEL, utext.getTextFromFile("/proc/version")};
	v_info.push_back(kernel);

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
	if (!HasData())
		return;

	//ensure a clean body
	ccw_body->clear();

	//define size and position
	int x_info = 10;
	int h_info = ccw_body->getHeight()/v_info.size(); //default height
	int w_info = width-2*x_info;

	//init info texts
	for(size_t i=0; i<v_info.size(); i++){
		CComponentsExtTextForm *info = new CComponentsExtTextForm(10, CC_APPEND, w_info, h_info, g_Locale->getText(v_info[i].caption), v_info[i].info_text, NULL, ccw_body);
		info->setLabelAndTextFont(font);
		info->setTextModes(CTextBox::TOP , CTextBox::AUTO_HIGH | CTextBox::TOP | CTextBox::AUTO_LINEBREAK_NO_BREAKCHARS);
		info->doPaintBg(false);
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
