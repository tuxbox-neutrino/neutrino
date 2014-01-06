/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, 2014, Thilo Graf 'dbt'

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

#ifndef __CC_FORM_HEADER_H__
#define __CC_FORM_HEADER_H__


#include "cc_frm.h"
#include "cc_item_picture.h"
#include "cc_item_text.h"
#include "cc_frm_icons.h"

class CComponentsHeader : public CComponentsForm
{
	private:
		void initVarHeader();
	protected:
		CComponentsPicture * cch_icon_obj;
		CComponentsText * cch_text_obj;
		CComponentsIconForm * cch_btn_obj;
		std::string cch_text;
		const char*  cch_icon_name;
		fb_pixel_t cch_col_text;
		Font* cch_font;
		int cch_items_y, cch_icon_x, cch_icon_w, cch_text_x, cch_buttons, cch_buttons_w, cch_buttons_h, cch_buttons_space, cch_offset;
		std::vector<std::string> v_cch_btn;
		int cch_size_mode;
		int cch_caption_align;
		bool userHeight;

		void initIcon();
		void initCaption();
		void initButtons();
		void initDefaultButtons();
		void initButtonFormSize();

	public:
		enum
		{
			CC_BTN_HELP = 0x02,
			CC_BTN_INFO = 0x04,
			CC_BTN_MENU = 0x40,
			CC_BTN_EXIT = 0x80

		};

		enum
		{
			CC_HEADER_ITEM_ICON 	= 0,
			CC_HEADER_ITEM_TEXT 	= 1,
			CC_HEADER_ITEM_BUTTONS	= 2
		};

		enum
		{
			CC_HEADER_SIZE_LARGE 	= 0,
			CC_HEADER_SIZE_SMALL 	= 1
		};
		CComponentsHeader();
		CComponentsHeader(const int x_pos, const int y_pos, const int w, const int h = 0, const std::string& caption = "header", const char* icon_name = NULL, const int buttons = 0, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		CComponentsHeader(const int x_pos, const int y_pos, const int w, const int h = 0, neutrino_locale_t caption_locale = NONEXISTANT_LOCALE, const char* icon_name = NULL, const int buttons = 0,bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		virtual ~CComponentsHeader();

		
		virtual void setCaption(const std::string& caption, const int& align_mode = CTextBox::NO_AUTO_LINEBREAK);
		virtual void setCaption(neutrino_locale_t caption_locale, const int& align_mode = CTextBox::NO_AUTO_LINEBREAK);
		virtual void setCaptionAlignment(const int& align_mode){cch_caption_align = align_mode;};
		virtual void setCaptionFont(Font* font_name);
		virtual void setCaptionColor(fb_pixel_t text_color){cch_col_text = text_color;};
		virtual void setOffset(const int offset){cch_offset = offset;};
		virtual void setIcon(const char* icon_name);
		virtual void addButtonIcon(const std::string& button_name);
		virtual void removeButtonIcons();
		virtual void setDefaultButtons(const int buttons);
		virtual void setButtonsSpace(const int buttons_space){cch_buttons_space = buttons_space;};
		virtual void initCCItems();
		virtual void setSizeMode(const int& size_mode){cch_size_mode = size_mode;};
		virtual CComponentsText* getTextObject(){return cch_text_obj;};
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};


class CComponentsFooter : public CComponentsHeader
{
	protected:
		void initVarFooter();
	public:
		CComponentsFooter();
		CComponentsFooter(	const int x_pos, const int y_pos, const int w, const int h = 0,
					const int buttons = 0,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_INFOBAR_SHADOW_PLUS_1, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
};

#endif
