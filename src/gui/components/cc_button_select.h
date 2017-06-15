/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2017, Thilo Graf 'dbt'

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

#ifndef __CC_BTN_SEL_H__
#define __CC_BTN_SEL_H__


#include "cc_frm_button.h"

/*!
Class for inheritation of button select handling inside other CComponentsForm objects and their derivations
*/
class CCButtonSelect
{
	protected:
		CComponentsFrmChain *btn_container;

	public:
		CCButtonSelect();

		///returns selected button object, return value as pointer to object, NULL means nothing is selected
		CComponentsButton* getSelectedButtonObject();
		
		///returns pointer to internal button container
		CComponentsFrmChain* getButtonChainObject();

		///returns id of select button, return value as int, -1 = nothing is selected
		int getSelectedButton();

		/**Select a definied button inside button chain object
		* @param[in]	item_id
		* 	@li 	optional: expects type size_t
		* @param[in]	fr_col
		* 	@li 	optional: expects type fb_pixel_t, as default frame color
		* @param[in]	sel_fr_col
		* 	@li 	optional: expects type fb_pixel_t, as selected frame color
		* @param[in]	bg_col
		* 	@li 	optional: expects type fb_pixel_t, as default background color
		* @param[in]	sel_bg_col
		* 	@li 	optional: expects type fb_pixel_t, as selected background color
		* @param[in]	text_col
		* 	@li 	optional: expects type fb_pixel_t, as default text color
		* @param[in]	sel_text_col
		* 	@li 	optional: expects type fb_pixel_t, as selected text color
		* @param[in]	frame_width
		* 	@li 	optional: expects type int, default = 1
		* @param[in]	sel_frame_width
		* 	@li 	optional: expects type int, default = 1
		*/
		void setSelectedButton(size_t item_id,
					const fb_pixel_t& fr_col 	= COL_MENUCONTENTSELECTED_PLUS_2,
					const fb_pixel_t& sel_fr_col 	= COL_MENUCONTENTSELECTED_PLUS_0,
					const fb_pixel_t& bg_col 	= COL_MENUCONTENT_PLUS_0,
					const fb_pixel_t& sel_bg_col 	= COL_MENUCONTENTSELECTED_PLUS_0,
					const fb_pixel_t& text_col 	= COL_MENUCONTENT_TEXT,
					const fb_pixel_t& sel_text_col 	= COL_MENUCONTENTSELECTED_TEXT,
					const int& frame_width 		= 1,
					const int& sel_frame_width 	= 1);
};

#endif //__CC_BTN_SEL_H__
