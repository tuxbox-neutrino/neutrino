/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

	License: GPL

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc.h"

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsHeader inherit from CComponentsForm
CComponentsHeader::CComponentsHeader()
{
	//CComponentsHeader
	initVarHeader();
}

CComponentsHeader::CComponentsHeader(	const int x_pos, const int y_pos, const int w, const int h, const std::string& caption, const char* icon_name, const int buttons, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsHeader
	initVarHeader();

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h > 0 ? h : g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	
	cch_text	= caption;
	cch_icon_name	= icon_name;
	cch_buttons	= buttons;
	initCCHDefaultButtons();
	initCCHItems();
}

CComponentsHeader::CComponentsHeader(	const int x_pos, const int y_pos, const int w, const int h, neutrino_locale_t caption_locale, const char* icon_name, const int buttons, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsHeader
	initVarHeader();
	
	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h;
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	
	cch_locale_text = caption_locale;
	cch_icon_name	= icon_name;
	cch_buttons	= buttons;
	initCCHDefaultButtons();
	initCCHItems();
}

#if 0 
CComponentsHeader::~CComponentsHeader()
{
	hide();
	clearSavedScreen();
	clearCCItems();
	clear();
}
#endif

void CComponentsHeader::initVarHeader()
{
	//CComponentsHeader
	cch_font 		= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	cch_icon_obj		= NULL;
	cch_text_obj		= NULL;
	cch_icon_name		= NULL;
	cch_btn_obj		= NULL;
	cch_text		= "header";
	cch_locale_text 	= NONEXISTANT_LOCALE;
	cch_col_text		= COL_MENUHEAD;
	cch_items_y 		= 0;
	cch_icon_x 		= 0;
	cch_icon_w		= 5;
	cch_text_x		= 0;
	ccif_width 		= 0;
	cch_buttons		= 0;
	cch_btn_offset		= 8;
	v_cch_btn.clear();	
	
	//CComponentsForm
	initVarForm();
	cc_item_type 		= CC_ITEMTYPE_FRM_HEADER;
	height 			= cch_font->getHeight();	
	col_body 		= COL_MENUHEAD_PLUS_0;
	corner_rad		= RADIUS_LARGE,
	corner_type		= CORNER_TOP;
}

void CComponentsHeader::setHeaderText(const std::string& caption)
{
	cch_text	= caption;
}

void CComponentsHeader::setHeaderText(neutrino_locale_t caption_locale)
{
	cch_text	= g_Locale->getText(caption_locale);
}

void CComponentsHeader::setHeaderIcon(const char* icon_name)
{
	cch_icon_name 	= icon_name;
}

void CComponentsHeader::initCCHeaderIcon()
{
	if (cch_icon_name == NULL) {
 		cch_icon_w = cch_btn_offset;
		return;
	}

	if (cch_icon_name)
		cch_icon_obj = new CComponentsPicture(cch_icon_x, cch_items_y, 0, 0, cch_icon_name);
	cch_icon_obj->setWidth(height-2*fr_thickness);
	cch_icon_obj->setHeight(height);
	cch_icon_obj->setPictureAlign(CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
	cch_icon_obj->doPaintBg(false);

	//corner of icon item
	cch_icon_obj->setCornerRadius(corner_rad-fr_thickness);
	int cc_icon_corner_type = corner_type;
	if (corner_type == CORNER_TOP_LEFT || corner_type == CORNER_TOP)
		cc_icon_corner_type = CORNER_TOP_LEFT;
	else
		cc_icon_corner_type = CORNER_LEFT;
	cch_icon_obj->setCornerType(cc_icon_corner_type);

	//set width of icon object
	cch_icon_w = cch_icon_obj->getWidth();
}

void CComponentsHeader::addHeaderButton(const std::string& button_name)
{
	if (cch_btn_obj){
		delete cch_btn_obj;
		cch_btn_obj = NULL;
	}
	v_cch_btn.push_back(button_name);
	initCCHeaderButtons();
}

void CComponentsHeader::removeHeaderButtons()
{
	v_cch_btn.clear();
	if (cch_btn_obj){
		delete cch_btn_obj;
		cch_btn_obj = NULL;
	}
}

void CComponentsHeader::initCCHDefaultButtons()
{
	if (cch_buttons & CC_BTN_EXIT)
		v_cch_btn.push_back(NEUTRINO_ICON_BUTTON_HOME);
	if (cch_buttons & CC_BTN_HELP)
		v_cch_btn.push_back(NEUTRINO_ICON_BUTTON_HELP);
	if (cch_buttons & CC_BTN_INFO)
		v_cch_btn.push_back(NEUTRINO_ICON_BUTTON_INFO);
	if (cch_buttons & CC_BTN_MENU)
		v_cch_btn.push_back(NEUTRINO_ICON_BUTTON_MENU);
}

// calculate minimal width of icon form
void CComponentsHeader::initCCButtonFormSize()
{
	ccif_width = 0;
	for(size_t i=0; i<v_cch_btn.size(); i++){
		int bw, bh;
		frameBuffer->getIconSize(v_cch_btn[i].c_str(), &bw, &bh);
		ccif_width += (bw + cch_btn_offset);
	}
}

void CComponentsHeader::initCCHeaderButtons()
{
	//exit if no button defined
	if (v_cch_btn.empty())
		return;
	
	initCCButtonFormSize();

	cch_btn_obj = new CComponentsIconForm();
	cch_btn_obj->setDimensionsAll(0+width-ccif_width, 0, ccif_width-cch_btn_offset, height);
	cch_btn_obj->doPaintBg(false);
	cch_btn_obj->setIconOffset(cch_btn_offset);
	cch_btn_obj->setIconAlign(CComponentsIconForm::CC_ICONS_FRM_ALIGN_RIGHT);
	cch_btn_obj->removeAllIcons();
	cch_btn_obj->addIcon(v_cch_btn);
}

void CComponentsHeader::initCCHeaderText()
{
	cch_text_x = cch_icon_x+cch_icon_w;
	cch_text_obj = new CComponentsText(cch_text_x, cch_items_y, width-cch_icon_w-fr_thickness, height-2*fr_thickness, cch_text.c_str());
	cch_text_obj->setTextFont(cch_font);
	cch_text_obj->setTextColor(cch_col_text);
	cch_text_obj->setColorBody(col_body);
	cch_text_obj->doPaintBg(false);
	
	//corner of text item
	cch_text_obj->setCornerRadius(corner_rad-fr_thickness);
	cch_text_obj->setCornerType(corner_type);		
}

void CComponentsHeader::initCCHItems()
{
#if 0
	//clean up first possible old item objects, includes delete and clean up vector
	clearCCItems();
#endif

	//init icon
	initCCHeaderIcon();

	//init text
	initCCHeaderText();

	//init buttons
	initCCHeaderButtons();

	//add elements
	if (cch_icon_obj)
		addCCItem(cch_icon_obj); //icon
	if (cch_text_obj)
		addCCItem(cch_text_obj); //text
	if (cch_btn_obj)
		addCCItem(cch_btn_obj); //buttons

}
	
void CComponentsHeader::paint(bool do_save_bg)
{
	//paint body
	paintInit(do_save_bg);

	//paint
	paintCCItems();
}
