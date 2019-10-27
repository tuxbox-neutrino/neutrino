/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2017, Thilo Graf 'dbt'

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
#include "cc_item.h"
#include <string>

//! Sub class of CComponentsItem. Shows box with text and optional icon on screen.
/*!
InfoBox has been originally intended for displaying text information or menue hints,
but is also usable like each other CCItems.
*/

class CComponentsInfoBox : public CComponentsText
{
	private:
		///property: property: space around fram and beetween picture and textbox, see also setSpaceOffset()
		int x_offset;

		///object: picture object
		CComponentsPicture * pic;
		///property: path or default name of displayed image
		std::string pic_default_name;

		///paint picture, used in initVarInfobox()
		void paintPicture();
		///property: path or name of displayed image
		std::string pic_name;

		int pic_height, pic_width;

	public:
		///object: internal used CTextBox object
		CComponentsText * cctext;

		CComponentsInfoBox(	const int& x_pos = 0,
					const int& y_pos = 0,
					const int& w = 800,
					const int& h = 600,
					std::string info_text = "",
					const int mode = CTextBox::AUTO_WIDTH,
					Font* font_text = NULL,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT_TEXT,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		~CComponentsInfoBox();

		///set property: space around fram and beetween picture and textbox
		void setSpaceOffset(const int offset){x_offset = offset;};
		///set property: path or name of displayed image, parameter as string
		void setPicture(const std::string& picture_name, const int& dx = -1, const int& dy = -1);
		///set property: path or name of displayed image, parameter as const char*
		void setPicture(const char* picture_name, const int& dx = -1, const int& dy = -1);
		///retur internal picture object
		CComponentsPicture * getPictureObject(){return pic;}
		///paint item
		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
};

#endif
