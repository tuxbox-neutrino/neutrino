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

#ifndef __CC_ITEM_INFOBOX__
#define __CC_ITEM_INFOBOX__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc_item_text.h"
#include "cc_item_picture.h"
#include <string>

//! Sub class of CComponentsItem. Shows box with text and optional icon on screen.
/*!
InfoBox has been originally intended for displaying text information or menue hints,
but is also usable like each other CCItems.
*/

#define INFO_BOX_Y_OFFSET	2
class CComponentsInfoBox : public CComponentsText
{
	private:
		///property: start position of textbox within infobox
		int x_text;
		///property: space between picture and textbox
		int x_offset;

		///object: picture object
		CComponentsPicture * pic;
		///property: path or default name of displayed image
		std::string pic_default_name;

		///initialize all needed default attributes
		void initVarInfobox();
		///paint picture, used in initVarInfobox()
		void paintPicture();
		///property: path or name of displayed image
		std::string pic_name;
		
	public:
		///object: internal used CTextBox object
		CComponentsText * cctext;

		CComponentsInfoBox();
		CComponentsInfoBox(	const int x_pos, const int y_pos, const int w, const int h,
					std::string info_text = "", const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT, fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		
		~CComponentsInfoBox();

		///set property: space between picture and textbox
		inline void setSpaceOffset(const int offset){x_offset = offset;};
		///set property: path or name of displayed image
		inline void setPicture(const std::string& picture_name){pic_name = picture_name;};

		///paint item
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

#endif
