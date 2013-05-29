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

#ifndef __CC_FORM_H__
#define __CC_FORM_H__


#include "config.h"
#include <gui/components/cc.h>
#include <gui/components/cc_item_text.h>
#include <gui/components/cc_item_picture.h>
#include <vector>
#include <string>


class CComponentsForm : public CComponentsItem
{
	protected:
		std::vector<CComponentsItem*>	v_cc_items;			
		void initVarForm();
		void paintForm(bool do_save_bg);
	public:
		
		CComponentsForm();
		CComponentsForm(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		virtual ~CComponentsForm();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
		virtual void addCCItem(CComponentsItem* cc_Item);
		virtual void insertCCItem(const uint& cc_item_id, CComponentsItem* cc_Item);
		virtual void removeCCItem(const uint& cc_item_id);
		virtual void replaceCCItem(const uint& cc_item_id, CComponentsItem* new_cc_Item);
		virtual void replaceCCItem(CComponentsItem* old_cc_Item, CComponentsItem* new_cc_Item);
		virtual int getCCItemId(CComponentsItem* cc_Item);
		virtual CComponentsItem* getCCItem(const uint& cc_item_id);
		virtual void paintCCItems();
		virtual	void clearCCItems();
		virtual void cleanCCForm();
};

class CComponentsIconForm : public CComponentsForm
{
	private:
		std::vector<std::string> v_icons;
		int ccif_offset, ccif_icon_align;
		void initMaxHeight(int *pheight);

	protected:
 		void initVarIconForm();

	public:
		CComponentsIconForm();
		CComponentsIconForm(const int x_pos, const int y_pos, const int w, const int h, const std::vector<std::string> &v_icon_names, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
// 		~CComponentsIconForm(); //inherited from CComponentsForm

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void initCCIcons();
		void addIcon(const std::string& icon_name);
		void addIcon(std::vector<std::string> icon_name);
		void removeIcons(){v_icons.clear();};
		void insertIcon(const uint& icon_id, const std::string& icon_name);
		void removeIcon(const uint& icon_id);
		void removeIcon(const std::string& icon_name);
		void removeAllIcons();
		void setIconOffset(const int offset){ccif_offset = offset;};

		enum //alignements
		{
			CC_ICONS_FRM_ALIGN_RIGHT ,
			CC_ICONS_FRM_ALIGN_LEFT
		};
		void setIconAlign(int alignment){ccif_icon_align = alignment;};

		int getIconId(const std::string& icon_name);
};



class CComponentsHeader : public CComponentsForm
{
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

		void initIcon();
		void initCaption();
		void initButtons();
		void initDefaultButtons();
		void initButtonFormSize();

		void initVarHeader();

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
		CComponentsHeader();
		CComponentsHeader(const int x_pos, const int y_pos, const int w, const int h = 0, const std::string& caption = "header", const char* icon_name = NULL, const int buttons = 0, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		CComponentsHeader(const int x_pos, const int y_pos, const int w, const int h = 0, neutrino_locale_t caption_locale = NONEXISTANT_LOCALE, const char* icon_name = NULL, const int buttons = 0,bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		virtual ~CComponentsHeader();

		
		virtual void setCaption(const std::string& caption);
		virtual void setCaption(neutrino_locale_t caption_locale);
		virtual void setCaptionFont(Font* font_name);
		virtual void setCaptionColor(fb_pixel_t text_color){cch_col_text = text_color;};
		virtual void setOffset(const int offset){cch_offset = offset;};
		virtual void setIcon(const char* icon_name);
		virtual void addButtonIcon(const std::string& button_name);
		virtual void removeButtonIcons();
		virtual void setDefaultButtons(const int buttons);
		virtual void setButtonsSpace(const int buttons_space){cch_buttons_space = buttons_space;};
		virtual void initCCItems();

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

class CComponentsWindow : public CComponentsForm
{
	private:
		CComponentsHeader * ccw_head;
		std::string ccw_caption;
		const char* ccw_icon_name;
		int ccw_start_y;
		int ccw_buttons;

		void initHeader();
		void initCCWItems();

	protected:
		void initVarWindow();

	public:
		enum
		{
			CC_WINDOW_ITEM_HEADER 	= 0
		};
		CComponentsWindow();
		CComponentsWindow(const std::string& caption, const char* iconname = NULL);
		CComponentsWindow(neutrino_locale_t locale_caption, const char* iconname = NULL);
		~CComponentsWindow();

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void setWindowCaption(const std::string& text){ccw_caption = text;};
		void setWindowCaption(neutrino_locale_t locale_text);
		void setWindowIcon(const char* iconname){ccw_icon_name = iconname;};
		void setWindowHeaderButtons(const int& buttons){ccw_buttons = buttons;};

		int getStartY(); //y value for start of the area below header
};

#endif
