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

#include <config.h>
#include "cc_base.h"
#include "cc_frm_chain.h"
#include "cc_item_picture.h"
#include "cc_item_text.h"
#include <gui/widget/icons.h>
#include <string>
#include <driver/neutrinofonts.h>
#include <driver/neutrino_msg_t.h>

#define COL_BUTTON_BODY COL_MENUFOOT_PLUS_0
#define COL_BUTTON_TEXT_ENABLED COL_MENUCONTENTSELECTED_PLUS_0
#define COL_BUTTON_TEXT_DISABLED COL_MENUCONTENTINACTIVE_PLUS_0

//! Sub class of CComponentsForm.
/*!
Shows a button box with caption and optional icon.
*/
class CComponentsButton : public CComponentsFrmChain, public CCTextScreen
{
	protected:
		///object: picture object
		CComponentsPicture *cc_btn_icon_obj;
		///object: label object
		CComponentsLabel *cc_btn_text_obj;

		///initialize all required attributes and objects
		void initVarButton(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					const std::string& icon_name,
					CComponentsForm* parent,
					bool selected,
					bool enabled,
					int shadow_mode,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow);

		///property: button text as string, see also setCaption() and getCaptionString()
		std::string cc_btn_text;
		///property: button text as locale, see also setCaption() and getCaptionLocale()
		neutrino_locale_t cc_btn_text_locale;

		///property: icon name, only icons supported, to find in gui/widget/icons.h
		std::string cc_btn_icon;

		///property: container for all assigned event message values, see driver/rcinput.h for possible values, default value = CRCInput::RC_nokey, see also setButtonDirectKey(), hasButtonDirectKey()
		std::vector<neutrino_msg_t>cc_directKeys;
		///property: assigned return value, see also setButtonResult(), getButtonResult(), default value = -1 (not defined)
		int 	cc_btn_result;
		///property: assigned alias value, see also setButtonAlias(), getButtonAlias(), default value = -1 (not defined)
		int 	cc_btn_alias;

		///property: text color
		fb_pixel_t cc_btn_text_col;
		///property: text color for disabled button
		fb_pixel_t cc_btn_text_disable_col;
		///object: text font
		Font* cc_btn_font;
		///object: dynamic font object handler
		CNeutrinoFonts 	*cc_btn_dy_font;

		///initialize picture object
		void initIcon();
		///initialize label object
		void initCaption();

		///initialize picture and label object
		void initCCBtnItems();

	public:
		///basic constructor for button object with most needed params, no button icon is definied here
		CComponentsButton(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					const std::string& icon_name = "",
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		CComponentsButton(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const neutrino_locale_t& caption_locale,
					const std::string& icon_name = "",
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		CComponentsButton(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const neutrino_locale_t& caption_locale,
					const char* icon_name = NULL,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		CComponentsButton(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					const char* icon_name = NULL,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		///set text color
		void setButtonTextColor(fb_pixel_t caption_color){cc_btn_text_col = caption_color;};

		/**Member to modify background behavior of embeded caption object.
		* @param[in]  mode
		* 	@li bool, default = true
		* @return
		*	void
		* @see
		* 	Parent member: CCTextScreen::enableTboxSaveScreen()
		* 	CTextBox::enableSaveScreen()
		* 	disableTboxSaveScreen()
		*/
		void enableTboxSaveScreen(bool mode)
		{
			if (cc_txt_save_screen == mode)
				return;
			cc_txt_save_screen = mode;
			for(size_t i=0; i<v_cc_items.size(); i++){
				if (v_cc_items[i]->getItemType() == CC_ITEMTYPE_LABEL)
					static_cast<CComponentsLabel*>(v_cc_items[i])->enableTboxSaveScreen(cc_txt_save_screen);
			}
		};

		///set caption: parameter as string
		void setCaption(const std::string& text);
		///set caption: parameter as locale
		void setCaption(const neutrino_locale_t locale_text);

		///get caption, type as std::string
		std::string getCaptionString(){return cc_btn_text;};
		///get loacalized caption id, type = neutrino_locale_t
		neutrino_locale_t getCaptionLocale(){return cc_btn_text_locale;};

		///property: set font for label caption, parameter as font object, value NULL causes usaage of dynamic font
		void setButtonFont(Font* font){cc_btn_font = font; initCCBtnItems();};

		///reinitialize items
		void Refresh(){initCCBtnItems();};

		///paint button object
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);

		/**
		* Assigns a single event msg value to button object
		* @param[in]	neutrino_msg_t
		* 	@li 	excepts type msg_result_t
		* @note		This method adds only one message key value into the cc_directKeys container, \n
		* 		Already existant keys will be removed. When more than one key value is required, \n
		* 		use setButtonDirectKeys().
		* @see		setButtonDirectKeys(), driver/rcinput.h for possible values
		*/

		void setButtonDirectKey(const neutrino_msg_t& msg){cc_directKeys.clear(); cc_directKeys.push_back(msg);}
		/**
		* Assigns a container with any event msg values to button object
		* @param[in]	v_directKeys
		* 	@li 	excepts type std::vector<neutrino_msg_t>
		* @note		This method adds any message key values into the cc_directKeys container, \n
		* 		Already existant keys will be removed. When only one key value is required, \n
		* 		use setButtonDirectKey().
		* @see		driver/rcinput.h for possible values
		*/
		void setButtonDirectKeys(const std::vector<neutrino_msg_t> &v_directKeys){cc_directKeys = v_directKeys;}

		/**
		* Returns current primary event msg value of button object.
		* @return	neutrino_msg_t \n
		* 		If no key is avaliable return value = RC_NOKEY.
		* @param[in]	id
		* 	@li 	expects type size_t as item id from cc_directKeys container of the button object \n
		* 		Default parameter is 0. This returns the first existant message value from  cc_directKeys container. \n
		* 		If required use getKeySize() to get current count of avalable keys.
		* @see		bool hasButtonDirectKey(), size_t getKeySize(), driver/rcinput.h for possible values
		*/
		neutrino_msg_t getButtonDirectKey(const size_t& id = 0)
		{
			for (size_t i= 0; i< cc_directKeys.size(); i++){
				if (i == 0){
					if (cc_directKeys[i] != RC_NOKEY)
						return cc_directKeys[i];
				}else{
					if (cc_directKeys[i] == id)
						return cc_directKeys[i];
				}
			}
			return RC_NOKEY;
		}

		/**
		 * Returns count of existant direct keys of cc_directKeys container.
		 * @return	size_t
		 * @see		getButtonDirectKey(), driver/rcinput.h for possible values
		 */
		size_t getKeySize() {return cc_directKeys.size();}

		/**
		* Returns true if defined parameter event msg value of button object is found in cc_directKeys container.
		* @return	bool
		* @param[in]	msg
		* 	@li 	expects type neutrino_msg_t as filter for searched message
		* @see		neutrino_msg_t getButtonDirectKey(), driver/rcinput.h for possible values
		*/
		bool hasButtonDirectKey(const neutrino_msg_t& msg)
		{
			for (size_t i= 0; i< cc_directKeys.size(); i++)
				if (cc_directKeys[i] == msg)
					return true;
			return false;
		}

		///assigns an return value to button object, parameter1 as int
		void setButtonResult(const int& result_value){cc_btn_result = result_value;}
		///returns current result value of button object
		int getButtonResult(){return cc_btn_result;}

		///assigns an alias value to button object, parameter1 as int, e.g. previous known as mbYes, mbNo... from message boxes 
		void setButtonAlias(const int& alias_value){cc_btn_alias = alias_value;}
		///returns an alias value from button object, see also cc_btn_alias
		int getButtonAlias(){return cc_btn_alias;}
};

//! Sub class of CComponentsButton.
/*!
Shows a button box with caption and prepared red icon.
*/
class CComponentsButtonRed : public CComponentsButton
{
	public:
		CComponentsButtonRed(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption, NEUTRINO_ICON_BUTTON_RED, parent, selected, enabled, shadow_mode, color_frame, color_body, color_shadow)
		{
			cc_item_type.id 	= CC_ITEMTYPE_BUTTON_RED;
			cc_item_type.name 	= "cc_base_red_button";
		};
		CComponentsButtonRed(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const neutrino_locale_t& caption_locale,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption_locale, NEUTRINO_ICON_BUTTON_RED, parent, selected, enabled, shadow_mode, color_frame, color_body, color_shadow)
		{
			cc_item_type.id 	= CC_ITEMTYPE_BUTTON_RED;
			cc_item_type.name 	= "cc_base_localized_red_button";
		};
};

//! Sub class of CComponentsButton.
/*!
Shows a button box with caption and prepared green icon.
*/
class CComponentsButtonGreen : public CComponentsButton
{
	public:
		CComponentsButtonGreen(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption, NEUTRINO_ICON_BUTTON_GREEN, parent, selected, enabled, shadow_mode, color_frame, color_body, color_shadow)
		{
			cc_item_type.id 	= CC_ITEMTYPE_BUTTON_GREEN;
			cc_item_type.name 	= "cc_base_green_button";

		};
		CComponentsButtonGreen(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const neutrino_locale_t& caption_locale,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption_locale, NEUTRINO_ICON_BUTTON_GREEN, parent, selected, enabled, shadow_mode, color_frame, color_body, color_shadow)
		{
			cc_item_type.id 	= CC_ITEMTYPE_BUTTON_GREEN;
			cc_item_type.name 	= "cc_base_localized_green_button";
		};
};

//! Sub class of CComponentsButton.
/*!
Shows a button box with caption and prepared yellow icon.
*/
class CComponentsButtonYellow : public CComponentsButton
{
	public:
		CComponentsButtonYellow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption, NEUTRINO_ICON_BUTTON_YELLOW, parent, selected, enabled, shadow_mode, color_frame, color_body, color_shadow)
		{
			cc_item_type.id 	= CC_ITEMTYPE_BUTTON_YELLOW;
			cc_item_type.name 	= "cc_base_yellow_button";
		};
		CComponentsButtonYellow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const neutrino_locale_t& caption_locale,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption_locale, NEUTRINO_ICON_BUTTON_YELLOW, parent, selected, enabled, shadow_mode, color_frame, color_body, color_shadow)
		{
			cc_item_type.id		= CC_ITEMTYPE_BUTTON_YELLOW;
			cc_item_type.name 	= "cc_base_localized_yellow_button";
		};
};

//! Sub class of CComponentsButton.
/*!
Shows a button box with caption and prepared blue icon.
*/
class CComponentsButtonBlue : public CComponentsButton
{
	public:
		CComponentsButtonBlue(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const std::string& caption,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption, NEUTRINO_ICON_BUTTON_BLUE, parent, selected, enabled, shadow_mode, color_frame, color_body, color_shadow)
		{
			cc_item_type.id		= CC_ITEMTYPE_BUTTON_BLUE;
			cc_item_type.name 	= "cc_base_blue_button";
		};
		CComponentsButtonBlue(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const neutrino_locale_t& caption_locale,
					CComponentsForm *parent = NULL,
					bool selected = false,
					bool enabled = true,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0, fb_pixel_t color_body = COL_BUTTON_BODY, fb_pixel_t color_shadow = COL_SHADOW_PLUS_0)
					:CComponentsButton(x_pos, y_pos, w, h, caption_locale, NEUTRINO_ICON_BUTTON_BLUE, parent, selected, enabled, shadow_mode, color_frame, color_body, color_shadow)
		{
			cc_item_type.id 	= CC_ITEMTYPE_BUTTON_BLUE;
			cc_item_type.name 	= "cc_base_localized_blue_button";
		};
};

#endif	/*__CC_BUTTONS_H__*/
