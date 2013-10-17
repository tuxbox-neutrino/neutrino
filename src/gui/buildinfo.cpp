/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Copyright (C) 2013, M. Liebmann 'micha-bbg'
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

#include <global.h>
#include <neutrino.h>
//#include <sys/utsname.h>
#include <string>
#include <driver/screen_max.h>
#include <driver/neutrinofonts.h>
#include <gui/components/cc_item_text.h>
#include <gui/components/cc_item_shapes.h>
#include <gui/buildinfo.h>
#include <system/helpers.h>

CBuildInfo::CBuildInfo(): config ('\t')
{
	Init();
}

//init all var members
void CBuildInfo::Init(void)
{
	cc_win		= NULL;
	cc_info 	= NULL;
	item_offset	= 10;
	bodyHeight	= 0;
	v_info.clear();
	config.loadConfig("/.version");
}

CBuildInfo::~CBuildInfo()
{
	hide();
	delete cc_win;
}

void CBuildInfo::Clean()
{
	if (cc_win){
		delete cc_win;
		cc_win 	= NULL;
		cc_info = NULL;
	}
}

int CBuildInfo::exec(CMenuTarget* parent, const std::string &)
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
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
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

	hide();

	return res;
}

void CBuildInfo::ShowWindow()
{
	if (cc_win == NULL){
		cc_win = new CComponentsWindow(LOCALE_BUILDINFO_MENU, NEUTRINO_ICON_INFO);
		cc_win->setWindowHeaderButtons(CComponentsHeader::CC_BTN_MENU | CComponentsHeader::CC_BTN_EXIT);
		CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
		int w = (frameBuffer->getScreenWidth()*2)/3;
		int h = frameBuffer->getScreenHeight();
		cc_win->setDimensionsAll(getScreenStartX(w), getScreenStartY(h), w, h);

		CComponentsForm* bo = cc_win->getBodyObject();
		if (bo) bodyHeight = bo->getHeight();

	}

	InitInfos();

	cc_win->paint();
}

void CBuildInfo::InitInfos()
{
	v_info.clear();

#ifdef USED_COMPILER
	build_info_t compiler = {LOCALE_BUILDINFO_COMPILED_WITH, USED_COMPILER};
	v_info.push_back(compiler);
#endif

#ifdef USED_CXXFLAGS
	std::string cxxflags = USED_CXXFLAGS;
	cxxflags = trim(cxxflags);
	// Remove double spaces
	size_t pos = cxxflags.find("  ");
	while (pos != std::string::npos) {
		cxxflags.erase(pos, 1);
		pos = cxxflags.find("  ", pos);
	}
	build_info_t flags = {LOCALE_BUILDINFO_COMPILER_FLAGS, cxxflags};
	v_info.push_back(flags);
#endif

#ifdef USED_BUILD
	build_info_t build = {LOCALE_BUILDINFO_COMPILED_ON, USED_BUILD};
	v_info.push_back(build);
#endif

	build_info_t creator	= {LOCALE_BUILDINFO_CREATOR, config.getString("creator", "n/a")};
	v_info.push_back(creator);

	FILE *fp = fopen("/proc/version", "r");
	if (fp) {
		char zeile[1024];
		memset(zeile, 0, sizeof(zeile));
		fgets(zeile, sizeof(zeile)-1, fp);
		fclose(fp);
		std::string zeile_s = zeile;
		zeile_s = trim(zeile_s);
		build_info_t kernel = {LOCALE_BUILDINFO_KERNEL, zeile_s};
		v_info.push_back(kernel);
	}

// ###########################################################

	int dx = 0, dy = 27;
	Font * item_font = *(CNeutrinoFonts::getInstance()->getDynFont(dx, dy));

	//initialize container for infos
	if (cc_info == NULL)
		cc_info = new CComponentsForm();
	cc_win->addWindowItem(cc_info);
	cc_info->setPos(item_offset, item_offset);
	cc_info->setWidth((cc_win->getWidth())-2*item_offset);

	//calculate max width of label and info_text
	int w_label = 0, w_text = 0, w = 0;
	for (size_t i = 0; i < v_info.size(); i++) {
		w = item_font->getRenderWidth(g_Locale->getText(v_info[i].caption), true);
		w_label = std::max(w_label, w);

		w = item_font->getRenderWidth(v_info[i].info_text.c_str(), true);
		w_text = std::max(w_text, w);
	}

	int x_label = 0, y_text = 0;
	int x_text = x_label + w_label + item_offset;
	int item_height = item_font->getHeight();
	int item_spacer = item_height / 2;

	//recalc w_text to avoid an overlap with pip TODO: calculate within cc_info itself
	w_text = std::min(w_text, cc_win->getWidth() - x_text - 2*item_offset);
	//create label and text items
	int h_tmp = 0;
	size_t i = 0;

	for (i = 0; i < v_info.size(); i++) {
		CComponentsLabel *cc_label = new CComponentsLabel();
		cc_label->setDimensionsAll(x_label, y_text, w_label, item_height);
		cc_label->setText(v_info[i].caption, CTextBox::NO_AUTO_LINEBREAK, item_font);

		//add label object to window body
		cc_info->addCCItem(cc_label);

		CComponentsText *cc_text = new CComponentsText();
		cc_text->setDimensionsAll(x_text, y_text, w_text, item_height);
		int textMode = CTextBox::AUTO_HIGH | CTextBox::TOP | CTextBox::AUTO_LINEBREAK_NO_BREAKCHARS;
		cc_text->setText(v_info[i].info_text, textMode, item_font);

		//The rest of body height, less 1 line for each additional entry
		int rest = bodyHeight-h_tmp-((v_info.size()-(i+1))*(item_height+item_spacer));
		int lines = cc_text->getTextLinesAutoHeight(rest, w_text, textMode);
		cc_text->setHeight(lines*item_height);
		y_text += lines*item_height + item_spacer;

		//add text object to window body
		cc_info->addCCItem(cc_text);

		// <hr style="width: 85%"> ;-)
		if (v_info[i].caption == LOCALE_BUILDINFO_CREATOR) {
			int w1 = (cc_win->getWidth()*85)/100;
			int x1 = cc_win->getRealXPos() + ((cc_win->getWidth() - w1)/2);
			CComponentsShapeSquare *cc_shape;
			cc_shape = new CComponentsShapeSquare(x1, y_text, w1, 2, CC_SHADOW_OFF, COL_MENUHEAD_PLUS_0, COL_MENUHEAD_PLUS_0);
			y_text += item_spacer;
			h_tmp += item_spacer;
			cc_info->addCCItem(cc_shape);
		}

		//set height for info form
		h_tmp += lines*item_height + item_spacer;
	}

	//assign height of info form
	cc_info->setHeight(h_tmp);

	int ho_h = 0, fo_h = 0;
	CComponentsHeader* ho = cc_win->getHeaderObject();
	if (ho) ho_h = ho->getHeight();
	CComponentsFooter* fo = cc_win->getFooterObject();
	if (fo) fo_h = fo->getHeight();
	h_tmp += ho_h + fo_h + 12;
	cc_win->setHeight(h_tmp);
	cc_win->setYPos(getScreenStartY(h_tmp));
}

void CBuildInfo::hide()
{
	printf("[CBuildInfo]   [%s - %d] hide...\n", __FUNCTION__, __LINE__);
	if (cc_win){
		cc_win->hide();
		Clean();
	}
}
