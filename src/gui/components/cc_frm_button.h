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

#ifndef __CC_BUTTONS_H__
#define __CC_BUTTONS_H__

#include "config.h"
#include "cc.h"
#include "cc_frm.h"
#include <string>


class CComponentsButton : public CComponentsForm
{
	protected:
		void initVarButton();

		///caption and icon  properties
		std::string cc_btn_capt;  	//text
		std::string cc_btn_icon;  	//icon name, only icons supported, to find in gui/widget/icons.h
		fb_pixel_t cc_btn_capt_col; 	//text color
		Font* cc_btn_font;		//text font
		int cc_btn_text_w, cc_btn_text_h; //width and height of text, too long text will be truncated

		///icon and text objects
		CComponentsPicture *cc_btn_icon_obj;
		CComponentsLabel *cc_btn_capt_obj;

		///initialize of objects
		void initIcon();
		void initCaption();
		void initCCBtnItems();
	
	public:
		///basic constructor for button object with most needed params, no button icon is definied here
		CComponentsButton(	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& caption, const std::string& icon_name,
					bool selected = false, bool enabled = true, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_LIGHT_GRAY, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		
		virtual void setButtonTextColor(fb_pixel_t caption_color){cc_btn_capt_col = caption_color;};
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

///sub classes for button objects with most needed params, and predefined color buttons, but functionality is the same as in CComponentsButton
class CComponentsButtonRed : public CComponentsButton
{
	public:
		CComponentsButtonRed(	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& caption,
					bool selected = false, bool enabled = true, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_LIGHT_GRAY, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption, NEUTRINO_ICON_BUTTON_RED, selected, enabled, has_shadow, color_frame, color_body, color_shadow)
		{
			cc_item_type 	= CC_ITEMTYPE_BUTTON_RED;
		};
};

class CComponentsButtonGreen : public CComponentsButton
{
	public:
		CComponentsButtonGreen(	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& caption,
					bool selected = false, bool enabled = true, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_LIGHT_GRAY, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption, NEUTRINO_ICON_BUTTON_GREEN, selected, enabled, has_shadow, color_frame, color_body, color_shadow)
		{
			cc_item_type 	= CC_ITEMTYPE_BUTTON_GREEN;
		};
};

class CComponentsButtonYellow : public CComponentsButton
{
	public:
		CComponentsButtonYellow(	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& caption,
					bool selected = false, bool enabled = true, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_LIGHT_GRAY, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption, NEUTRINO_ICON_BUTTON_YELLOW, selected, enabled, has_shadow, color_frame, color_body, color_shadow)
		{
			cc_item_type 	= CC_ITEMTYPE_BUTTON_YELLOW;
		};
};

class CComponentsButtonBlue : public CComponentsButton
{
	public:
		CComponentsButtonBlue(	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& caption,
					bool selected = false, bool enabled = true, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_LIGHT_GRAY, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption, NEUTRINO_ICON_BUTTON_BLUE, selected, enabled, has_shadow, color_frame, color_body, color_shadow)
		{
			cc_item_type 	= CC_ITEMTYPE_BUTTON_BLUE;
		};
};


#endif	/*__CC_BUTTONS_H__*/
