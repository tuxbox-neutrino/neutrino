/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

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

#include "cc_frm_button.h"

#define FRAME_TH 3
#define H_SPACE 4

using namespace std;

CComponentsButton::CComponentsButton( 	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& caption, const std::string& icon_name,
					bool selected, bool enabled, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarButton();
	cc_btn_icon	= icon_name;
	cc_btn_capt	= caption;
	cc_btn_capt_col	= COL_MENUCONTENT_TEXT;
	cc_btn_text_w 	= cc_btn_font->getRenderWidth(cc_btn_capt, true);
	cc_btn_text_h	= cc_btn_font->getHeight();
	
	x 		= x_pos;
	y 		= y_pos;
	width 		= max(w, cc_btn_text_w);
	height	 	= max(h, cc_btn_text_h);
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	cc_item_enabled  = enabled;
	cc_item_selected = selected;
	fr_thickness 	= FRAME_TH;	
}

void CComponentsButton::initVarButton()
{
	initVarForm();
	cc_item_type 	= CC_ITEMTYPE_BUTTON;
	cc_btn_icon_obj	= NULL;
	cc_btn_capt_obj = NULL;
	cc_btn_font	= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];
	cc_btn_icon	= "";
	cc_btn_capt	= "";
	cc_btn_text_w	= 0;
	cc_btn_text_h	= 0;
}

void CComponentsButton::initIcon()
{
	//init cch_icon_obj only if an icon available
	if (cc_btn_icon.empty()) {
		if (cc_btn_icon_obj)
			delete cc_btn_icon_obj;
		cc_btn_icon_obj = NULL;
		return;
	}

	//initialize icon object
	if (cc_btn_icon_obj == NULL){
		cc_btn_icon_obj = new CComponentsPicture(0, 0, 0, 0, cc_btn_icon);
		addCCItem(cc_btn_icon_obj);
	}

	//get first icon dimensions
	int icon_w = cc_btn_icon_obj->getWidth();
	int icon_h = cc_btn_icon_obj->getHeight();

	//position of icon default centered
	int icon_x = width/2-icon_w/2;
	int icon_y = height/2-icon_h/2;

	//set properties to picture object
	if (cc_btn_icon_obj){
		cc_btn_icon_obj->setPos(icon_x, icon_y);
		cc_btn_icon_obj->setPictureAlign(CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
		cc_btn_icon_obj->doPaintBg(false);
	}
	
}

void CComponentsButton::initCaption()
{
	if (cc_btn_capt_obj == NULL){
		cc_btn_capt_obj = new CComponentsLabel();

		addCCItem(cc_btn_capt_obj);
	}
	
	//text position is default centere
	int cap_x = (width/2)-(cc_btn_text_w/2);
	int cap_y = (height/2)-(cc_btn_text_h/2); 

	//if we have a icon, then we must calculate centered position for booth items together
	if (cc_btn_icon_obj){
		int face_w = cc_btn_icon_obj->getWidth() + H_SPACE + cc_btn_text_w + 2*fr_thickness;
		int face_x = width/2 - face_w/2;
		cc_btn_icon_obj->setXPos(face_x);
		cap_x = face_x + cc_btn_icon_obj->getWidth() + H_SPACE;
	}

	//set properties to label object
	if (cc_btn_capt_obj){			
		cc_btn_capt_obj->setDimensionsAll(cap_x, cap_y, width-cap_x, height);
		cc_btn_capt_obj->setTextColor(this->cc_item_enabled ? COL_MENUCONTENT_TEXT : COL_MENUCONTENTINACTIVE_TEXT);
		cc_btn_capt_obj->setText(cc_btn_capt, CTextBox::NO_AUTO_LINEBREAK, cc_btn_font);
		cc_btn_capt_obj->forceTextPaint(); //here required;
		cc_btn_capt_obj->doPaintBg(false);

		//corner of text item
		cc_btn_capt_obj->setCornerRadius(corner_rad-fr_thickness);
		cc_btn_capt_obj->setCornerType(corner_type);
	}
}

void CComponentsButton::initCCBtnItems()
{
	initIcon();

	initCaption();
}


void CComponentsButton::paint(bool do_save_bg)
{
	//prepare items before paint
	initCCBtnItems();

	//paint form contents
	paintForm(do_save_bg);
}
