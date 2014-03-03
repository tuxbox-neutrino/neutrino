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

//! Sub class of CComponentsForm. Shows a header with prepared items.
/*!
CComponentsHeader provides prepared items like icon, caption and context button icons, mostly for usage in menues or simple windows
*/
class CComponentsHeader : public CComponentsForm
{
	private:
		///member: init genaral variables, parameters for mostly used properties
		void initVarHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h = 0,
					const std::string& caption = "header",
					const std::string& = "",
					const int& buttons = 0,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUHEAD_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

	protected:
		///object: icon object, see also setIcon()
		CComponentsPicture * cch_icon_obj;
		///object: caption object, see also setCaption()
		CComponentsText * cch_text_obj;
		///object: context button object, see also addButtonIcon(), removeButtonIcons()
		CComponentsIconForm * cch_btn_obj;

		///property: caption text, see also setCaption()
		std::string cch_text;
		///property: icon name, see also setIcon()
		std::string  cch_icon_name;
		///property: caption text color, see also setCaptionColor()
		fb_pixel_t cch_col_text;
		///property: caption font, see also setCaptionFont()
		Font* cch_font;

		///property: internal y-position for all items
		int cch_items_y;
		///property: internal x-position for icon object
		int cch_icon_x;
		///property: internal width for icon object
		int cch_icon_w;
		///property: internal x-position for caption object
		int cch_text_x;
		///property: internal context button definition button icons, see modes CC_BTN_HELP, CC_BTN_INFO, CC_BTN_MENU, CC_BTN_EXIT
		int cch_buttons;
		///property: internal width for context button object
		int cch_buttons_w;
		///property: internal height for context button object
		int cch_buttons_h;
		///property: internal offset of context button icons within context button object
		int cch_buttons_space;
		///property: internal offset for header items
		int cch_offset;
		///property: internal container of icon names for context button object, see also addButtonIcon()
		std::vector<std::string> v_cch_btn;
		///property: size of header, possible values are CC_HEADER_SIZE_LARGE, CC_HEADER_SIZE_SMALL
		int cch_size_mode;
		///property: alignment of caption within header, see also setCaptionAlignment(), possible values are CTextBox::CENTER, default = CTextBox::NO_AUTO_LINEBREAK (left)
		int cch_caption_align;

		///init font object and recalculates height if required
		void initCaptionFont(Font* font = NULL);
		///sub: init icon object
		void initIcon();
		///sub: init caption object
		void initCaption();
		///sub: init context button object
		void initButtons();
		///sub: init default buttons for context button object
		void initDefaultButtons();
		///sub: init default buttons for context button object
		void initButtonFormSize();

	public:
		enum
		{
			CC_HEADER_ITEM_ICON 	= 0,
			CC_HEADER_ITEM_TEXT 	= 1,
			CC_HEADER_ITEM_BUTTONS	= 2
		};

		CComponentsHeader(CComponentsForm *parent = NULL);
		CComponentsHeader(	const int& x_pos, const int& y_pos, const int& w, const int& h = 0,
					const std::string& caption = "",
					const std::string& = "",
					const int& buttons = 0,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUHEAD_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		virtual ~CComponentsHeader();

		///set caption text, parameters: string, int align_mode (default left) 
		virtual void setCaption(const std::string& caption, const int& align_mode = CTextBox::NO_AUTO_LINEBREAK);
		///set caption text, parameters: loacle, int align_mode (default left)
		virtual void setCaption(neutrino_locale_t caption_locale, const int& align_mode = CTextBox::NO_AUTO_LINEBREAK);

		///set alignment of caption within header, possible paramters are CTextBox::CENTER, CTextBox::NO_AUTO_LINEBREAK
		virtual void setCaptionAlignment(const int& align_mode){cch_caption_align = align_mode;};
		///set text font object for caption
		virtual void setCaptionFont(Font* font);
		///set text color for caption
		virtual void setCaptionColor(fb_pixel_t text_color){cch_col_text = text_color;};

		///set offset between items
		virtual void setOffset(const int offset){cch_offset = offset;};
		///set name of icon
		virtual void setIcon(const char* icon_name);
		///set name of icon
		virtual void setIcon(const std::string& icon_name);

		///add separate button icons to context button object
		virtual void addButtonIcon(const std::string& button_name);
		///remove button icons from context button object
		virtual void removeButtonIcons();
		
		enum
		{
			CC_BTN_HELP = 0x02,
			CC_BTN_INFO = 0x04,
			CC_BTN_MENU = 0x40,
			CC_BTN_EXIT = 0x80

		};
		///set internal context button icons, possible modes CC_BTN_HELP, CC_BTN_INFO, CC_BTN_MENU, CC_BTN_EXIT
		virtual void setDefaultButtons(const int buttons);
		///set offset between icons within context button object
		virtual void setButtonsSpace(const int buttons_space){cch_buttons_space = buttons_space;};

		enum
		{
			CC_HEADER_SIZE_LARGE 	= 0,
			CC_HEADER_SIZE_SMALL 	= 1
		};
		///set size of header, possible values are CC_HEADER_SIZE_LARGE, CC_HEADER_SIZE_SMALL
		virtual void setSizeMode(const int& size_mode){cch_size_mode = size_mode; initCCItems();};

		///init all items within header object
		virtual void initCCItems();
		///returns the text object
		virtual CComponentsText* getTextObject(){return cch_text_obj;};

		///paint header
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

//! Sub class of CComponentsHeader.
/*!
CComponentsHeaderLocalized provides prepared items like icon, caption and context button icons, mostly for usage in menues or simple windows
Caption is defined with locales.
*/
class CComponentsHeaderLocalized : public CComponentsHeader
{
	public:
		CComponentsHeaderLocalized(	const int& x_pos, const int& y_pos, const int& w, const int& h = 0,
						neutrino_locale_t caption_locale = NONEXISTANT_LOCALE,
						const std::string& = "",
						const int& buttons = 0,
						CComponentsForm *parent = NULL,
						bool has_shadow = CC_SHADOW_OFF,
						fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
						fb_pixel_t color_body = COL_MENUHEAD_PLUS_0,
						fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
};

#endif
