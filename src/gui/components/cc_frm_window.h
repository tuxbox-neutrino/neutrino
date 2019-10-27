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

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CC_FORM_WINDOW_H__
#define __CC_FORM_WINDOW_H__

#include "cc_frm.h"
#include "cc_frm_icons.h"
#include "cc_frm_header.h"
#include "cc_frm_footer.h"

#define CCW_PERCENT - //placeholder for negative sign '-', used for discret dimensions parameters


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

class CComponentsWindow : public CComponentsForm, CCHeaderTypes
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
		cc_title_alignment_t ccw_align_mode;
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
		///header bg color
		fb_pixel_t ccw_col_head;
		///header text color
		fb_pixel_t ccw_col_head_text;
		///footer bg color
		fb_pixel_t ccw_col_footer;
		///footer heigh, default defined by footer object itself
		int ccw_h_footer;
		///footer button font
		Font*	ccw_button_font;

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
		void init(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					const std::string& iconname,
					CComponentsForm *parent,
					const int& shadow_mode,
					const fb_pixel_t& color_frame,
					const fb_pixel_t& color_body,
					const fb_pixel_t& color_shadow,
					const int& frame_width
			);
		///initialize width and height
		void initWindowSize();
		///initialize position
		void initWindowPos();

		///returns true, if current page is changed, see also: setCurrentPage()
		bool isPageChanged();


	public:
		enum
		{
			CC_WINDOW_ITEM_HEADER 	= 0
		};

		enum
		{
			CC_WINDOW_LEFT_SIDEBAR 	= 1,
			CC_WINDOW_RIGHT_SIDEBAR = 2
		};


		/**simple constructor for CComponentsWindow, this shows a window over full screen
		* @param[in]	parent
		* 	@li 	optional: expects type CComponentsForm * as possible parent object, default = NULL
		*/
		CComponentsWindow(CComponentsForm *parent = NULL);

		/**
		* advanced constructor for CComponentsWindow, provides parameters for the most required properties, and caption as string, x_pos or y_pos = 0 will center window
		* @param[in]	x_pos
		* 	@li 	expects type const &int, defines x position on screen
		* @param[in]	y_pos
		* 	@li 	expects type const &int, defines y position on screen
		* @param[in]	w
		* 	@li 	expects type const &int, width of window, Note: value = 0 uses full screen
		* @param[in]	h
		* 	@li 	expects type const &int, height of window, Note: value = 0 uses full screen
		* @param[in]	caption
		* 	@li 	optional: expects type const std::string&, defines title of window header
		* @param[in]	iconname
		* 	@li 	optional: expects type const std::string&, defines icon name of window header
		* @param[in]	parent
		* 	@li 	optional: expects type CComponentsForm * as possible parent object, default = NULL
		* @param[in]	shadow_mode
		* 	@li 	optional: expects type int as mode, default = CC_SHADOW_OFF \n
		* 		possible values are \n
		* 		CC_SHADOW_ON = (CC_SHADOW_RIGHT | CC_SHADOW_BOTTOM | CC_SHADOW_CORNER_BOTTOM_LEFT | CC_SHADOW_CORNER_BOTTOM_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT) \n
		* 		Take a look into cc_types.h
		* @param[in]	color_frame
		* 	@li 	optional: expects type fb_pixel_t, defines frame color, default = COL_FRAME_PLUS_0
		* @param[in]	color_body
		* 	@li 	optional: expects type fb_pixel_t, defines color color, default = COL_MENUCONTENT_PLUS_0
		* @param[in]	color_shadow
		* 	@li 	optional: expects type fb_pixel_t, defines shadow color, default = COL_SHADOW_PLUS_0
		* @param[in]	frame_width
		* 	@li 	optional: expects type int, defines frame width around, default = 0
		*
		* @note		Discret dimensions parameters: values < 0 to -100 will be interpreted as percent values related to screen.
		* 		For better readability please use placeholder 'CCW_PERCENT' as negative sign '-' \n
		* 		Example: \n
		* 		this inits a window with position x100 y100 on screen with dimensions 700px x 800px \n
		* 		CComponentsWindow win(100, 100, 700, 800, "Test window");\n
		*		this inits a window with position x100 y100 on screen with 50% of screen size assigned with discret percental screen dimensions \n
		* 		CComponentsWindow win(100, 100, CCW_PERCENT 50, CCW_PERCENT 50, "Test window");
		*/
		CComponentsWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption = "",
					const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					const fb_pixel_t& color_frame = COL_FRAME_PLUS_0,
					const fb_pixel_t& color_body = COL_MENUCONTENT_PLUS_0,
					const fb_pixel_t& color_shadow = COL_SHADOW_PLUS_0,
					const int& frame_width = 0);

		/**
		* advanced constructor for CComponentsWindow, provides parameters for the most required properties
		* @param[in]	locale_text
		* 	@li 	optional: expects type neutrino_locale_t, defines title of window header
		* @see		for other parameters take a look to CComponentsWindow base class above
		*/
		CComponentsWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					neutrino_locale_t locale_text = NONEXISTANT_LOCALE,
					const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					const fb_pixel_t& color_frame = COL_FRAME_PLUS_0,
					const fb_pixel_t& color_body = COL_MENUCONTENT_PLUS_0,
					const fb_pixel_t& color_shadow = COL_SHADOW_PLUS_0,
					const int& frame_width = 0);

		virtual ~CComponentsWindow(){};

		/**
		* Add an item to body object, also usable is addCCItem() to add items to the window object
		* @param[in]	cc_Item
		* 	@li 	expects type CComponentsItem* , defines a cc item
		* @return	Returns item ID
		* @see		Take a look to cc_types.h for possible types.
		*/
		int addWindowItem(CComponentsItem* cc_Item);

		/**
		* enable/disable paint of footer, default true
		* @param[in]	show
		* 	@li 	expects type bool, default = true
		* @see		ccw_show_footer, showHeader()
		*/
		void showFooter(bool show = true){ccw_show_footer = show; initCCWItems();}

		/**
		* enable/disable paint of header, default true
		* @param[in]	show
		* 	@li 	expects type bool, default = true
		* @see		ccw_show_header, showFooter()
		*/
		void showHeader(bool show = true){ccw_show_header = show; initCCWItems();}

		/**
		* enable/disable paint of sidebar,
		* @param[in]	show
		* 	@li 	optional: expects type const int&, default = enabled
		*/
		void enableSidebar(const int& sidbar_type = CC_WINDOW_LEFT_SIDEBAR | CC_WINDOW_RIGHT_SIDEBAR);

		/**
		* sets title text in header
		* @param[in]	text
		* 	@li 	expects type const std::string&, defines title of window header
		* @param[in]	align_mode
		* 	@li 	optional: expects type const cc_title_alignment_t&, defines allignment of title text
		* @see		CTextBox for alignment modes
		*/
		void setWindowCaption(const std::string& text, const cc_title_alignment_t& align_mode = DEFAULT_TITLE_ALIGN){ccw_caption = text; ccw_align_mode = align_mode;}

		/**
		* sets title text in header
		* @param[in]	text
		* 	@li 	expects type neutrino_locale_t
		* @param[in]	align_mode
		* 	@li 	optional: expects type const cc_title_alignment_t&, defines allignment of title text
		* @see		CTextBox for alignment modes
		*/
		void setWindowCaption(neutrino_locale_t locale_text, const cc_title_alignment_t& align_mode = DEFAULT_TITLE_ALIGN);

		/**
		* Sets header text color
		* @param[in]	const fb_pixel_t&
		* 	@li 	expects type const fb_pixel_t&
		*/
		void setWindowHeaderTextColor(const fb_pixel_t& color){ccw_col_head_text = color;}

		/**
		* Sets header background color
		* @param[in]	const fb_pixel_t&
		* 	@li 	expects type const fb_pixel_t&
		*/
		void setWindowHeaderColor(const fb_pixel_t& color){ccw_col_head = color;}

		/**
		* sets title text alignment
		* @param[in]	align_mode
		* 	@li 	expects type const cc_title_alignment_t&
		* @see		CTextBox for alignment modes
		*/
		void setWindowCaptionAlignment(const cc_title_alignment_t& align_mode){ccw_align_mode = align_mode;};

		/**
		* Sets icon name of window header.
		* @param[in]	iconname
		* 	@li 	expects type const std::string&
		*/
		void setWindowIcon(const std::string& iconname){ccw_icon_name = iconname; initHeader();};

		///set default header icon buttons, see also getHeaderObject()
		/**
		* Sets context buttons in window header.
		* @param[in]	buttons
		* 	@li 	expects type const int&
		* @note		possible types are:
		* 		CC_BTN_HELP,
				CC_BTN_INFO,
				CC_BTN_MENU,
				CC_BTN_EXIT,
				CC_BTN_MUTE_ZAP_ACTIVE,
				CC_BTN_MUTE_ZAP_INACTIVE,
				CC_BTN_OKAY,
				CC_BTN_MUTE,
				CC_BTN_UP,
				CC_BTN_DOWN,
				CC_BTN_LEFT,
				CC_BTN_RIGHT,
				CC_BTN_FORWARD,
				CC_BTN_BACKWARD,
				CC_BTN_PAUSE,
				CC_BTN_PLAY,
				CC_BTN_RECORD_ACTIVE,
				CC_BTN_RECORD_INACTIVE,
				CC_BTN_RECORD_STOP,
		* @see		cc_frm_header.h for current types
		*/
		void setWindowHeaderButtons(const int& buttons){ccw_buttons = buttons;}

		/**
		* Gets a pointer to the internal header object, use this to get direct access to header properities
		* @return	CComponentsHeader*
		*/
		CComponentsHeader* getHeaderObject(){return ccw_head;}

		/**
		* Gets a pointer to the internal body object, use this to get access to body properities
		* @return	CComponentsForm*
		*/
		CComponentsForm* getBodyObject(){return ccw_body;}

		/**
		* Gets a pointer to the internal footer object, use this to get access to footer properities
		* @return	CComponentsFooter*
		*/
		CComponentsFooter* getFooterObject(){return ccw_footer;}

		/**
		* Sets footer background color
		* @param[in]	color
		* 	@li 	expects type const fb_pixel_t&
		*/
		void setWindowFooterColor(const fb_pixel_t& color){ccw_col_footer = color;}

		/**
		* Sets font for footer buttons
		* @param[in]	font_type
		* 	@li 	expects type Font*
		*/
		void setWindowFooterFont(Font* font_type){ccw_button_font = font_type;}

		/**
		* Gets a pointer to the internal left side bar object, use this to get access to left sidebar properities
		* @return	CComponentsFrmChain*
		*/
		CComponentsFrmChain* getLeftSidebarObject(){return ccw_left_sidebar;}

		/**
		* Gets a pointer to the internal right side bar object, use this to get access to right sidebar properities
		* @return	CComponentsFrmChain*
		*/
		CComponentsFrmChain* getRightSidebarObject(){return ccw_right_sidebar;}

		/**
		* Sets width of sidebars
		* @param[in]	sidebar_width
		* 	@li 	expects type const int&
		*/
		void setWidthSidebar(const int& sidebar_width){ccw_w_sidebar = sidebar_width; initCCWItems();}

		/**
		* Sets current page number
		* @param[in]	sidebar_width
		* 	@li 	expects type const int&
		* @note		This is simliar to setCurrentPage() known from basic class CComponentsForm, but here it is related only for window body object.
		*/
		void setCurrentPage(const uint8_t& current_page);

		/**
		* Gets current page number
		* @return	CComponentsFrmChain*
		* @note		This simliar to getCurrentPage() known from basic class CComponentsForm, but here it is related only for window body object
		*/
		uint8_t getCurrentPage();

		/**
		* Paints window body items, this paints only the current page, body = page, current page is definied in body object, see setCurrentPage()
		* @param[in]	do_save_bg
		* 	@li 	optional: expects type bool, default = CC_SAVE_SCREEN_NO (false), sets background save mode
		*/
		void paintCurPage(const bool &do_save_bg = CC_SAVE_SCREEN_NO);

		/**
		* Paints defined page of body, parameter number 0...n
		* @param[in]	page_number
		* 	@li 	expects type const uint8_t& as page number
		* @param[in]	do_save_bg
		* 	@li 	optional: expects type bool, default = CC_SAVE_SCREEN_NO (false), sets background save mode
		*/
		void paintPage(const uint8_t& page_number, const bool &do_save_bg = CC_SAVE_SCREEN_NO);

		/**
		* enable/disable page scroll
		* @param[in]	mode
		* 	@li 	optional: expects type const int&, default enabled for up/down keys, only for body!
		*/
		void enablePageScroll(const int& mode = PG_SCROLL_M_UP_DOWN_KEY);

		/**
		* Sets width of body scrollbar
		* @param[in]	crollbar_width
		* 	@li 	expects type const int&
		*/
		void setScrollBarWidth(const int& scrollbar_width);

		/**
		* Reinit position and dimensions and reinitialize mostly elemenatary properties
		*/
		void Refresh(){initCCWItems();}

		/**
		* Paint window
		* @param[in]	do_save_bg
		* 	@li 	optional: expects type bool, sets background save mode
		*/
		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);

		/**
		* Adds an additional exec key to current collection, default exit keys are CRCInput::RC_home and CRCInput::RC_setup
		* @param[in]	key
		* 	@li 	expects type const neutrino_msg_t&
		* @see		river/rcinput.h for possible keys
		*/
		virtual void addExitKey(const neutrino_msg_t& key){getBodyObject()->addExitKey(key);}

		/**
		* Removes all current exec keys from current collection.
		* @note 	use addExitKey() if new exec key is required
		*/
		virtual void removeExitKeys(){getBodyObject()->removeExitKeys();}

		/**
		* Sets an image for window background.
		* @note 	The assigned image is assigned into body object! Main container, header and footer will be not touched.
		*/
		bool setBodyBGImage(const std::string& image_path);
};

class CComponentsWindowMax : public CComponentsWindow
{
	public:
		/**
		* Simple constructor for CComponentsWindow, this shows only a centered window based up current screen settings
		* @see		for other parameters take a look to CComponentsWindow base class above
		* @param[in]	caption
		* 	@li 	expects type const std::string&, defines title of window header
		* @see		for other parameters take a look to CComponentsWindow base class above
		*/
		CComponentsWindowMax(	const std::string& caption, const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					const fb_pixel_t& color_frame = COL_FRAME_PLUS_0,
					const fb_pixel_t& color_body = COL_MENUCONTENT_PLUS_0,
					const fb_pixel_t& color_shadow = COL_SHADOW_PLUS_0,
					const int& frame_width = 0);

		/**
		* Simple constructor for CComponentsWindow, this shows only a centered window based up current screen settings
		* @see		for other parameters take a look to CComponentsWindow base class above
		* @param[in]	locale_text
		* 	@li 	expects type neutrino_locale_t, defines title of window header
		* @see		for other parameters take a look to CComponentsWindow base class above
		*/
		CComponentsWindowMax(	neutrino_locale_t locale_caption, const std::string& iconname = "",
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					const fb_pixel_t& color_frame = COL_FRAME_PLUS_0,
					const fb_pixel_t& color_body = COL_MENUCONTENT_PLUS_0,
					const fb_pixel_t& color_shadow = COL_SHADOW_PLUS_0,
					const int& frame_width = 0);
};

#endif
