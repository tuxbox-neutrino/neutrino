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

#ifndef __CC_FORM_WINDOW_H__
#define __CC_FORM_WINDOW_H__

#include "cc_frm.h"
#include "cc_frm_icons.h"
#include "cc_frm_header.h"
#include "cc_frm_footer.h"

//! Sub class of CComponentsForm. Shows a window with prepared items.
/*!
CComponentsWindow provides prepared items like header, footer and a container for
items like text, labels, pictures ...
*/
/*
        x
       y+-------------------------------------------------------+
        |icon        caption                            buttons |header (ccw_head)
        +-x-----------------------------------------------------+
        |cc_item0                                               |
        |cc_item1                                               |body (ccw_body)
        |               add items here directly with            |
        |               addWindowItem() or                      |
        y               with ccw_body->addCCItem()              |
        |               Note: x/y related to body object        |
        |                                                       |
        +-------------------------------------------------------+
        |        add cc_items with ccw_footer->addCCItem()      |footer(ccw_footer)
        +-------------------------------------------------------+

*/

class CComponentsWindow : public CComponentsForm
{
	protected:
		///object: header object, to get access to header properties see also getHeaderObject()
		CComponentsHeader * ccw_head;
		///object: left sidebar chain object, this is a container that contains sidebar items
		CComponentsFrmChain * ccw_left_sidebar;
		///object: right sidebar chain object, this is a container that contains sidebar items
		CComponentsFrmChain * ccw_right_sidebar;
		///object: body object, this is the container for all needed items, to add with addWindowItem()
		CComponentsForm * ccw_body;
		///object: footer object, to get access to header properties see also getFooterObject(
		CComponentsFooter * ccw_footer;
		///property: caption in header, see also getHeaderObject()
		std::string ccw_caption;
		///property: alignment mode for header caption
		int ccw_align_mode;
		///property: icon name in header, see also getHeaderObject()
		std::string ccw_icon_name;
		///property: assigned default icon buttons in header, see also getHeaderObject()
		int ccw_buttons;
		///property: value = true, let show footer, see showFooter()
		bool ccw_show_footer;
		///property: value = true, let show header, see showHeader()
		bool ccw_show_header;
		///property: value = true, let show left sidebar, see enableSideBar()
		bool ccw_show_l_sideber;
		///property: value = true, let show right sidebar, see enableSidebar()
		bool ccw_show_r_sideber;
		///width of sidebars
		int ccw_w_sidebar;

		///initialze header object
		void initHeader();
		///initialze left sidebar object
		void initLeftSideBar();
		///initialze right sidebar object
		void initRightSideBar();
		///initialze body object
		void initBody();
		///initialze footer object
		void initFooter();
		///initialze all window objects at once
		void initCCWItems();
		///initialize all attributes
		void initVarWindow(	const int& x_pos = CC_CENTERED, const int& y_pos = CC_CENTERED, const int& w = 0, const int& h = 0,
					const std::string& caption = "",
					const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		///initialize width and height
		void initWindowSize();
		///initialize position
		void initWindowPos();

	public:
		enum
		{
			CC_WINDOW_ITEM_HEADER 	= 0
		};
		///simple constructor for CComponentsWindow, this shows a window over full screen
		CComponentsWindow(CComponentsForm *parent = NULL);

		///advanced constructor for CComponentsWindow, provides parameters for the most required properties, and caption as string, x_pos or y_pos = 0 will center window
		CComponentsWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption = "",
					const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		///advanced constructor for CComponentsWindow, provides parameters for the most required properties, and caption from locales, x_pos or y_pos = 0 will center window
		CComponentsWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					neutrino_locale_t locale_text = NONEXISTANT_LOCALE,
					const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		///add item to body object, also usable is addCCItem() to add items to the windo object
		void addWindowItem(CComponentsItem* cc_Item);

		///allow/disallow paint a footer, default true, see also ccw_show_footer, showHeader()
		void showFooter(bool show = true){ccw_show_footer = show; initCCWItems();};
		///allow/disallow paint a header, default true, see also ccw_show_header, showFooter()
		void showHeader(bool show = true){ccw_show_header = show; initCCWItems();};

		enum
		{
			CC_WINDOW_LEFT_SIDEBAR 	= 1,
			CC_WINDOW_RIGHT_SIDEBAR = 2
		};
		///allow/disallow paint a sidebar, default are enabled
		void enableSidebar(const int& sidbar_type = CC_WINDOW_LEFT_SIDEBAR | CC_WINDOW_RIGHT_SIDEBAR);

		///set caption in header with string, see also getHeaderObject()
		void setWindowCaption(const std::string& text, const int& align_mode = CTextBox::NO_AUTO_LINEBREAK){ccw_caption = text; ccw_align_mode = align_mode;};

		///set caption in header from locales, see also getHeaderObject()
		void setWindowCaption(neutrino_locale_t locale_text, const int& align_mode = CTextBox::NO_AUTO_LINEBREAK);
		///set caption alignment, see CTextBox for possible modes
		void setWindowCaptionAlignment(const int& align_mode){ccw_align_mode = align_mode;};

		///set icon name in header, see also getHeaderObject()
		void setWindowIcon(const std::string& iconname){ccw_icon_name = iconname; initHeader();};

		///set default header icon buttons, see also getHeaderObject()
		void setWindowHeaderButtons(const int& buttons){ccw_buttons = buttons;};

		///returns a pointer to the internal header object, use this to get access to header properities
		CComponentsHeader* getHeaderObject(){return ccw_head;};

		///returns a pointer to the internal body object, use this to get access to body properities
		CComponentsForm* getBodyObject(){return ccw_body;};
		///returns a pointer to the internal footer object, use this to get access to footer properities
		CComponentsFooter* getFooterObject(){return ccw_footer;};

		///returns a pointer to the internal left side bar object, use this to get access to left sidebar properities
		CComponentsFrmChain* getLeftSidebarObject(){return ccw_left_sidebar;};
		///returns a pointer to the internal right side bar object, use this to get access to right sidebar properities
		CComponentsFrmChain* getRightSidebarObject(){return ccw_right_sidebar;};
		///sets width of sidebars
		void setWidthSidebar(const int& sidebar_width){ccw_w_sidebar = sidebar_width; initCCWItems();};

		///sets current page, NOTE: this is simliar with setCurrentPage() known from basic class CComponentsForm, but here it is related only for window body object
		void setCurrentPage(const u_int8_t& current_page);
		///get current page, NOTE: this is simliar with getCurrentPage() known from basic class CComponentsForm, but here it is related only for window body object
		u_int8_t getCurrentPage();

		///paint window body items, this paints only the current page, body = page, current page is definied in body object, see setCurrentPage()
		void paintCurPage(bool do_save_bg = CC_SAVE_SCREEN_NO);
		///paint defined page of body, parameter number 0...n
		void paintPage(const u_int8_t& page_number, bool do_save_bg = CC_SAVE_SCREEN_NO);
		///set width of body scrollbar
		void setScrollBarWidth(const int& scrollbar_width);

		///refresh position and dimension and reinitialize elemenatary properties
		void Refresh(){initCCWItems();};

		///paint all window items, this overwriting paint() from CComponentsForm
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

class CComponentsWindowMax : public CComponentsWindow
{
	public:
		///simple constructor for CComponentsWindow, provides parameters for caption as string and icon, this shows a centered window based up current screen settings
		CComponentsWindowMax(	const std::string& caption, const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		///simple constructor for CComponentsWindow, provides parameters for caption from locales and icon, this shows a centered window based up current screen settings
		CComponentsWindowMax(	neutrino_locale_t locale_caption, const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
};

#endif
