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

#ifndef __CC_FORM_FOOTER_H__
#define __CC_FORM_FOOTER_H__

#include "cc_frm_header.h"
#include "cc_frm_button.h"
#include <gui/widget/buttons.h> //for compatibility with 'button_label' type

//for 'button_label' type with string
typedef struct button_label_s
{
	const char *		button;
	std::string 		text;
	neutrino_msg_t 		btn_msg;
	int 			btn_result;
	int 			btn_alias;
} button_label_s_struct;

typedef struct button_label_l
{
	const char *      	button;
	neutrino_locale_t 	locale;
	neutrino_msg_t		btn_msg;
	int 			btn_result;
	int 			btn_alias;
} button_label_l_struct;

/*!
CComponentsFooter, sub class of CComponentsHeader provides prepared container for footer
It's usable like a header but without caption, and context button icons as default.
Additional implemented is a button container (chain) and some methods which can add
buttons via struct, vector or text and icon parameters. Also a compatible but limited methode
to add button labels known by older button handler, to find in gui/widget/buttons.h/cpp.
functionality. Why limited ?: old parameter 'struct button_label' doesn't provide newer parameters. 
Missing parameters are filled with default values and must be assigned afterward, if required.
*/
class CComponentsFooter : public CComponentsHeader
{
	private:
		void initVarFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h = 0,
					const int& buttons = 0,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_INFOBAR_SHADOW_PLUS_1,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		///show button frame and background, default false
		bool btn_contour;

		///property: set font for label caption, see also setButtonFont()
		Font* ccf_btn_font;

		///container for button objects
		CComponentsFrmChain *chain;

	public:
		CComponentsFooter(CComponentsForm *parent = NULL);
		CComponentsFooter(	const int& x_pos, const int& y_pos, const int& w, const int& h = 0,
					const int& buttons = 0,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_body = COL_INFOBAR_SHADOW_PLUS_1,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		///add button labels with string label type as content, count as size_t, chain_width as int, label width as int
		void setButtonLabels(const struct button_label_s * const content, const size_t& label_count, const int& chain_width = 0, const int& label_width = 0);
		///add button labels with locale label type as content, count as size_t, chain_width as int, label width as int
		void setButtonLabels(const struct button_label_l * const content, const size_t& label_count, const int& chain_width = 0, const int& label_width = 0);
		///add button labels with locale label type as content, parameter 1 as vector, chain_width as int, label width as int
		void setButtonLabels(const std::vector<button_label_l>v_content, const int& chain_width, const int& label_width);
		///add button labels with string label type as content, parameter 1 as vector, chain_width as int, label width as int
		void setButtonLabels(const std::vector<button_label_s>v_content, const int& chain_width, const int& label_width);

		///add button labels with old label type, count as size_t, chain_width as int, label width as int
		///NOTE: for compatibility with older button handler find in gui/widget/buttons.h, if possible, don't use this
		void setButtonLabels(const struct button_label * const content, const size_t& label_count, const int& chain_width = 0, const int& label_width = 0);

		///add single button label with string label type as content, chain_width as int, label width as int
		void setButtonLabel(const char *button_icon, const std::string& text, const int& chain_width = 0, const int& label_width = 0, const neutrino_msg_t& msg = CRCInput::RC_nokey, const int& result_value = -1, const int& alias_value = -1);
		///add single button label with locale label type as content, chain_width as int, label width as int
		void setButtonLabel(const char *button_icon, const neutrino_locale_t& locale, const int& chain_width = 0, const int& label_width = 0, const neutrino_msg_t& msg = CRCInput::RC_nokey, const int& result_value = -1, const int& alias_value = -1);
		
		///causes show/hide countour of button frame and background, parameter bool show, default= true
		void showButtonContour(bool show = true);

		///select a definied button, parameter1 as size_t
		void setSelectedButton(size_t item_id);
		///returns id of select button, return value as int, -1 = nothing is selected
		int getSelectedButton();
		///returns selected button object, return value as pointer to object, NULL means nothing is selected
		CComponentsButton* getSelectedButtonObject();

		///property: set font for label caption, parameter as font object, value NULL causes usage of dynamic font
		void setButtonFont(Font* font){ccf_btn_font = font;};

		///returns pointer to internal button container
		CComponentsFrmChain* getButtonChainObject(){return chain;};

		///this is a nearly methode similar with the older button handler find in gui/widget/buttons.h, some parameters are different, but require minimalized input
		///this member sets some basic parameters and will paint concurrently on execute, explicit call of paint() is not required
		void paintButtons(	const int& x_pos,
					const int& y_pos,
					const int& w,
					const int& h,
					const size_t& label_count,
					const struct button_label * const content,
					const int& label_width = 0,
					const int& context_buttons = 0,
					Font* font = NULL,
					bool do_save_bg = CC_SAVE_SCREEN_NO
				);
};

#endif
