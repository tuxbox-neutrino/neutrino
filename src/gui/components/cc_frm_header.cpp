/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, 2014 Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_frm_header.h"
#include <system/debug.h>
using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsHeader inherit from CComponentsForm
CComponentsHeader::CComponentsHeader(CComponentsForm* parent)
{
	//CComponentsHeader
	initVarHeader(1, 1, 0, 0, "", "", 0, parent);
}

CComponentsHeader::CComponentsHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					const std::string& icon_name,
					const int& buttons,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	initVarHeader(x_pos, y_pos, w, h, caption, icon_name, buttons, parent, has_shadow, color_frame, color_body, color_shadow);
}

CComponentsHeaderLocalized::CComponentsHeaderLocalized(	const int& x_pos, const int& y_pos, const int& w, const int& h,
							neutrino_locale_t caption_locale,
							const std::string& icon_name,
							const int& buttons,
							CComponentsForm* parent,
							bool has_shadow,
							fb_pixel_t color_frame,
							fb_pixel_t color_body,
							fb_pixel_t color_shadow)
							:CComponentsHeader(	x_pos, y_pos, w, h,
										g_Locale->getText(caption_locale),
										icon_name, buttons,
										parent,
										has_shadow,
										color_frame, color_body, color_shadow){};

void CComponentsHeader::initVarHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					const std::string& icon_name,
					const int& buttons,
					CComponentsForm* parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	cc_item_type 		= CC_ITEMTYPE_FRM_HEADER;
	
	x	= x_pos;
	y	= y_pos;

	//init header width
	width 	= w == 0 ? frameBuffer->getScreenWidth(true) : w;

	//init header default height
	height 		= max(h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());

	cch_size_mode	= CC_HEADER_SIZE_LARGE;
	initCaptionFont();	//sets cch_font and calculate height if required;

	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	col_body 	= COL_MENUHEAD_PLUS_0;
	cch_text	= caption;
	cch_icon_name	= icon_name;

	corner_rad	= RADIUS_LARGE,
	corner_type	= CORNER_TOP;

	cch_icon_obj		= NULL;
	cch_text_obj		= NULL;
	cch_btn_obj		= NULL;
	cch_col_text		= COL_MENUHEAD_TEXT;
	cch_caption_align	= CTextBox::NO_AUTO_LINEBREAK;
	cch_items_y 		= CC_CENTERED;
	cch_offset		= 8;
	cch_icon_x 		= cch_offset;
	cch_icon_w		= 0;
	cch_text_x		= cch_offset;
	cch_buttons_space	= cch_offset;

	addContextButton(buttons);
	initCCItems();
	initParent(parent);
}

CComponentsHeader::~CComponentsHeader()
{
	dprintf(DEBUG_DEBUG, "[~CComponentsHeader]   [%s - %d] delete...\n", __func__, __LINE__);
	v_cch_btn.clear();
}

void CComponentsHeader::setCaption(const std::string& caption, const int& align_mode)
{
	cch_text		= caption;
	cch_caption_align 	= align_mode;
}

void CComponentsHeader::setCaption(neutrino_locale_t caption_locale, const int& align_mode)
{
	cch_text		= g_Locale->getText(caption_locale);
	cch_caption_align 	= align_mode;
}

void CComponentsHeader::setCaptionFont(Font* font)
{
	initCaptionFont(font); //cch_font = font
}

void CComponentsHeader::initCaptionFont(Font* font)
{
	Font *l_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	Font *s_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];

	if (font == NULL){
		cch_font = (cch_size_mode == CC_HEADER_SIZE_LARGE? l_font : s_font);

		//select matching height
		if (cch_size_mode == CC_HEADER_SIZE_LARGE)
			height	= std::max(height, l_font->getHeight());
		else
			height	= std::min(height, s_font->getHeight());
	}
	else{
		cch_font = font;
		height = std::max(height, cch_font->getHeight());
	}
}

void CComponentsHeader::setIcon(const char* icon_name)
{
	if (icon_name){
		string s_icon = string(icon_name);
		setIcon(s_icon);
	}
	else
		setIcon("");
}

void CComponentsHeader::setIcon(const std::string& icon_name)
{
	cch_icon_name 	= icon_name;
	initIcon();
}

void CComponentsHeader::initIcon()
{
	//init cch_icon_obj only if an icon available
	if (cch_icon_name.empty()) {
		cch_icon_w = 0;
		if (cch_icon_obj){
			delete cch_icon_obj;
			cch_icon_obj = NULL;
		}
		return;
	}

	//create instance for cch_icon_obj and add to container at once
	if (cch_icon_obj == NULL){
		dprintf(DEBUG_DEBUG, "[CComponentsHeader]\n    [%s - %d] init header icon: %s\n", __func__, __LINE__, cch_icon_name.c_str());
		cch_icon_obj = new CComponentsPicture(cch_icon_x, cch_items_y, 0, 0, cch_icon_name, this);
	}

	//set properties for icon object
	if (cch_icon_obj){
		//get dimensions of header icon
		int iw = 0;
		int ih = 0;
		cch_icon_obj->getImageSize(&iw, &ih);
		dprintf(DEBUG_INFO, "[CComponentsHeader]\n    [%s - %d] init icon size: iw = %d, ih = %d\n", __func__, __LINE__, iw, ih);
		cch_icon_obj->setWidth(iw);
		cch_icon_obj->setHeight(ih);
		cch_icon_obj->doPaintBg(false);

		//set corner mode of icon item
		int cc_icon_corner_type = corner_type;
		if (corner_type == CORNER_TOP_LEFT || corner_type == CORNER_TOP)
			cc_icon_corner_type = CORNER_TOP_LEFT;
		else
			cc_icon_corner_type = CORNER_LEFT;
		cch_icon_obj->setCorner(corner_rad-fr_thickness, cc_icon_corner_type);

		//global set width of icon object
		cch_icon_w = cch_icon_obj->getWidth();

		//global adapt height
		height = max(height, cch_icon_obj->getHeight());

// 		//re-align height of icon object
// 		cch_icon_obj->setHeight(height);
	}
}

void CComponentsHeader::addContextButton(const std::string& button_name)
{
	v_cch_btn.push_back(button_name);
	dprintf(DEBUG_DEBUG, "[CComponentsHeader]  %s added %d default buttons...\n", __func__, (int)v_cch_btn.size());
}

void CComponentsHeader::addContextButton(const std::vector<std::string>& v_button_names)
{
	for (size_t i= 0; i< v_button_names.size(); i++)
		addContextButton(v_button_names[i]);
}

void CComponentsHeader::addContextButton(const int& buttons)
{
	if (buttons & CC_BTN_EXIT)
		addContextButton(NEUTRINO_ICON_BUTTON_HOME);
	if (buttons & CC_BTN_HELP)
		addContextButton(NEUTRINO_ICON_BUTTON_HELP);
	if (buttons & CC_BTN_INFO)
		addContextButton(NEUTRINO_ICON_BUTTON_INFO);
	if (buttons & CC_BTN_MENU)
		addContextButton(NEUTRINO_ICON_BUTTON_MENU);
	if (buttons & CC_BTN_MUTE_ZAP_ACTIVE )
		addContextButton(NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE);
	if (buttons & CC_BTN_MUTE_ZAP_INACTIVE )
		addContextButton(NEUTRINO_ICON_BUTTON_MUTE_ZAP_INACTIVE);
	if (buttons & CC_BTN_OKAY)
		addContextButton(NEUTRINO_ICON_BUTTON_OKAY);
	if (buttons & CC_BTN_MUTE)
		addContextButton(NEUTRINO_ICON_BUTTON_MUTE);
	if (buttons & CC_BTN_TOP)
		addContextButton(NEUTRINO_ICON_BUTTON_TOP);
	if (buttons & CC_BTN_DOWN)
		addContextButton(NEUTRINO_ICON_BUTTON_DOWN);
	if (buttons & CC_BTN_RIGHT)
		addContextButton(NEUTRINO_ICON_BUTTON_RIGHT);
	if (buttons & CC_BTN_LEFT)
		addContextButton(NEUTRINO_ICON_BUTTON_LEFT);
	if (buttons & CC_BTN_FORWARD)
		addContextButton(NEUTRINO_ICON_BUTTON_FORWARD);
	if (buttons & CC_BTN_BACKWARD)
		addContextButton(NEUTRINO_ICON_BUTTON_BACKWARD);
	if (buttons & CC_BTN_PAUSE)
		addContextButton(NEUTRINO_ICON_BUTTON_PAUSE);
	if (buttons & CC_BTN_PLAY)
		addContextButton(NEUTRINO_ICON_BUTTON_PLAY);
	if (buttons & CC_BTN_RECORD_ACTIVE)
		addContextButton(NEUTRINO_ICON_BUTTON_RECORD_ACTIVE);
	if (buttons & CC_BTN_RECORD_INACTIVE)
		addContextButton(NEUTRINO_ICON_BUTTON_RECORD_INACTIVE);
	if (buttons & CC_BTN_RECORD_STOP)
		addContextButton(NEUTRINO_ICON_BUTTON_STOP);
}

void CComponentsHeader::removeContextButtons()
{
	dprintf(DEBUG_DEBUG, "[CComponentsHeader]\t    [%s - %d] removing %u context buttons...\n", __func__, __LINE__, v_cch_btn.size());
	v_cch_btn.clear();
	if (cch_btn_obj)
		cch_btn_obj->clear();
}

void CComponentsHeader::initButtons()
{
	//exit if no button defined
	if (v_cch_btn.empty()){
		if (cch_btn_obj)
			cch_btn_obj->clear(); //clean up, but hold instance
		return;
	}

	//create instance for header buttons chain object and add to container
	if (cch_btn_obj == NULL){
		dprintf(DEBUG_DEBUG, "[CComponentsHeader]\n    [%s - %d] init header buttons...\n", __func__, __LINE__);
		cch_btn_obj = new CComponentsIconForm(this);
	}

	//set button form properties
	if (cch_btn_obj){
		cch_btn_obj->setDimensionsAll(0, cch_items_y, 0, 0);
		cch_btn_obj->doPaintBg(false);
		cch_btn_obj->setAppendOffset(cch_buttons_space, 0);
		cch_btn_obj->removeAllIcons();
		cch_btn_obj->addIcon(v_cch_btn);

		//set corner mode of button item
		int cc_btn_corner_type = corner_type;
		if (corner_type == CORNER_TOP_RIGHT || corner_type == CORNER_TOP)
			cc_btn_corner_type = CORNER_TOP_RIGHT;
		else
			cc_btn_corner_type = CORNER_RIGHT;
		cch_btn_obj->setCorner(corner_rad-fr_thickness, cc_btn_corner_type);

		//global adapt height
		height = max(height, cch_btn_obj->getHeight());

		//re-align height of button object
		cch_btn_obj->setHeight(height);

		//re-align height of icon object
		if (cch_icon_obj)
			cch_icon_obj->setHeight(height);
	}
}

void CComponentsHeader::initCaption()
{
	//recalc header text position if header icon is defined
	int cc_text_w = 0;
	if (!cch_icon_name.empty()){
		cch_text_x = cch_icon_x+cch_icon_w+cch_offset;
	}

	//calc width of text object in header
	cc_text_w = width-cch_text_x-cch_offset;
	int buttons_w = 0;
	if (cch_btn_obj){
		//get width of buttons object
		buttons_w = cch_btn_obj->getWidth();
		//set x position of buttons
		cch_btn_obj->setXPos(width - buttons_w);
	}
	//set required width of caption object
	cc_text_w -= buttons_w-cch_offset;

	//create cch_text_obj and add to collection
	if (cch_text_obj == NULL){
		dprintf(DEBUG_DEBUG, "[CComponentsHeader]\n    [%s - %d] init header text: %s [ x %d w %d ]\n", __func__, __LINE__, cch_text.c_str(), cch_text_x, cc_text_w);
		cch_text_obj = new CComponentsText();
	}

	//add text item
	if (!cch_text_obj->isAdded())
		addCCItem(cch_text_obj); //text

	//set header text properties
	if (cch_text_obj){
			//set alignment of text item in dependency from text alignment
		if (cch_caption_align == CTextBox::CENTER)
			cch_text_x = CC_CENTERED;
		cch_text_obj->setDimensionsAll(cch_text_x, cch_items_y, cc_text_w, height);
		cch_text_obj->doPaintBg(true);
		cch_text_obj->setText(cch_text, cch_caption_align, cch_font);
		cch_text_obj->forceTextPaint(); //here required
		cch_text_obj->setTextColor(cch_col_text);
		cch_text_obj->setColorBody(col_body);

		//corner of text item
		cch_text_obj->setCorner(corner_rad-fr_thickness, corner_type);

		/*
		   global adapt height not needed here again
		   because this object is initialized at last
		*/
		//height = max(height, cch_text_obj->getHeight());
	}
}

void CComponentsHeader::initCCItems()
{
	//set size
	initCaptionFont();

	//init icon
	initIcon();

	//init buttons
	initButtons();

	//init text
	initCaption();
}
	
void CComponentsHeader::paint(bool do_save_bg)
{
	//prepare items
	initCCItems();
	
	//paint form contents
	paintForm(do_save_bg);
}
